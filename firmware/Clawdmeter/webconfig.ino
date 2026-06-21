bool checkAuth() {
  if (!webServer.authenticate("admin", config.admin_pass)) {
    webServer.requestAuthentication();
    return false;
  }
  return true;
}

void handleRoot() {
  if (!checkAuth()) return;
  String html;
  html.reserve(8192);

  html += "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>Clawdmeter</title><style>";
  html += ":root{--bg:#0f172a;--card:#1e293b;--accent:#00ddff;--text:#f1f5f9;--muted:#94a3b8;--border:#334155}";
  html += "*{box-sizing:border-box}body{font-family:'Segoe UI',sans-serif;background:var(--bg);color:var(--text);margin:0;padding:20px}";
  html += ".c{max-width:480px;margin:0 auto}";
  html += "h1{color:var(--accent);text-align:center;font-size:1.8rem;letter-spacing:3px;margin-bottom:5px}";
  html += ".sub{text-align:center;color:var(--muted);font-size:.85rem;margin-bottom:20px}";
  html += ".panel{background:var(--card);padding:20px;border-radius:12px;border:1px solid var(--border);margin-bottom:16px}";
  html += ".panel h2{color:var(--accent);font-size:1rem;margin:0 0 15px}";
  html += "label{display:block;margin-top:12px;font-weight:600;font-size:.9rem}";
  html += "input[type=text],input[type=number]{width:100%;padding:10px;margin-top:4px;border:1px solid var(--border);border-radius:6px;background:#0f172a;color:var(--text);font-size:15px}";
  html += "input:focus{border-color:var(--accent);outline:none;box-shadow:0 0 0 3px rgba(0,221,255,.2)}";
  html += ".range-row{display:flex;align-items:center;gap:10px;margin-top:6px}";
  html += ".range-row input[type=range]{flex:1;accent-color:var(--accent)}";
  html += ".range-val{min-width:36px;text-align:right;font-family:monospace;color:var(--accent)}";
  html += "button{background:var(--accent);color:#0f172a;padding:14px;border:none;border-radius:8px;cursor:pointer;width:100%;font-size:1.1rem;font-weight:700;margin-top:10px}";
  html += "button:hover{opacity:.9}";
  html += ".btn-danger{background:#ef4444;color:#fff;font-size:.9rem;padding:10px;margin-top:20px}";
  html += ".status{text-align:center;padding:12px;background:rgba(0,221,255,.08);border:1px solid var(--accent);border-radius:8px;margin-bottom:16px;font-family:monospace;font-size:.85rem}";
  html += ".help{font-size:.8rem;color:var(--muted);margin-top:4px}";
  html += "</style></head><body><div class='c'>";

  html += "<h1>CLAWDMETER</h1>";
  const char* planRaw = usage.plan;
  if (strncmp(planRaw, "default_", 8) == 0) planRaw += 8;
  if (strncmp(planRaw, "claude_", 7) == 0)  planRaw += 7;
  String planClean = String(planRaw);
  planClean.replace('_', ' ');
  if (planClean.length() > 0 && planClean[0] >= 'a' && planClean[0] <= 'z') planClean[0] -= 32;
  if (dataValid && usage.plan[0] && strcmp(usage.plan, "?") != 0) {
    html += "<div class='sub'>v2.0 &mdash; " + planClean + "</div>";
  } else {
    html += "<div class='sub'>v2.0 &mdash; Claude Usage Monitor</div>";
  }

  // Status
  html += "<div class='status'>";
  html += "IP: " + WiFi.localIP().toString() + " &bull; ";
  html += "WiFi: " + String(WiFi.RSSI()) + "dBm &bull; ";
  html += dataValid ? "<span style='color:#00cc44'>Online</span>" : "<span style='color:#ff3333'>Offline</span>";
  html += "<br>Upd: " + String(lastSuccess);
  html += "</div>";

  // Proxy settings
  html += "<form method='GET' action='/save'>";
  html += "<div class='panel'><h2>Proxy</h2>";
  html += "<label>IP del servidor proxy</label>";
  html += "<input type='text' name='proxy_ip' value='" + String(config.proxy_ip) + "'>";
  html += "<div class='help'>IP de la Mac/PC donde corre el proxy Node.js</div>";
  html += "<label>Puerto</label>";
  html += "<input type='number' name='proxy_port' value='" + String(config.proxy_port) + "'>";
  html += "</div>";

  // Display settings
  html += "<div class='panel'><h2>Display</h2>";
  html += "<label>Intervalo de refresco (segundos)</label>";
  html += "<input type='number' name='refresh_sec' value='" + String(config.refresh_sec) + "' min='10' max='600'>";

  html += "<label>Brillo LCD</label>";
  html += "<div class='range-row'><input type='range' name='lcd_bright' min='10' max='255' value='" + String(config.lcd_brightness) + "' oninput='this.nextElementSibling.textContent=this.value'>";
  html += "<span class='range-val'>" + String(config.lcd_brightness) + "</span></div>";

  html += "<label>Brillo LED RGB</label>";
  html += "<div class='range-row'><input type='range' name='led_bright' min='0' max='255' value='" + String(config.led_brightness) + "' oninput='this.nextElementSibling.textContent=this.value'>";
  html += "<span class='range-val'>" + String(config.led_brightness) + "</span></div>";
  html += "<div class='help'>0 = LED apagado</div>";

  html += "<label>Volver a pantalla principal (segundos)</label>";
  html += "<input type='number' name='home_timeout' value='" + String(config.home_timeout_sec) + "' min='0' max='600'>";
  html += "<div class='help'>Tras este tiempo sin tocar el boton, vuelve a la pantalla de uso. 0 = nunca</div>";

  html += "<label style='margin-top:16px;display:flex;align-items:center;gap:8px;cursor:pointer'>";
  html += "<input type='checkbox' name='flip' value='1' style='width:18px;height:18px;accent-color:var(--accent)' ";
  html += String(config.flip_screen ? "checked" : "") + "> Invertir pantalla (180&deg;)</label>";
  html += "<div class='help'>Para cuando el USB queda del otro lado</div>";

  html += "<label>Zona horaria</label>";
  html += "<select name='timezone' style='width:100%;padding:10px;margin-top:4px;border:1px solid var(--border);border-radius:6px;background:#0f172a;color:var(--text);font-size:15px'>";
  const char* tzOptions[][2] = {
    {"ART3", "Argentina (UTC-3)"},
    {"UTC0", "UTC"},
    {"EST5EDT,M3.2.0,M11.1.0", "US Eastern (UTC-5/-4)"},
    {"CST6CDT,M3.2.0,M11.1.0", "US Central (UTC-6/-5)"},
    {"MST7MDT,M3.2.0,M11.1.0", "US Mountain (UTC-7/-6)"},
    {"PST8PDT,M3.2.0,M11.1.0", "US Pacific (UTC-8/-7)"},
    {"BRT3", "Brasil (UTC-3)"},
    {"CLT4CLST,M8.2.6/24,M5.2.6/24", "Chile (UTC-4/-3)"},
    {"COT5", "Colombia (UTC-5)"},
    {"CST6", "Mexico Centro (UTC-6)"},
    {"CET-1CEST,M3.5.0,M10.5.0/3", "Europa Central (UTC+1/+2)"},
    {"GMT0BST,M3.5.0/1,M10.5.0", "UK (UTC+0/+1)"},
  };
  for (int i = 0; i < 12; i++) {
    html += "<option value='" + String(tzOptions[i][0]) + "'";
    if (String(config.timezone) == String(tzOptions[i][0])) html += " selected";
    html += ">" + String(tzOptions[i][1]) + "</option>";
  }
  html += "</select>";
  html += "</div>";

  // Alertas
  html += "<div class='panel'><h2>Alertas</h2>";
  html += "<label style='display:flex;align-items:center;gap:8px;cursor:pointer'>";
  html += "<input type='checkbox' name='alerts_en' value='1' style='width:18px;height:18px;accent-color:var(--accent)' ";
  html += String(config.alerts_enabled ? "checked" : "") + "> Alertas sonoras activadas</label>";
  html += "<div class='help'>Beep + parpadeo del LED cuando el uso (5h o 7 dias) cruza un umbral</div>";

  html += "<label>Umbral de aviso (%)</label>";
  html += "<input type='number' name='warn_thr' value='" + String(config.warn_threshold) + "' min='50' max='99'>";
  html += "<label>Umbral critico (%)</label>";
  html += "<input type='number' name='crit_thr' value='" + String(config.crit_threshold) + "' min='51' max='100'>";

  html += "<label>GPIO del buzzer</label>";
  html += "<input type='number' name='buzzer_pin' value='" + String(config.buzzer_pin) + "' min='0' max='48'>";
  html += "<div class='help'>Buzzer pasivo (PWM). 0 = sin sonido (solo parpadeo). Pines seguros: 11, 12, 13, 14, 21. Un pin reservado se ignora.</div>";
  html += "</div>";

  html += "<div class='panel'><h2>Clima</h2>";
  html += "<label>Ciudad</label>";
  html += "<div style='display:flex;gap:8px;align-items:center'>";
  html += "<input type='text' id='city' name='city' value='" + String(config.city) + "' placeholder='Ej: Buenos Aires'>";
  html += "<button type='button' onclick='buscarCiudad()' style='width:auto;padding:10px 16px;margin-top:4px;font-size:.9rem'>Buscar</button>";
  html += "</div>";
  html += "<div class='help' id='cityres'>Busca la ciudad y completa lat/lon automaticamente (Open-Meteo, sin API key)</div>";
  html += "<label>Latitud</label>";
  html += "<input type='text' name='lat' value='" + String(config.lat, 4) + "'>";
  html += "<label>Longitud</label>";
  html += "<input type='text' name='lon' value='" + String(config.lon, 4) + "'>";
  html += "<div class='help'>Tambien se pueden cargar a mano. Ambos en 0 = pantalla de clima desactivada.</div>";
  html += "</div>";

  html += "<script>";
  html += "async function buscarCiudad(){";
  html += "const q=document.getElementById('city').value.trim();";
  html += "const res=document.getElementById('cityres');";
  html += "if(!q){res.textContent='Escribi una ciudad primero';return;}";
  html += "res.textContent='Buscando...';";
  html += "try{";
  html += "const r=await fetch('https://geocoding-api.open-meteo.com/v1/search?name='+encodeURIComponent(q)+'&count=1&language=es');";
  html += "const j=await r.json();";
  html += "if(!j.results||!j.results.length){res.textContent='Ciudad no encontrada';return;}";
  html += "const c=j.results[0];";
  html += "document.querySelector('input[name=lat]').value=c.latitude;";
  html += "document.querySelector('input[name=lon]').value=c.longitude;";
  html += "document.getElementById('city').value=c.name;";
  html += "res.textContent=c.name+(c.admin1?', '+c.admin1:'')+(c.country?', '+c.country:'')+' — lat/lon completados, guarda para aplicar';";
  html += "}catch(e){res.textContent='Error: '+e.message;}";
  html += "}";
  html += "</script>";

  html += "<div class='panel'><h2>Seguridad</h2>";
  html += "<label>Contrase&ntilde;a de admin</label>";
  html += "<input type='text' name='admin_pass' value='" + String(config.admin_pass) + "'>";
  html += "<div class='help'>Usuario: admin &mdash; se pide al entrar a esta p&aacute;gina</div>";
  html += "</div>";

  html += "<button type='submit'>Guardar</button></form>";

  // Reset WiFi
  html += "<form method='GET' action='/reset-wifi' onsubmit='return confirm(\"Borrar WiFi y reiniciar?\")'>";
  html += "<button type='submit' class='btn-danger'>Resetear WiFi</button></form>";
  html += "<div class='help' style='text-align:center;margin-top:4px'>Borra las credenciales WiFi guardadas y reinicia en modo AP</div>";

  html += "<div style='text-align:center;margin-top:24px;padding:14px;border-top:1px solid var(--accent);color:var(--accent);font-size:.8rem;letter-spacing:1px'>";
  html += "CLAWDMETER v2.0 &mdash; Claude Usage Monitor</div>";

  html += "</div></body></html>";

  webServer.send(200, "text/html", html);
}

