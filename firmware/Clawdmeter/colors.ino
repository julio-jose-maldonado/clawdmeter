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

// Tira externa de 3 WS2812B: 5h y 7d muestran la PROYECCION (no el uso crudo),
// el extra muestra la SALUD del sistema. Todo con fade suave por hue (ver tickExtLeds).

// Tono objetivo de un LED de proyeccion (mismo criterio que la PWA):
// rojo = tocas el limite antes del reset; ambar = subiendo pero resetea a tiempo;
// verde = estable/bajando o sin proyeccion.
float projHue(bool hits, bool rising) {
  if (hits)   return HUE_LED_RED;
  if (rising) return HUE_LED_AMBER;
  return HUE_LED_GREEN;
}

// Estado de salud: rojo si no llegan datos / Brave o sesion caidos; ambar si los
// datos estan viejos; verde si todo fresco.
int getHealthState() {
  if (WiFi.status() != WL_CONNECTED) return HEALTH_DOWN;
  if (!dataValid || lastSuccessMillis == 0) return HEALTH_DOWN;
  unsigned long cycle = (unsigned long)config.refresh_sec * 1000;
  if (cycle == 0) cycle = 60000;
  unsigned long elapsed = millis() - lastSuccessMillis;
  if (elapsed > cycle * 3 || !usage.proxy_ok) return HEALTH_DOWN;
  if (usage.data_stale || elapsed > cycle * 2) return HEALTH_STALE;
  return HEALTH_OK;
}

float healthHue() {
  switch (getHealthState()) {
    case HEALTH_DOWN:  return HUE_LED_RED;
    case HEALTH_STALE: return HUE_LED_AMBER;
    default:           return HUE_LED_GREEN;
  }
}

// Fija los tonos objetivo de 5h/7d segun la proyeccion. Llamar tras cada refresh.
// El LED extra (salud) lo recalcula tickExtLeds en vivo.
void updateUsageLeds() {
  extLedTargetHue[LED_5H] = projHue(usage.five_hour_hits, usage.five_hour_rising);
  extLedTargetHue[LED_7D] = projHue(usage.seven_day_hits, usage.seven_day_rising);
}

// Fade no bloqueante hacia el tono objetivo, interpolando por hue (verde->ambar->
// rojo sin pasar por colores feos). Llamado desde loop() en cada vuelta.
void tickExtLeds() {
  static unsigned long last = 0;
  if (millis() - last < 30) return;
  last = millis();

  extLedTargetHue[LED_EXTRA] = healthHue();  // la salud se evalua en vivo

  float scale = config.led_brightness / 255.0f;
  for (int i = 0; i < EXT_LED_COUNT; i++) {
    float diff = extLedTargetHue[i] - extLedHue[i];
    if (diff > 180)  diff -= 360;            // camino mas corto en el circulo
    if (diff < -180) diff += 360;
    if (fabs(diff) < 1.0f) {
      extLedHue[i] = extLedTargetHue[i];
    } else {
      extLedHue[i] += diff * 0.06f;          // easing suave (~1s)
      if (extLedHue[i] < 0)    extLedHue[i] += 360;
      if (extLedHue[i] >= 360) extLedHue[i] -= 360;
    }
    uint8_t r, g, b;
    hsvToRgb(extLedHue[i], 1.0f, 1.0f, r, g, b);
    extLeds.setPixelColor(i, extLeds.Color(r * scale, g * scale, b * scale));
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
