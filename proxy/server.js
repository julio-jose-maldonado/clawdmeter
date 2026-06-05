const { chromium } = require('playwright');
const express = require('express');
const path = require('path');

const PORT = parseInt(process.env.PROXY_PORT || '3456');
const CDP_PORT = process.env.CDP_PORT || '9222';

let browser, page;
let cachedOrg = null;
let cachedUsage = null;
let lastFetch = 0;
const CACHE_TTL = 60_000;

async function initBrowser() {
  console.log('Conectando a Brave...');
  browser = await chromium.connectOverCDP(`http://127.0.0.1:${CDP_PORT}`);
  const context = browser.contexts()[0];

  page = await context.newPage();
  console.log('Navegando a claude.ai...');
  await page.goto('https://claude.ai', { waitUntil: 'domcontentloaded', timeout: 30000 });

  const title = await page.title();
  if (title.includes('Just a moment')) {
    console.log('Cloudflare challenge... esperando (puede tardar 10-15s)');
    await page.waitForFunction(
      () => !document.title.includes('Just a moment'),
      { timeout: 60000 }
    );
    await new Promise(r => setTimeout(r, 2000));
  }

  const finalTitle = await page.title();
  if (finalTitle.includes('Log in') || finalTitle.includes('Sign up')) {
    console.error('\n⚠  NO HAY SESION — ejecutá: ./start.sh --login');
    console.error('   Eso abre Brave visible para que inicies sesión manualmente.\n');
    process.exit(1);
  }

  console.log(`Conectado: ${finalTitle}`);
}

async function fetchFromClaude(path) {
  return await page.evaluate(async (apiPath) => {
    try {
      const resp = await fetch(`/api/${apiPath}`);
      if (!resp.ok) return { error: resp.status, message: resp.statusText };
      return await resp.json();
    } catch (e) {
      return { error: 'fetch_error', message: e.message };
    }
  }, path);
}

async function refreshData() {
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

const app = express();
app.use((_, res, next) => { res.header('Access-Control-Allow-Origin', '*'); next(); });
app.use(express.static(path.join(__dirname, 'public')));

app.get('/api/usage', async (req, res) => {
  try {
    await refreshData();
    if (!cachedUsage) return res.status(503).json({ error: 'No data yet' });
    res.json({
      org: { name: cachedOrg?.name, plan: cachedOrg?.rate_limit_tier },
      usage: cachedUsage,
      cached_at: lastFetch,
    });
  } catch (e) { res.status(500).json({ error: e.message }); }
});

app.get('/health', (_, res) => {
  res.json({ status: 'ok', hasData: !!cachedUsage, lastFetch });
});

app.get('/api/debug/{*path}', async (req, res) => {
  try {
    const data = await fetchFromClaude(req.params.path);
    res.json(data);
  } catch (e) { res.status(500).json({ error: e.message }); }
});

(async () => {
  await initBrowser();
  await refreshData();
  app.listen(PORT, '0.0.0.0', () => {
    console.log(`\nClawdmeter proxy en http://localhost:${PORT}/api/usage`);
    console.log(`Clawdmeter app en http://localhost:${PORT}/\n`);
  });
})();

process.on('SIGINT', async () => {
  if (browser) await browser.close();
  process.exit(0);
});
