import React from 'react';
import clsx from 'clsx';
import { motion, AnimatePresence } from 'framer-motion';
import { mkLog } from '../utils/constants';

const TONE_COLORS = {
  info:     'text-zinc-300',
  success:  'text-emerald-400',
  warning:  'text-amber-300',
  error:    'text-red-400',
  critical: 'text-red-300',
};

export default function EventLog({ logs, logRef }) {
  return (
    <div className="flex flex-col rounded-2xl border border-white/10 bg-zinc-950 overflow-hidden" style={{ minHeight: 320 }}>
      <div className="flex items-center justify-between border-b border-white/10 px-5 py-3">
        <h2 className="text-sm font-semibold text-white">Event Log</h2>
        <span className="rounded bg-zinc-800 px-2 py-0.5 font-mono text-[10px] text-zinc-400">{logs.length} events</span>
      </div>
      <div
        ref={logRef}
        className="flex-1 overflow-y-auto p-4 font-mono text-[11px] space-y-1.5"
        style={{ maxHeight: 340 }}
      >
        <AnimatePresence initial={false}>
          {logs.map(entry => (
            <motion.div
              key={entry.id}
              initial={{ opacity: 0, x: -8 }}
              animate={{ opacity: 1, x: 0 }}
              transition={{ duration: 0.2 }}
              className="flex gap-2"
            >
              <span className="shrink-0 text-zinc-600">[{entry.time}]</span>
              <span className={clsx(TONE_COLORS[entry.tone] || 'text-zinc-300')}>{entry.message}</span>
            </motion.div>
          ))}
        </AnimatePresence>
      </div>
    </div>
  );
}
