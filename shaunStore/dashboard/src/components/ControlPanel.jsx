import React from 'react';
import { PRIORITY_COLORS, PRIORITY_ORDER, CONSISTENCY_OPTS } from '../utils/constants';

export default function ControlPanel({ injectTraffic, killMaster, masterStatus }) {
  const [qty,  setQty]  = React.useState(1);
  const [type, setType] = React.useState('write');
  const [prio, setPrio] = React.useState('Standard');
  const [cons, setCons] = React.useState('Eventual');

  const isElecting = masterStatus === 'electing';
  const isOffline  = masterStatus !== 'online';

  return (
    <div className="flex flex-col gap-3">
      {/* Send panel */}
      <div className="rounded-2xl border border-white/10 bg-zinc-900/80 p-5">
        <h2 className="mb-4 text-[10px] font-bold uppercase tracking-widest text-zinc-500">Send Request</h2>

        {/* Write/Read toggle */}
        <div className="mb-4 flex overflow-hidden rounded-xl border border-white/10">
          {[['write','✍ Write (SET)'],['read','📖 Read (GET)']].map(([t,label]) => (
            <button key={t} onClick={() => setType(t)}
              className={`flex-1 py-2.5 text-xs font-bold uppercase tracking-widest transition ${
                type===t ? (t==='write'?'bg-blue-600':'bg-emerald-600') + ' text-white' : 'text-zinc-500 hover:text-zinc-300'
              }`}>{label}</button>
          ))}
        </div>

        {/* Qty */}
        <label className="mb-3 block">
          <span className="mb-1 block text-[10px] uppercase tracking-widest text-zinc-500">Batch (1–100)</span>
          <input type="number" min={1} max={100} value={qty}
            onChange={e => setQty(Math.max(1,Math.min(100,+e.target.value||1)))}
            className="w-full rounded-lg border border-white/10 bg-zinc-800 px-3 py-2 text-sm text-white outline-none focus:border-blue-500/50"/>
        </label>

        {type === 'write' && (
          <>
            <label className="mb-3 block">
              <span className="mb-1 block text-[10px] uppercase tracking-widest text-zinc-500">Priority</span>
              <div className="flex gap-2">
                {PRIORITY_ORDER.map(p => (
                  <button key={p} onClick={() => setPrio(p)}
                    className="flex-1 rounded-lg border py-2 text-[10px] font-bold uppercase tracking-widest transition"
                    style={prio===p
                      ? {borderColor:PRIORITY_COLORS[p]+'88',background:PRIORITY_COLORS[p]+'22',color:PRIORITY_COLORS[p]}
                      : {borderColor:'rgba(255,255,255,0.1)',color:'#52525b'}}>{p}</button>
                ))}
              </div>
            </label>
            <label className="mb-1 block">
              <span className="mb-1 block text-[10px] uppercase tracking-widest text-zinc-500">Consistency</span>
              <select value={cons} onChange={e=>setCons(e.target.value)}
                className="w-full rounded-lg border border-white/10 bg-zinc-800 px-3 py-2 text-sm text-white outline-none">
                {CONSISTENCY_OPTS.map(o=><option key={o}>{o}</option>)}
              </select>
              <p className="mt-1 text-[9px] text-zinc-600">
                {cons==='Strong'?'Blocks until ⌊n/2⌋+1 slave ACKs received.':cons==='Bounded Staleness'?'ACK immediately; slaves may lag up to 50 offsets.':'ACK immediately after master applies write.'}
              </p>
            </label>
          </>
        )}

        {type === 'read' && (
          <label className="mb-1 block">
            <span className="mb-1 block text-[10px] uppercase tracking-widest text-zinc-500">Read Consistency</span>
            <select value={cons} onChange={e=>setCons(e.target.value)}
              className="w-full rounded-lg border border-white/10 bg-zinc-800 px-3 py-2 text-sm text-white outline-none">
              {CONSISTENCY_OPTS.map(o=><option key={o}>{o}</option>)}
            </select>
            <p className="mt-1 text-[9px] text-zinc-600">
              {cons==='Bounded Staleness'?'Tolerate stale slaves (lag > 50 ok).':'Reject if slave lag > 50 offsets (STALE_DATA).'}
            </p>
          </label>
        )}

        <button
          onClick={() => injectTraffic({ qty, type, priority: prio, consistency: cons })}
          className="mt-4 w-full rounded-xl py-3 text-xs font-bold uppercase tracking-widest bg-blue-600 hover:bg-blue-500 text-white transition">
          Send {qty > 1 ? `${qty}×` : ''} {type === 'write' ? `${prio} Write` : 'Read (→ slave)'}
        </button>
      </div>

      {/* Kill master */}
      <button onClick={killMaster} disabled={isOffline}
        className={`rounded-2xl border py-3 text-xs font-bold uppercase tracking-widest transition ${
          isElecting  ? 'border-yellow-400/40 bg-yellow-400/10 text-yellow-300 animate-pulse cursor-wait' :
          isOffline   ? 'border-zinc-700 bg-zinc-800/30 text-zinc-600 cursor-not-allowed' :
                        'border-red-500/40 bg-red-500/10 text-red-400 hover:bg-red-500/20'}`}>
        {isElecting ? '⚡ Electing new master…' : isOffline ? '✓ New master active' : '💀 Kill Master (simulateCrash)'}
      </button>

      {/* Reference notes */}
      <div className="rounded-2xl border border-white/10 bg-zinc-900/80 p-4 text-[9px] text-zinc-500 space-y-1">
        <div className="mb-1 text-[9px] font-bold uppercase tracking-widest text-zinc-600">Backend Behaviour</div>
        <div>• <b className="text-zinc-400">Writes → Master only.</b> Applied locally → PriorityReplicationEngine queue.</div>
        <div>• <b className="text-zinc-400">Reads → Slave only.</b> Master never serves reads.</div>
        <div>• <b className="text-zinc-400">Queue is in-memory.</b> Master crash = queue wiped, writes LOST.</div>
        <div>• <b className="text-zinc-400">Dispatch:</b> 70% Critical / 20% Standard / 10% Low weighted round-robin.</div>
        <div>• <b className="text-zinc-400">Aging:</b> Low→Std after 5s, Std→Crit after 4s (real: 250ms each).</div>
        <div>• <b className="text-zinc-400">Election winner</b> = slave with highest replication offset.</div>
        <div>• <b className="text-zinc-400">Strong write</b> blocks until ⌊n/2⌋+1 ACKs received.</div>
      </div>
    </div>
  );
}
