import React from 'react';

function Stat({ label, value, sub, tone='default' }) {
  const border = { red:'border-red-500/30', yellow:'border-yellow-400/30', green:'border-emerald-500/30', default:'border-white/10' }[tone];
  const color  = { red:'text-red-300', yellow:'text-yellow-200', green:'text-emerald-300', default:'text-white' }[tone];
  return (
    <div className={`rounded-2xl border bg-zinc-900/70 p-4 ${border}`}>
      <div className="text-[9px] font-semibold uppercase tracking-widest text-zinc-500">{label}</div>
      <div className={`mt-2 text-2xl font-bold tabular-nums ${color}`}>{value}</div>
      {sub && <div className="mt-1 text-[10px] text-zinc-600">{sub}</div>}
    </div>
  );
}

export default function MetricsDashboard({ snap, metrics }) {
  const { master } = snap;
  const crashed  = master.status === 'electing';
  const offline  = master.status !== 'online';
  return (
    <div className="grid grid-cols-3 gap-3 lg:grid-cols-6">
      <Stat label="Writes Applied" value={snap.writeCount} sub="committed to master log" />
      <Stat label="Reads Served"   value={snap.readCount}  sub="via slaves (master bypassed)" />
      <Stat
        label="Active Nodes"
        value={`${metrics.activeNodes}/${metrics.totalNodes}`}
        sub={`${metrics.stale} stale slave(s)`}
        tone={offline ? 'red' : 'default'}
      />
      <Stat
        label="Master"
        value={offline ? (crashed ? 'ELECTING' : 'OFFLINE') : 'ONLINE'}
        sub={`term ${master.term} · offset ${master.offset}`}
        tone={crashed ? 'yellow' : offline ? 'red' : 'green'}
      />
      <Stat label="Queue Depth"     value={metrics.queueDepth} sub="in priority queue" />
      <Stat
        label="Strong Pending"
        value={metrics.strongPendingCount}
        sub="awaiting quorum ACK"
        tone={metrics.strongPendingCount > 0 ? 'yellow' : 'default'}
      />
    </div>
  );
}
