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

void drawBar(int x, int y, int w, int h, float pct, uint16_t color) {
  spr.drawRect(x, y, w, h, COL_GRAY);
  int filled = (int)((pct / 100.0f) * (w - 2));
  if (filled > 0) {
    spr.fillRect(x + 1, y + 1, filled, h - 2, color);
  }
}

void drawRateSection(int x, int y, int w, const char* label, float pct, const char* resetStr) {
  char buf[16];

  spr.setTextColor(COL_AMBER, COL_BG);
  spr.setTextDatum(TL_DATUM);
  spr.drawString(label, x, y, 2);

  spr.setTextColor(COL_WHITE, COL_BG);
  spr.setTextDatum(TR_DATUM);
  spr.drawString(resetStr, x + w, y, 2);
  y += 18;

  spr.setTextColor(barColor(pct), COL_BG);
  spr.setTextDatum(TL_DATUM);
  snprintf(buf, sizeof(buf), "%.0f%%", pct);
  spr.drawString(buf, x, y, 4);

  drawBar(x + 56, y + 4, w - 56, 12, pct, barColor(pct));
}

void drawScreen() {
  spr.fillSprite(COL_BG);

  char buf[32];

  // Header
  spr.fillRect(0, 0, SCREEN_W, HEADER_H, COL_DARKGRAY);
  spr.setTextColor(COL_CYAN, COL_DARKGRAY);
  spr.setTextDatum(ML_DATUM);
  if (dataValid && usage.plan[0] && strcmp(usage.plan, "?") != 0) {
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

  spr.setTextColor(COL_CYAN, COL_DARKGRAY);
  spr.setTextDatum(MR_DATUM);
  struct tm tm;
  getLocalTime(&tm);
  snprintf(buf, sizeof(buf), "%02d:%02d", tm.tm_hour, tm.tm_min);
  spr.drawString(buf, SCREEN_W - 8, HEADER_H / 2, 2);

  spr.drawFastHLine(0, HEADER_H, SCREEN_W, COL_AMBER);

  if (!dataValid) {
    spr.setTextDatum(MC_DATUM);
    if (lastError[0]) {
      spr.setTextColor(COL_RED, COL_BG);
      spr.drawString(lastError, SCREEN_W / 2, SCREEN_H / 2 - 8, 2);
      spr.setTextColor(COL_GRAY, COL_BG);
      spr.drawString("Reintentando...", SCREEN_W / 2, SCREEN_H / 2 + 12, 1);
    } else {
      spr.setTextColor(COL_YELLOW, COL_BG);
      spr.drawString("Cargando...", SCREEN_W / 2, SCREEN_H / 2, 2);
    }
    spr.pushSprite(0, 0);
    return;
  }

  // Left column
  int y = CONTENT_Y + 20;
  drawRateSection(COL_LEFT_X, y, COL_LEFT_W, "5 HORAS", usage.five_hour_pct, usage.five_hour_reset);

  y += 52;
  spr.drawFastHLine(COL_LEFT_X, y, COL_LEFT_W, COL_DARKGRAY);
  y += 8;

  drawRateSection(COL_LEFT_X, y, COL_LEFT_W, "7 DIAS", usage.seven_day_pct, usage.seven_day_reset);

  // Vertical divider
  spr.drawFastVLine(DIVIDER_X, CONTENT_Y, CONTENT_H, COL_DARKGRAY);

  // Right column: EXTRA USAGE
  if (usage.extra_pct >= 0) {
    int ry = CONTENT_Y + 10;

    spr.setTextColor(COL_AMBER, COL_BG);
    spr.setTextDatum(TC_DATUM);
    spr.drawString("EXTRA USAGE", COL_RIGHT_X + COL_RIGHT_W / 2, ry, 2);
    ry += 22;

    spr.setTextColor(barColor(usage.extra_pct), COL_BG);
    spr.setTextDatum(TC_DATUM);
    snprintf(buf, sizeof(buf), "$%.2f", usage.extra_used / 100.0f);
    spr.drawString(buf, COL_RIGHT_X + COL_RIGHT_W / 2, ry, 4);
    ry += 28;

    spr.setTextColor(COL_WHITE, COL_BG);
    spr.setTextDatum(TC_DATUM);
    snprintf(buf, sizeof(buf), "/ $%.0f", usage.extra_limit / 100.0f);
    spr.drawString(buf, COL_RIGHT_X + COL_RIGHT_W / 2, ry, 2);
    ry += 22;

    drawBar(COL_RIGHT_X, ry, COL_RIGHT_W, 12, usage.extra_pct, barColor(usage.extra_pct));
    ry += 16;

    spr.setTextColor(COL_WHITE, COL_BG);
    spr.setTextDatum(TC_DATUM);
    snprintf(buf, sizeof(buf), "%.1f%%", usage.extra_pct);
    spr.drawString(buf, COL_RIGHT_X + COL_RIGHT_W / 2, ry, 1);
  }

  // Footer
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

  spr.pushSprite(0, 0);
}
