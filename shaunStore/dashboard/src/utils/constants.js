// Node layout (% positions on the canvas)
export const MASTER_POS = { x: 50, y: 40 };

export const CLIENT_NODES = [
  { id: 'C1', label: 'Client-1', x: 8,  y: 7 },
  { id: 'C2', label: 'Client-2', x: 26, y: 7 },
  { id: 'C3', label: 'Client-3', x: 50, y: 7 },
  { id: 'C4', label: 'Client-4', x: 74, y: 7 },
  { id: 'C5', label: 'Client-5', x: 92, y: 7 },
];

export const SLAVE_NODES = [
  { id:'S1',  label:'S1',  x:8,  y:73 },
  { id:'S2',  label:'S2',  x:26, y:73 },
  { id:'S3',  label:'S3',  x:44, y:73 },
  { id:'S4',  label:'S4',  x:62, y:73 },
  { id:'S5',  label:'S5',  x:80, y:73 },
  { id:'S6',  label:'S6',  x:8,  y:89 },
  { id:'S7',  label:'S7',  x:26, y:89 },
  { id:'S8',  label:'S8',  x:44, y:89 },
  { id:'S9',  label:'S9',  x:62, y:89 },
  { id:'S10', label:'S10', x:80, y:89 },
];

export const PRIORITY_COLORS = { Critical:'#ef4444', Standard:'#3b82f6', Low:'#22c55e' };
export const PRIORITY_ORDER  = ['Critical','Standard','Low'];
export const CONSISTENCY_OPTS = ['Eventual','Bounded Staleness','Strong'];

// Dispatch schedule: 7 Critical, 2 Standard, 1 Low (70/20/10%)
export const DISPATCH_SCHEDULE = [
  'Critical','Critical','Critical','Critical','Critical','Critical','Critical',
  'Standard','Standard','Low',
];

export const T = {
  TICK:       40,    // ms — animation frame
  TRAVEL:   1300,    // packet flying (client→node, master→slave)
  RESPONSE:  900,    // response flying back
  DISPATCH: 1800,    // dispatcher pops one write every N ms
  AGE_1:    5000,    // Low → Standard
  AGE_2:    4000,    // Standard → Critical (from when it became Standard)
  HEARTBEAT:1500,    // slaves detect missing heartbeat
  ELECTION: 4000,    // total crash→new-master time
  DONE_TTL:  700,    // how long a done/lost packet stays visible
};

export const mkLog = (msg, tone='info') => ({
  id: `${Date.now()}-${Math.random().toString(36).slice(2,6)}`,
  time: new Date().toLocaleTimeString(),
  message: msg, tone,
});
