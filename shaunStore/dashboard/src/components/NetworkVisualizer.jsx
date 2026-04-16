import React from 'react';
import { MASTER_POS, CLIENT_NODES, SLAVE_NODES, PRIORITY_COLORS, PRIORITY_ORDER } from '../utils/constants';

const TIER_W = { Critical: 70, Standard: 20, Low: 10 };

function lerp(from, to, t) {
  return { x: from.x + (to.x - from.x) * t, y: from.y + (to.y - from.y) * t };
}

function ClientBox({ node }) {
  return (
    <div className="absolute -translate-x-1/2 -translate-y-1/2 rounded-lg border border-white/10 bg-zinc-800/80 px-3 py-1.5 text-center select-none"
      style={{ left: `${node.x}%`, top: `${node.y}%` }}>
      <div className="text-[9px] uppercase tracking-widest text-zinc-500">Client</div>
      <div className="text-xs font-semibold text-zinc-200">{node.label}</div>
    </div>
  );
}

function SlaveBox({ slave }) {
  const off    = slave.status === 'offline';
  const stale  = slave.stale && !off;
  const cand   = slave.votes > 0 && !off;
  const border = off ? 'rgba(239,68,68,0.3)' : cand ? 'rgba(250,204,21,0.5)' : stale ? 'rgba(251,146,60,0.45)' : 'rgba(255,255,255,0.1)';
  const bg     = off ? 'rgba(239,68,68,0.06)' : cand ? 'rgba(250,204,21,0.07)' : stale ? 'rgba(251,146,60,0.07)' : 'rgba(255,255,255,0.04)';
  return (
    <div className="absolute -translate-x-1/2 -translate-y-1/2 text-center select-none rounded-xl px-3 py-2"
      style={{ left:`${slave.x}%`, top:`${slave.y}%`, border:`1px solid ${border}`, background:bg, opacity:off?0.45:1, minWidth:70 }}>
      <div className="text-[9px] uppercase tracking-widest" style={{ color: off?'#ef4444':cand?'#fbbf24':stale?'#fb923c':'#52525b' }}>
        {off?'OFFLINE':cand?'CAND':'SLAVE'}
      </div>
      <div className="text-xs font-bold text-white mt-0.5">{slave.label}</div>
      <div className="text-[9px] text-zinc-600">off:{slave.offset}</div>
      {stale && <div className="text-[8px] text-orange-300 bg-orange-500/20 rounded px-1 mt-0.5">STALE</div>}
      {cand  && <div className="text-[8px] text-yellow-300 bg-yellow-500/20 rounded px-1 mt-0.5">{slave.votes}v</div>}
    </div>
  );
}

