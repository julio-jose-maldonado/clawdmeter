void loadConfig() {
  prefs.begin("clawdmeter", true);
  strlcpy(config.proxy_ip, prefs.getString("proxy_ip", "192.168.0.176").c_str(), sizeof(config.proxy_ip));
  config.proxy_port = prefs.getUShort("proxy_port", 3456);
  config.refresh_sec = prefs.getUShort("refresh_sec", 60);
  config.led_brightness = prefs.getUChar("led_bright", 100);
  config.lcd_brightness = prefs.getUChar("lcd_bright", 200);
  config.flip_screen = prefs.getBool("flip", false);
  strlcpy(config.timezone, prefs.getString("timezone", "ART3").c_str(), sizeof(config.timezone));
  strlcpy(config.admin_pass, prefs.getString("admin_pass", "clawdmeter").c_str(), sizeof(config.admin_pass));
  config.lat = prefs.getFloat("lat", 0.0f);
  config.lon = prefs.getFloat("lon", 0.0f);
  strlcpy(config.city, prefs.getString("city", "").c_str(), sizeof(config.city));
  config.home_timeout_sec = prefs.getUShort("home_timeout", 5);
  config.buzzer_pin = prefs.getUChar("buzzer_pin", BUZZER_PIN_DEFAULT);
  config.alerts_enabled = prefs.getBool("alerts_en", true);
  config.warn_threshold = prefs.getUChar("warn_thr", 80);
  config.crit_threshold = prefs.getUChar("crit_thr", 95);
  config.night_dim_enabled = prefs.getBool("night_en", false);
  config.night_start_hour = prefs.getUChar("night_start", 23);
  config.night_end_hour = prefs.getUChar("night_end", 7);
  config.night_brightness = prefs.getUChar("night_bright", 30);
  prefs.end();
}

void saveConfig() {
  prefs.begin("clawdmeter", false);
  prefs.putString("proxy_ip", config.proxy_ip);
  prefs.putUShort("proxy_port", config.proxy_port);
  prefs.putUShort("refresh_sec", config.refresh_sec);
  prefs.putUChar("led_bright", config.led_brightness);
  prefs.putUChar("lcd_bright", config.lcd_brightness);
  prefs.putBool("flip", config.flip_screen);
  prefs.putString("timezone", config.timezone);
  prefs.putString("admin_pass", config.admin_pass);
  prefs.putFloat("lat", config.lat);
  prefs.putFloat("lon", config.lon);
  prefs.putString("city", config.city);
  prefs.putUShort("home_timeout", config.home_timeout_sec);
  prefs.putUChar("buzzer_pin", config.buzzer_pin);
  prefs.putBool("alerts_en", config.alerts_enabled);
  prefs.putUChar("warn_thr", config.warn_threshold);
  prefs.putUChar("crit_thr", config.crit_threshold);
  prefs.putBool("night_en", config.night_dim_enabled);
  prefs.putUChar("night_start", config.night_start_hour);
  prefs.putUChar("night_end", config.night_end_hour);
  prefs.putUChar("night_bright", config.night_brightness);
  prefs.end();
}
