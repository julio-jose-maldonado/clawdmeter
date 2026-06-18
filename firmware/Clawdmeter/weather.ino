// Clima via Open-Meteo (sin API key): fetch, cache y descripciones WMO

bool fetchWeather() {
  char url[224];
  snprintf(url, sizeof(url),
           "https://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,relative_humidity_2m,apparent_temperature,"
           "weather_code,wind_speed_10m,is_day",
           config.lat, config.lon);

  HTTPClient http;
  http.begin(url);
  http.setTimeout(10000);
  int httpCode = http.GET();

  if (httpCode != 200) {
    Serial.printf("Clima HTTP error: %d\n", httpCode);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    Serial.println("Clima: JSON invalido");
    return false;
  }

  JsonVariant cur = doc["current"];
  if (cur.isNull()) return false;

  weather.temp     = cur["temperature_2m"] | 0.0f;
  weather.feels    = cur["apparent_temperature"] | 0.0f;
  weather.humidity = cur["relative_humidity_2m"] | 0;
  weather.wind     = cur["wind_speed_10m"] | 0.0f;
  weather.code     = cur["weather_code"] | -1;
  weather.is_day   = (cur["is_day"] | 1) == 1;

  Serial.printf("Clima OK — %.1fC | %d%% hum | %.1f km/h | code %d\n",
                weather.temp, weather.humidity, weather.wind, weather.code);
  return true;
}

// Refresco cada WEATHER_TTL_MS, solo con lat/lon configurados
void refreshWeatherIfDue() {
  if (config.lat == 0.0f && config.lon == 0.0f) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (lastWeatherFetch != 0 && millis() - lastWeatherFetch < WEATHER_TTL_MS) return;

  weatherValid = fetchWeather();
  lastWeatherFetch = millis();
  if (currentScreen == SCREEN_WEATHER) drawScreen();
}

const char* weatherDesc(int code) {
  if (code == 0)   return "Despejado";
  if (code <= 2)   return "Parcialmente nublado";
  if (code == 3)   return "Nublado";
  if (code <= 48)  return "Niebla";
  if (code <= 57)  return "Llovizna";
  if (code <= 67)  return "Lluvia";
  if (code <= 77)  return "Nieve";
  if (code <= 82)  return "Chubascos";
  if (code <= 86)  return "Nieve";
  if (code >= 95)  return "Tormenta";
  return "?";
}
