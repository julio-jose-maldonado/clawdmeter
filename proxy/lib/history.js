// Histórico de uso: persiste muestras en SQLite (better-sqlite3, archivo único,
// sin servidor) y deriva series + estadísticas. Base compartida: la PWA toma la
// serie completa y el ESP32 una versión compacta (sparkline + proyección).
//
// El backend de storage está aislado tras esta API (load/record/series/summary/
// sparkline): se puede cambiar sin tocar usage.js, server.js, la PWA ni el firmware.

const fs = require('node:fs');
const path = require('node:path');
const Database = require('better-sqlite3');

const DATA_DIR = path.join(__dirname, '..', 'data');
const FILE = path.join(DATA_DIR, 'history.db');

const RETENTION_MS = 14 * 24 * 60 * 60 * 1000; // se guardan 14 días
const MIN_INTERVAL_MS = 55_000;                // como mucho ~1 muestra/min

// Ventana de regresión y mínimo de datos por métrica. La de 7d/extra se mueve
// lentísimo: estimar su ritmo con 25 min de datos es puro ruido, por eso usa
// ventanas largas y exige varias horas de historia antes de proyectar.
const PROJ = {
  h5: { windowMs: 30 * 60 * 1000,      minSpanMs: 20 * 60 * 1000 },      // 5h
  d7: { windowMs: 6 * 60 * 60 * 1000,  minSpanMs: 3 * 60 * 60 * 1000 },  // 7 días
  ex: { windowMs: 12 * 60 * 60 * 1000, minSpanMs: 6 * 60 * 60 * 1000 },  // extra
};

// clave de columna en la DB -> nombre de métrica del API
const METRIC_KEY = { five_hour: 'h5', seven_day: 'd7', extra_usage: 'ex' };

let lastRecord = 0;

const round1 = (v) => (v == null ? null : Math.round(v * 10) / 10);
const round2 = (v) => Math.round(v * 100) / 100;

if (!fs.existsSync(DATA_DIR)) fs.mkdirSync(DATA_DIR, { recursive: true });

const db = new Database(FILE);
db.pragma('journal_mode = WAL'); // escrituras concurrentes seguras + durabilidad
db.exec(`CREATE TABLE IF NOT EXISTS samples (
  t  INTEGER PRIMARY KEY,
  h5 REAL,
  d7 REAL,
  ex REAL
)`);

const stmtInsert = db.prepare('INSERT OR REPLACE INTO samples (t, h5, d7, ex) VALUES (?, ?, ?, ?)');
const stmtWindow = db.prepare('SELECT t, h5, d7, ex FROM samples WHERE t >= ? ORDER BY t');
const stmtPrune = db.prepare('DELETE FROM samples WHERE t < ?');
const stmtCount = db.prepare('SELECT COUNT(*) AS n FROM samples');
const stmtLast = {
  h5: db.prepare('SELECT h5 AS v FROM samples WHERE h5 IS NOT NULL ORDER BY t DESC LIMIT 1'),
  d7: db.prepare('SELECT d7 AS v FROM samples WHERE d7 IS NOT NULL ORDER BY t DESC LIMIT 1'),
  ex: db.prepare('SELECT ex AS v FROM samples WHERE ex IS NOT NULL ORDER BY t DESC LIMIT 1'),
};

function load() {
  stmtPrune.run(Date.now() - RETENTION_MS);
  const { n } = stmtCount.get();
  console.log(`Histórico (SQLite): ${n} muestras (retención ${RETENTION_MS / 86400000}d)`);
}

// u = { h5, d7, ex }  (porcentajes; ex null/<0 cuando no hay extra usage)
function record(u) {
  const now = Date.now();
  if (now - lastRecord < MIN_INTERVAL_MS) return;
  lastRecord = now;

  try {
    stmtInsert.run(
      now,
      round1(u.h5 || 0),
      round1(u.d7 || 0),
      u.ex == null || u.ex < 0 ? null : round1(u.ex),
    );
    stmtPrune.run(now - RETENTION_MS);
  } catch (e) {
    console.error('Histórico: error al guardar:', e.message);
  }
}

function rowsInWindow(windowMs) {
  return stmtWindow.all(Date.now() - windowMs);
}

function lastValue(key) {
  const row = stmtLast[key].get();
  return row ? row.v : null;
}

// Reduce a ~maxPoints buckets temporales, promediando cada uno.
function downsample(rows, key, maxPoints) {
  const pts = rows.filter((s) => s[key] != null).map((s) => ({ t: s.t, v: s[key] }));
  if (pts.length <= maxPoints) return pts;

  const t0 = pts[0].t;
  const span = Math.max(1, pts[pts.length - 1].t - t0);
  const buckets = Array.from({ length: maxPoints }, () => ({ sum: 0, n: 0, tsum: 0 }));
  for (const p of pts) {
    let i = Math.floor(((p.t - t0) / span) * maxPoints);
    if (i >= maxPoints) i = maxPoints - 1;
    const b = buckets[i];
    b.sum += p.v; b.n++; b.tsum += p.t;
  }
  return buckets
    .filter((b) => b.n > 0)
    .map((b) => ({ t: Math.round(b.tsum / b.n), v: round1(b.sum / b.n) }));
}

