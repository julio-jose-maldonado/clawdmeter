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

// ---- PANTALLA 0: USO DE CLAUDE ----

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

// ---- PANTALLA 1: FECHA Y HORA ----

void drawClockContent() {
  char buf[48];

  time_t now = time(nullptr);
  struct tm tm;
  localtime_r(&now, &tm);

  if (tm.tm_year + 1900 < 2020) {
    spr.setTextColor(COL_YELLOW, COL_BG);
    spr.setTextDatum(MC_DATUM);
    spr.drawString("Sincronizando hora...", SCREEN_W / 2, SCREEN_H / 2, 2);
    return;
  }

  // Hora grande HH:MM (font 7, estilo 7 segmentos) + segundos
  snprintf(buf, sizeof(buf), "%02d:%02d", tm.tm_hour, tm.tm_min);
  spr.setTextColor(COL_CYAN, COL_BG);
  spr.setTextDatum(MC_DATUM);
  spr.drawString(buf, SCREEN_W / 2 - 20, CONTENT_Y + 42, 7);

  snprintf(buf, sizeof(buf), "%02d", tm.tm_sec);
  spr.setTextColor(COL_AMBER, COL_BG);
  spr.setTextDatum(ML_DATUM);
  spr.drawString(buf, SCREEN_W / 2 + 78, CONTENT_Y + 54, 4);

  // Fecha en espanol
  const char* DOW[] = {"Domingo", "Lunes", "Martes", "Miercoles", "Jueves", "Viernes", "Sabado"};
  const char* MON[] = {"Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio",
                       "Julio", "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre"};
  snprintf(buf, sizeof(buf), "%s %d de %s %d",
           DOW[tm.tm_wday], tm.tm_mday, MON[tm.tm_mon], tm.tm_year + 1900);
  spr.setTextColor(COL_WHITE, COL_BG);
  spr.setTextDatum(MC_DATUM);
  spr.drawString(buf, SCREEN_W / 2, CONTENT_Y + 102, 2);
}

// ---- PANTALLA 2: CLIMA ----

void drawWeatherContent() {
  char buf[32];

  if (config.lat == 0.0f && config.lon == 0.0f) {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(COL_YELLOW, COL_BG);
    spr.drawString("Configura lat/lon en", SCREEN_W / 2, SCREEN_H / 2 - 10, 2);
    spr.setTextColor(COL_CYAN, COL_BG);
    spr.drawString("http://clawdmeter.local", SCREEN_W / 2, SCREEN_H / 2 + 12, 2);
    return;
  }

  if (!weatherValid) {
    spr.setTextColor(COL_YELLOW, COL_BG);
    spr.setTextDatum(MC_DATUM);
    spr.drawString("Cargando clima...", SCREEN_W / 2, SCREEN_H / 2, 2);
    return;
  }

  // Izquierda: temperatura grande
  int x = COL_LEFT_X + 8;
  int y = CONTENT_Y + 16;

  snprintf(buf, sizeof(buf), "%.1f", weather.temp);
  spr.setTextColor(tempColor(weather.temp), COL_BG);
  spr.setTextDatum(TL_DATUM);
  spr.drawString(buf, x, y, 6);

  int tw = spr.textWidth(buf, 6);
  spr.drawCircle(x + tw + 10, y + 5, 4, COL_WHITE);  // simbolo de grados
  spr.setTextColor(COL_WHITE, COL_BG);
  spr.drawString("C", x + tw + 18, y + 2, 4);

  y += 56;
  spr.setTextColor(COL_AMBER, COL_BG);
  spr.drawString(weatherDesc(weather.code), x, y, 2);

  y += 22;
  spr.setTextColor(COL_WHITE, COL_BG);
  snprintf(buf, sizeof(buf), "Sensacion: %.1f C", weather.feels);
  spr.drawString(buf, x, y, 2);

  if (config.city[0]) {
    y += 20;
    spr.setTextColor(COL_CYAN, COL_BG);
    spr.drawString(config.city, x, y, 2);
  }

  // Divider
  spr.drawFastVLine(DIVIDER_X, CONTENT_Y, CONTENT_H, COL_DARKGRAY);

  // Derecha: humedad y viento
  int cx = COL_RIGHT_X + COL_RIGHT_W / 2;
  int ry = CONTENT_Y + 12;

  spr.setTextColor(COL_AMBER, COL_BG);
  spr.setTextDatum(TC_DATUM);
  spr.drawString("HUMEDAD", cx, ry, 2);
  ry += 20;

  spr.setTextColor(COL_WHITE, COL_BG);
  snprintf(buf, sizeof(buf), "%d%%", weather.humidity);
  spr.drawString(buf, cx, ry, 4);
  ry += 36;

  spr.setTextColor(COL_AMBER, COL_BG);
  spr.drawString("VIENTO", cx, ry, 2);
  ry += 20;

  spr.setTextColor(COL_WHITE, COL_BG);
  snprintf(buf, sizeof(buf), "%.0f km/h", weather.wind);
  spr.drawString(buf, cx, ry, 4);
}

// Redibuja la pantalla de reloj cuando cambia el segundo
void tickClockRedraw() {
  if (currentScreen != SCREEN_CLOCK) return;

  static int lastSec = -1;
  time_t now = time(nullptr);
  struct tm tm;
  localtime_r(&now, &tm);
  if (tm.tm_sec != lastSec) {
    lastSec = tm.tm_sec;
    drawScreen();
  }
}

// ---- DISPATCH ----

void drawScreen() {
  spr.fillSprite(COL_BG);
  drawHeader();

  switch (currentScreen) {
    case SCREEN_CLOCK:   drawClockContent();   break;
    case SCREEN_WEATHER: drawWeatherContent(); break;
    default:             drawUsageContent();   break;
  }

  drawFooter();
  spr.pushSprite(0, 0);
}
