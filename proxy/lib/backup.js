// Backups de las DBs (history.db + config.db) usando el backup online de SQLite
// (consistente aunque el proxy esté escribiendo). Se guardan FUERA del proyecto,
// así sobreviven aunque se borre data/. Uno por día, rotando los últimos KEEP.
//
// CLI:  node lib/backup.js now                 -> backup ya
//       node lib/backup.js list                -> lista los backups
//       node lib/backup.js restore <YYYY-MM-DD> -> restaura ese dia (con el proxy apagado)

const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const Database = require('better-sqlite3');

const DATA_DIR = path.join(__dirname, '..', 'data');
const BACKUP_DIR = path.join(os.homedir(), 'Library', 'Application Support', 'clawdmeter', 'backups');
const KEEP = 14;               // cuantos backups por DB se conservan
const DBS = ['history.db', 'config.db'];

function ensureDir() {
  if (!fs.existsSync(BACKUP_DIR)) fs.mkdirSync(BACKUP_DIR, { recursive: true });
}

function today() {
  return new Date().toISOString().slice(0, 10); // YYYY-MM-DD
}

// Borra los backups mas viejos de una DB, dejando los KEEP mas nuevos.
function rotate(base) {
  const files = fs.readdirSync(BACKUP_DIR)
    .filter((f) => f.startsWith(base + '-') && f.endsWith('.db'))
    .sort(); // YYYY-MM-DD ordena cronologicamente
  while (files.length > KEEP) {
    try { fs.unlinkSync(path.join(BACKUP_DIR, files.shift())); } catch { /* ya no esta */ }
  }
}

// Snapshot consistente de cada DB existente -> BACKUP_DIR/<base>-<fecha>.db
async function backupNow() {
  ensureDir();
  const stamp = today();
  for (const name of DBS) {
    const src = path.join(DATA_DIR, name);
    if (!fs.existsSync(src)) continue;
    const base = name.replace(/\.db$/, '');
    const dest = path.join(BACKUP_DIR, `${base}-${stamp}.db`);
    const tmp = dest + '.tmp';
    const db = new Database(src, { readonly: true });
    try {
      await db.backup(tmp);
    } finally {
      db.close();
    }
    fs.renameSync(tmp, dest); // atomico: sobreescribe el del dia si ya existia
    rotate(base);
  }
  console.log(`Backup OK -> ${BACKUP_DIR} (${stamp})`);
}

function listBackups() {
  ensureDir();
  const files = fs.readdirSync(BACKUP_DIR).filter((f) => f.endsWith('.db')).sort();
  if (!files.length) { console.log('(sin backups en ' + BACKUP_DIR + ')'); return; }
  console.log('Backups en ' + BACKUP_DIR + ':');
  for (const f of files) {
    const s = fs.statSync(path.join(BACKUP_DIR, f));
    console.log(`  ${f}  (${(s.size / 1024).toFixed(0)} KB)`);
  }
}

// Restaura un dia: copia los backups de esa fecha sobre data/. El proxy debe
// estar APAGADO (si no, las DBs estan abiertas y la copia puede romperse).
function restore(stamp) {
  if (!stamp) { console.error('Falta la fecha: node lib/backup.js restore <YYYY-MM-DD>'); return; }
  if (!fs.existsSync(DATA_DIR)) fs.mkdirSync(DATA_DIR, { recursive: true });
  let restored = 0;
  for (const name of DBS) {
    const base = name.replace(/\.db$/, '');
    const snap = path.join(BACKUP_DIR, `${base}-${stamp}.db`);
    if (!fs.existsSync(snap)) { console.warn(`(no hay backup de ${base} para ${stamp})`); continue; }
    // limpia WAL/SHM viejos para que la DB restaurada quede consistente
    for (const ext of ['', '-wal', '-shm']) {
      try { fs.rmSync(path.join(DATA_DIR, name + ext), { force: true }); } catch { /* nada */ }
    }
    fs.copyFileSync(snap, path.join(DATA_DIR, name));
    restored++;
    console.log(`Restaurado ${name} desde ${stamp}`);
  }
  if (restored) console.log('Listo. Arranca el proxy de nuevo.');
}

module.exports = { backupNow, listBackups, restore, BACKUP_DIR };

if (require.main === module) {
  const [cmd, arg] = process.argv.slice(2);
  if (cmd === 'now') backupNow().catch((e) => { console.error(e.message); process.exit(1); });
  else if (cmd === 'list') listBackups();
  else if (cmd === 'restore') restore(arg);
  else console.log('uso: node lib/backup.js [now | list | restore <YYYY-MM-DD>]');
}