void handleSave() {
  if (!checkAuth()) return;
  if (webServer.hasArg("proxy_ip"))    strlcpy(config.proxy_ip, webServer.arg("proxy_ip").c_str(), sizeof(config.proxy_ip));
  if (webServer.hasArg("proxy_port"))  config.proxy_port = webServer.arg("proxy_port").toInt();
  if (webServer.hasArg("refresh_sec")) config.refresh_sec = constrain(webServer.arg("refresh_sec").toInt(), 10, 600);
  if (webServer.hasArg("home_timeout")) config.home_timeout_sec = constrain(webServer.arg("home_timeout").toInt(), 0, 600);
  if (webServer.hasArg("led_bright"))  config.led_brightness = webServer.arg("led_bright").toInt();
  if (webServer.hasArg("lcd_bright")) {
    config.lcd_brightness = webServer.arg("lcd_bright").toInt();
    setBacklight(config.lcd_brightness);
  }

  bool newFlip = webServer.hasArg("flip");
  if (newFlip != config.flip_screen) {
    config.flip_screen = newFlip;
    tft.setRotation(config.flip_screen ? 3 : 1);
  }

  if (webServer.hasArg("timezone")) {
    strlcpy(config.timezone, webServer.arg("timezone").c_str(), sizeof(config.timezone));
    configTzTime(config.timezone, "pool.ntp.org", "time.google.com");
  }

  if (webServer.hasArg("admin_pass") && webServer.arg("admin_pass").length() > 0) {
    strlcpy(config.admin_pass, webServer.arg("admin_pass").c_str(), sizeof(config.admin_pass));
  }

  // Alertas: checkbox solo aparece como arg si esta tildado
  config.alerts_enabled = webServer.hasArg("alerts_en");
  if (webServer.hasArg("warn_thr")) config.warn_threshold = constrain(webServer.arg("warn_thr").toInt(), 50, 99);
  if (webServer.hasArg("crit_thr")) config.crit_threshold = constrain(webServer.arg("crit_thr").toInt(), 51, 100);
  if (webServer.hasArg("buzzer_pin")) {
    int p = webServer.arg("buzzer_pin").toInt();
    if (buzzerPinValid(p)) config.buzzer_pin = p;  // un pin reservado se ignora
  }

  if (webServer.hasArg("city")) strlcpy(config.city, webServer.arg("city").c_str(), sizeof(config.city));

  if (webServer.hasArg("lat") && webServer.hasArg("lon")) {
    float newLat = webServer.arg("lat").toFloat();
    float newLon = webServer.arg("lon").toFloat();
    if (newLat != config.lat || newLon != config.lon) {
      config.lat = newLat;
      config.lon = newLon;
      weatherValid = false;
      lastWeatherFetch = 0;  // fuerza refetch con las nuevas coordenadas
    }
  }

  saveConfig();

  if (dataValid) updateUsageLeds();

  webServer.sendHeader("Location", "/", true);
  webServer.send(302, "text/plain", "Saved. Redirecting...");
}

