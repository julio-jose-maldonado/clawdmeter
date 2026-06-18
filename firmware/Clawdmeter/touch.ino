// Boton touch TTP223: rotacion de pantallas y vuelta automatica a la principal

void handleTouchButton() {
  static bool lastState = false;

  bool pressed = digitalRead(TOUCH_BTN_PIN) == HIGH;
  if (pressed && !lastState && millis() - lastTouchMillis > 300) {
    lastTouchMillis = millis();
    currentScreen = (currentScreen + 1) % NUM_SCREENS;
    Serial.printf("Touch: pantalla %d\n", currentScreen);
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
