import React from 'react';

export default function MetricsDashboard({ metrics, masterStatus, waitingStrong }) {
  const cards = [
    {
      title: "Total Writes",
      value: metrics.totalWrites,
      caption: "replicated via master",
      accent: "from-sky-500/30 to-sky-300/5",
    },
    {
      title: "Active Nodes",
      value: `${metrics.activeNodes}/16`, // 5 clients + 1 master + 10 slaves
      caption: `${metrics.staleFollowers} follower(s) stale`,
      accent: "from-emerald-500/30 to-emerald-300/5",
    },
    {
      title: "Master Status",
      value: masterStatus === "online" ? "HEALTHY" : "FAILED",
      caption: waitingStrong.length > 0 ? `${waitingStrong.length} strong ACK(s) pending` : "no quorum waits",
      accent: waitingStrong.length > 0 ? "from-yellow-500/35 to-yellow-300/10" : "from-indigo-500/30 to-indigo-300/5",
    },
    {
      title: "In Flight",
      value: metrics.inFlight,
      caption: "packets in motion",
      accent: "from-orange-500/30 to-orange-300/5",
    },
  ];

  return (
    <header className="grid gap-4 md:grid-cols-4">
      {cards.map((card, i) => (
        <div key={i} className={`rounded-3xl border border-white/10 bg-gradient-to-br ${card.accent} p-4 shadow-neon backdrop-blur`}>
          <div className="text-xs uppercase tracking-[0.28em] text-white/65">{card.title}</div>
          <div className="mt-3 text-3xl font-semibold">{card.value}</div>
          <div className="mt-2 text-sm text-white/60">{card.caption}</div>
        </div>
      ))}
    </header>
  );
}
