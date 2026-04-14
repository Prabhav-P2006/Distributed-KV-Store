import React from 'react';
import { useClusterState } from './hooks/useClusterState';
import MetricsDashboard from './components/MetricsDashboard';
import ControlPanel from './components/ControlPanel';
import NetworkVisualizer from './components/NetworkVisualizer';

export default function App() {
  const state = useClusterState();

  return (
    <div className="min-h-screen p-5 text-ink bg-slatebg">
      <div className="mx-auto flex max-w-[1600px] flex-col gap-5 h-full">
        <MetricsDashboard 
          metrics={state.metrics}
          masterStatus={state.masterStatus}
          waitingStrong={state.waitingStrong}
        />

        <div className="grid gap-5 xl:grid-cols-[1fr_360px] h-[calc(100vh-140px)] min-h-[800px]">
          <NetworkVisualizer 
            nodes={state.nodes}
            masterStatus={state.masterStatus}
            masterOffset={state.masterOffset}
            queueState={state.queueState}
            waitingStrong={state.waitingStrong}
            votePulse={state.votePulse}
            packets={state.packets}
          />
          <ControlPanel 
            injectTraffic={state.injectTraffic}
            killMaster={state.killMaster}
            logs={state.logs}
            logRef={state.logRef}
          />
        </div>
      </div>
    </div>
  );
}
