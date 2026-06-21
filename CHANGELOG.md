# Changelog

## [2.8.0] - 2026-06-21

### Added
- Atenuacion nocturna de la pantalla (`tickNightDim` / `applyBacklight` en `colors.ino`): en el rango horario configurado (hora local NTP) baja el brillo de la LCD a un nivel reducido, y lo restaura de dia
- Campos en la web config (panel Display): activar atenuacion, rango horario (de/a) y brillo nocturno; persistidos en NVS

### Changed
- El brillo de la LCD pasa a aplicarse via `applyBacklight()` (decide dia/noche), tambien al guardar en la web config

## [2.7.0] - 2026-06-21

### Added
- Comprobacion de inicio (`startupSelfTest` en `alerts.ino`): al bootear, barrido de los 3 LEDs externos con el tono propio de cada alerta (grave=5h, medio=7d, agudo=extra) para verificar tira + buzzer. Suena si hay buzzer configurado, sin importar si las alertas estan activadas

## [2.6.0] - 2026-06-21

### Added
- Toque largo (~1s) en el boton touch gira la pantalla 180° y lo guarda en NVS (`toggleFlip` en `touch.ino`); el toque corto sigue cambiando de pantalla

### Changed
- `handleTouchButton` distingue toque corto (cambia pantalla, al soltar) de toque largo (flip), con debounce de 50ms

## [2.4.1] - 2026-06-21

### Fixed
- Al guardar en la web config, la pantalla se redibuja al instante (`drawScreen()` en `handleSave`). Antes, invertir la pantalla 180° no se veia hasta el siguiente refresco (parecia no funcionar)

## [2.5.0] - 2026-06-21

### Added
- Alertas por umbral de uso (`alerts.ino`): beep en buzzer pasivo + parpadeo del LED de la metrica cuando el uso de 5h, 7 dias o extra cruza el umbral de aviso (def. 80%) o critico (def. 95%)
- Tono distinto por metrica (grave=5h, medio=7d, agudo=extra) para reconocerla de oido; severidad por cantidad de beeps (1=aviso, 2=critico)
- Buzzer pasivo (PWM/`tone`) con pin configurable desde la web UI (def. GPIO11); pin `0` = solo parpadeo, sin sonido
- Panel "Alertas" en la web config: on/off, umbrales de aviso/critico y GPIO del buzzer, con validacion de pin seguro (`buzzerPinValid`)
- Persistencia en NVS de `buzzer_pin`, `alerts_en`, `warn_thr`, `crit_thr`

### Changed
- La alerta suena una sola vez por cruce (no repite en cada refresh) y se rearma cuando el uso baja del umbral; el primer arranque fija la linea base sin sonar

## [2.4.0] - 2026-06-20

### Added
- Tira externa de 3 WS2812B (GPIO2, Adafruit NeoPixel): un LED por metrica con gradiente verde a rojo — 5h, 7 dias y extra usage (`updateUsageLeds` en `colors.ino`)
- LED indicador azul tenue cuando no hay plan de extra usage
- Efecto ambiental en el LED integrado (GPIO38): transicion fluida entre tonos aleatorios via HSV (`tickAmbientLed`)
- Diagrama de conexiones y tabla de pines en el README

### Changed
- El LED integrado pasa de mostrar el uso de 5h a ser decorativo (ambiental); el uso ahora se refleja en la tira externa
- `updateRgbLed(pct)` reemplazado por `updateUsageLeds()` en `setup`, `loop` y web config

## [2.3.0] - 2026-06-20

### Added
- Multipantalla: vista de uso + vista de reloj y clima, alternables con boton touch TTP223 (GPIO10)

## [2.2.0] - 2026-06-05

### Changed
- Proxy refactorizado en modulos `lib/browser.js` (Brave/CDP, fetch a claude.ai) y `lib/usage.js` (cache y refresh)
- `server.js` reducido a entry point con rutas Express
- Gestor de paquetes cambiado de npm a pnpm (`pnpm-lock.yaml`, `start.sh`, README)

### Removed
- `package-lock.json` (reemplazado por pnpm)

## [2.1.0] - 2026-06-05

### Added
- Indicador de ultima actualizacion en footer del firmware (`Upd HH:MM:SS`) con color segun antiguedad (blanco/amarillo/rojo)
- Nombre del plan de Claude en header del firmware y pagina de web config
- Dashboard web como PWA instalable (`proxy/public/`: manifest, service worker, icono)
- Endpoint de debug `GET /api/debug/{*path}` en el proxy
- Reintentos HTTP (hasta 3) en fetches del firmware al proxy
- Flags de Chrome en `start.sh` para evitar throttling en background

### Changed
- `clawdmeter_app.html` movido a `proxy/public/index.html` y servido en la raiz del proxy
- Dashboard web usa `/api/usage` relativo, gradientes de color suaves, mascot y glow segun uso 5h
- Reconexion WiFi con espera acotada de 10s en vez de delay fijo de 5s
- Refresh fallido ya no borra datos de uso validos en el firmware
- Proxy sirve archivos estaticos desde `public/`

### Removed
- Script de consola para DevTools (`clawdmeter_console.js`) — el proxy con Playwright es el unico flujo

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
