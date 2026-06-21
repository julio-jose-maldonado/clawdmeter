// Boton touch TTP223:
//   - toque corto  -> rota a la siguiente pantalla (uso / reloj / clima)
//   - toque largo  -> gira la pantalla 180 (flip) y lo guarda en NVS
// Mas la vuelta automatica a la pantalla principal tras un tiempo sin tocar.

#define LONG_PRESS_MS  1000  // mantener apretado este tiempo = girar 180
#define DEBOUNCE_MS    50    // pulsacion minima para contar como toque corto

// Invierte la pantalla 180, persiste y redibuja al instante.
void toggleFlip() {
  config.flip_screen = !config.flip_screen;
  tft.setRotation(config.flip_screen ? 3 : 1);
  saveConfig();
  drawScreen();
  Serial.printf("Touch largo: flip=%d\n", config.flip_screen);
}

void handleTouchButton() {
  static bool lastState = false;
  static unsigned long pressStart = 0;
  static bool longHandled = false;  // evita que el toque largo dispare tambien el corto

  bool pressed = digitalRead(TOUCH_BTN_PIN) == HIGH;

  // Flanco de bajada: arranca la pulsacion
  if (pressed && !lastState) {
    pressStart = millis();
    longHandled = false;
  }

  // Mantenido: al cruzar el umbral, gira 180 una sola vez (sin soltar)
  if (pressed && !longHandled && millis() - pressStart >= LONG_PRESS_MS) {
    longHandled = true;
    lastTouchMillis = millis();
    toggleFlip();
  }

  // Flanco de subida: si fue corto (y no se trato como largo), cambia de pantalla
  if (!pressed && lastState && !longHandled && millis() - pressStart >= DEBOUNCE_MS) {
    lastTouchMillis = millis();
    currentScreen = (currentScreen + 1) % NUM_SCREENS;
    Serial.printf("Touch corto: pantalla %d\n", currentScreen);
    drawScreen();
  }

  lastState = pressed;

  // Volver a la pantalla principal tras N segundos sin tocar (0 = nunca)
  if (currentScreen != SCREEN_USAGE && config.home_timeout_sec > 0 &&
      millis() - lastTouchMillis >= (unsigned long)config.home_timeout_sec * 1000) {
    currentScreen = SCREEN_USAGE;
    Serial.println("Timeout: vuelta a pantalla principal");
    drawScreen();
  }
}
