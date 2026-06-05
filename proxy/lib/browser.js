const { chromium } = require('playwright');

const CDP_PORT = process.env.CDP_PORT || '9222';

let browser, page;

async function init() {
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

async function close() {
  if (browser) await browser.close();
}

module.exports = { init, fetchFromClaude, close };
