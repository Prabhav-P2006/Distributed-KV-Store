import React from 'react';
import { useClusterState } from './hooks/useClusterState';
import MetricsDashboard   from './components/MetricsDashboard';
import NetworkVisualizer  from './components/NetworkVisualizer';
import ControlPanel       from './components/ControlPanel';
import EventLog           from './components/EventLog';

export default function App() {
  const state = useClusterState();

  return (
    <div className="min-h-screen bg-zinc-950 text-white p-4 lg:p-6">
      <div className="mx-auto max-w-[1600px] flex flex-col gap-4">

        {/* Title */}
        <div className="flex items-center justify-between">
          <div>
            <h1 className="text-lg font-bold tracking-tight text-white">
              shaunStore <span className="text-zinc-500">/ Distributed KV Dashboard</span>
            </h1>
            <p className="text-xs text-zinc-600 mt-0.5">
              C++ · RESP Protocol · Raft-style Election · Priority Replication · 5 Clients · 1 Master · 10 Slaves
            </p>
          </div>
          <div className="flex items-center gap-2">
            <span
              className={`h-2 w-2 rounded-full ${state.snap.master.status === 'online' ? 'bg-emerald-400' : state.snap.master.status === 'electing' ? 'bg-yellow-400 animate-pulse' : 'bg-red-500'}`}
            />
            <span className="text-xs text-zinc-400">
              {state.snap.master.status === 'online' ? 'Cluster Healthy' : state.snap.master.status === 'electing' ? 'Election in Progress' : 'Master Offline'}
            </span>
          </div>
        </div>

        {/* Metrics */}
        <MetricsDashboard
          snap={state.snap}
          metrics={state.metrics}
        />

        {/* Main layout */}
        <div className="grid gap-4 xl:grid-cols-[1fr_300px]">
          {/* Left: topology + log */}
          <div className="flex flex-col gap-4">
            <NetworkVisualizer
              snap={state.snap}
              metrics={state.metrics}
            />
            <EventLog logs={state.snap.logs} logRef={state.logRef} />
          </div>

          {/* Right: control panel */}
          <ControlPanel
            injectTraffic={state.injectTraffic}
            killMaster={state.killMaster}
            masterStatus={state.snap.master.status}
          />
        </div>

      </div>
    </div>
  );
}
