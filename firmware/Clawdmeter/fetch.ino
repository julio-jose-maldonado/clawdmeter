// Pedidos HTTP al proxy: config, tendencia y uso. Toda la conectividad de bajo
// nivel (WiFi/NTP) vive en network.ino; aca van solo los GET al proxy.

// Baja la config del proxy (GET /api/config con Basic Auth), la aplica y la
// cachea en NVS. Si el proxy esta caido o da 401, se queda con lo de la NVS.
bool fetchConfig() {
  if (WiFi.status() != WL_CONNECTED) return false;

  char url[160];
  snprintf(url, sizeof(url), "http://%s:%d/api/config", config.proxy_ip, config.proxy_port);

  HTTPClient http;
  http.begin(url);
  http.setAuthorization(config.proxy_user, config.proxy_pass);  // Basic Auth
  http.setTimeout(10000);
  int code = http.GET();
  if (code != 200) {
    http.end();
    if (code == 401) Serial.println("Config: 401 — revisar proxy_user/proxy_pass");
    else             Serial.printf("Config: HTTP %d\n", code);
    return false;
  }

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    Serial.println("Config: JSON invalido");
    return false;
  }

  // Cada campo: valor del proxy o, si falta, el actual (clamp segun rangos).
  config.refresh_sec       = constrain((int)(doc["refresh_sec"]      | config.refresh_sec), 10, 3600);
  config.led_brightness    = constrain((int)(doc["led_brightness"]   | config.led_brightness), 0, 255);
  config.lcd_brightness    = constrain((int)(doc["lcd_brightness"]   | config.lcd_brightness), 0, 255);
  config.flip_screen       = doc["flip_screen"] | config.flip_screen;
  strlcpy(config.timezone, doc["timezone"] | config.timezone, sizeof(config.timezone));
  config.lat               = doc["lat"] | config.lat;
  config.lon               = doc["lon"] | config.lon;
  strlcpy(config.city,     doc["city"] | config.city, sizeof(config.city));
  config.home_timeout_sec  = constrain((int)(doc["home_timeout_sec"] | config.home_timeout_sec), 0, 3600);
  int bp = doc["buzzer_pin"] | config.buzzer_pin;
  if (buzzerPinValid(bp)) config.buzzer_pin = bp;
  config.alerts_enabled    = doc["alerts_enabled"] | config.alerts_enabled;
  config.warn_threshold    = constrain((int)(doc["warn_threshold"]   | config.warn_threshold), 1, 100);
  config.crit_threshold    = constrain((int)(doc["crit_threshold"]   | config.crit_threshold), 1, 100);
  config.night_dim_enabled = doc["night_dim_enabled"] | config.night_dim_enabled;
  config.night_start_hour  = constrain((int)(doc["night_start_hour"] | config.night_start_hour), 0, 23);
  config.night_end_hour    = constrain((int)(doc["night_end_hour"]   | config.night_end_hour), 0, 23);
  config.night_brightness  = constrain((int)(doc["night_brightness"] | config.night_brightness), 0, 255);

  saveConfig();   // cachea en NVS (queda para el proximo arranque)
  applyConfig();  // aplica rotacion / brillo / timezone en caliente

  Serial.println("Config bajada del proxy y aplicada");
  return true;
}

// Sparkline + proyeccion de la ventana de 5h desde el proxy (/api/history).
// Independiente de fetchUsage: si falla, se conserva el ultimo dato bueno.
bool fetchTrend() {
  if (WiFi.status() != WL_CONNECTED) return false;

  char url[176];
  snprintf(url, sizeof(url),
           "http://%s:%d/api/history/sparkline?metric=five_hour&points=%d&window=5h",
           config.proxy_ip, config.proxy_port, TREND_MAX_PTS);

  HTTPClient http;
  http.begin(url);
  http.setTimeout(10000);
  int httpCode = http.GET();
  if (httpCode != 200) {
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, payload)) return false;

  JsonArray pts = doc["points"];
  uint8_t n = 0;
  for (JsonVariant v : pts) {
    if (n >= TREND_MAX_PTS) break;
    trend.points[n++] = (uint8_t)constrain((int)(v | 0), 0, 100);
  }
  trend.count   = n;
  trend.peak    = (uint8_t)constrain((int)(doc["peak"] | 0), 0, 100);
  trend.current = (uint8_t)constrain((int)(doc["current"] | 0), 0, 100);
  trend.etaMin   = (int16_t)(doc["etaMin"] | -1);
  trend.resetMin = (int16_t)(doc["resetMin"] | -1);
  trend.hitsLimit = doc["hitsLimit"] | false;
  strlcpy(trend.trend, doc["trend"] | "flat", sizeof(trend.trend));

  Serial.printf("Trend OK — %d pts | actual %d%% | pico %d%% | eta %d min | %s\n",
                trend.count, trend.current, trend.peak, trend.etaMin, trend.trend);
  return true;
}

