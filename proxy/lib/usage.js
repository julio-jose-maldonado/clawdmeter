const { fetchFromClaude } = require('./browser');

let cachedOrg = null;
let cachedUsage = null;
let lastFetch = 0;
const CACHE_TTL = 60_000;

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
}

function getData() {
  return {
    org: cachedOrg ? { name: cachedOrg.name, plan: cachedOrg.rate_limit_tier } : null,
    usage: cachedUsage,
    cached_at: lastFetch,
  };
}

function hasData() {
  return !!cachedUsage;
}

module.exports = { refresh, getData, hasData };
