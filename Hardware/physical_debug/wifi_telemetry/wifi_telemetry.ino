// ORCA-BOARD Control Rev A — telemetría por Wi-Fi para pruebas de potencia
// Publica el estado de la placa en http://<ip>/status.json (y una página en http://<ip>/).
// Muestrea Vbus (INA226), Isens (ACS725) y temperatura del chip a 10 Hz, guarda mínimos/máximos,
// y cuenta arranques y brownouts en NVS para detectar reinicios durante las pruebas.
//
// Wi-Fi: por defecto crea su propia red (AP) "ORCA-Board" / "orca1234", IP 192.168.4.1.
// Para unirse a una red existente, compilar con -DORCA_WIFI_SSID="..." -DORCA_WIFI_PASS="...";
// si no conecta en 15 s, vuelve al modo AP.

#include <Arduino.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_system.h>

#ifndef ORCA_WIFI_SSID
#define ORCA_WIFI_SSID ""
#endif
#ifndef ORCA_WIFI_PASS
#define ORCA_WIFI_PASS ""
#endif

constexpr char AP_SSID[] = "ORCA-Board";
constexpr char AP_PASS[] = "orca1234";
constexpr char HOSTNAME[] = "orca-board";

constexpr int OB_SDA = 18, OB_SCL = 17, ISENS_PIN = 9;
constexpr uint8_t ADDR_INA226 = 0x40, ADDR_PCA9685_ALT = 0x41;
constexpr float ISENS_NOMINAL_ZERO_MV = 330.0f, ISENS_MV_PER_A = 264.0f;
constexpr uint32_t SAMPLE_PERIOD_MS = 100, SERIAL_PERIOD_MS = 1000;

WebServer server(80);
Preferences prefs;

uint32_t bootCount = 0, brownoutCount = 0, samples = 0;
esp_reset_reason_t resetReason;
const char *wifiMode = "ap";
float isensZeroMv = ISENS_NOMINAL_ZERO_MV;
bool isensZeroCalibrated = false;
float isensMv = 0, isensA = 0, isensMaxA = 0;
float vbus = NAN, vbusMin = NAN, vbusMax = NAN;
bool inaOk = false, pcaAt41 = false;

const char *reasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXT";
    case ESP_RST_SW: return "SW";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    case ESP_RST_USB: return "USB";
    case ESP_RST_JTAG: return "JTAG";
    default: return "UNKNOWN";
  }
}

float readIsensMv(int n) {
  uint32_t sum = 0;
  for (int i = 0; i < n; i++) sum += analogReadMilliVolts(ISENS_PIN);
  return sum / (float)n;
}

bool present(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

// Solo lectura: mientras INA226 y PCA9685 compartan 0x40, una escritura llegaría a los dos
bool inaRead(uint8_t reg, uint16_t &value) {
  Wire.beginTransmission(ADDR_INA226);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(ADDR_INA226, (uint8_t)2) != 2) return false;
  value = (Wire.read() << 8) | Wire.read();
  return true;
}

void sample() {
  isensMv = readIsensMv(16);
  isensA = (isensMv - isensZeroMv) / ISENS_MV_PER_A;
  if (isensA > isensMaxA) isensMaxA = isensA;

  uint16_t id = 0, raw = 0;
  inaOk = inaRead(0xFE, id) && id == 0x5449;  // ID ≠ 0x5449 = colisión con el PCA9685 o sin INA226
  if (inaOk && inaRead(0x02, raw)) {
    vbus = raw * 0.00125f;  // registro de bus voltage, 1.25 mV/LSB
    vbusMin = isnan(vbusMin) ? vbus : min(vbusMin, vbus);
    vbusMax = isnan(vbusMax) ? vbus : max(vbusMax, vbus);
  } else {
    vbus = NAN;
  }
  pcaAt41 = present(ADDR_PCA9685_ALT);
  samples++;
}

String num(float v, int decimals) { return isnan(v) ? String("null") : String(v, decimals); }

