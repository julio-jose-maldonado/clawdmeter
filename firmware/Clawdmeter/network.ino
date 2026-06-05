void connectWiFi() {
  showMessage("Conectando WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.begin();

  int wait = 0;
  while (WiFi.status() != WL_CONNECTED && wait < 20) {
    delay(500);
    wait++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    showMessage("WiFi OK", WiFi.localIP().toString().c_str(), COL_GREEN);
    delay(1000);
    return;
  }

  WiFi.disconnect(false);
  delay(500);
  WiFi.mode(WIFI_AP_STA);
  delay(500);
  showMessage("Abriendo AP...", "Clawdmeter-Setup", COL_YELLOW);

  wm.setConfigPortalTimeout(180);
  if (!wm.startConfigPortal("Clawdmeter-Setup")) {
    showMessage("WiFi FAIL", "Reiniciando...", COL_RED);
    delay(3000);
    ESP.restart();
  }

  showMessage("WiFi OK", WiFi.localIP().toString().c_str(), COL_GREEN);
  delay(1000);
}

void syncTime() {
  configTzTime(config.timezone, "pool.ntp.org", "time.google.com");
  struct tm tm;
  int retries = 0;
  while (!getLocalTime(&tm) && retries < 10) {
    delay(500);
    retries++;
  }
  Serial.printf("NTP sync: %04d-%02d-%02d %02d:%02d:%02d\n",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec);
}

void formatTimeRemaining(const char* isoStr, char* out, size_t len) {
  if (!isoStr || strlen(isoStr) < 10) {
    snprintf(out, len, "--");
    return;
  }

  int yr, mo, dy, hr, mn, sc;
  sscanf(isoStr, "%d-%d-%dT%d:%d:%d", &yr, &mo, &dy, &hr, &mn, &sc);

  struct tm expiry = {0};
  expiry.tm_year = yr - 1900;
  expiry.tm_mon  = mo - 1;
  expiry.tm_mday = dy;
  expiry.tm_hour = hr;
  expiry.tm_min  = mn;
  expiry.tm_sec  = sc;

  time_t expiryEpoch = mktime(&expiry);

  struct tm now_tm;
  time_t now_epoch;
  time(&now_epoch);
  gmtime_r(&now_epoch, &now_tm);
  time_t nowUtc = mktime(&now_tm);

  long diff = (long)(expiryEpoch - nowUtc);
  if (diff <= 0) {
    snprintf(out, len, "Reset!");
    return;
  }

  int hours = diff / 3600;
  int mins  = (diff % 3600) / 60;

  if (hours > 0) {
    snprintf(out, len, "%dh %dm", hours, mins);
  } else {
    snprintf(out, len, "%dm", mins);
  }
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

  lastSuccessMillis = millis();
  struct tm now;
  if (getLocalTime(&now)) {
    snprintf(lastSuccess, sizeof(lastSuccess), "%02d:%02d:%02d", now.tm_hour, now.tm_min, now.tm_sec);
  }

  Serial.printf("Data OK — 5h: %.0f%% | 7d: %.0f%% | extra: %.1f%%\n",
                usage.five_hour_pct, usage.seven_day_pct, usage.extra_pct);

  return true;
}
