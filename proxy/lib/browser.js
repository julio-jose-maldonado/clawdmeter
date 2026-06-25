const { chromium } = require('playwright');

const CDP_PORT = process.env.CDP_PORT || '9222';
const RECONNECT_BACKOFF_MS = 5000; // no martillar a Brave si está caído

let browser = null;
let page = null;
let connectingPromise = null;
let lastConnectAttempt = 0;

// Estado de salud del proxy, expuesto a /api/usage, /health y la PWA.
const status = {
  connected: false,   // CDP a Brave vivo
  sessionValid: false, // sesión de claude.ai activa
  lastError: null,
  lastOkAt: 0,
};

async function doConnect() {
  console.log('Conectando a Brave...');
  browser = await chromium.connectOverCDP(`http://127.0.0.1:${CDP_PORT}`);

  // Si Brave se cierra, no crasheamos: marcamos caído y reintentamos al próximo fetch.
  browser.on('disconnected', () => {
    status.connected = false;
    status.lastError = 'Brave se cerró o el CDP se desconectó';
    page = null;
    browser = null;
    console.warn('⚠  Brave desconectado — se reintentará al próximo fetch');
  });

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
    await new Promise((r) => setTimeout(r, 2000));
  }

  const finalTitle = await page.title();
  status.connected = true;
  if (finalTitle.includes('Log in') || finalTitle.includes('Sign up')) {
    status.sessionValid = false;
    status.lastError = 'Sin sesión en claude.ai — ejecutá ./start.sh --login';
    console.error('\n⚠  NO HAY SESION — ejecutá: ./start.sh --login');
    console.error('   Eso abre Brave visible para que inicies sesión manualmente.\n');
  } else {
    status.sessionValid = true;
    status.lastError = null;
    console.log(`Conectado: ${finalTitle}`);
  }
}

// Conecta con dedupe + backoff. NO lanza: deja el resultado en `status`.
async function connect() {
  if (connectingPromise) return connectingPromise;
  if (!status.connected && Date.now() - lastConnectAttempt < RECONNECT_BACKOFF_MS) {
    return; // dentro del backoff: devolvemos estado caído sin reintentar
  }
  lastConnectAttempt = Date.now();
  connectingPromise = (async () => {
    try {
      await doConnect();
    } catch (e) {
      status.connected = false;
      status.lastError = `No se pudo conectar a Brave: ${e.message}`;
      console.error('⚠ ', status.lastError);
    } finally {
      connectingPromise = null;
    }
  })();
  return connectingPromise;
}

async function init() {
  // No exit: si falla, el proxy igual queda arriba sirviendo el estado caído.
  await connect();
}

function isReady() {
  return status.connected && page && !page.isClosed();
}

async function ensureReady() {
  if (isReady()) return true;
  await connect();
  return isReady();
}

async function fetchFromClaude(path) {
  if (!(await ensureReady())) {
    return { error: 'proxy_down', message: status.lastError || 'sin conexión a Brave' };
  }

  try {
    const data = await page.evaluate(async (apiPath) => {
      try {
        const resp = await fetch(`/api/${apiPath}`);
        if (!resp.ok) return { error: resp.status, message: resp.statusText };
        return await resp.json();
      } catch (e) {
        return { error: 'fetch_error', message: e.message };
      }
    }, path);

    // 401/403 desde claude.ai = sesión expirada
    if (data && (data.error === 401 || data.error === 403)) {
      status.sessionValid = false;
      status.lastError = 'Sesión expirada — ejecutá ./start.sh --login';
    } else if (!data || data.error == null) {
      status.sessionValid = true;
      status.lastOkAt = Date.now();
      status.lastError = null;
    }
    return data;
  } catch (e) {
    // La página o el navegador murieron a mitad de camino.
    status.connected = false;
    status.lastError = `Conexión perdida: ${e.message}`;
    page = null;
    return { error: 'proxy_down', message: e.message };
  }
}

function getStatus() {
  return {
    connected: status.connected,
    sessionValid: status.sessionValid,
    ok: status.connected && status.sessionValid,
    lastError: status.lastError,
    lastOkAt: status.lastOkAt,
  };
}

async function close() {
  if (browser) {
    try { await browser.close(); } catch { /* ya estaba cerrado */ }
  }
}

module.exports = { init, fetchFromClaude, close, getStatus };