bool fetchUsage() {
  if (WiFi.status() != WL_CONNECTED) {
    snprintf(lastError, sizeof(lastError), "WiFi desconectado");
    return false;
  }

  char url[128];
  snprintf(url, sizeof(url), "http://%s:%d/api/usage", config.proxy_ip, config.proxy_port);

  const int MAX_RETRIES = 3;
  String payload;

  for (int attempt = 0; attempt < MAX_RETRIES; attempt++) {
    if (attempt > 0) {
      Serial.printf("Retry %d/%d...\n", attempt + 1, MAX_RETRIES);
      delay(1000);
    }

    HTTPClient http;
    http.begin(url);
    http.setTimeout(10000);
    int httpCode = http.GET();

    if (httpCode == 200) {
      payload = http.getString();
      http.end();
      lastError[0] = '\0';
      goto parse;
    }

    if (httpCode <= 0) {
      snprintf(lastError, sizeof(lastError), "Proxy no responde (%s:%d)", config.proxy_ip, config.proxy_port);
      Serial.printf("HTTP error: %d (attempt %d)\n", httpCode, attempt + 1);
    } else {
      snprintf(lastError, sizeof(lastError), "Proxy error HTTP %d", httpCode);
      Serial.printf("HTTP %d (attempt %d)\n", httpCode, attempt + 1);
    }
    http.end();
  }
  return false;

parse:

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    snprintf(lastError, sizeof(lastError), "JSON invalido");
    Serial.printf("JSON error: %s\n", err.c_str());
    return false;
  }

  strlcpy(usage.org_name, doc["org"]["name"] | "?", sizeof(usage.org_name));
  strlcpy(usage.plan, doc["org"]["plan"] | "?", sizeof(usage.plan));

  usage.five_hour_pct = doc["usage"]["five_hour"]["utilization"] | 0.0f;
  const char* fhReset = doc["usage"]["five_hour"]["resets_at"] | "";
  formatTimeRemaining(fhReset, usage.five_hour_reset, sizeof(usage.five_hour_reset));

  usage.seven_day_pct = doc["usage"]["seven_day"]["utilization"] | 0.0f;
  const char* sdReset = doc["usage"]["seven_day"]["resets_at"] | "";
  formatTimeRemaining(sdReset, usage.seven_day_reset, sizeof(usage.seven_day_reset));

  JsonVariant extra = doc["usage"]["extra_usage"];
  if (!extra.isNull() && extra["is_enabled"] == true) {
    usage.extra_pct   = extra["utilization"] | 0.0f;
    usage.extra_used  = extra["used_credits"] | 0.0f;
    usage.extra_limit = extra["monthly_limit"] | 0.0f;
  } else {
    usage.extra_pct = -1;
  }

  // Proyeccion embebida (para los LEDs) + estado del proxy (para el LED de salud)
  JsonVariant proj = doc["projection"];
  usage.five_hour_hits   = proj["five_hour"]["hitsLimit"] | false;
  usage.five_hour_rising = strcmp(proj["five_hour"]["trend"] | "flat", "up") == 0;
  usage.seven_day_hits   = proj["seven_day"]["hitsLimit"] | false;
  usage.seven_day_rising = strcmp(proj["seven_day"]["trend"] | "flat", "up") == 0;
  usage.proxy_ok   = doc["proxy"]["ok"] | true;
  usage.data_stale = doc["stale"] | false;

  lastSuccessMillis = millis();
  struct tm now;
  if (getLocalTime(&now)) {
    snprintf(lastSuccess, sizeof(lastSuccess), "%02d:%02d:%02d", now.tm_hour, now.tm_min, now.tm_sec);
  }

  Serial.printf("Data OK — 5h: %.0f%% | 7d: %.0f%% | extra: %.1f%%\n",
                usage.five_hour_pct, usage.seven_day_pct, usage.extra_pct);

  return true;
}
