import React from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import { queueOrder, priorityPalette } from '../utils/constants';

export default function NetworkVisualizer({ nodes, masterStatus, masterOffset, queueState, waitingStrong, votePulse, packets }) {
  const masterId = nodes.master.id;
  const connections = nodes.slaves.map((slave) => ({
    id: `${masterId}-${slave.id}`,
    x1: `${nodes.master.x}%`,
    y1: `${nodes.master.y + 7}%`,
    x2: `${slave.x}%`,
    y2: `${slave.y}%`,
    stale: slave.stale,
  }));

  const findNodeStr = (id) => {
    if (id === nodes.master.id) return nodes.master;
    const s = nodes.slaves.find(x => x.id === id);
    if (s) return s;
    const c = nodes.clients.find(x => x.id === id);
    return c;
  };

  return (
    <section className="relative overflow-hidden rounded-[32px] border border-white/10 bg-slatebg/75 p-4 shadow-neon flex flex-col h-full">
      <div className="mb-3 flex items-center justify-between">
        <div>
          <h2 className="text-xl font-semibold">Network Topology</h2>
          <p className="text-sm text-white/60">5 Clients, 1 Master, 10 Slaves</p>
        </div>
        <div className="rounded-full border border-white/10 bg-white/5 px-4 py-2 text-xs uppercase tracking-[0.28em] text-white/50">
          master offset {masterOffset}
        </div>
      </div>

      <div className="relative h-[720px] flex-1 overflow-hidden rounded-[28px] border border-white/10 bg-[#05101d] grid-shell">
        <svg className="absolute inset-0 h-full w-full">
          {connections.map((line) => (
            <motion.line
              key={line.id}
              x1={line.x1}
              y1={line.y1}
              x2={line.x2}
              y2={line.y2}
              stroke={line.stale ? "#ffae42" : "rgba(139, 244, 255, 0.2)"}
              strokeWidth="2"
              strokeDasharray={line.stale ? "10 8" : "4 6"}
              initial={{ pathLength: 0 }}
              animate={{ pathLength: 1, opacity: line.stale ? 0.8 : 0.4 }}
              transition={{ duration: 0.8 }}
            />
          ))}
        </svg>

        {/* CLIENTS */}
        {nodes.clients.map((client) => (
          <div
            key={client.id}
            className="absolute -translate-x-1/2 -translate-y-1/2 rounded-[16px] border border-white/10 bg-white/5 px-4 py-2 text-center backdrop-blur"
            style={{ left: `${client.x}%`, top: `${client.y}%` }}
          >
            <div className="text-[10px] uppercase tracking-[0.2em] text-white/50">Client</div>
            <div className="text-sm font-semibold text-white/90">{client.label}</div>
          </div>
        ))}

        {/* MASTER */}
        <div className="absolute left-1/2 top-[35%] w-[290px] -translate-x-1/2 -translate-y-1/2">
          <motion.div
            animate={{
              boxShadow: waitingStrong.length > 0
                ? ["0 0 0 rgba(255, 211, 105, 0.1)", "0 0 36px rgba(255, 211, 105, 0.52)", "0 0 0 rgba(255, 211, 105, 0.1)"]
                : masterStatus === "offline"
                  ? "0 0 24px rgba(255, 93, 93, 0.5)"
                  : "0 0 24px rgba(73, 199, 255, 0.28)",
              scale: votePulse ? [1, 1.03, 1] : 1,
            }}
            transition={{ duration: 1.4, repeat: waitingStrong.length > 0 || votePulse ? Infinity : 0 }}
            className={`rounded-[28px] border px-5 py-4 backdrop-blur ${
              masterStatus === "offline"
                ? "border-red-400/40 bg-red-500/15"
                : waitingStrong.length > 0
                  ? "border-yellow-300/45 bg-yellow-400/10"
                  : "border-cyan-300/35 bg-cyan-300/10"
            }`}
          >
            <div className="flex items-center justify-between">
              <div>
                <div className="text-xs uppercase tracking-[0.3em] text-white/50">{nodes.master.label}</div>
              </div>
              <div className={`rounded-full px-3 py-1 text-xs font-semibold uppercase tracking-[0.2em] ${
                masterStatus === "offline" ? "bg-red-500/20 text-red-200" : "bg-emerald-500/20 text-emerald-200"
              }`}>
                {masterStatus}
              </div>
            </div>

            <div className="mt-4 grid gap-3">
              {queueOrder.map((queue) => (
                <div key={queue}>
                  <div className="mb-1 flex items-center justify-between text-[10px] uppercase tracking-[0.2em] text-white/55">
                    <span>{queue}</span>
                    <span>{queueState[queue].length} q'd</span>
                  </div>
                  <div className="relative h-4 overflow-hidden rounded-full bg-white/5">
                    <div
                      className="absolute inset-y-0 left-0 rounded-full opacity-70 transition-all duration-300"
                      style={{
                        width: `${Math.min(100, queueState[queue].length * 15)}%`,
                        background: `linear-gradient(90deg, ${priorityPalette[queue]}, rgba(255,255,255,0.15))`,
                      }}
                    ></div>
                    <div className="absolute inset-0 flex items-center gap-1 px-1">
                      {queueState[queue].slice(0, 10).map((packet) => (
                        <motion.span
                          key={packet.id}
                          layout
                          className="h-2 w-2 rounded-full"
                          style={{ background: packet.color }}
                          animate={{ scale: [0.95, 1.2, 0.95] }}
                          transition={{ duration: 1.2, repeat: Infinity }}
                        ></motion.span>
                      ))}
                    </div>
                  </div>
                </div>
              ))}
            </div>
          </motion.div>
        </div>

        {/* SLAVES */}
        {nodes.slaves.map((slave) => (
          <motion.div
            key={slave.id}
            layout
            className="absolute -translate-x-1/2 -translate-y-1/2"
            style={{ left: `${slave.x}%`, top: `${slave.y}%` }}
            animate={{
              scale: slave.votes > 0 ? [1, 1.08, 1] : 1,
              opacity: slave.status === "offline" ? 0.45 : 1,
            }}
            transition={{ duration: 1.2, repeat: slave.votes > 0 ? 2 : 0 }}
          >
            <div className={`w-[120px] rounded-[24px] border px-3 py-3 text-center shadow-lg backdrop-blur ${
              slave.stale
                ? "border-orange-300/45 bg-orange-500/10"
                : slave.status === "offline"
                  ? "border-slate-500/40 bg-slate-700/20"
                  : "border-white/10 bg-white/5"
            }`}>
              <div className="text-[10px] uppercase tracking-[0.28em] text-white/45">{slave.role}</div>
              <div className="mt-1 text-base font-semibold">{slave.label}</div>
              <div className="mt-1 text-[10px] text-white/55">offs: {slave.offset}</div>
              {slave.stale && <div className="mt-2 rounded-full bg-orange-500/15 px-2 py-0.5 text-[9px] uppercase tracking-[0.22em] text-orange-200">STALE</div>}
              {slave.votes > 0 && <div className="mt-2 rounded-full bg-cyan-400/15 px-2 py-0.5 text-[9px] uppercase tracking-[0.22em] text-cyan-200">{slave.votes} v</div>}
            </div>
          </motion.div>
        ))}

        {/* PACKET ANIMATIONS */}
        <AnimatePresence>
          {/* CLIENT TO NODE (Read or Write Hop 1) */}
          {packets
            .filter((p) => p.stage === "client-to-node")
            .map((packet) => {
              const src = findNodeStr(packet.source);
              const dst = packet.type === 'read' ? findNodeStr(packet.targetId) : nodes.master;
              if (!src || !dst) return null;
              
              const progress = Math.min(1, Math.max(0, packet.progress));
              const left = src.x + (dst.x - src.x) * progress;
              const top = src.y + (dst.y - src.y) * progress;
              return (
                 <motion.div
                   key={packet.id}
                   className="absolute h-3 w-3 -translate-x-1/2 -translate-y-1/2 rounded-full shadow-[0_0_12px_rgba(255,255,255,0.6)]"
                   style={{ left: `${left}%`, top: `${top}%`, background: packet.color, opacity: progress >= 1 ? 0 : 1 }}
                   animate={{ scale: [0.8, 1.15, 0.8] }}
                   transition={{ duration: 0.8, repeat: Infinity }}
                 />
              );
            })}

          {/* MASTER TO SLAVES (Write Hop 2) */}
          {packets
            .filter((packet) => packet.stage === "traveling")
            .map((packet) =>
              nodes.slaves.map((slave, index) => {
                const progress = Math.min(1, Math.max(0, packet.progress - index * 0.03));
                const left = nodes.master.x + (slave.x - nodes.master.x) * progress;
                const top = nodes.master.y + 8 + (slave.y - (nodes.master.y + 8)) * progress;
                return (
                  <motion.div
                    key={`${packet.id}-${slave.id}`}
                    className="absolute h-3.5 w-3.5 -translate-x-1/2 -translate-y-1/2 rounded-full shadow-[0_0_18px_rgba(255,255,255,0.5)]"
                    style={{
                      left: `${left}%`,
                      top: `${top}%`,
                      background: packet.color,
                      opacity: progress >= 1 ? 0 : 1,
                    }}
                    animate={{
                      scale: [0.8, 1.15, 0.8],
                    }}
                    transition={{ duration: 0.8, repeat: Infinity }}
                  ></motion.div>
                );
              })
            )
            .flat()}
        </AnimatePresence>
      </div>
    </section>
  );
}
