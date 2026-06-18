# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Clawdmeter: a physical Claude usage monitor. An ESP32 with LCD display (plus an installable PWA) shows Claude rate-limit usage (5-hour, 7-day, extra usage) by reading from a local proxy that scrapes claude.ai's internal API through a real browser session. No API keys — auth lives in a dedicated Brave browser profile.

## Commands

```bash
./start.sh --login   # First time: opens Brave visibly to log into claude.ai
./start.sh           # Normal run: launches Brave (background) + proxy on :3456
```

- `start.sh` runs `pnpm install` in `proxy/` automatically if `node_modules` is missing. Use **pnpm**, not npm.
- Requires `.env` (copy from `.env.example`): `PROXY_PORT` (default 3456), `CDP_PORT` (default 9222).
- There are no tests or linters configured.
- To run the proxy alone (Brave must already be running with CDP): `cd proxy && node server.js`.
- Firmware is built/flashed from Arduino IDE (no CLI build). Board: ESP32S3 Dev Module, 16MB Flash, OPI PSRAM, USB CDC On Boot Enabled. Libraries: TFT_eSPI (needs `User_Setup.h` configured for the Waveshare ESP32-S3 LCD 1.47" B), ArduinoJson, WiFiManager (tzapu).

## Architecture

Data flow:

```
Brave (dedicated profile at ~/.clawdmeter-browser, CDP on :9222)
  → Playwright connectOverCDP keeps a claude.ai page open
  → page.evaluate(fetch) hits claude.ai/api/* using the browser's cookies
  → Express proxy on localhost:3456 caches and serves /api/usage
  → ESP32 and PWA each poll /api/usage every 60s
```

Why a browser: claude.ai blocks direct requests (Cloudflare + TLS fingerprinting). All claude.ai access must go through `page.evaluate()` inside a real browser page — never direct HTTP from Node. Brave is used specifically so it doesn't interfere with the user's main Chrome/Firefox.

### proxy/ (Node, CommonJS, Express 5)

- `server.js` — entry point and routes: `/api/usage`, `/health`, `/api/debug/{*path}` (passthrough to any claude.ai API path, useful for exploring the internal API).
- `lib/browser.js` — CDP connection, claude.ai navigation, Cloudflare-challenge wait, session detection (exits with login instructions if no session), and `fetchFromClaude(path)`.
- `lib/usage.js` — in-memory cache (60s TTL) of org info + usage data. Org UUID is fetched once from `/api/organizations`, then usage from `/api/organizations/{uuid}/usage`.
- `public/` — PWA dashboard (vanilla HTML/JS, manifest, service worker) served statically.

### firmware/Clawdmeter/ (Arduino, ESP32-S3)

Arduino IDE concatenates all `.ino` files in the folder into one compilation unit — functions defined in one file are callable from another without headers. All globals, structs (`Config`, `UsageData`), color/layout `#define`s, and `setup()/loop()` live in `Clawdmeter.ino`; the other files (`config`, `colors`, `display`, `network`, `webconfig`) only define functions against those globals.

- Config persists in NVS via `Preferences`; runtime configuration via a web UI served by the ESP32 at `http://clawdmeter.local` (mDNS).
- Display is a 320x172 landscape ST7789 driven through a full-screen `TFT_eSprite` (draw to sprite, push once — avoids flicker).
- WS2812 RGB LED shows a green→red gradient based on 5-hour usage.

## Conventions

- Code comments, console output, and docs are in Spanish.
- The claude.ai usage API shape: `five_hour` / `seven_day` / `extra_usage`, each with `utilization` (percent) and reset timestamps. The proxy response wraps it as `{ org: {name, plan}, usage, cached_at }` — the ESP32 (`network.ino`) and PWA both parse this shape, so changes to it must be coordinated across all three.
