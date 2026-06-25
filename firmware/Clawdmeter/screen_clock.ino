// Pantalla FECHA Y HORA (SCREEN_CLOCK): reloj grande HH:MM + segundos + fecha en
// español. tickClockRedraw() la refresca cada segundo mientras esta activa.

void drawClockContent() {
  char buf[48];

  time_t now = time(nullptr);
  struct tm tm;
  localtime_r(&now, &tm);

  if (tm.tm_year + 1900 < 2020) {
    spr.setTextColor(COL_YELLOW, COL_BG);
    spr.setTextDatum(MC_DATUM);
    spr.drawString("Sincronizando hora...", SCREEN_W / 2, SCREEN_H / 2, 2);
    return;
  }

  // Hora grande HH:MM (font 7, estilo 7 segmentos) + segundos
  snprintf(buf, sizeof(buf), "%02d:%02d", tm.tm_hour, tm.tm_min);
  spr.setTextColor(COL_CYAN, COL_BG);
  spr.setTextDatum(MC_DATUM);
  spr.drawString(buf, SCREEN_W / 2 - 20, CONTENT_Y + 42, 7);

  snprintf(buf, sizeof(buf), "%02d", tm.tm_sec);
  spr.setTextColor(COL_AMBER, COL_BG);
  spr.setTextDatum(ML_DATUM);
  spr.drawString(buf, SCREEN_W / 2 + 78, CONTENT_Y + 54, 4);

  // Fecha en espanol
  const char* DOW[] = {"Domingo", "Lunes", "Martes", "Miercoles", "Jueves", "Viernes", "Sabado"};
  const char* MON[] = {"Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio",
                       "Julio", "Agosto", "Septiembre", "Octubre", "Noviembre", "Diciembre"};
  snprintf(buf, sizeof(buf), "%s %d de %s %d",
           DOW[tm.tm_wday], tm.tm_mday, MON[tm.tm_mon], tm.tm_year + 1900);
  spr.setTextColor(COL_WHITE, COL_BG);
  spr.setTextDatum(MC_DATUM);
  spr.drawString(buf, SCREEN_W / 2, CONTENT_Y + 102, 2);
}

// Redibuja la pantalla de reloj cuando cambia el segundo
void tickClockRedraw() {
  if (currentScreen != SCREEN_CLOCK) return;

  static int lastSec = -1;
  time_t now = time(nullptr);
  struct tm tm;
  localtime_r(&now, &tm);
  if (tm.tm_sec != lastSec) {
    lastSec = tm.tm_sec;
    drawScreen();
  }
}
