import { useState, useEffect, useMemo, useRef } from 'react';
import { 
  createSlaveNodes, 
  backgroundClients, 
  createLogEntry,
  priorityPalette 
} from '../utils/constants';

export function useClusterState() {
  const [masterId, setMasterId] = useState("master-1");
  const [masterStatus, setMasterStatus] = useState("online");
  const [slaves, setSlaves] = useState(createSlaveNodes);
  const [packets, setPackets] = useState([]);
  const [logs, setLogs] = useState(() => [
    createLogEntry("Dashboard initialized. Master online with 10 followers.", "success"),
    createLogEntry("Priority dispatcher active. Clients loaded.", "info"),
  ]);
  const [masterOffset, setMasterOffset] = useState(0);
  const [writeCount, setWriteCount] = useState(0);
  const [waitingStrong, setWaitingStrong] = useState([]);
  const [votePulse, setVotePulse] = useState(false);
  const logRef = useRef(null);

  const masterNode = useMemo(() => ({
    id: masterId,
    label: masterId === "master-1" ? "MASTER" : `M: ${masterId.toUpperCase()}`,
    x: 50,
    y: 35,
    role: "master",
    status: masterStatus,
  }), [masterId, masterStatus]);

  const nodes = useMemo(() => {
    const nextSlaves = slaves.map((slave) =>
      slave.id === masterId
        ? { ...slave, role: "master", x: 50, y: 35, status: masterStatus }
        : slave.id === "master-1" && masterId !== "master-1"
          ? { ...slave, role: "slave", x: 50, y: 85, status: "offline", stale: false }
          : slave
    );

    const visibleSlaves = nextSlaves
      .filter((node) => node.id !== masterId)
      .map((node, index) => ({
        ...node,
        x: 10 + (index % 5) * 20,
        y: 70 + Math.floor(index / 5) * 15,
      }));

    return { master: masterNode, slaves: visibleSlaves, clients: backgroundClients };
  }, [slaves, masterId, masterNode, masterStatus]);

  const queueState = useMemo(() => ({
    Critical: packets.filter((packet) => packet.stage === "queued" && packet.queue === "Critical"),
    Standard: packets.filter((packet) => packet.stage === "queued" && packet.queue === "Standard"),
    Low: packets.filter((packet) => packet.stage === "queued" && packet.queue === "Low"),
  }), [packets]);

  const activeNodesCount = useMemo(() => {
    const mCount = masterStatus === 'online' ? 1 : 0;
    const sCount = nodes.slaves.filter(s => s.status === 'online').length;
    const cCount = nodes.clients.length;
    return mCount + sCount + cCount;
  }, [masterStatus, nodes.slaves, nodes.clients]);

  const metrics = useMemo(() => ({
    totalWrites: writeCount,
    activeNodes: activeNodesCount,
    staleFollowers: nodes.slaves.filter((slave) => slave.stale).length,
    inFlight: packets.filter((p) => p.stage === "traveling" || p.stage === "client-to-node").length,
  }), [activeNodesCount, nodes.slaves, packets, writeCount]);

  useEffect(() => {
    if (logRef.current) logRef.current.scrollTop = logRef.current.scrollHeight;
  }, [logs]);

  // Main Event Loop
  useEffect(() => {
    const loop = window.setInterval(() => {
      const now = Date.now();
      const strongReady = [];
      const staleMasterOffset = masterOffset;

      setPackets((current) => {
        const next = [];
        const byStage = { Critical: [], Standard: [], Low: [] };

        for (const packet of current) {
          if (packet.stage === "client-to-node") {
            const duration = packet.type === 'read' ? 1200 : 800; 
            const progress = Math.min(1, (now - packet.dispatchedAt) / duration);
            if (progress >= 1) {
              if (packet.type === 'read') {
                next.push({ ...packet, stage: "delivered", deliveredAt: now });
              } else {
                next.push({ ...packet, stage: "queued", queuedAt: now });
              }
            } else {
              next.push({ ...packet, progress });
            }
          } else if (packet.stage === "queued") {
            let queue = packet.queue;
            let color = packet.color;
            if (packet.queue === "Low" && now - packet.queuedAt > 3000) {
              queue = "Standard";
              color = priorityPalette.Aging;
              if (!packet.aged) {
                setLogs((entries) => [...entries, createLogEntry(`Aging promoted ${packet.id} from LOW to STD`, "warning")]);
              }
            }
            byStage[queue].push({ ...packet, queue, color, aged: queue !== "Low" || packet.aged });
          } else if (packet.stage === "traveling") {
            const duration = packet.consistency === "Strong" ? 2600 : 1800;
            const progress = Math.min(1, (now - packet.dispatchedAt) / duration);
            const delivered = Math.floor(progress * packet.targetCount);
            if (packet.consistency === "Strong" && delivered >= 6 && !packet.clientAcked) {
              strongReady.push(packet.id);
            }

            if (progress >= 1) {
              next.push({ ...packet, stage: "delivered", deliveredAt: now, deliveredTargets: packet.targetCount });
            } else {
              next.push({ ...packet, progress, deliveredTargets: delivered });
            }
          } else if (packet.stage === "delivered") {
            if (now - packet.deliveredAt < 1400) {
              next.push(packet);
            }
          }
        }

        // Only drain queues if master is ONLINE
        if (masterStatus === "online") {
          const weightedSchedule = ["Critical", "Critical", "Critical", "Critical", "Standard", "Standard", "Low"];
          const availableQueue = weightedSchedule.find((tier) => byStage[tier].length > 0);
          if (availableQueue) {
            const [dispatched, ...rest] = byStage[availableQueue];
            byStage[availableQueue] = rest;
            next.push({
              ...dispatched,
              stage: "traveling",
              progress: 0,
              dispatchedAt: now,
            });
            // Update Master Offset & write count only when effectively replicated
            setMasterOffset((o) => o + 1);
            setWriteCount((c) => c + 1);
          }
        }

        Object.values(byStage).flat().forEach((packet) => next.push(packet));
        return next;
      });

      if (strongReady.length > 0) {
        setWaitingStrong((current) => current.filter((id) => !strongReady.includes(id)));
        setLogs((entries) => [
          ...entries,
          ...strongReady.map((id) => createLogEntry(`Strong write quorum met.`, "success")),
        ]);
        setPackets((current) => current.map((p) => strongReady.includes(p.id) ? { ...p, clientAcked: true } : p));
      }

      setSlaves((current) =>
        current.map((slave) => {
          if (slave.id === masterId) return { ...slave, role: "master", status: masterStatus, stale: false };
          const online = slave.status === "online";
          const lag = staleMasterOffset - slave.offset;
          return { ...slave, stale: online && lag > 2 };
        })
      );
    }, 150);

    return () => window.clearInterval(loop);
  }, [masterId, masterOffset, masterStatus]);

  // Slave Sync Loop
  useEffect(() => {
    const syncLoop = window.setInterval(() => {
      setSlaves((current) =>
        current.map((slave) => {
          if (slave.id === masterId || slave.status !== "online") return slave;
          const catchup = Math.random() > 0.42 ? 1 : 0;
          return { ...slave, offset: Math.min(masterOffset, slave.offset + catchup) };
        })
      );
    }, 400);
    return () => window.clearInterval(syncLoop);
  }, [masterId, masterOffset]);

  function appendLog(message, tone = "info") {
    setLogs((current) => [...current, createLogEntry(message, tone)]);
  }

  function injectTraffic({ qty, type, priority, consistency }) {
    if (type === "write" && masterStatus !== "online") {
      appendLog("Master is offline. Writes queued until failover completes.", "warning");
    }

    const activeSlaves = slaves.filter(s => s.status === 'online' && s.id !== masterId);
    if (type === "read" && activeSlaves.length === 0) {
      appendLog("No active followers available for READ.", "error");
      return;
    }

    const newPackets = [];
    const now = Date.now();

    for(let i=0; i < qty; i++) {
        const client = backgroundClients[Math.floor(Math.random() * backgroundClients.length)];
        const packetId = `${now}-${Math.random().toString(16).slice(2)}`;
        
        if (type === "read") {
            const slave = activeSlaves[Math.floor(Math.random() * activeSlaves.length)];
            newPackets.push({
                id: packetId,
                type: "read",
                source: client.id,
                targetId: slave.id,
                color: "#dbe7ff", // White/Blue for reads
                stage: "client-to-node",
                createdAt: now,
                dispatchedAt: now,
            });
        } else {
            newPackets.push({
                id: packetId,
                type: "write",
                queue: priority,
                consistency: consistency,
                source: client.id,
                color: priorityPalette[priority],
                stage: "client-to-node",
                createdAt: now,
                dispatchedAt: now,
                queuedAt: now,
                targetCount: 10,
                clientAcked: consistency !== "Strong",
                aged: false,
            });
        }
    }

    setPackets((current) => [...current, ...newPackets]);

    if(type === 'read') {
        appendLog(`Spawned ${qty} READ(s) directly to slaves.`, "info");
    } else {
        appendLog(`Spawned ${qty} WRITE(s) to Master [${priority}].`, priority === "Critical" ? "critical" : "info");
        if (consistency === "Strong") {
            const newStrongIds = newPackets.map(p => p.id);
            setWaitingStrong((c) => [...c, ...newStrongIds]);
        }
    }
  }

  function killMaster() {
    if (masterStatus === "offline") return;

    setMasterStatus("offline");
    setVotePulse(true);
    appendLog("Chaos Monkey triggered. Master marked OFFLINE.", "error");

    window.setTimeout(() => {
      appendLog("Heartbeat timeout. Followers entering CANDIDATE state.", "warning");
      setSlaves((current) =>
        current.map((slave) =>
          slave.id === masterId ? slave : { ...slave, votes: Math.floor(Math.random() * 3) + 1 }
        )
      );
    }, 800);

    window.setTimeout(() => {
      const promoted = slaves
        .filter((slave) => slave.id !== masterId && slave.status === "online")
        .sort((a,b) => b.offset - a.offset)[0]; // Promote the one with highest offset

      if (!promoted) {
        appendLog("No follower available for promotion.", "error");
        return;
      }

      setMasterId(promoted.id);
      setMasterStatus("online");
      setVotePulse(false);
      setSlaves((current) =>
        current.map((slave) => {
          if (slave.id === masterId) {
             return { ...slave, status: "offline", role: "slave", votes: 0 };
          }
          if (slave.id === promoted.id) {
             return { ...slave, status: "online", role: "master", offset: masterOffset, votes: 7 };
          }
          return { ...slave, role: "slave", votes: 0 };
        })
      );
      appendLog(`Election Settled: ${promoted.label} promoted to MASTER.`, "success");
    }, 2000);
  }

  return {
    nodes,
    masterStatus,
    metrics,
    queueState,
    waitingStrong,
    votePulse,
    injectTraffic,
    killMaster,
    packets,
    logs,
    logRef,
    masterOffset
  };
}
