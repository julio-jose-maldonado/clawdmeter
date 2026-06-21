#!/bin/bash
#
# Instala (o desinstala con --uninstall) un LaunchAgent de macOS que arranca
# el proxy de Clawdmeter (start.sh) automaticamente al iniciar sesion.
#
# Uso:
#   ./scripts/install-autostart.sh              # instala y carga
#   ./scripts/install-autostart.sh --uninstall  # quita el auto-arranque
#
# Requisito: haber hecho el login una vez (./start.sh --login).
#

set -e

LABEL="com.clawdmeter.proxy"
PLIST="$HOME/Library/LaunchAgents/$LABEL.plist"
REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
LOG_DIR="$HOME/Library/Logs"

uninstall() {
  launchctl bootout "gui/$(id -u)/$LABEL" 2>/dev/null \
    || launchctl unload "$PLIST" 2>/dev/null || true
  rm -f "$PLIST"
  echo "Auto-arranque desinstalado ($LABEL)."
}

if [ "$1" = "--uninstall" ]; then
  uninstall
  exit 0
fi

# ---- Chequeos ----
if [ ! -f "$REPO_DIR/start.sh" ]; then
  echo "Error: no encuentro start.sh en $REPO_DIR"; exit 1
fi
if [ ! -f "$REPO_DIR/.env" ]; then
  echo "Error: falta .env (copialo de .env.example y completalo)"; exit 1
fi

NODE_BIN="$(command -v node || true)"
PNPM_BIN="$(command -v pnpm || true)"
if [ -z "$NODE_BIN" ] || [ -z "$PNPM_BIN" ]; then
  echo "Error: node y/o pnpm no estan en el PATH actual; instalalos antes."; exit 1
fi

# Los LaunchAgents arrancan con un PATH minimo: le inyectamos los dirs de
# node/pnpm (homebrew, nvm, etc.) ademas de los estandar.
BIN_PATH="$(dirname "$NODE_BIN"):$(dirname "$PNPM_BIN"):/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin"

mkdir -p "$HOME/Library/LaunchAgents" "$LOG_DIR"

# ---- Generar el plist con las rutas reales ----
cat > "$PLIST" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key>
  <string>$LABEL</string>
  <key>ProgramArguments</key>
  <array>
    <string>/bin/bash</string>
    <string>$REPO_DIR/start.sh</string>
  </array>
  <key>WorkingDirectory</key>
  <string>$REPO_DIR</string>
  <key>EnvironmentVariables</key>
  <dict>
    <key>PATH</key>
    <string>$BIN_PATH</string>
  </dict>
  <key>RunAtLoad</key>
  <true/>
  <key>KeepAlive</key>
  <true/>
  <key>ThrottleInterval</key>
  <integer>60</integer>
  <key>StandardOutPath</key>
  <string>$LOG_DIR/clawdmeter.log</string>
  <key>StandardErrorPath</key>
  <string>$LOG_DIR/clawdmeter.log</string>
</dict>
</plist>
EOF

# ---- (Re)cargar ----
launchctl bootout "gui/$(id -u)/$LABEL" 2>/dev/null || true
launchctl bootstrap "gui/$(id -u)" "$PLIST" 2>/dev/null || launchctl load "$PLIST"

echo "Auto-arranque instalado:"
echo "  plist: $PLIST"
echo "  logs:  $LOG_DIR/clawdmeter.log"
echo ""
echo "Arranca solo al iniciar sesion. Para ver el log en vivo:"
echo "  tail -f \"$LOG_DIR/clawdmeter.log\""
echo "Para desinstalar:"
echo "  ./scripts/install-autostart.sh --uninstall"
