import { useState, useEffect, useRef, useCallback } from 'react';
import {
  CLIENT_NODES, SLAVE_NODES, MASTER_POS,
  PRIORITY_COLORS, DISPATCH_SCHEDULE, T, mkLog,
} from '../utils/constants';

// ── Helpers ───────────────────────────────────────────────────────────────────
let _pid = 0;
const uid = () => `p${++_pid}-${Date.now()}`;

function initSim() {
  return {
    // Master is a separate node (not one of the 10 slaves)
    master: { status: 'online', offset: 0, term: 1, electingFrom: null },
    // 10 slave nodes — always 10
    slaves: SLAVE_NODES.map(n => ({ ...n, offset: 0, status: 'online', stale: false, votes: 0 })),
    // Priority queues (in-memory, wiped on master crash)
    queue:  { Critical: [], Standard: [], Low: [] },
    // In-flight packets (all stages)
    packets: [],
    // Stats
    writeCount: 0,
    readCount:  0,
    // Logs
    logs: [
      mkLog('Cluster online. Master + 10 slaves registered. 5 clients ready.', 'success'),
      mkLog('PriorityReplicationEngine: 70/20/10 dispatch schedule active.', 'info'),
    ],
    // Dispatcher state
    schedIdx:    0,
    lastDispatch: 0,
    strongMap:   {},  // { writeId: { acks, required } }
  };
}

