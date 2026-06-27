// Web config del ESP32: SOLO bootstrap (lo necesario para llegar al proxy).
// El resto de los ajustes (brillo, alertas, clima, timezone, etc.) se configuran
// desde la PWA (/config.html) y el ESP32 los baja del proxy con fetchConfig().

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
  html.reserve(4096);

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
  html += "button{background:var(--accent);color:#0f172a;padding:14px;border:none;border-radius:8px;cursor:pointer;width:100%;font-size:1.1rem;font-weight:700;margin-top:10px}";
  html += "button:hover{opacity:.9}";
  html += ".btn-danger{background:#ef4444;color:#fff;font-size:.9rem;padding:10px;margin-top:20px}";
  html += ".status{text-align:center;padding:12px;background:rgba(0,221,255,.08);border:1px solid var(--accent);border-radius:8px;margin-bottom:16px;font-family:monospace;font-size:.85rem}";
  html += ".help{font-size:.8rem;color:var(--muted);margin-top:4px}";
  html += "a{color:var(--accent)}";
  html += "</style></head><body><div class='c'>";

  html += "<h1>CLAWDMETER</h1>";
  html += "<div class='sub'>Conexion (bootstrap)</div>";

  // Status
  html += "<div class='status'>";
  html += "IP: " + WiFi.localIP().toString() + " &bull; ";
  html += "WiFi: " + String(WiFi.RSSI()) + "dBm &bull; ";
  html += dataValid ? "<span style='color:#00cc44'>Online</span>" : "<span style='color:#ff3333'>Offline</span>";
  html += "<br>Upd: " + String(lastSuccess);
  html += "</div>";

  html += "<form method='POST' action='/save'>";

  // Proxy
  html += "<div class='panel'><h2>Proxy</h2>";
  html += "<label>IP del servidor proxy</label>";
  html += "<input type='text' name='proxy_ip' value='" + String(config.proxy_ip) + "'>";
  html += "<div class='help'>IP de la Mac/PC donde corre el proxy Node.js</div>";
  html += "<label>Puerto</label>";
  html += "<input type='number' name='proxy_port' value='" + String(config.proxy_port) + "'>";
  html += "<label>Usuario del proxy</label>";
  html += "<input type='text' name='proxy_user' value='" + String(config.proxy_user) + "'>";
  html += "<label>Contrasena del proxy</label>";
  html += "<input type='text' name='proxy_pass' value='" + String(config.proxy_pass) + "'>";
  html += "<div class='help'>Credenciales para bajar la config (las mismas de la pagina /config.html)</div>";
  html += "</div>";

  // Seguridad de esta pagina
  html += "<div class='panel'><h2>Seguridad</h2>";
  html += "<label>Contrasena de admin (de esta pagina)</label>";
  html += "<input type='text' name='admin_pass' value='" + String(config.admin_pass) + "'>";
  html += "<div class='help'>Usuario: admin &mdash; se pide al entrar aca</div>";
  html += "</div>";

  // Nota: el resto va por la PWA
  html += "<div class='panel'><h2>El resto de los ajustes</h2>";
  html += "<div class='help'>Brillo, alertas, clima, zona horaria, etc. se configuran desde la PWA: ";
  html += "<a href='http://" + String(config.proxy_ip) + ":" + String(config.proxy_port) + "/config.html'>";
  html += String(config.proxy_ip) + ":" + String(config.proxy_port) + "/config.html</a>. ";
  html += "El ESP32 los baja del proxy al arrancar.</div>";
  html += "</div>";

  html += "<button type='submit'>Guardar</button></form>";

  // Reset WiFi
  html += "<form method='POST' action='/reset-wifi' onsubmit='return confirm(\"Borrar WiFi y reiniciar?\")'>";
  html += "<button type='submit' class='btn-danger'>Resetear WiFi</button></form>";
  html += "<div class='help' style='text-align:center;margin-top:4px'>Borra las credenciales WiFi y reinicia en modo AP</div>";

  html += "<div style='text-align:center;margin-top:24px;padding:14px;border-top:1px solid var(--accent);color:var(--accent);font-size:.8rem;letter-spacing:1px'>";
  html += "CLAWDMETER &mdash; Bootstrap</div>";

  html += "</div></body></html>";

  webServer.send(200, "text/html", html);
}

void handleSave() {
  if (!checkAuth()) return;

  if (webServer.hasArg("proxy_ip"))   strlcpy(config.proxy_ip, webServer.arg("proxy_ip").c_str(), sizeof(config.proxy_ip));
  if (webServer.hasArg("proxy_port")) config.proxy_port = webServer.arg("proxy_port").toInt();
  if (webServer.hasArg("proxy_user")) strlcpy(config.proxy_user, webServer.arg("proxy_user").c_str(), sizeof(config.proxy_user));
  if (webServer.hasArg("proxy_pass")) strlcpy(config.proxy_pass, webServer.arg("proxy_pass").c_str(), sizeof(config.proxy_pass));
  if (webServer.hasArg("admin_pass") && webServer.arg("admin_pass").length() > 0)
    strlcpy(config.admin_pass, webServer.arg("admin_pass").c_str(), sizeof(config.admin_pass));

  saveConfig();
  if (fetchConfig()) drawScreen();  // con los nuevos datos del proxy, baja la config ya

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
  webServer.on("/save", HTTP_POST, handleSave);
  webServer.on("/reset-wifi", HTTP_POST, handleResetWifi);
  webServer.on("/api/status", HTTP_GET, handleApi);
  webServer.begin();

  if (MDNS.begin("clawdmeter")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS: http://clawdmeter.local");
  }
}
