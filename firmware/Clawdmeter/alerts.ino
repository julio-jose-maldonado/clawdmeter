// Alertas sonoras + parpadeo cuando el uso cruza umbrales (5h / 7 dias / extra).
// Buzzer pasivo en config.buzzer_pin (PWM via tone()). Una alerta por cruce:
// no repite en cada refresh y se rearma sola cuando el uso baja del umbral.

// Tono propio por metrica (identidad sonora): grave=5h, medio=7d, agudo=extra.
// La severidad va por cantidad de beeps (aviso=1, critico=2).
#define FREQ_5H    1500
#define FREQ_7D    2000
#define FREQ_EXTRA 2600

// Nivel anterior por metrica (0=normal, 1=aviso, 2=critico) para detectar subidas.
static uint8_t lastLevel5h = 0;
static uint8_t lastLevel7d = 0;
static uint8_t lastLevelExtra = 0;

// Valida que el pin sea seguro para el buzzer en esta placa (ESP32-S3, OPI PSRAM).
// 0 = desactivado. Rechaza strapping/flash/PSRAM/USB/Serial y los pines ya usados.
bool buzzerPinValid(int p) {
  if (p == 0) return true;                 // 0 = buzzer desactivado
  if (p < 1 || p > 48) return false;       // fuera de rango
  if (p >= 22 && p <= 25) return false;    // no existen en el S3
  const int reserved[] = {2, 3, 10, 19, 20, 26, 27, 28, 29, 30, 31, 32,
                          33, 34, 35, 36, 37, 38, 43, 44, 45, 46};
  for (unsigned i = 0; i < sizeof(reserved) / sizeof(reserved[0]); i++) {
    if (p == reserved[i]) return false;
  }
  return true;
}

// Nivel de alerta segun el porcentaje y los umbrales configurados.
uint8_t alertLevel(float pct) {
  if (pct < 0) return 0;                       // metrica sin datos / no aplica
  if (pct >= config.crit_threshold) return 2;  // critico
  if (pct >= config.warn_threshold) return 1;  // aviso
  return 0;
}

// Beep corto en el buzzer pasivo (si hay pin valido configurado).
void beep(uint16_t freq, uint16_t durMs) {
  if (config.buzzer_pin == 0) return;
  tone(config.buzzer_pin, freq, durMs);
  delay(durMs);
  noTone(config.buzzer_pin);
}

// Parpadeo blanco del LED de la metrica + beep(s): 1 pulso=aviso, 2=critico.
// Cada metrica usa su propia frecuencia (freq) para reconocerla de oido.
void fireAlert(uint8_t level, uint8_t ledIdx, uint16_t freq) {
  uint32_t prev = extLeds.getPixelColor(ledIdx);
  uint8_t pulses = (level == 2) ? 2 : 1;
  for (uint8_t i = 0; i < pulses; i++) {
    extLeds.setPixelColor(ledIdx, extLeds.Color(255, 255, 255));
    extLeds.show();
    beep(freq, 140);
    extLeds.setPixelColor(ledIdx, 0);
    extLeds.show();
    delay(90);
  }
  extLeds.setPixelColor(ledIdx, prev);  // restaura el color de uso
  extLeds.show();
}

// Comprobacion de inicio: barrido de los 3 LEDs, cada uno con el tono de su
// alerta (grave=5h, medio=7d, agudo=extra). Verifica tira + buzzer en cada boot.
// Suena si hay buzzer configurado (independiente de alerts_enabled).
void startupSelfTest() {
  const uint16_t freqs[EXT_LED_COUNT] = {FREQ_5H, FREQ_7D, FREQ_EXTRA};
  for (uint8_t i = 0; i < EXT_LED_COUNT; i++) {
    extLeds.setPixelColor(i, extLeds.Color(255, 255, 255));
    extLeds.show();
    if (config.buzzer_pin) {
      tone(config.buzzer_pin, freqs[i], 150);
      delay(150);
      noTone(config.buzzer_pin);
    } else {
      delay(150);
    }
    extLeds.setPixelColor(i, 0);
    extLeds.show();
    delay(80);
  }
}

// Fija la linea base sin sonar (uso ya presente en el primer arranque).
void initAlertBaseline() {
  lastLevel5h = alertLevel(usage.five_hour_pct);
  lastLevel7d = alertLevel(usage.seven_day_pct);
  lastLevelExtra = alertLevel(usage.extra_pct);
}

// Llamar tras cada refresh: dispara solo cuando el nivel SUBE; rearma al bajar.
void checkAlerts() {
  uint8_t n5 = alertLevel(usage.five_hour_pct);
  uint8_t n7 = alertLevel(usage.seven_day_pct);
  uint8_t ne = alertLevel(usage.extra_pct);
  if (config.alerts_enabled) {
    if (n5 > lastLevel5h)    fireAlert(n5, LED_5H,    FREQ_5H);
    if (n7 > lastLevel7d)    fireAlert(n7, LED_7D,    FREQ_7D);
    if (ne > lastLevelExtra) fireAlert(ne, LED_EXTRA, FREQ_EXTRA);
  }
  lastLevel5h = n5;
  lastLevel7d = n7;
  lastLevelExtra = ne;
}
