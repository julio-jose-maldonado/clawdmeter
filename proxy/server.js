const express = require('express');
const path = require('node:path');
const browser = require('./lib/browser');
const usage = require('./lib/usage');
const history = require('./lib/history');
const config = require('./lib/config');
const backup = require('./lib/backup');

const PORT = Number.parseInt(process.env.PROXY_PORT || '3456');
const SAMPLE_MS = 60_000; // muestreo continuo del histórico
const BACKUP_MS = 24 * 60 * 60 * 1000; // backup diario de las DBs

// Parseo de ventanas tipo "5h", "24h", "7d" -> milisegundos
function parseWindow(str, def) {
  if (!str) return def;
  const m = /^(\d+)\s*([hd])$/.exec(String(str).trim());
  if (!m) return def;
  const n = Number.parseInt(m[1]);
  return m[2] === 'd' ? n * 86400000 : n * 3600000;
}
const clamp = (v, lo, hi, def) =>
  Number.isFinite(v) ? Math.min(hi, Math.max(lo, v)) : def;

// Epoch ms del reset de una métrica, leído de los datos de uso cacheados (o null).
function resetMsFor(metric) {
  const u = usage.getData().usage;
  const iso = u && u[metric] && u[metric].resets_at;
  const t = iso ? Date.parse(iso) : Number.NaN;
  return Number.isFinite(t) ? t : null;
}

// Basic Auth solo para /api/config (lectura y escritura de los ajustes).
function requireConfigAuth(req, res, next) {
  const m = /^Basic (.+)$/.exec(req.headers.authorization || '');
  if (m) {
    const [u, p] = Buffer.from(m[1], 'base64').toString().split(':');
    if (u === config.get().config_user && config.checkPassword(p)) return next();
  }
  // 401 SIN WWW-Authenticate a proposito: con ese header, Brave/Chrome muestran su
  // diálogo nativo de login en vez de dejar que la PWA use su propio formulario.
  return res.status(401).json({ error: 'auth required' });
}

const app = express();
// Sin CORS abierto a proposito: la PWA es same-origin (la sirve este mismo proxy)
// y el ESP32 no es un browser. Asi ninguna web externa puede leer estos endpoints.
app.use(express.static(path.join(__dirname, 'public')));
app.use(express.json()); // body de POST /api/config

app.get('/api/usage', async (req, res) => {
  try {
    await usage.refresh();
    const data = usage.getData();
    // Sin datos todavía: 503 pero incluyendo el estado del proxy para que la PWA
    // muestre el motivo (Brave caído / sin sesión) en vez de un error opaco.
    if (!usage.hasData()) return res.status(503).json({ error: 'No data yet', proxy: data.proxy });
    // Proyección embebida (5h/7d/extra) para los LEDs del ESP32: una sola request.
    data.projection = history.projections({
      five_hour: resetMsFor('five_hour'),
      seven_day: resetMsFor('seven_day'),
      extra_usage: resetMsFor('extra_usage'),
    });
    // Con datos: los servimos aunque estén stale o el proxy esté caído (con el flag).
    res.json(data);
  } catch (e) { res.status(500).json({ error: e.message }); }
});

app.get('/health', (_, res) => {
  const data = usage.getData();
  res.json({
    status: 'ok',
    hasData: usage.hasData(),
    lastFetch: data.cached_at,
    stale: data.stale,
    proxy: data.proxy,
  });
});

// Histórico completo (para la PWA): series + métricas derivadas.
app.get('/api/history', (req, res) => {
  const windowMs = parseWindow(req.query.window, 24 * 3600000);
  const points = clamp(Number.parseInt(req.query.points), 10, 500, 120);
  // Si la query no trae warn, usa el umbral de la config (warn_threshold).
  const warn = clamp(Number.parseInt(req.query.warn), 1, 100, config.get().warn_threshold);
  const resets = {
    five_hour: resetMsFor('five_hour'),
    seven_day: resetMsFor('seven_day'),
    extra_usage: resetMsFor('extra_usage'),
  };
  res.json({
    window: req.query.window || '24h',
    generated_at: Date.now(),
    warn,
    series: history.series(windowMs, points),
    summary: history.summary(windowMs, warn, resets),
  });
});

// Histórico compacto (para el ESP32): sparkline + proyección de una métrica.
app.get('/api/history/sparkline', (req, res) => {
  const metric = ['five_hour', 'seven_day', 'extra_usage'].includes(req.query.metric)
    ? req.query.metric
    : 'five_hour';
  const windowMs = parseWindow(req.query.window, 5 * 3600000);
  const points = clamp(Number.parseInt(req.query.points), 8, 64, 32);
  res.json(history.sparkline(metric, windowMs, points, resetMsFor(metric)));
});

// Configuración centralizada: el ESP32 la baja al arrancar y la PWA la edita.
// El bootstrap (WiFi + IP/puerto del proxy) NO está acá: vive en el ESP32.
// Nunca devolvemos el hash del password al cliente.
const publicConfig = (c) => { const { config_pass: _hash, ...rest } = c; return rest; };
app.get('/api/config', requireConfigAuth, (_, res) => res.json(publicConfig(config.get())));
app.post('/api/config', requireConfigAuth, (req, res) => {
  try {
    res.json(publicConfig(config.update(req.body)));
  } catch (e) { res.status(400).json({ error: e.message }); }
});

(async () => {
  history.load();
  config.load();
  await browser.init();
  await usage.refresh();
  // Muestreo continuo: el histórico sigue creciendo aunque nadie mire la app.
  setInterval(() => { usage.refresh().catch(() => {}); }, SAMPLE_MS);
  // Backup de las DBs (fuera del proyecto): al arrancar y una vez por día.
  backup.backupNow().catch((e) => console.error('Backup inicial falló:', e.message));
  setInterval(() => { backup.backupNow().catch(() => {}); }, BACKUP_MS);
  app.listen(PORT, '0.0.0.0', () => {
    console.log(`\nClawdmeter proxy en http://localhost:${PORT}/api/usage`);
    console.log(`Clawdmeter app en http://localhost:${PORT}/\n`);
  });
})();

process.on('SIGINT', async () => {
  await browser.close();
  process.exit(0);
});
