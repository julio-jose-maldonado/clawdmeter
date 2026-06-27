// Configuración centralizada en el proxy (SQLite). Fuente de verdad de todos los
// ajustes de comportamiento; el ESP32 la baja vía GET /api/config y la PWA la edita.
// NO incluye el bootstrap (WiFi + proxy_ip/port): eso queda local en el ESP32.

const fs = require('node:fs');
const path = require('node:path');
const Database = require('better-sqlite3');
const bcrypt = require('bcryptjs');

const DATA_DIR = path.join(__dirname, '..', 'data');
const FILE = path.join(DATA_DIR, 'config.db');

// Defaults espejados de firmware/Clawdmeter/config.ino (loadConfig()).
const DEFAULTS = {
  refresh_sec: 60,
  led_brightness: 100,
  lcd_brightness: 200,
  flip_screen: false,
  timezone: 'ART3',
  lat: 0.0,
  lon: 0.0,
  city: '',
  home_timeout_sec: 5,
  buzzer_pin: 11,
  alerts_enabled: true,
  warn_threshold: 80,
  crit_threshold: 95,
  night_dim_enabled: false,
  night_start_hour: 23,
  night_end_hour: 7,
  night_brightness: 30,
  // Usuario para entrar a la pagina de config. El password se guarda hasheado
  // (bcrypt) y aparte: nunca como texto plano. Default 'clawdmeter' (cambialo).
  config_user: 'admin',
};

// Tipo + rango por campo (valida y coerciona lo que entra por POST).
const SCHEMA = {
  refresh_sec:       { type: 'int',   min: 10,   max: 3600 },
  led_brightness:    { type: 'int',   min: 0,    max: 255 },
  lcd_brightness:    { type: 'int',   min: 0,    max: 255 },
  flip_screen:       { type: 'bool' },
  timezone:          { type: 'str',   max: 48 },
  lat:               { type: 'float', min: -90,  max: 90 },
  lon:               { type: 'float', min: -180, max: 180 },
  city:              { type: 'str',   max: 40 },
  home_timeout_sec:  { type: 'int',   min: 0,    max: 3600 },
  buzzer_pin:        { type: 'int',   min: 0,    max: 48 },
  alerts_enabled:    { type: 'bool' },
  warn_threshold:    { type: 'int',   min: 1,    max: 100 },
  crit_threshold:    { type: 'int',   min: 1,    max: 100 },
  night_dim_enabled: { type: 'bool' },
  night_start_hour:  { type: 'int',   min: 0,    max: 23 },
  night_end_hour:    { type: 'int',   min: 0,    max: 23 },
  night_brightness:  { type: 'int',   min: 0,    max: 255 },
  config_user:       { type: 'str',   max: 32 },
};

if (!fs.existsSync(DATA_DIR)) fs.mkdirSync(DATA_DIR, { recursive: true });
const db = new Database(FILE);
db.pragma('journal_mode = WAL');
db.exec('CREATE TABLE IF NOT EXISTS config (id INTEGER PRIMARY KEY CHECK (id = 1), json TEXT)');
const stmtGet = db.prepare('SELECT json FROM config WHERE id = 1');
const stmtSet = db.prepare(
  'INSERT INTO config (id, json) VALUES (1, ?) ON CONFLICT(id) DO UPDATE SET json = excluded.json'
);

function stored() {
  const row = stmtGet.get();
  if (!row) return {};
  try { return JSON.parse(row.json); } catch { return {}; }
}

// Config efectiva = defaults + lo guardado.
function get() {
  return { ...DEFAULTS, ...stored() };
}

function coerce(key, val) {
  const s = SCHEMA[key];
  if (!s) return undefined; // campo desconocido -> ignorado
  if (s.type === 'bool') return val === true || val === 'true' || val === 1 || val === '1';
  if (s.type === 'str') {
    let v = String(val);
    return s.max ? v.slice(0, s.max) : v;
  }
  let n = s.type === 'float' ? parseFloat(val) : parseInt(val, 10);
  if (!Number.isFinite(n)) return undefined;
  if (s.min != null) n = Math.max(s.min, n);
  if (s.max != null) n = Math.min(s.max, n);
  return n;
}

// Aplica un patch parcial validando cada campo; ignora claves desconocidas.
// config_pass es especial: se guarda HASHEADO (bcrypt) y nunca se vacia.
function update(patch) {
  const next = { ...get() };
  for (const [k, v] of Object.entries(patch || {})) {
    if (k === 'config_pass') {
      if (typeof v === 'string' && v !== '') next.config_pass = bcrypt.hashSync(v, 10);
      continue;
    }
    const c = coerce(k, v);
    if (c !== undefined) next[k] = c;
  }
  stmtSet.run(JSON.stringify(next));
  return next;
}

// Compara un password en texto contra el hash guardado.
function checkPassword(plain) {
  const hash = get().config_pass;
  return typeof hash === 'string' && bcrypt.compareSync(plain || '', hash);
}

function load() {
  if (!stmtGet.get()) stmtSet.run(JSON.stringify(DEFAULTS));
  if (!stored().config_pass) {
    // primer arranque: siembra el hash del password default
    stmtSet.run(JSON.stringify({ ...stored(), config_pass: bcrypt.hashSync('clawdmeter', 10) }));
  }
  console.log('Config (SQLite): lista');
}

module.exports = { get, update, load, checkPassword, DEFAULTS, SCHEMA };