// Ritmo (pct/min) por regresión lineal. Devuelve null si no hay datos suficientes
// (pocas muestras o span más corto que el mínimo de la métrica) -> no se proyecta.
function slopePerMin(key) {
  const cfg = PROJ[key] || PROJ.h5;
  const pts = rowsInWindow(cfg.windowMs).filter((s) => s[key] != null);
  if (pts.length < 3) return null;
  if (pts[pts.length - 1].t - pts[0].t < cfg.minSpanMs) return null;

  const t0 = pts[0].t;
  let n = 0, sx = 0, sy = 0, sxx = 0, sxy = 0;
  for (const p of pts) {
    const x = (p.t - t0) / 60000, y = p[key];
    n++; sx += x; sy += y; sxx += x * x; sxy += x * y;
  }
  const denom = n * sxx - sx * sx;
  if (Math.abs(denom) < 1e-9) return 0;
  return (n * sxy - sx * sy) / denom;
}

// Minutos hasta el reset de la ventana (resetMs = epoch ms del reset, o null).
function minsToReset(resetMs) {
  if (resetMs == null) return -1;
  const d = Math.round((resetMs - Date.now()) / 60000);
  return d > 0 ? d : 0;
}

// Proyección: tendencia, ETA al 100% al ritmo actual y cruce con el reset.
// hitsLimit = true si llegás al 100% ANTES de que la ventana resetee.
function projection(key, resetMs) {
  const resetMin = minsToReset(resetMs);
  const current = lastValue(key);
  if (current == null) {
    return { current: null, trend: 'flat', etaMin: -1, resetMin, hitsLimit: false, slopePerMin: 0 };
  }

  const m = slopePerMin(key);
  if (m == null) {
    return { current, trend: 'flat', etaMin: -1, resetMin, hitsLimit: false, slopePerMin: 0 };
  }

  let trend = 'flat';
  if (m > 0.05) trend = 'up';
  else if (m < -0.05) trend = 'down';

  let etaMin = -1;
  if (m > 0.05 && current < 100) etaMin = Math.round((100 - current) / m);

  // ¿Tocás el límite antes de que la ventana resetee? Si no sabemos el reset
  // (resetMin < 0), nos quedamos con la ETA cruda.
  const hitsLimit = etaMin > 0 && (resetMin < 0 || etaMin <= resetMin);

  return { current, trend, etaMin, resetMin, hitsLimit, slopePerMin: round2(m) };
}

function metricSummary(key, rows, warnPct, resetMs) {
  const vals = rows.map((s) => s[key]).filter((v) => v != null);
  const peak = vals.length ? round1(Math.max(...vals)) : null;
  const avg = vals.length ? round1(vals.reduce((a, b) => a + b, 0) / vals.length) : null;
  const redZoneFrac = vals.length
    ? round2(vals.filter((v) => v >= warnPct).length / vals.length)
    : 0;
  return { peak, avg, redZoneFrac, ...projection(key, resetMs) };
}

// --- API pública del módulo ---

// Serie completa (para la PWA): { five_hour:[{t,v}], seven_day, extra_usage }
function series(windowMs, points) {
  const rows = rowsInWindow(windowMs);
  const out = {};
  for (const [metric, key] of Object.entries(METRIC_KEY)) {
    out[metric] = downsample(rows, key, points);
  }
  return out;
}

// Resumen de métricas derivadas (para la PWA).
// resets = { five_hour: epochMs, seven_day: epochMs, extra_usage: epochMs }.
function summary(windowMs, warnPct = 80, resets = {}) {
  const rows = rowsInWindow(windowMs);
  const out = { sampleCount: rows.length };
  for (const [metric, key] of Object.entries(METRIC_KEY)) {
    out[metric] = metricSummary(key, rows, warnPct, resets[metric]);
  }
  return out;
}

// Solo las proyecciones por métrica (sin series ni stats). Liviano, para
// embeber en /api/usage y alimentar los LEDs del ESP32. resets = mapa metric->ms.
function projections(resets = {}) {
  const out = {};
  for (const [metric, key] of Object.entries(METRIC_KEY)) {
    out[metric] = projection(key, resets[metric]);
  }
  return out;
}

// Versión compacta para el ESP32: array de enteros 0-100 + proyección.
function sparkline(metric, windowMs, points, resetMs) {
  const key = METRIC_KEY[metric] || 'h5';
  const rows = rowsInWindow(windowMs);
  const ds = downsample(rows, key, points);
  const proj = projection(key, resetMs);
  const vals = ds.map((p) => p.v);
  return {
    metric,
    points: vals.map((v) => Math.round(v)),
    peak: vals.length ? Math.round(Math.max(...vals)) : 0,
    current: proj.current == null ? 0 : Math.round(proj.current),
    trend: proj.trend,
    etaMin: proj.etaMin,
    resetMin: proj.resetMin,
    hitsLimit: proj.hitsLimit,
  };
}

module.exports = { load, record, series, summary, sparkline, projections };
