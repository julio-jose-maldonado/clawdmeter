// Pantalla TENDENCIA (SCREEN_TREND): sparkline de las ultimas 5h + % actual +
// proyeccion. Los datos llegan en la struct global `trend` (la llena fetchTrend
// en network.ino). El dispatch esta en drawScreen() (display.ino); usa los
// helpers comunes (barColor, etc.) y el sprite global `spr`.

void drawTrendContent() {
  char buf[40];

  if (!trendValid || trend.count == 0) {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(COL_YELLOW, COL_BG);
    spr.drawString("Sin historico aun...", SCREEN_W / 2, SCREEN_H / 2, 2);
    return;
  }

  int top = CONTENT_Y + 4;

  // % actual grande (izquierda)
  spr.setTextColor(COL_AMBER, COL_BG);
  spr.setTextDatum(TL_DATUM);
  spr.drawString("5H AHORA", COL_LEFT_X, top, 2);

  spr.setTextColor(barColor(trend.current), COL_BG);
  spr.setTextDatum(TL_DATUM);
  snprintf(buf, sizeof(buf), "%d", trend.current);
  spr.drawString(buf, COL_LEFT_X, top + 16, 6);
  int nw = spr.textWidth(buf, 6);          // la fuente 6 no incluye '%': va aparte
  spr.drawString("%", COL_LEFT_X + nw + 4, top + 38, 4);

  // tendencia + proyeccion (derecha)
  const char* tl = strcmp(trend.trend, "up") == 0 ? "SUBE"
                 : strcmp(trend.trend, "down") == 0 ? "BAJA" : "ESTABLE";
  uint16_t tc = strcmp(trend.trend, "up") == 0 ? COL_RED
              : strcmp(trend.trend, "down") == 0 ? COL_GREEN : COL_CYAN;
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(tc, COL_BG);
  spr.drawString(tl, SCREEN_W - 8, top, 2);

  if (trend.etaMin > 0) {
    // ETA al limite como dato principal; color segun si lo tocas antes del reset
    int h = trend.etaMin / 60, m = trend.etaMin % 60;
    if (h > 0) snprintf(buf, sizeof(buf), "Limite en %dh%dm", h, m);
    else       snprintf(buf, sizeof(buf), "Limite en %dm", m);
    spr.setTextColor(trend.hitsLimit ? COL_RED : COL_GREEN, COL_BG);
  } else {
    snprintf(buf, sizeof(buf), "OK hasta reset");
    spr.setTextColor(COL_GREEN, COL_BG);
  }
  spr.setTextDatum(TR_DATUM);
  spr.drawString(buf, SCREEN_W - 8, top + 20, 2);

  spr.setTextColor(COL_AMBER, COL_BG);
  snprintf(buf, sizeof(buf), "pico %d%%", trend.peak);
  spr.drawString(buf, SCREEN_W - 8, top + 38, 2);

  // sparkline
  int gx = COL_LEFT_X;
  int gy = top + 66;
  int gw = SCREEN_W - 2 * COL_LEFT_X;
  int gh = (SCREEN_H - FOOTER_H) - gy - 4;
  spr.drawRect(gx, gy, gw, gh, COL_DARKGRAY);

  // linea de pico (tenue)
  int peakY = gy + gh - 1 - (int)((trend.peak / 100.0f) * (gh - 2));
  spr.drawFastHLine(gx + 1, peakY, gw - 2, COL_GRAY);

  // curva de uso
  if (trend.count == 1) {
    int y = gy + gh - 1 - (int)((trend.points[0] / 100.0f) * (gh - 2));
    spr.fillCircle(gx + gw / 2, y, 2, COL_CYAN);
  } else {
    int prevX = -1, prevY = -1;
    for (int i = 0; i < trend.count; i++) {
      int x = gx + 1 + (int)((float)i / (trend.count - 1) * (gw - 2));
      int y = gy + gh - 1 - (int)((trend.points[i] / 100.0f) * (gh - 2));
      if (prevX >= 0) spr.drawLine(prevX, prevY, x, y, COL_CYAN);
      prevX = x; prevY = y;
    }
  }
}
