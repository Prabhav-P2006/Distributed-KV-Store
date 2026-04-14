import React, { useState } from 'react';
import { queueOrder, consistencyOptions } from '../utils/constants';
import clsx from 'clsx';

export default function ControlPanel({ injectTraffic, killMaster, logs, logRef }) {
  const [qty, setQty] = useState(1);
  const [reqType, setReqType] = useState("write");
  const [priority, setPriority] = useState("Standard");
  const [consistency, setConsistency] = useState("Eventual");

  return (
    <div className="flex flex-col gap-4">
      <section className="grid-shell rounded-[28px] border border-white/10 bg-slatebg/80 p-5 shadow-neon">
        <div className="mb-5 flex items-center justify-between">
          <div>
            <h2 className="text-xl font-semibold">Control Center</h2>
            <p className="mt-1 text-sm text-white/60">Inject live traffic into the simulated cluster.</p>
          </div>
        </div>

        <div className="space-y-4">
          <label className="block">
            <div className="mb-2 text-xs uppercase tracking-[0.25em] text-white/50">Request Type</div>
            <select
              value={reqType}
              onChange={(e) => setReqType(e.target.value)}
              className="w-full rounded-2xl border border-white/10 bg-white/5 px-4 py-3 text-sm outline-none transition focus:border-cyan-300/70"
            >
              <option value="write">Write</option>
              <option value="read">Read</option>
            </select>
          </label>

          <label className="block">
            <div className="mb-2 text-xs uppercase tracking-[0.25em] text-white/50">Number of Messages</div>
            <input
              type="number"
              min="1"
              max="100"
              value={qty}
              onChange={(e) => setQty(Math.max(1, Math.min(100, Number(e.target.value) || 1)))}
              className="w-full rounded-2xl border border-white/10 bg-white/5 px-4 py-3 text-sm outline-none transition focus:border-cyan-300/70"
            />
          </label>

          {reqType === 'write' && (
            <>
              <label className="block">
                <div className="mb-2 text-xs uppercase tracking-[0.25em] text-white/50">Priority</div>
                <select
                  value={priority}
                  onChange={(e) => setPriority(e.target.value)}
                  className="w-full rounded-2xl border border-white/10 bg-white/5 px-4 py-3 text-sm outline-none transition focus:border-cyan-300/70"
                >
                  {queueOrder.map((o) => <option key={o} value={o}>{o}</option>)}
                </select>
              </label>
              <label className="block">
                <div className="mb-2 text-xs uppercase tracking-[0.25em] text-white/50">Consistency</div>
                <select
                  value={consistency}
                  onChange={(e) => setConsistency(e.target.value)}
                  className="w-full rounded-2xl border border-white/10 bg-white/5 px-4 py-3 text-sm outline-none transition focus:border-cyan-300/70"
                >
                  {consistencyOptions.map((o) => <option key={o} value={o}>{o}</option>)}
                </select>
              </label>
            </>
          )}
        </div>

        <div className="mt-5 grid gap-3">
          <button
            onClick={() => injectTraffic({ qty, type: reqType, priority, consistency })}
            className="rounded-2xl bg-cyan-300 px-4 py-3 font-semibold text-slate-900 transition hover:bg-cyan-200"
          >
            Send Request
          </button>
          <button
            onClick={killMaster}
            className="rounded-2xl border border-red-400/30 bg-red-500/10 px-4 py-3 font-semibold text-red-200 transition hover:bg-red-500/20"
          >
            Kill Master
          </button>
        </div>
      </section>

      <section className="flex-1 rounded-[28px] border border-white/10 bg-black/40 p-5 shadow-neon flex flex-col min-h-[300px]">
        <div className="mb-4 flex items-center justify-between">
          <div>
            <h2 className="text-xl font-semibold">Live Log</h2>
          </div>
          <div className="rounded-full border border-white/10 bg-white/5 px-3 py-1 text-xs uppercase tracking-[0.25em] text-white/40">
            {logs.length} events
          </div>
        </div>

        <div ref={logRef} className="terminal-scrollbar flex-1 overflow-y-auto rounded-[24px] border border-emerald-400/15 bg-[#03100a] p-4 font-mono text-sm max-h-[500px]">
          {logs.map((entry) => (
            <div key={entry.id} className="mb-3 flex gap-3 animate-in fade-in slide-in-from-bottom-2 duration-300">
              <span className="text-emerald-400/70">[{entry.time}]</span>
              <span className={clsx({
                "text-cyan-200": entry.tone === 'info',
                "text-emerald-300": entry.tone === 'success',
                "text-yellow-200": entry.tone === 'warning',
                "text-red-300": entry.tone === 'error',
                "text-red-200": entry.tone === 'critical',
              })}>
                {entry.message}
              </span>
            </div>
          ))}
        </div>
      </section>
    </div>
  );
}
