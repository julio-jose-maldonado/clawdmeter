// Estado de los servicios de Claude (status.claude.com, Statuspage de Atlassian).
// La API es pública, sin auth y sin Cloudflare, así que acá sí podemos hacer
// fetch directo desde Node — no hace falta pasar por Brave como con claude.ai.
const SUMMARY_URL = 'https://status.claude.com/api/v2/summary.json';
const CACHE_TTL = 150_000; // la statuspage cambia lento; 2.5 min alcanza
const FETCH_TIMEOUT = 10_000;

// Componentes que mostramos: id estable de Statuspage + fallback por nombre
// (por si algún día rotan los ids). El resto (Console, Cowork, Gov) no interesa.
const WATCHED = [
  { id: 'rwppv331jlwc', name: 'claude.ai', label: 'claude.ai' },
  { id: 'yyzkbfz2thpt', name: 'Claude Code', label: 'Claude Code' },
  { id: 'k8w3r06qmzrp', name: 'Claude API', label: 'API' },
];

let cached = null;
let lastFetch = 0;
let lastError = null;

function pickComponent(components, w) {
  return components.find((c) => c.id === w.id)
    || components.find((c) => c.name && c.name.startsWith(w.name));
}

async function refresh() {
  const now = Date.now();
  if (now - lastFetch < CACHE_TTL && cached) return;
  try {
    const r = await fetch(SUMMARY_URL, { signal: AbortSignal.timeout(FETCH_TIMEOUT) });
    if (!r.ok) throw new Error(`HTTP ${r.status}`);
    const d = await r.json();

    const components = WATCHED.map((w) => {
      const c = pickComponent(d.components || [], w);
      // status posibles: operational / degraded_performance / partial_outage /
      // major_outage / under_maintenance (o "unknown" si el componente no está)
      return { label: w.label, status: c ? c.status : 'unknown' };
    });

    // Incidentes abiertos que tocan alguno de nuestros componentes; si el
    // incidente no lista componentes lo incluimos igual (suele ser global).
    const watchedIds = new Set(WATCHED.map((w) => w.id));
    const incidents = (d.incidents || [])
      .filter((i) => !i.components?.length || i.components.some((c) => watchedIds.has(c.id)))
      .map((i) => ({
        name: i.name,
        impact: i.impact, // none / minor / major / critical
        status: i.status, // investigating / identified / monitoring / resolved
        updated_at: i.updated_at,
        url: i.shortlink,
      }));

    cached = {
      indicator: d.status?.indicator || 'none',
      description: d.status?.description || '',
      components,
      incidents,
    };
    lastFetch = now;
    lastError = null;
  } catch (e) {
    // Sin red o statuspage caída: conservamos el último dato y marcamos stale.
    lastError = e.message;
  }
}

function getData() {
  return {
    ...(cached || { indicator: 'unknown', description: '', components: [], incidents: [] }),
    cached_at: lastFetch,
    stale: lastFetch > 0 ? Date.now() - lastFetch > CACHE_TTL * 2 : true,
    error: lastError,
  };
}

module.exports = { refresh, getData };
