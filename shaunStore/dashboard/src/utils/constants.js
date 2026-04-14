export const priorityPalette = {
  Critical: "#ff5d5d",
  Standard: "#49c7ff",
  Low: "#61d67c",
  Aging: "#ffae42",
};

export const queueOrder = ["Critical", "Standard", "Low"];
export const consistencyOptions = ["Strong", "Eventual", "Bounded Staleness"];

export const backgroundClients = [
  { id: "client-alpha", label: "Alpha", x: 10, y: 5 },
  { id: "client-bravo", label: "Bravo", x: 30, y: 5 },
  { id: "client-charlie", label: "Charlie", x: 50, y: 5 },
  { id: "client-delta", label: "Delta", x: 70, y: 5 },
  { id: "client-echo", label: "Echo", x: 90, y: 5 },
];

export const createSlaveNodes = () =>
  Array.from({ length: 10 }, (_, index) => ({
    id: `slave-${index + 1}`,
    label: `S${index + 1}`,
    x: 10 + (index % 5) * 20,
    y: 70 + Math.floor(index / 5) * 15,
    role: "slave",
    status: "online",
    offset: 0,
    stale: false,
    votes: 0,
  }));

export const createLogEntry = (message, tone = "info") => ({
  id: `${Date.now()}-${Math.random().toString(16).slice(2)}`,
  time: new Date().toLocaleTimeString(),
  tone,
  message,
});