void handleResetWifi() {
  if (!checkAuth()) return;
  wm.resetSettings();
  webServer.send(200, "text/plain", "WiFi reset. Reiniciando...");
  delay(1000);
  ESP.restart();
}

void handleApi() {
  if (!dataValid) {
    webServer.send(503, "application/json", "{\"error\":\"no data\"}");
    return;
  }

  JsonDocument doc;
  doc["five_hour_pct"] = usage.five_hour_pct;
  doc["five_hour_reset"] = usage.five_hour_reset;
  doc["seven_day_pct"] = usage.seven_day_pct;
  doc["seven_day_reset"] = usage.seven_day_reset;
  doc["extra_pct"] = usage.extra_pct;
  doc["extra_used"] = usage.extra_used / 100.0f;
  doc["extra_limit"] = usage.extra_limit / 100.0f;

  String json;
  serializeJson(doc, json);
  webServer.send(200, "application/json", json);
}

void setupWebServer() {
  webServer.on("/", HTTP_GET, handleRoot);
  webServer.on("/save", HTTP_GET, handleSave);
  webServer.on("/reset-wifi", HTTP_GET, handleResetWifi);
  webServer.on("/api/status", HTTP_GET, handleApi);
  webServer.begin();

  if (MDNS.begin("clawdmeter")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS: http://clawdmeter.local");
  }
}
