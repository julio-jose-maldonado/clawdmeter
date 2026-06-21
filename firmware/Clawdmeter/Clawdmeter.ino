/*
 * Clawdmeter - Monitor de uso de Claude para ESP32
 * Waveshare ESP32-S3 LCD 1.47" (version B)
 *
 * Requiere:
 *   - TFT_eSPI (con User_Setup.h configurado para esta placa)
 *   - ArduinoJson
 *   - WiFiManager (by tzapu)
 *
 * Configuracion de placa en Arduino IDE:
 *   Board: ESP32S3 Dev Module
 *   Flash Size: 16MB
 *   PSRAM: OPI PSRAM
 *   USB CDC On Boot: Enabled
 */

#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>

// ---- HARDWARE ----
const int LCD_BL_PIN = 46;
const int RGB_LED_PIN = 38;    // LED WS2812 integrado: efecto ambiental
const int TOUCH_BTN_PIN = 10;  // TTP223: VCC->3V3, GND->GND, IO->GPIO10 (activo alto)

// Tira externa de 3 WS2812B encadenados: estados de uso (5h / 7 dias / extra).
// Cablear DIN del primer modulo a EXT_LED_PIN, alimentar a 5V y GND comun.
#define EXT_LED_PIN   2
#define EXT_LED_COUNT 3
#define LED_5H    0
#define LED_7D    1
#define LED_EXTRA 2

// Buzzer pasivo (PWM/tone): alertas sonoras al cruzar umbrales de uso.
// El pin es configurable desde la web UI; este es solo el valor por defecto.
#define BUZZER_PIN_DEFAULT 11

// ---- COLORES (RGB565) ----
#define COL_BG       0x0000
#define COL_CYAN     0x07FF
#define COL_GREEN    0x07E0
#define COL_YELLOW   0xFFE0
#define COL_RED      0xF800
#define COL_ORANGE   0xFD20
#define COL_WHITE    0xFFFF
#define COL_GRAY     0x4208
#define COL_DARKGRAY 0x2104
#define COL_AMBER    0xFC80

// ---- LAYOUT ----
#define SCREEN_W 320
#define SCREEN_H 172
#define HEADER_H 20
#define FOOTER_H 16
#define CONTENT_Y (HEADER_H + 2)
#define CONTENT_H (SCREEN_H - HEADER_H - FOOTER_H - 4)
#define COL_LEFT_X 6
#define COL_LEFT_W 175
#define DIVIDER_X 185
#define COL_RIGHT_X 192
#define COL_RIGHT_W 122

// ---- CONFIG ----
struct Config {
  char proxy_ip[64];
  uint16_t proxy_port;
  uint16_t refresh_sec;
  uint8_t led_brightness;
  uint8_t lcd_brightness;
  bool flip_screen;
  char timezone[48];
  char admin_pass[32];
  float lat;
  float lon;
  char city[40];
  uint16_t home_timeout_sec;
  uint8_t buzzer_pin;       // 0 = buzzer desactivado
  bool    alerts_enabled;
  uint8_t warn_threshold;   // % de uso para alerta de aviso
  uint8_t crit_threshold;   // % de uso para alerta critica
};

struct UsageData {
  float five_hour_pct;
  char  five_hour_reset[16];
  float seven_day_pct;
  char  seven_day_reset[16];
  float extra_pct;
  float extra_used;
  float extra_limit;
  char  org_name[32];
  char  plan[32];
};

struct WeatherInfo {
  float temp;
  float feels;
  int   humidity;
  float wind;
  int   code;
  bool  is_day;
};

// ---- PANTALLAS ----
enum { SCREEN_USAGE = 0, SCREEN_CLOCK, SCREEN_WEATHER, NUM_SCREENS };

// ---- GLOBALS ----
Config config;
Preferences prefs;
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);
Adafruit_NeoPixel extLeds(EXT_LED_COUNT, EXT_LED_PIN, NEO_GRB + NEO_KHZ800);
WebServer webServer(80);
WiFiManager wm;

unsigned long lastRefresh = 0;
unsigned long lastSuccessMillis = 0;
bool dataValid = false;
char lastError[48] = "";
char lastSuccess[9] = "--:--";
UsageData usage;

int currentScreen = SCREEN_USAGE;
unsigned long lastTouchMillis = 0;
WeatherInfo weather;
bool weatherValid = false;
unsigned long lastWeatherFetch = 0;
const unsigned long WEATHER_TTL_MS = 15UL * 60UL * 1000UL;

// ---- SETUP ----
void setup() {
  Serial.begin(115200);
  delay(500);

  tft.init();
  loadConfig();
  pinMode(TOUCH_BTN_PIN, INPUT);
  tft.setRotation(config.flip_screen ? 3 : 1);
  tft.fillScreen(COL_BG);
  initBacklight();

  randomSeed(esp_random());
  extLeds.begin();
  extLeds.clear();
  extLeds.show();

  spr.createSprite(320, 172);
  spr.setSwapBytes(true);

  tft.setTextColor(COL_CYAN, COL_BG);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("CLAWDMETER", 160, 70, 4);
  tft.setTextColor(COL_GRAY, COL_BG);
  tft.drawString("v2.0", 160, 100, 2);
  delay(1500);

  connectWiFi();
  syncTime();
  setupWebServer();

  dataValid = fetchUsage();
  if (dataValid) {
    updateUsageLeds();
    initAlertBaseline();  // fija el nivel actual sin sonar en el arranque
  }
  drawScreen();

  lastRefresh = millis();

  Serial.printf("Config: proxy=%s:%d refresh=%ds led=%d lcd=%d\n",
                config.proxy_ip, config.proxy_port, config.refresh_sec,
                config.led_brightness, config.lcd_brightness);
}

// ---- LOOP ----
void loop() {
  webServer.handleClient();
  handleTouchButton();

  if (millis() - lastRefresh >= (unsigned long)config.refresh_sec * 1000) {
    bool ok = fetchUsage();
    if (ok) {
      dataValid = true;
      updateUsageLeds();
      checkAlerts();
    }
    drawScreen();
    lastRefresh = millis();
  }

  refreshWeatherIfDue();
  tickClockRedraw();
  tickAmbientLed();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost, reconnecting...");
    WiFi.reconnect();
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
      delay(100);
    }
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi reconnect failed, will retry next loop");
    }
  }

  delay(10);
}
