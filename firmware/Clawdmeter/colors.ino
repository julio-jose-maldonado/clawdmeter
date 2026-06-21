void initBacklight() {
  ledcAttach(LCD_BL_PIN, 5000, 8);
  ledcWrite(LCD_BL_PIN, config.lcd_brightness);
}

void setBacklight(uint8_t level) {
  ledcWrite(LCD_BL_PIN, level);
}

// Devuelve true si la hora local cae en el rango nocturno configurado.
bool isNightNow() {
  if (!config.night_dim_enabled) return false;
  time_t now = time(nullptr);
  if (now < 1700000000) return false;  // hora aun no sincronizada (NTP)
  struct tm tm;
  localtime_r(&now, &tm);
  int h = tm.tm_hour;
  int s = config.night_start_hour, e = config.night_end_hour;
  if (s == e) return false;            // rango nulo
  if (s < e)  return (h >= s && h < e);
  return (h >= s || h < e);            // cruza medianoche
}

// Aplica el brillo segun horario: normal de dia, atenuado de noche.
// Es idempotente (se puede llamar seguido sin problema).
void applyBacklight() {
  setBacklight(isNightNow() ? config.night_brightness : config.lcd_brightness);
}

// Revisa cada 10s si corresponde cambiar de brillo dia/noche.
void tickNightDim() {
  if (!config.night_dim_enabled) return;
  static unsigned long last = 0;
  if (millis() - last < 10000) return;
  last = millis();
  applyBacklight();
}

void pctToRGB(float pct, uint8_t &r, uint8_t &g, uint8_t &b) {
  pct = constrain(pct, 0, 100);
  if (pct <= 50) {
    r = (uint8_t)(pct / 50.0f * 255);
    g = 255;
  } else {
    r = 255;
    g = (uint8_t)((100 - pct) / 50.0f * 255);
  }
  b = 0;
}

uint16_t barColor(float pct) {
  uint8_t r, g, b;
  pctToRGB(pct, r, g, b);
  return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

uint16_t tempColor(float t) {
  if (t < 10)  return COL_CYAN;
  if (t < 25)  return COL_GREEN;
  if (t < 32)  return COL_AMBER;
  return COL_RED;
}

// Tira externa de 3 WS2812B: un LED por metrica (5h / 7 dias / extra),
// con el mismo gradiente verde->rojo que las barras del display.
void updateUsageLeds() {
  float scale = config.led_brightness / 255.0f;
  uint8_t r, g, b;

  pctToRGB(usage.five_hour_pct, r, g, b);
  extLeds.setPixelColor(LED_5H, extLeds.Color(r * scale, g * scale, b * scale));

  pctToRGB(usage.seven_day_pct, r, g, b);
  extLeds.setPixelColor(LED_7D, extLeds.Color(r * scale, g * scale, b * scale));

  if (usage.extra_pct >= 0) {
    pctToRGB(usage.extra_pct, r, g, b);
    extLeds.setPixelColor(LED_EXTRA, extLeds.Color(r * scale, g * scale, b * scale));
  } else {
    // Sin plan de extra usage: azul tenue como indicador de "no aplica".
    extLeds.setPixelColor(LED_EXTRA, extLeds.Color(0, 0, (uint8_t)(60 * scale)));
  }

  extLeds.show();
}

// Conversion HSV->RGB (h en grados 0..360, s y v en 0..1) para el LED ambiental.
void hsvToRgb(float h, float s, float v, uint8_t &r, uint8_t &g, uint8_t &b) {
  float c = v * s;
  float x = c * (1 - fabs(fmodf(h / 60.0f, 2) - 1));
  float m = v - c;
  float rf, gf, bf;
  if      (h < 60)  { rf = c; gf = x; bf = 0; }
  else if (h < 120) { rf = x; gf = c; bf = 0; }
  else if (h < 180) { rf = 0; gf = c; bf = x; }
  else if (h < 240) { rf = 0; gf = x; bf = c; }
  else if (h < 300) { rf = x; gf = 0; bf = c; }
  else              { rf = c; gf = 0; bf = x; }
  r = (uint8_t)((rf + m) * 255);
  g = (uint8_t)((gf + m) * 255);
  b = (uint8_t)((bf + m) * 255);
}

// LED integrado: deriva suavemente hacia tonos aleatorios (cambio fluido, sin saltos).
void tickAmbientLed() {
  static unsigned long last = 0;
  static float hue = 0;       // tono actual 0..360
  static float target = -1;   // tono objetivo aleatorio
  if (millis() - last < 30) return;
  last = millis();

  if (target < 0) target = random(0, 360);

  float diff = target - hue;            // camino mas corto en el circulo de tono
  if (diff > 180)  diff -= 360;
  if (diff < -180) diff += 360;

  if (fabs(diff) < 1.0f) {
    target = random(0, 360);            // al llegar, nuevo objetivo aleatorio
  } else {
    hue += diff * 0.02f;                // easing suave hacia el objetivo
    if (hue < 0)    hue += 360;
    if (hue >= 360) hue -= 360;
  }

  uint8_t r, g, b;
  hsvToRgb(hue, 1.0f, 1.0f, r, g, b);
  float scale = config.led_brightness / 255.0f;
  // El LED integrado tiene R/G invertidos respecto a neopixelWrite.
  neopixelWrite(RGB_LED_PIN, (uint8_t)(g * scale), (uint8_t)(r * scale), (uint8_t)(b * scale));
}