String statusJson() {
  char buf[768];
  snprintf(buf, sizeof buf,
           "{\"uptime_ms\":%lu,\"boot_count\":%lu,\"reset_reason\":\"%s\",\"brownouts\":%lu,"
           "\"vbus_v\":%s,\"vbus_min_v\":%s,\"vbus_max_v\":%s,\"ina226_ok\":%s,\"pca9685_at_0x41\":%s,"
           "\"isens_mv\":%.1f,\"isens_a\":%.3f,\"isens_max_a\":%.3f,\"isens_zero_mv\":%.1f,\"isens_zero_cal\":%s,"
           "\"chip_temp_c\":%.1f,\"free_heap\":%lu,\"min_free_heap\":%lu,"
           "\"wifi_mode\":\"%s\",\"ip\":\"%s\",\"rssi_dbm\":%d,\"ap_clients\":%d,\"samples\":%lu}",
           (unsigned long)millis(), (unsigned long)bootCount, reasonName(resetReason), (unsigned long)brownoutCount,
           num(vbus, 3).c_str(), num(vbusMin, 3).c_str(), num(vbusMax, 3).c_str(), inaOk ? "true" : "false",
           pcaAt41 ? "true" : "false", isensMv, isensA, isensMaxA, isensZeroMv, isensZeroCalibrated ? "true" : "false",
           temperatureRead(), (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap(), wifiMode,
           (strcmp(wifiMode, "sta") == 0 ? WiFi.localIP() : WiFi.softAPIP()).toString().c_str(),
           strcmp(wifiMode, "sta") == 0 ? WiFi.RSSI() : 0, WiFi.softAPgetStationNum(), (unsigned long)samples);
  return String(buf);
}

const char PAGE[] PROGMEM = R"html(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>ORCA telemetría</title>
<style>body{font-family:monospace;margin:16px;background:#111;color:#eee}td{padding:2px 12px}
.bad{color:#f66}.ok{color:#6f6}</style></head><body><h3>ORCA-BOARD — telemetría</h3>
<div id="st">esperando…</div><table id="t"></table><script>
async function tick(){try{const r=await fetch('/status.json',{cache:'no-store'});const j=await r.json();
document.getElementById('st').innerHTML='<span class="ok">EN LÍNEA</span>';
document.getElementById('t').innerHTML=Object.entries(j).map(([k,v])=>`<tr><td>${k}</td><td>${v}</td></tr>`).join('');}
catch(e){document.getElementById('st').innerHTML='<span class="bad">SIN RESPUESTA</span>';}}
setInterval(tick,1000);tick();</script></body></html>)html";

void startWifi() {
  WiFi.setHostname(HOSTNAME);
  if (strlen(ORCA_WIFI_SSID) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ORCA_WIFI_SSID, ORCA_WIFI_PASS);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(100);
    if (WiFi.status() == WL_CONNECTED) {
      wifiMode = "sta";
      WiFi.setAutoReconnect(true);
    }
  }
  if (strcmp(wifiMode, "sta") != 0) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    wifiMode = "ap";
  }
  WiFi.setSleep(false);
  MDNS.begin(HOSTNAME);
}

void setup() {
  Serial.begin(115200);
  resetReason = esp_reset_reason();

  prefs.begin("orca", false);
  bootCount = prefs.getUInt("boots", 0) + 1;
  prefs.putUInt("boots", bootCount);
  brownoutCount = prefs.getUInt("brownouts", 0);
  if (resetReason == ESP_RST_BROWNOUT) prefs.putUInt("brownouts", ++brownoutCount);

  Wire.begin(OB_SDA, OB_SCL, 100000);
  analogSetPinAttenuation(ISENS_PIN, ADC_11db);

  // Cero del ACS725 al arrancar (motores quietos). Fuera de 250-450 mV se usa el nominal.
  float zero = readIsensMv(256);
  if (zero > 250 && zero < 450) {
    isensZeroMv = zero;
    isensZeroCalibrated = true;
  }

  startWifi();
  server.on("/", [] { server.send_P(200, "text/html", PAGE); });
  server.on("/status.json", [] {
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", statusJson());
  });
  server.on("/reset_max", [] {
    isensMaxA = 0;
    vbusMin = vbusMax = NAN;
    server.send(200, "text/plain", "ok");
  });
  server.begin();
}

void loop() {
  static uint32_t lastSample = 0, lastSerial = 0;
  server.handleClient();
  uint32_t now = millis();
  if (now - lastSample >= SAMPLE_PERIOD_MS) {
    lastSample = now;
    sample();
  }
  if (now - lastSerial >= SERIAL_PERIOD_MS) {
    lastSerial = now;
    Serial.println(statusJson());
  }
}
