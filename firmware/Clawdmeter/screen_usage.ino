// Pantalla USO (SCREEN_USAGE, default): las 3 metricas — 5h, 7 dias y extra usage.
// drawBar y drawRateSection son helpers propios de esta pantalla.

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

void drawUsageContent() {
  char buf[32];

  if (!dataValid) {
    spr.setTextDatum(MC_DATUM);
    if (lastError[0]) {
      spr.setTextColor(COL_RED, COL_BG);
      spr.drawString(lastError, SCREEN_W / 2, SCREEN_H / 2 - 8, 2);
      spr.setTextColor(COL_YELLOW, COL_BG);
      spr.drawString("Reintentando...", SCREEN_W / 2, SCREEN_H / 2 + 12, 1);
    } else {
      spr.setTextColor(COL_YELLOW, COL_BG);
      spr.drawString("Cargando...", SCREEN_W / 2, SCREEN_H / 2, 2);
    }
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
}
