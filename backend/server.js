const path = require('path');
const fs = require('fs');
const express = require('express');
const { runNova } = require('./lib/runNova');

const app = express();
const PORT = process.env.PORT || 4000;
const FRONTEND_DIR = path.join(__dirname, '..', 'frontend');
const EXAMPLES_PATH = path.join(__dirname, 'data', 'examples.json');

app.use(express.json({ limit: '256kb' })); // a Nova script is source text; 256kb is generous
app.use(express.static(FRONTEND_DIR));

app.get('/api/examples', (req, res) => {
  res.type('application/json').send(fs.readFileSync(EXAMPLES_PATH, 'utf8'));
});

app.post('/api/run', async (req, res) => {
  const code = req.body && typeof req.body.code === 'string' ? req.body.code : null;
  if (code === null) {
    return res.status(400).json({ error: "Request body must be JSON: { \"code\": \"...\" }" });
  }
  if (code.length === 0) {
    return res.status(400).json({ error: 'No code to run.' });
  }

  try {
    const result = await runNova(code);
    res.json(result);
  } catch (err) {
    console.error('Unexpected error in /api/run:', err);
    res.status(500).json({ error: 'Internal server error while running the program.' });
  }
});

app.listen(PORT, () => {
  console.log(`Nova playground running at http://localhost:${PORT}`);
});