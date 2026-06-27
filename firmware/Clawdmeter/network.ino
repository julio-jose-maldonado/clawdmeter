// Conectividad de bajo nivel: WiFi (con portal de config), NTP y helper de tiempo.
// Los pedidos HTTP al proxy estan en fetch.ino.

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
