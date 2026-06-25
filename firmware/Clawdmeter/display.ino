// UI comun a todas las pantallas: mensajes de arranque, header, footer y el
// dispatch drawScreen(). Cada pantalla vive en su propio screen_*.ino
// (screen_usage / screen_trend / screen_clock / screen_weather).

void showMessage(const char* line1, const char* line2 = nullptr, uint16_t color = COL_CYAN) {
  tft.fillScreen(COL_BG);
  tft.setTextColor(color, COL_BG);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(line1, 160, line2 ? 70 : 86, 2);
  if (line2) {
    tft.setTextColor(COL_WHITE, COL_BG);
    tft.drawString(line2, 160, 95, 2);
  }
}

// ---- HEADER / FOOTER (comunes a todas las pantallas) ----

void drawHeader() {
  char buf[40];

  spr.fillRect(0, 0, SCREEN_W, HEADER_H, COL_DARKGRAY);
  spr.setTextColor(COL_CYAN, COL_DARKGRAY);
  spr.setTextDatum(ML_DATUM);

  if (currentScreen == SCREEN_CLOCK) {
    spr.drawString("CLAWDMETER - FECHA Y HORA", 8, HEADER_H / 2, 2);
  } else if (currentScreen == SCREEN_WEATHER) {
    spr.drawString("CLAWDMETER - CLIMA", 8, HEADER_H / 2, 2);
  } else if (currentScreen == SCREEN_TREND) {
    spr.drawString("CLAWDMETER - TENDENCIA 5H", 8, HEADER_H / 2, 2);
  } else if (dataValid && usage.plan[0] && strcmp(usage.plan, "?") != 0) {
    const char* label = usage.plan;
    if (strncmp(label, "default_", 8) == 0) label += 8;
    if (strncmp(label, "claude_", 7) == 0)  label += 7;
    char clean[32];
    strlcpy(clean, label, sizeof(clean));
    for (int i = 0; clean[i]; i++) {
      if (clean[i] == '_') clean[i] = ' ';
    }
    if (clean[0] >= 'a' && clean[0] <= 'z') clean[0] -= 32;
    snprintf(buf, sizeof(buf), "CLAWDMETER - %s", clean);
    spr.drawString(buf, 8, HEADER_H / 2, 2);
  } else {
    spr.drawString("CLAWDMETER", 8, HEADER_H / 2, 2);
  }

  if (currentScreen != SCREEN_CLOCK) {
    spr.setTextColor(COL_CYAN, COL_DARKGRAY);
    spr.setTextDatum(MR_DATUM);
    struct tm tm;
    getLocalTime(&tm);
    snprintf(buf, sizeof(buf), "%02d:%02d", tm.tm_hour, tm.tm_min);
    spr.drawString(buf, SCREEN_W - 8, HEADER_H / 2, 2);
  }

  spr.drawFastHLine(0, HEADER_H, SCREEN_W, COL_AMBER);
}

void drawFooter() {
  char buf[32];
  int footerY = SCREEN_H - FOOTER_H;
  spr.drawFastHLine(0, footerY, SCREEN_W, COL_AMBER);
  spr.fillRect(0, footerY + 1, SCREEN_W, FOOTER_H - 1, COL_DARKGRAY);

  int rssi = WiFi.RSSI();
  spr.setTextColor(COL_GREEN, COL_DARKGRAY);
  spr.setTextDatum(ML_DATUM);
  snprintf(buf, sizeof(buf), "WiFi %ddBm", rssi);
  spr.drawString(buf, 8, footerY + FOOTER_H / 2, 1);

  uint16_t updColor = COL_WHITE;
  if (lastSuccessMillis > 0) {
    unsigned long elapsed = millis() - lastSuccessMillis;
    unsigned long cycle = (unsigned long)config.refresh_sec * 1000;
    if (elapsed > cycle * 3) updColor = COL_RED;
    else if (elapsed > cycle * 2) updColor = COL_YELLOW;
  }
  spr.setTextColor(updColor, COL_DARKGRAY);
  spr.setTextDatum(MC_DATUM);
  snprintf(buf, sizeof(buf), "Upd %s", lastSuccess);
  spr.drawString(buf, SCREEN_W / 2, footerY + FOOTER_H / 2, 1);

  spr.setTextColor(COL_GREEN, COL_DARKGRAY);
  spr.setTextDatum(MR_DATUM);
  spr.drawString(WiFi.localIP().toString().c_str(), SCREEN_W - 8, footerY + FOOTER_H / 2, 1);
}

// ---- DISPATCH ----

void drawScreen() {
  spr.fillSprite(COL_BG);
  drawHeader();

  switch (currentScreen) {
    case SCREEN_TREND:   drawTrendContent();   break;
    case SCREEN_CLOCK:   drawClockContent();   break;
    case SCREEN_WEATHER: drawWeatherContent(); break;
    default:             drawUsageContent();   break;
  }

  drawFooter();
  spr.pushSprite(0, 0);
}
