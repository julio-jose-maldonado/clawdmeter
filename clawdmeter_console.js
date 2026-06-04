// ===========================================
// CLAWDMETER - Pegá esto en la consola de DevTools
// estando en claude.ai
// (F12 o Cmd+Option+I → Console → pegar → Enter)
//
// Envia los datos a localhost:3456 cada 5 min
// ===========================================

(async () => {
  const SERVER = 'http://localhost:3456';
  const INTERVAL = 5 * 60 * 1000; // 5 minutos

  const log = (msg) => console.log(`%c[Clawdmeter] ${msg}`, 'color: #00ccff; font-weight: bold');
  const err = (msg) => console.error(`[Clawdmeter] ${msg}`);

  async function fetchAndPush() {
    log('Obteniendo datos...');

    // 1. Organizations
    let org;
    try {
      const resp = await fetch('/api/organizations');
      if (!resp.ok) { err(`organizations: ${resp.status}`); return; }
      const orgs = await resp.json();
      org = Array.isArray(orgs) ? orgs[0] : orgs;
    } catch (e) { err(`Error organizations: ${e}`); return; }

    const orgId = org.uuid || org.id;
    const orgName = org.name || '?';
    const plan = org.rate_limit_tier || '?';

    // 2. Usage
    let usage;
    try {
      const resp = await fetch(`/api/organizations/${orgId}/usage`);
      if (!resp.ok) { err(`usage: ${resp.status}`); return; }
      usage = await resp.json();
    } catch (e) { err(`Error usage: ${e}`); return; }

    log(`Org: ${orgName} | 5h: ${usage.five_hour?.utilization}% | 7d: ${usage.seven_day?.utilization}%`);

    // 3. Push to local server
    const payload = {
      org: { name: orgName, plan },
      usage,
    };

    try {
      const resp = await fetch(`${SERVER}/api/push`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload),
      });
      if (resp.ok) {
        log('Datos enviados al server OK');
      } else {
        err(`Server respondio: ${resp.status}`);
      }
    } catch (e) {
      err(`No se pudo enviar al server (${e.message}). Esta corriendo?`);
      log('Datos locales OK, pero el server no esta disponible');
    }
  }

  // Primera ejecucion
  await fetchAndPush();

  // Loop cada 5 min
  log(`Actualizando cada ${INTERVAL/60000} minutos. Dejá esta tab abierta.`);
  setInterval(fetchAndPush, INTERVAL);
})();