export default function NetworkVisualizer({ snap, metrics }) {
  const { master, slaves, queue, packets } = snap;
  const queueDepth = metrics.queueDepth;

  return (
    <div className="flex flex-col rounded-2xl border border-white/10 bg-zinc-950 overflow-hidden">
      {/* Header */}
      <div className="flex items-center justify-between px-5 py-3 border-b border-white/10">
        <div>
          <h2 className="text-sm font-semibold text-white">Live Network Topology</h2>
          <p className="text-[11px] text-zinc-500">Client → Master queue → Dispatcher → Slave replication → ACK → Client response</p>
        </div>
        <div className="flex items-center gap-3 text-[10px] text-zinc-500">
          {[['#3b82f6','write'],['#94a3b8','read'],['#34d399','response'],['#ef4444','lost']].map(([c,l]) => (
            <span key={l} className="flex items-center gap-1">
              <span className="h-2 w-2 rounded-full" style={{ background:c }}/>
              {l}
            </span>
          ))}
          <span className="ml-2 rounded bg-zinc-800 px-2 py-1 font-mono text-zinc-300">offset {master.offset}</span>
        </div>
      </div>

      {/* Canvas */}
      <div className="relative" style={{ height: 680 }}>
        {/* Grid */}
        <div className="absolute inset-0" style={{
          backgroundImage:'linear-gradient(rgba(255,255,255,0.02) 1px,transparent 1px),linear-gradient(90deg,rgba(255,255,255,0.02) 1px,transparent 1px)',
          backgroundSize:'36px 36px',
        }}/>

        {/* SVG lines */}
        <svg className="absolute inset-0 h-full w-full pointer-events-none">
          {/* client → master guides */}
          {CLIENT_NODES.map(c => (
            <line key={c.id}
              x1={`${c.x}%`} y1={`${c.y+1.5}%`}
              x2={`${MASTER_POS.x}%`} y2={`${MASTER_POS.y-3}%`}
              stroke="rgba(255,255,255,0.04)" strokeWidth="1" strokeDasharray="3 8"
            />
          ))}
          {/* master → slave lines */}
          {slaves.map(sl => (
            <line key={sl.id}
              x1={`${MASTER_POS.x}%`} y1={`${MASTER_POS.y+4}%`}
              x2={`${sl.x}%`} y2={`${sl.y-2}%`}
              stroke={sl.status==='offline'?'rgba(239,68,68,0.12)':sl.stale?'rgba(251,146,60,0.4)':'rgba(59,130,246,0.2)'}
              strokeWidth={sl.stale?1.5:1}
              strokeDasharray={sl.status==='offline'?'2 8':sl.stale?'6 5':'none'}
            />
          ))}
        </svg>

        {/* Clients */}
        {CLIENT_NODES.map(c => <ClientBox key={c.id} node={c} />)}

        {/* Master Node */}
        <div className="absolute -translate-x-1/2 -translate-y-1/2"
          style={{ left:'50%', top:`${MASTER_POS.y}%` }}>
          <div className="rounded-2xl border bg-zinc-900 p-4 select-none"
            style={{
              width: 260,
              borderColor: master.status==='electing'?'rgba(251,191,36,0.5)':master.status!=='online'?'rgba(239,68,68,0.4)':'rgba(59,130,246,0.35)',
              boxShadow: master.status==='electing'?'0 0 28px rgba(251,191,36,0.4)':queueDepth>0?'0 0 20px rgba(59,130,246,0.25)':'0 0 14px rgba(59,130,246,0.15)',
            }}>
            {/* Header */}
            <div className="mb-3 flex items-center justify-between">
              <div>
                <div className="text-[9px] font-bold uppercase tracking-widest text-blue-400">
                  {master.status==='electing'?'⚡ ELECTION':'MASTER'}
                </div>
                <div className="text-sm font-bold text-white mt-0.5">
                  {master.electingFrom ? `Acting: ${master.electingFrom}` : 'Master (6379)'}
                </div>
              </div>
              <div className={`rounded-full px-2 py-0.5 text-[9px] font-bold uppercase ${
                master.status==='online'?'bg-emerald-500/20 text-emerald-300':
                master.status==='electing'?'bg-yellow-500/20 text-yellow-300':'bg-red-500/20 text-red-300'
              }`}>
                {master.status==='online'?'ONLINE':master.status==='electing'?'CRASHED':'OFFLINE'}
              </div>
            </div>

            {/* Priority Queues */}
            <div className="text-[8px] font-semibold uppercase tracking-widest text-zinc-600 mb-1.5">
              PriorityReplicationEngine queue
            </div>
            <div className="space-y-2">
              {PRIORITY_ORDER.map(tier => {
                const cnt = queue[tier].length;
                const pct = Math.min(100, cnt * 11);
                const col = PRIORITY_COLORS[tier];
                return (
                  <div key={tier}>
                    <div className="flex justify-between text-[9px] uppercase tracking-widest mb-0.5" style={{ color: col }}>
                      <span>{tier} ({TIER_W[tier]}%)</span>
                      <span>{cnt} queued</span>
                    </div>
                    <div className="relative h-4 rounded-lg bg-zinc-800 overflow-hidden">
                      <div className="absolute inset-y-0 left-0 rounded-lg transition-all duration-300"
                        style={{ width:`${pct}%`, background: col+'44' }}/>
                      <div className="absolute inset-0 flex items-center gap-1 px-1.5">
                        {queue[tier].slice(0,15).map(p => (
                          <span key={p.id} className="flex-shrink-0 rounded-full"
                            style={{ width:6, height:6, background:col }}/>
                        ))}
                      </div>
                    </div>
                  </div>
                );
              })}
            </div>

            {master.status !== 'online' && (
              <div className="mt-2 rounded-lg bg-red-500/10 border border-red-400/20 px-2 py-1 text-[9px] text-red-300">
                ⚠ Queue wiped — queued writes LOST on crash
              </div>
            )}
          </div>
        </div>

        {/* Slaves */}
        {slaves.map(sl => <SlaveBox key={sl.id} slave={sl} />)}

        {/* ── Packet dots ── */}
        {packets.map(p => {
          if (p.stage === 'done') return null;
          const pos = lerp(p.from, p.to, p.progress);
          const size = p.stage === 'fly_repl' ? 8 : 11;
          return (
            <div key={p.id}
              className="absolute -translate-x-1/2 -translate-y-1/2 rounded-full pointer-events-none"
              style={{
                left: `${pos.x}%`, top: `${pos.y}%`,
                width: size, height: size,
                background: p.color,
                boxShadow: `0 0 ${size+4}px ${p.color}cc`,
                transition: 'none',
              }}
            />
          );
        })}
      </div>
    </div>
  );
}
