#!/bin/bash
#
# Clawdmeter - Proxy con browser oculto
# Primer uso: ./start.sh --login  (abre Brave visible para loguearte)
# Después:    ./start.sh           (Brave oculto, usa sesión guardada)
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ENV_FILE="$SCRIPT_DIR/.env"
BRAVE="/Applications/Brave Browser.app/Contents/MacOS/Brave Browser"

# ---- Cargar .env ----
if [ ! -f "$ENV_FILE" ]; then
  echo "Error: No existe .env — copiá .env.example a .env y completalo"
  exit 1
fi

set -a
source "$ENV_FILE"
set +a

CDP_PORT="${CDP_PORT:-9222}"
PROXY_PORT="${PROXY_PORT:-3456}"
CHROME_DATA="${CHROME_DATA:-$HOME/.clawdmeter-browser}"

LOGIN_MODE=false
if [ "${1}" = "--login" ]; then
  LOGIN_MODE=true
fi

# ---- Primer uso: detectar si no existe el perfil ----
if [ ! -d "$CHROME_DATA" ] && [ "$LOGIN_MODE" = false ]; then
  echo ""
  echo "Primera vez — ejecutá con --login para iniciar sesión:"
  echo "  ./start.sh --login"
  echo ""
  exit 1
fi

# ---- Cleanup ----
PROXY_PID=""

cleanup() {
  echo ""
  echo "Deteniendo..."
  [ -n "$PROXY_PID" ] && kill "$PROXY_PID" 2>/dev/null
  pkill -f "Brave.*clawdmeter-browser" 2>/dev/null || true
  exit 0
}
trap cleanup INT TERM

# ---- Dependencias ----
if [ ! -d "$SCRIPT_DIR/proxy/node_modules" ]; then
  echo "Instalando dependencias..."
  cd "$SCRIPT_DIR/proxy"
  pnpm install
  cd "$SCRIPT_DIR"
fi

# ---- Matar instancias previas del proxy ----
pkill -f "Brave.*clawdmeter-browser" 2>/dev/null || true
lsof -ti :$PROXY_PORT 2>/dev/null | xargs kill 2>/dev/null || true
sleep 1

# ---- Brave ----
if [ "$LOGIN_MODE" = true ]; then
  echo ""
  echo "Abriendo Brave para login..."
  echo "Iniciá sesión en claude.ai, después cerrá Brave."
  echo ""
  "$BRAVE" \
    --remote-debugging-port=$CDP_PORT \
    --user-data-dir="$CHROME_DATA" \
    --no-first-run \
    --disable-extensions \
    2>/dev/null
  echo ""
  echo "Login completado. Ahora ejecutá:"
  echo "  ./start.sh"
  echo ""
  exit 0
else
  echo "Iniciando Brave (background)..."
  "$BRAVE" \
    --remote-debugging-port=$CDP_PORT \
    --user-data-dir="$CHROME_DATA" \
    --no-first-run \
    --disable-extensions \
    --disable-backgrounding-occluded-windows \
    --disable-renderer-backgrounding \
    2>/dev/null &
  sleep 15

  if ! lsof -i :$CDP_PORT >/dev/null 2>&1; then
    echo "Error: CDP no disponible en puerto $CDP_PORT"
    exit 1
  fi
  echo "  Brave OK (CDP :$CDP_PORT)"
fi

# ---- Proxy ----
echo "Iniciando proxy..."
cd "$SCRIPT_DIR/proxy"
node server.js 2>&1 &
PROXY_PID=$!

for i in $(seq 1 30); do
  if curl -s "http://localhost:$PROXY_PORT/health" >/dev/null 2>&1; then
    break
  fi
  sleep 1
done

if ! kill -0 "$PROXY_PID" 2>/dev/null; then
  echo "Error: Proxy no arrancó — puede que no haya sesión."
  echo "  Ejecutá: ./start.sh --login"
  exit 1
fi
echo "  Proxy OK (PID $PROXY_PID)"

echo ""
echo "╔═══════════════════════════════════════╗"
echo "║         CLAWDMETER CORRIENDO          ║"
echo "╠═══════════════════════════════════════╣"
echo "║  Proxy:  http://localhost:$PROXY_PORT        ║"
echo "║  ESP32:  http://clawdmeter.local      ║"
echo "╠═══════════════════════════════════════╣"
echo "║  Chrome y Firefox libres              ║"
echo "║  Ctrl+C para detener todo             ║"
echo "╚═══════════════════════════════════════╝"
echo ""

wait
