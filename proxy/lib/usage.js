const { fetchFromClaude, getStatus } = require('./browser');
const history = require('./history');

let cachedOrg = null;
let cachedUsage = null;
let lastFetch = 0;
const CACHE_TTL = 60_000;
const STALE_MS = 180_000; // datos viejos si no se refrescan hace > 3 min

async function refresh() {
  const now = Date.now();
  if (now - lastFetch < CACHE_TTL && cachedUsage) return;

  console.log(`[${new Date().toLocaleTimeString()}] Actualizando...`);

  if (!cachedOrg) {
    const orgs = await fetchFromClaude('organizations');
    if (orgs.error) { console.error('Error org:', orgs); return; }
    cachedOrg = Array.isArray(orgs) ? orgs[0] : orgs;
    console.log(`Org: ${cachedOrg.name}`);
  }

  const usage = await fetchFromClaude(`organizations/${cachedOrg.uuid}/usage`);
  if (usage.error) { console.error('Error usage:', usage); return; }

  cachedUsage = usage;
  lastFetch = now;

  const h5 = usage.five_hour?.utilization || 0;
  const d7 = usage.seven_day?.utilization || 0;
  const ex = usage.extra_usage?.utilization || 0;
  console.log(`OK — 5h: ${h5}% | 7d: ${d7}% | extra: ${ex}%`);

  history.record({
    h5,
    d7,
    ex: usage.extra_usage?.is_enabled ? ex : null,
  });
}

function getData() {
  return {
    org: cachedOrg ? { name: cachedOrg.name, plan: cachedOrg.rate_limit_tier } : null,
    usage: cachedUsage,
    cached_at: lastFetch,
    proxy: getStatus(),
    stale: lastFetch > 0 ? Date.now() - lastFetch > STALE_MS : true,
  };
}

function hasData() {
  return !!cachedUsage;
}

module.exports = { refresh, getData, hasData };
