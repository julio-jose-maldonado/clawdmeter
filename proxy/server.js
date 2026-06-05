const express = require('express');
const path = require('path');
const browser = require('./lib/browser');
const { fetchFromClaude } = require('./lib/browser');
const usage = require('./lib/usage');

const PORT = parseInt(process.env.PROXY_PORT || '3456');

const app = express();
app.use((_, res, next) => { res.header('Access-Control-Allow-Origin', '*'); next(); });
app.use(express.static(path.join(__dirname, 'public')));

app.get('/api/usage', async (req, res) => {
  try {
    await usage.refresh();
    if (!usage.hasData()) return res.status(503).json({ error: 'No data yet' });
    res.json(usage.getData());
  } catch (e) { res.status(500).json({ error: e.message }); }
});

app.get('/health', (_, res) => {
  const data = usage.getData();
  res.json({ status: 'ok', hasData: usage.hasData(), lastFetch: data.cached_at });
});

app.get('/api/debug/{*path}', async (req, res) => {
  try {
    const data = await fetchFromClaude(req.params.path);
    res.json(data);
  } catch (e) { res.status(500).json({ error: e.message }); }
});

(async () => {
  await browser.init();
  await usage.refresh();
  app.listen(PORT, '0.0.0.0', () => {
    console.log(`\nClawdmeter proxy en http://localhost:${PORT}/api/usage`);
    console.log(`Clawdmeter app en http://localhost:${PORT}/\n`);
  });
})();

process.on('SIGINT', async () => {
  await browser.close();
  process.exit(0);
});
