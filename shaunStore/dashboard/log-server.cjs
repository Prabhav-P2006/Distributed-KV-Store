const express = require('express');
const fs = require('fs');
const path = require('path');
const cors = require('cors');

const app = express();
app.use(cors());
app.use(express.json());

const LOG_DIR = '/tmp/shaunstore-logs';
if (!fs.existsSync(LOG_DIR)) fs.mkdirSync(LOG_DIR);

app.post('/log', (req, res) => {
  const { node, msg } = req.body;
  console.log(`[LOG] -> ${node}: ${msg}`);
  const filePath = path.join(LOG_DIR, `${node}.log`);
  const timestamp = new Date().toLocaleTimeString();
  fs.appendFileSync(filePath, `[${timestamp}] ${msg}\n`);
  res.sendStatus(200);
});

app.post('/clear', (req, res) => {
  if (fs.existsSync(LOG_DIR)) {
    fs.readdirSync(LOG_DIR).forEach(file => {
      fs.unlinkSync(path.join(LOG_DIR, file));
    });
  }
  res.sendStatus(200);
});

const PORT = 3001;
app.listen(PORT, () => {
  console.log(`Log server running on http://localhost:${PORT}`);
});
