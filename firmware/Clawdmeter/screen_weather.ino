// Pantalla CLIMA (SCREEN_WEATHER): temperatura grande, descripcion, humedad y
// viento. Los datos los trae fetchWeather() (weather.ino) en la struct `weather`.

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