// ── Hook ──────────────────────────────────────────────────────────────────────
export function useClusterState() {
  const S       = useRef(initSim());          // single mutable sim object
  const logRef  = useRef(null);               // DOM ref for log scroll
  const [snap, setSnap] = useState(S.current); // snapshot for rendering

  // Push a log entry
  function log(msg, tone = 'info') {
    const s = S.current;
    s.logs = [...s.logs.slice(-300), mkLog(msg, tone)];
  }

  // Scroll logs on update
  useEffect(() => {
    if (logRef.current) logRef.current.scrollTop = logRef.current.scrollHeight;
  }, [snap.logs]);

  // ── Slave offset catchup ────────────────────────────────────────────────────
  useEffect(() => {
    const id = setInterval(() => {
      const s = S.current;
      if (s.master.status !== 'online') return;
      const mo = s.master.offset;
      s.slaves = s.slaves.map(sl => {
        if (sl.status !== 'online') return sl;
        const step = Math.random() > 0.4 ? 1 : 0;
        const off  = Math.min(mo, sl.offset + step);
        return { ...sl, offset: off, stale: (mo - off) > 50 };
      });
    }, 600);
    return () => clearInterval(id);
  }, []);

  // ── Main simulation tick ────────────────────────────────────────────────────
  useEffect(() => {
    const id = setInterval(() => {
      const s   = S.current;
      const now = Date.now();

      // 1. Advance packets + collect arrivals
      const arrived = [];
      const next    = [];
      for (const p of s.packets) {
        const dur  = p.duration ?? T.TRAVEL;
        const prog = Math.min(1, (now - p.startedAt) / dur);
        if (p.stage === 'done') {
          if (now - p.doneAt < T.DONE_TTL) next.push(p);
        } else if (prog >= 1) {
          arrived.push(p);
          next.push({ ...p, progress: 1, stage: 'done', doneAt: now });
        } else {
          next.push({ ...p, progress: prog });
        }
      }
      s.packets = next;

      // 2. Process arrivals
      const newPkts = [];
      for (const p of arrived) {

        // ── Write arrives at Master ─────────────────────────────────────────
        if (p.stage === 'fly_write') {
          if (s.master.status === 'online') {
            s.queue[p.priority] = [...s.queue[p.priority], { ...p, enqueuedAt: now }];
          } else {
            log(`[LOST] Write from ${p.clientId} — master offline`, 'error');
          }
        }

        // ── Read arrives at Slave ───────────────────────────────────────────
        else if (p.stage === 'fly_read') {
          const sl = s.slaves.find(x => x.id === p.targetId);
          const staleRejected = sl && sl.stale && p.consistency !== 'Bounded Staleness';
          if (staleRejected) {
            log(`[STALE_DATA] ${p.targetId}: lag ${s.master.offset - (sl?.offset ?? 0)} > 50. Rejected.`, 'warning');
          } else {
            s.readCount++;
            log(`[GET → ${p.targetId}] ${sl?.stale ? 'OK (stale tolerated)' : 'OK'} consistency:${p.consistency}`, 'success');
          }
          const cNode = CLIENT_NODES.find(c => c.id === p.clientId) ?? CLIENT_NODES[0];
          newPkts.push({
            id: uid(), stage: 'fly_resp', progress: 0, startedAt: now,
            duration: T.RESPONSE,
            from: { x: sl?.x ?? MASTER_POS.x, y: sl?.y ?? MASTER_POS.y },
            to:   { x: cNode.x, y: cNode.y },
            color: staleRejected ? '#f87171' : '#34d399',
          });
        }

        // ── Replication arrives at Slave ────────────────────────────────────
        else if (p.stage === 'fly_repl') {
          s.slaves = s.slaves.map(sl =>
            sl.id === p.targetId
              ? { ...sl, offset: Math.max(sl.offset, p.writeOffset) }
              : sl
          );
          if (p.consistency === 'Strong' && s.strongMap[p.writeId]) {
            s.strongMap[p.writeId].acks++;
            const sm = s.strongMap[p.writeId];
            if (sm.acks >= sm.required && !sm.resolved) {
              sm.resolved = true;
              log(`[QUORUM MET] offset ${p.writeOffset}: ${sm.acks}/${sm.required} ACKs. OK → client.`, 'success');
              const cNode = CLIENT_NODES.find(c => c.id === sm.clientId) ?? CLIENT_NODES[0];
              newPkts.push({
                id: uid(), stage: 'fly_resp', progress: 0, startedAt: now,
                duration: T.RESPONSE,
                from: MASTER_POS,
                to: { x: cNode.x, y: cNode.y },
                color: '#34d399',
              });
            }
          }
        }
        // fly_resp: no action needed on arrival
      }

      s.packets = [...s.packets, ...newPkts];

      // 3. Dispatch: pop one entry from priority queue every T.DISPATCH ms
      if (s.master.status === 'online' && (now - s.lastDispatch) >= T.DISPATCH) {
        let dispatched = null;

        for (let i = 0; i < DISPATCH_SCHEDULE.length; i++) {
          const tier = DISPATCH_SCHEDULE[s.schedIdx % DISPATCH_SCHEDULE.length];
          s.schedIdx++;
          if (s.queue[tier].length > 0) {
            dispatched = s.queue[tier][0];
            s.queue[tier] = s.queue[tier].slice(1);
            break;
          }
        }
        if (!dispatched) {
          for (const tier of ['Critical', 'Standard', 'Low']) {
            if (s.queue[tier].length > 0) {
              dispatched = s.queue[tier][0];
              s.queue[tier] = s.queue[tier].slice(1);
              break;
            }
          }
        }

        if (dispatched) {
          s.master.offset++;
          s.writeCount++;
          s.lastDispatch = now;
          const newOffset = s.master.offset;
          const onSlaves  = s.slaves.filter(sl => sl.status === 'online');
          log(`[DISPATCH] ${dispatched.priority} write → offset ${newOffset} → ${onSlaves.length} replicas`, dispatched.priority === 'Critical' ? 'critical' : 'info');

          // Fan-out replication to all online slaves
          for (const sl of onSlaves) {
            s.packets.push({
              id: uid(), stage: 'fly_repl', progress: 0, startedAt: now,
              duration: T.TRAVEL,
              from: MASTER_POS,
              to:   { x: sl.x, y: sl.y },
              color: PRIORITY_COLORS[dispatched.priority],
              targetId: sl.id,
              writeId:  dispatched.id,
              writeOffset: newOffset,
              consistency: dispatched.consistency,
            });
          }

          if (dispatched.consistency !== 'Strong') {
            const cNode = CLIENT_NODES.find(c => c.id === dispatched.clientId) ?? CLIENT_NODES[0];
            s.packets.push({
              id: uid(), stage: 'fly_resp', progress: 0, startedAt: now,
              duration: T.RESPONSE,
              from: MASTER_POS,
              to: { x: cNode.x, y: cNode.y },
              color: '#34d399',
            });
          } else {
            const required = Math.max(1, Math.floor(onSlaves.length / 2) + 1);
            s.strongMap[dispatched.id] = { acks: 0, required, clientId: dispatched.clientId, resolved: false };
            log(`[STRONG] Waiting for ${required}/${onSlaves.length} ACKs before responding…`, 'warning');
          }
        }
      }

      // 4. Aging: promote packets in queue
      {
        const critNew = [...s.queue.Critical];
        const stdNew  = [];
        const lowNew  = [];

        for (const p of s.queue.Standard) {
          const age = now - (p.agedAt ?? p.enqueuedAt ?? now);
          if (age >= T.AGE_2) {
            log(`[AGING] Standard→Critical: ${p.id.slice(-6)} (${(age/1000).toFixed(1)}s in queue)`, 'warning');
            critNew.push({ ...p, priority: 'Critical', color: PRIORITY_COLORS.Critical, agedAt: now });
          } else {
            stdNew.push(p);
          }
        }
        for (const p of s.queue.Low) {
          const age = now - (p.enqueuedAt ?? now);
          if (age >= T.AGE_1) {
            log(`[AGING] Low→Standard: ${p.id.slice(-6)} (${(age/1000).toFixed(1)}s in queue)`, 'warning');
            stdNew.push({ ...p, priority: 'Standard', color: PRIORITY_COLORS.Standard, agedAt: now, enqueuedAt: now });
          } else {
            lowNew.push(p);
          }
        }
        s.queue = { Critical: critNew, Standard: stdNew, Low: lowNew };
      }

      // 5. Snapshot for rendering (shallow copy triggers re-render)
      setSnap({
        master:     { ...s.master },
        slaves:     s.slaves,
        queue:      { Critical: s.queue.Critical, Standard: s.queue.Standard, Low: s.queue.Low },
        packets:    s.packets,
        writeCount: s.writeCount,
        readCount:  s.readCount,
        logs:       s.logs,
        strongMap:  s.strongMap,
      });
    }, T.TICK);
    return () => clearInterval(id);
  }, []);

  // ── Actions ───────────────────────────────────────────────────────────────────
  const injectTraffic = useCallback(({ qty, type, priority, consistency }) => {
    const s   = S.current;
    const now = Date.now();
    const onSlaves = s.slaves.filter(sl => sl.status === 'online');

    if (type === 'read' && onSlaves.length === 0) {
      log('No online slaves for reads.', 'error');
      return;
    }
    if (type === 'write' && s.master.status !== 'online') {
      log(`Master offline — writes will be LOST on arrival.`, 'warning');
    }

    for (let i = 0; i < qty; i++) {
      const client = CLIENT_NODES[Math.floor(Math.random() * CLIENT_NODES.length)];
      const pkt = {
        id: uid(), progress: 0, startedAt: now + i * 80, duration: T.TRAVEL,
        clientId: client.id,
        from:     { x: client.x, y: client.y },
      };

      if (type === 'write') {
        s.packets.push({
          ...pkt,
          stage:       'fly_write',
          priority,
          consistency,
          color:       PRIORITY_COLORS[priority],
          to:          MASTER_POS,
        });
      } else {
        const sl = onSlaves[Math.floor(Math.random() * onSlaves.length)];
        s.packets.push({
          ...pkt,
          stage:       'fly_read',
          consistency,
          color:       consistency === 'Strong' ? '#a78bfa' : '#94a3b8',
          targetId:    sl.id,
          to:          { x: sl.x, y: sl.y },
        });
      }
    }

    log(
      type === 'write'
        ? `[SET] ${qty} × ${priority} write(s) [${consistency}] → Master`
        : `[GET] ${qty} read(s) → slaves (master bypassed)`,
      'info'
    );
  }, []);

  const killMaster = useCallback(() => {
    const s = S.current;
    if (s.master.status !== 'online') return;

    s.master.status = 'electing';
    log('[CRASH] Master process killed. In-memory queue WIPED.', 'error');
    log('[CRASH] All queued + in-flight writes to master are LOST.', 'error');

    // Wipe queue and mark in-flight writes as lost (done immediately)
    s.queue   = { Critical: [], Standard: [], Low: [] };
    s.packets = s.packets.map(p =>
      p.stage === 'fly_write'
        ? { ...p, stage: 'done', doneAt: Date.now(), color: '#ef4444' }
        : p
    );

    // Step 1: slaves detect missing heartbeat
    setTimeout(() => {
      log(`Heartbeat timeout (${T.HEARTBEAT}ms). Slaves enter CANDIDATE state.`, 'warning');
      S.current.slaves = S.current.slaves.map(sl =>
        sl.status === 'online' ? { ...sl, votes: Math.floor(Math.random() * 5) + 1 } : sl
      );
    }, T.HEARTBEAT);

    // Step 2: election resolves — slave with highest offset wins
    setTimeout(() => {
      const candidates = S.current.slaves.filter(sl => sl.status === 'online');
      if (candidates.length === 0) { log('No candidates! Cluster unavailable.', 'error'); return; }

      const winner = candidates.reduce((best, sl) => sl.offset > best.offset ? sl : best);
      const newTerm = S.current.master.term + 1;

      S.current.master = { status: 'online', offset: winner.offset, term: newTerm, electingFrom: winner.id };
      S.current.slaves = S.current.slaves.map(sl => ({ ...sl, votes: 0 }));

      log(`[ELECTION T${newTerm}] ${winner.label} promoted to acting master (offset ${winner.offset}).`, 'success');
      log(`[NEW_MASTER] Broadcast complete. All slaves following ${winner.label}.`, 'success');
    }, T.ELECTION);
  }, []);

  // ── Derived metrics (16/16 — master is separate from 10 slaves) ──────────────
  const metrics = {
    totalNodes:  5 + 1 + 10,  // clients + master + slaves = 16
    activeNodes: 5 + (snap.master.status === 'online' ? 1 : 0) + snap.slaves.filter(s => s.status === 'online').length,
    stale:       snap.slaves.filter(s => s.stale).length,
    queueDepth:  snap.queue.Critical.length + snap.queue.Standard.length + snap.queue.Low.length,
    inFlight:    snap.packets.filter(p => p.stage !== 'done').length,
    strongPendingCount: Object.values(snap.strongMap).filter(v => !v.resolved).length,
  };

  return { snap, metrics, logRef, injectTraffic, killMaster };
}
