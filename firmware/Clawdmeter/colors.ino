void initBacklight() {
  ledcAttach(LCD_BL_PIN, 5000, 8);
  ledcWrite(LCD_BL_PIN, config.lcd_brightness);
}

void setBacklight(uint8_t level) {
  ledcWrite(LCD_BL_PIN, level);
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

void updateRgbLed(float pct) {
  uint8_t r, g, b;
  pctToRGB(pct, r, g, b);
  float scale = config.led_brightness / 255.0f;
  neopixelWrite(RGB_LED_PIN, (uint8_t)(g * scale), (uint8_t)(r * scale), 0);
}
