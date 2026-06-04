# Changelog

## [2.0.0] - 2026-06-03

### Added
- Firmware ESP32 con layout landscape (320x172) en dos columnas
- WiFiManager: portal AP "Clawdmeter-Setup" para configurar WiFi
- Web config en `http://clawdmeter.local` (proxy IP, brillo, timezone, flip screen)
- LED RGB con gradiente verde a rojo segun uso de 5h
- Persistencia de config en NVS (sobrevive reinicios)
- mDNS: accesible como `clawdmeter.local`
- Dashboard web (`clawdmeter_app.html`) como alternativa al ESP32
- Mensajes de error descriptivos en pantalla (proxy no responde, WiFi desconectado, etc.)
- Fallback a portal AP despues de 3 intentos fallidos de WiFi

### Changed
- Proxy usa perfil persistente de Brave (`~/.clawdmeter-browser`) en vez de `/tmp/`
- Autenticacion por cookies del browser — sin session keys en archivos
- Login manual una sola vez con `./start.sh --login`
- Firmware dividido en modulos: config, colors, display, network, webconfig
- Wait de CDP aumentado a 15s para redes lentas

### Removed
- Dependencia de `CLAUDE_SESSION_KEY` en `.env`
- Scripts Python no utilizados (proxy.py, clawdmeter_proto.py, clawdmeter_app.py)
- Virtual environment de Python

## [1.0.0] - 2026-05-31

### Added
- Proxy Node.js con Playwright + Brave via CDP
- Consulta a claude.ai API interna (organizations, usage)
- Bypass de Cloudflare usando browser real
- Datos: 5h utilization, 7d utilization, extra usage
- Script `start.sh` para arrancar todo
- Script de consola para DevTools (`clawdmeter_console.js`)
