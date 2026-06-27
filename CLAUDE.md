# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Clawdmeter: a physical Claude usage monitor. An ESP32 with LCD display (plus an installable PWA) shows Claude rate-limit usage (5-hour, 7-day, extra usage) by reading from a local proxy that scrapes claude.ai's internal API through a real browser session. No API keys — auth lives in a dedicated Brave browser profile.

## Commands

```bash
./start.sh --login                          # First time: opens Brave visibly to log into claude.ai
./start.sh                                  # Normal run: launches Brave (background) + proxy on :3456
cd proxy && node server.js                  # Run the proxy alone (Brave must already be up with CDP)
./scripts/install-autostart.sh              # (macOS) auto-start the proxy at login (LaunchAgent)
./scripts/install-autostart.sh --uninstall  # Remove the auto-start LaunchAgent
```

- `start.sh` runs `pnpm install` in `proxy/` automatically if `node_modules` is missing. Use **pnpm**, not npm.
- The proxy uses **better-sqlite3** (native module) for the usage-history DB. pnpm gates native build scripts; `proxy/pnpm-workspace.yaml` (`allowBuilds: better-sqlite3`) pre-approves it so `pnpm install` builds it without prompts. On a brand-new Node major with no prebuilt binary it compiles from source (needs Xcode CLT + Python).
- Requires `.env` (copy from `.env.example`): `PROXY_PORT` (default 3456), `CDP_PORT` (default 9222). Behavior settings (brightness, thresholds, weather, etc.) are NOT in `.env` — they live in the config DB (see `lib/config.js` / `/api/config`).
- There are no tests or linters configured.
- The auto-start `scripts/install-autostart.sh` installs a LaunchAgent (`com.clawdmeter.proxy`) that runs `start.sh` at login and restarts it if it dies (logs to `~/Library/Logs/clawdmeter.log`); `--uninstall` removes it. The project must live **outside** `~/Documents`, `~/Desktop`, `~/Downloads` (macOS TCC) or the agent fails with "Operation not permitted" — that's why the repo lives at `~/clawdmeter`.
- Firmware is built/flashed from Arduino IDE (no CLI build). Board: ESP32S3 Dev Module, 16MB Flash, OPI PSRAM, USB CDC On Boot Enabled. Libraries: TFT_eSPI (needs `User_Setup.h` configured for the Waveshare ESP32-S3 LCD 1.47" B), ArduinoJson, WiFiManager (tzapu), Adafruit NeoPixel.

## Architecture

Data flow:

```
Brave (dedicated profile at ~/.clawdmeter-browser, CDP on :9222)
  → Playwright connectOverCDP keeps a claude.ai page open
  → page.evaluate(fetch) hits claude.ai/api/* using the browser's cookies
  → Express proxy on localhost:3456 caches /api/usage and persists a usage time-series to SQLite
  → ESP32 and PWA each poll /api/usage every 60s; the proxy also self-samples every 60s so history keeps growing even with no clients
```

Why a browser: claude.ai blocks direct requests (Cloudflare + TLS fingerprinting). All claude.ai access must go through `page.evaluate()` inside a real browser page — never direct HTTP from Node. Brave is used specifically so it doesn't interfere with the user's main Chrome/Firefox.

### proxy/ (Node, CommonJS, Express 5)

- `server.js` — entry point and routes: `/api/usage`, `/api/history` (PWA: downsampled series + derived stats), `/api/history/sparkline` (ESP32: compact sparkline + projection), `/api/config` (GET reads / POST updates the centralized config), `/health`. No open CORS and no debug passthrough on purpose: the PWA is same-origin and the ESP32 isn't a browser, so nothing here is readable cross-origin. To explore the raw claude.ai API ad-hoc, connect to the running Brave over CDP and `page.evaluate(fetch(...))` rather than exposing an endpoint.
- `lib/browser.js` — CDP connection, claude.ai navigation, Cloudflare-challenge wait, session detection, and `fetchFromClaude(path)`. **Resilient**: never `process.exit`s — on no session / Brave closed it keeps the proxy up, reconnects on demand (with backoff), and exposes health via `getStatus()` (`{connected, sessionValid, ok, lastError}`).
- `lib/usage.js` — in-memory cache (60s TTL) of org info + usage data. Org UUID is fetched once from `/api/organizations`, then usage from `/api/organizations/{uuid}/usage`. Records each successful refresh into the history and embeds proxy health + a `stale` flag in `getData()`.
- `lib/history.js` — SQLite (`better-sqlite3`) usage time-series at `proxy/data/history.db` (14-day ring buffer, gitignored). Derives series, stats (peak/avg/red-zone) and **projection** per metric: linear-regression ETA to 100% crossed with the window reset (`hitsLimit`). Projection windows are per-metric (5h fast, 7d/extra slow) and require a minimum data span before projecting, so a freshly-started proxy shows "OK" instead of noise.
- `lib/config.js` — SQLite (`proxy/data/config.db`, single-row JSON) = source of truth for all **behavior** settings (brightness, refresh, thresholds, weather, night-dim, buzzer, timezone). NOT the bootstrap: WiFi + proxy IP/port stay local on the ESP32. Inputs validated/coerced per a field `SCHEMA`. Edited from the dedicated **`public/config.html`** page (separate from the dashboard, linked from it) via `POST /api/config`; the ESP32 fetches `GET /api/config` on boot and caches it to NVS. The page's own login lives in the same config: `config_user` + `config_pass` (bcrypt-hashed, never returned by the API, default `admin`/`clawdmeter`, changeable from the page's Seguridad panel). `/api/config` is Basic-Auth gated by those (no `WWW-Authenticate` header so the browser doesn't show its native dialog — the page has its own login form).
- `lib/backup.js` — online SQLite backups (consistent while running) of `history.db` + `config.db` to `~/Library/Application Support/clawdmeter/backups/` (**outside** the project, so they survive a `data/` wipe). Runs on proxy startup + daily, keeps the last 14 per DB. CLI: `node lib/backup.js [now|list|restore <YYYY-MM-DD>]` (run `restore` with the proxy stopped). NOTE: never point tests at the real `data/` dir — use a temp dir.
- `public/` — PWA dashboard (vanilla HTML/JS, manifest, service worker) served statically. The dedicated config page is `public/config.html`. Shows the live device plus a history view (canvas chart + stat cards, responsive row/column).

### firmware/Clawdmeter/ (Arduino, ESP32-S3)

Arduino IDE concatenates all `.ino` files in the folder into one compilation unit — functions defined in one file are callable from another without headers. All globals, structs (`Config`, `UsageData`), color/layout `#define`s, and `setup()/loop()` live in `Clawdmeter.ino`; the other files (`config`, `colors`, `alerts`, `display`, `screen_usage`, `screen_trend`, `screen_clock`, `screen_weather`, `network`, `fetch`, `touch`, `weather`, `webconfig`) only define functions against those globals. `network.ino` is low-level connectivity (WiFi/NTP); the HTTP GETs to the proxy (`fetchConfig`/`fetchTrend`/`fetchUsage`) live in `fetch.ino`. `display.ino` holds the common UI (boot messages, header, footer, `drawScreen()` dispatch); each screen's drawing lives in its own `screen_*.ino`.

- Config persists in NVS via `Preferences`. The ESP32 web UI at `http://clawdmeter.local` (mDNS) is **bootstrap-only** (WiFi + proxy IP/port + proxy credentials + this page's admin pass). All **behavior** settings (brightness, alerts, weather, timezone, night-dim, etc.) come from the proxy: `fetchConfig()` (`network.ino`) does a Basic-Auth `GET /api/config` on boot and every 5 min, validates/clamps each field, caches to NVS (`saveConfig`) and applies via `applyConfig()` (rotation + backlight + timezone). NVS holds sane defaults so the device works offline / before first contact with the proxy.
- Display is a 320x172 landscape ST7789 driven through a full-screen `TFT_eSprite` (draw to sprite, push once — avoids flicker). Screens cycle on touch: usage → trend (5h sparkline + projection) → clock → weather (`SCREEN_*` enum, `drawScreen()`). Note: TFT_eSPI font 6 has digits only (no `%`/letters), so big-number screens draw units in a smaller font separately.
- LEDs (`colors.ino`): the onboard WS2812 (GPIO38) runs a smooth random ambient color cycle (`tickAmbientLed`); an external chain of 3 WS2812B (Adafruit NeoPixel on `EXT_LED_PIN`, default GPIO2) shows **state, not raw usage**: LED0=5h and LED1=7d show the **projection** (green = safe / won't hit the limit before reset, amber = rising but resets in time, red = will hit the limit before reset — same green/amber/red criterion as the PWA cards), LED2=extra shows **system health** (green=fresh, amber=stale, red=down/no session). `updateUsageLeds()` sets target hues from the projection embedded in `/api/usage`; `tickExtLeds()` eases each LED toward its target by HSV hue (smooth green→amber→red, no abrupt jumps). The external strip needs 5V power and common ground.
- Alerts (`alerts.ino`): when 5-hour, 7-day or extra usage crosses a warn (default 80%) or critical (default 95%) threshold, a passive buzzer beeps (`tone()` on `config.buzzer_pin`, default GPIO11, configurable in the web UI; `0` = silent) and the metric's LED flashes. Each metric has its own tone (`FREQ_5H/7D/EXTRA`) and severity is the beep count (1=warn, 2=crit). `checkAlerts()` fires only on a rising level (once per crossing, rearms when usage drops); `initAlertBaseline()` sets the baseline at boot without sounding. `buzzerPinValid()` blocks reserved/strapping/flash pins.

## Conventions

- Code comments, console output, and docs are in Spanish.
- The claude.ai usage API shape: `five_hour` / `seven_day` / `extra_usage`, each with `utilization` (percent) and reset timestamps. claude.ai exposes only the **percent** for the 5h/7d windows — no token or dollar counts (those fields exist but are `null`); only `extra_usage` carries real `used_credits`/`monthly_limit` in USD. The proxy `/api/usage` response wraps it as `{ org: {name, plan}, usage, cached_at, proxy: {connected, sessionValid, ok, lastError}, stale, projection: {five_hour, seven_day, extra_usage} }` — the ESP32 (`network.ino`) and PWA both parse this shape, so changes to it must be coordinated across all three.
