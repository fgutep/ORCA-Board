// ORCA-BOARD Control Rev A — autoprueba completa por Wi-Fi y por USB
// Cada 5 s prueba I2C de placa (INA226, PCA9685, BNO085), mux TCA9548A, Isens (ACS725) y CAN,
// y publica el informe:
//   - Wi-Fi (AP "ORCA-Board" / "orca1234"): http://192.168.4.1/ (página), /report.txt, /status.json
//   - USB: el mismo informe por el puerto serie
// /status.json incluye los campos de wifi_telemetry, así que orca_monitor.py también funciona.
//
// Escrituras I2C: solo de verificación y restaurando el valor (PCA9685 LED0_ON_L, INA226 config,
// control del TCA9548A). Con el PCA9685 en SLEEP (estado al encender) no sale PWM a los servos.

#include <Arduino.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>
#include <driver/twai.h>
#include <esp_chip_info.h>
#include <esp_mac.h>
#include <stdarg.h>

constexpr char AP_SSID[] = "ORCA-Board";
constexpr char AP_PASS[] = "orca1234";
constexpr char HOSTNAME[] = "orca-board";

// Pines (ver 05_revision_CAN_I2C_sensores.md)
constexpr int OB_SDA = 18, OB_SCL = 17;                  // I2C de placa
constexpr int EXT_SDA = 39, EXT_SCL = 38, EXT_RST = 40;  // I2C externo (TCA9548A)
constexpr int ISENS_PIN = 9;                             // ACS725 VIOUT
constexpr gpio_num_t CAN_TX = GPIO_NUM_6, CAN_RX = GPIO_NUM_7;

constexpr uint8_t ADDR_INA226 = 0x40, ADDR_PCA9685 = 0x41, ADDR_ALLCALL = 0x70, ADDR_BNO085 = 0x4A,
                  ADDR_BNO085_ALT = 0x4B, ADDR_TCA9548A = 0x70;
constexpr bool BNO085_MOUNTED = false;  // cambiar a true cuando se monte U1
constexpr float ISENS_NOMINAL_ZERO_MV = 330.0f, ISENS_MV_PER_A = 264.0f;
constexpr uint32_t SAMPLE_PERIOD_MS = 100, TEST_PERIOD_MS = 5000;

TwoWire &obI2c = Wire;
TwoWire &extI2c = Wire1;
WebServer server(80);
Preferences prefs;

// Estado persistente y telemetría (mismos campos que wifi_telemetry)
uint32_t bootCount = 0, brownoutCount = 0, samples = 0, cycle = 0;
esp_reset_reason_t resetReason;
float isensZeroMv = ISENS_NOMINAL_ZERO_MV;
bool isensZeroCalibrated = false;
float isensMv = 0, isensA = 0, isensMaxA = 0;
float vbus = NAN, vbusMin = NAN, vbusMax = NAN;
bool inaOk = false, pcaAt41 = false, canDriverOk = false;

// Resultado de la última autoprueba
String report, obScan, muxScan[4];
int nPass = 0, nFail = 0;
bool tInaId = false, tInaWrite = false, tPcaId = false, tPcaWrite = false, tAllCall = false, tBno = false,
     tTca = false, tIsens = false, tCan = false;
uint32_t canTec = 0, canRec = 0, canBusErr = 0;

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

// ---------- informe ----------

void line(const char *fmt, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  report += buf;
  report += '\n';
}

bool check(bool ok, const char *fmt, ...) {
  char buf[220];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  line("  [%s] %s", ok ? "PASS" : "FAIL", buf);
  ok ? nPass++ : nFail++;
  return ok;
}

// ---------- I2C ----------

bool present(TwoWire &bus, uint8_t addr) {
  bus.beginTransmission(addr);
  return bus.endTransmission() == 0;
}

String scan(TwoWire &bus, uint8_t skip = 0) {
  String s;
  for (uint8_t a = 1; a < 127; a++) {
    if (a != skip && present(bus, a)) {
      char b[6];
      snprintf(b, sizeof b, "0x%02X ", a);
      s += b;
    }
  }
  s.trim();
  return s.length() ? s : String("(ninguno)");
}

int32_t read16(TwoWire &bus, uint8_t addr, uint8_t reg) {
  bus.beginTransmission(addr);
  bus.write(reg);
  if (bus.endTransmission(false) != 0 || bus.requestFrom(addr, (uint8_t)2) != 2) return -1;
  return (bus.read() << 8) | bus.read();
}

bool write16(TwoWire &bus, uint8_t addr, uint8_t reg, uint16_t v) {
  bus.beginTransmission(addr);
  bus.write(reg);
  bus.write(v >> 8);
  bus.write(v & 0xFF);
  return bus.endTransmission() == 0;
}

int32_t read8(TwoWire &bus, uint8_t addr, uint8_t reg) {
  bus.beginTransmission(addr);
  bus.write(reg);
  if (bus.endTransmission(false) != 0 || bus.requestFrom(addr, (uint8_t)1) != 1) return -1;
  return bus.read();
}

bool write8(TwoWire &bus, uint8_t addr, uint8_t reg, uint8_t v) {
  bus.beginTransmission(addr);
  bus.write(reg);
  bus.write(v);
  return bus.endTransmission() == 0;
}

// ---------- telemetría a 10 Hz ----------

float readIsensMv(int n) {
  uint32_t sum = 0;
  for (int i = 0; i < n; i++) sum += analogReadMilliVolts(ISENS_PIN);
  return sum / (float)n;
}

void sample() {
  isensMv = readIsensMv(16);
  isensA = (isensMv - isensZeroMv) / ISENS_MV_PER_A;
  if (isensA > isensMaxA) isensMaxA = isensA;

  int32_t id = read16(obI2c, ADDR_INA226, 0xFE);
  int32_t raw = read16(obI2c, ADDR_INA226, 0x02);
  inaOk = id == 0x5449 && raw >= 0;
  if (inaOk) {
    vbus = raw * 0.00125f;  // registro de bus voltage, 1.25 mV/LSB
    vbusMin = isnan(vbusMin) ? vbus : min(vbusMin, vbus);
    vbusMax = isnan(vbusMax) ? vbus : max(vbusMax, vbus);
  } else {
    vbus = NAN;
  }
  pcaAt41 = present(obI2c, ADDR_PCA9685);
  samples++;
}

// ---------- autoprueba ----------

void testOnboardI2c() {
  line("[I2C placa] IO18 SDA / IO17 SCL");
  obScan = scan(obI2c);
  line("  dispositivos: %s (esperado 0x40 0x41 0x70%s)", obScan.c_str(), BNO085_MOUNTED ? " 0x4A" : "");

  int32_t id = read16(obI2c, ADDR_INA226, 0xFE);
  tInaId = check(id == 0x5449, "INA226 en 0x40: ID 0x%04lX (esperado 0x5449) | Vbus = %.3f V", id,
                 read16(obI2c, ADDR_INA226, 0x02) * 0.00125f);
  // Escribe el mismo valor de configuración que tiene y lo relee: prueba la escritura sin cambiar nada
  int32_t cfg = read16(obI2c, ADDR_INA226, 0x00);
  tInaWrite = check(cfg >= 0 && write16(obI2c, ADDR_INA226, 0x00, cfg) && read16(obI2c, ADDR_INA226, 0x00) == cfg,
                    "INA226 escritura/lectura de config: 0x%04lX", cfg);

  int32_t mode1 = read8(obI2c, ADDR_PCA9685, 0x00);
  tPcaId = check(mode1 >= 0, "PCA9685 en 0x41: MODE1 = 0x%02lX (0x11 al encender = SLEEP + ALLCALL)", mode1);
  // LED0_ON_L (0x06): escribir 0x5A, releer, restaurar
  int32_t orig = read8(obI2c, ADDR_PCA9685, 0x06);
  bool wr = orig >= 0 && write8(obI2c, ADDR_PCA9685, 0x06, 0x5A);
  int32_t back = read8(obI2c, ADDR_PCA9685, 0x06);
  if (orig >= 0) write8(obI2c, ADDR_PCA9685, 0x06, orig);
  tPcaWrite = check(wr && back == 0x5A, "PCA9685 escritura/lectura LED0_ON_L: escrito 0x5A, leido 0x%02lX", back);

  tAllCall = check(present(obI2c, ADDR_ALLCALL), "PCA9685 LED All Call en 0x70");

  tBno = present(obI2c, ADDR_BNO085) || present(obI2c, ADDR_BNO085_ALT);
  if (BNO085_MOUNTED) {
    check(tBno, "BNO085 en 0x4A/0x4B");
  } else {
    line("  [N/A ] BNO085 no montado (%s en 0x4A/0x4B)", tBno ? "algo responde" : "nada responde");
  }
}

void testMux() {
  line("[Mux I2C] IO39 SDA / IO38 SCL / IO40 RESET");
  int ctrl = -1;
  if (present(extI2c, ADDR_TCA9548A)) {
    extI2c.beginTransmission(ADDR_TCA9548A);
    extI2c.write(0x01);
    extI2c.endTransmission();
    extI2c.requestFrom(ADDR_TCA9548A, (uint8_t)1);
    ctrl = extI2c.available() ? extI2c.read() : -1;
  }
  tTca = check(ctrl == 0x01, "TCA9548A en 0x70: control escrito 0x01, leido 0x%02X", ctrl);
  if (ctrl < 0) return;

  // Canales 0-3 = J14-J17 (en el conector: pin 3 = SCL, pin 4 = SDA). Sin pull-ups en la placa.
  static const char *conn[] = {"CH0 J14 (5V) ", "CH1 J15 (5V) ", "CH2 J16 (3V3)", "CH3 J17 (3V3)"};
  for (uint8_t ch = 0; ch < 4; ch++) {
    extI2c.beginTransmission(ADDR_TCA9548A);
    extI2c.write(1 << ch);
    extI2c.endTransmission();
    muxScan[ch] = scan(extI2c, ADDR_TCA9548A);
    line("  [INFO] %s: %s", conn[ch], muxScan[ch].c_str());
  }
  extI2c.beginTransmission(ADDR_TCA9548A);
  extI2c.write(0x00);
  extI2c.endTransmission();
}

void testIsens() {
  line("[Isens] ACS725 en IO9 (0 A = 0.1 x VCC, 264 mV/A)");
  float mv = readIsensMv(64);
  tIsens = check(mv > 250 && mv < 450, "VIOUT = %.0f mV -> I = %.3f A (cero calibrado %.0f mV, max %.3f A)", mv,
                 (mv - isensZeroMv) / ISENS_MV_PER_A, isensZeroMv, isensMaxA);
}

void testCan() {
  line("[CAN] SN65HVD230, TX IO6 / RX IO7, 500 kbit/s, NO_ACK + autorrecepcion");
  if (!canDriverOk) {
    tCan = check(false, "No se pudo iniciar el driver TWAI");
    return;
  }
  // La trama sale por IO6, pasa por el transceptor al bus (R15 120 ohm) y vuelve por IO7
  twai_message_t tx = {};
  tx.identifier = 0x123;
  tx.self = 1;
  tx.data_length_code = 4;
  tx.data[0] = 0x0C;
  tx.data[1] = 0xA0;
  tx.data[2] = cycle >> 8;
  tx.data[3] = cycle;

  twai_message_t rx;
  while (twai_receive(&rx, 0) == ESP_OK) {}  // vaciar
  bool sent = twai_transmit(&tx, pdMS_TO_TICKS(100)) == ESP_OK;
  bool got = sent && twai_receive(&rx, pdMS_TO_TICKS(200)) == ESP_OK && rx.identifier == tx.identifier &&
             rx.data_length_code == 4 && memcmp(rx.data, tx.data, 4) == 0;

  twai_status_info_t st;
  twai_get_status_info(&st);
  canTec = st.tx_error_counter;
  canRec = st.rx_error_counter;
  canBusErr = st.bus_error_count;
  static const char *states[] = {"STOPPED", "RUNNING", "BUS_OFF", "RECOVERING"};
  tCan = check(got, "eco %s | estado %s, TEC %lu, REC %lu, errores de bus %lu",
               got ? "recibido" : (sent ? "no recibido" : "no transmitido"), states[st.state], canTec, canRec,
               canBusErr);
  if (st.state == TWAI_STATE_BUS_OFF) twai_initiate_recovery();
  if (st.state == TWAI_STATE_STOPPED) twai_start();
}

void runTests() {
  report = "";
  nPass = nFail = 0;
  esp_chip_info_t info;
  esp_chip_info(&info);
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  line("===== ORCA selftest #%lu | %s rev v%d.%d | MAC %02x:%02x:%02x:%02x:%02x:%02x | uptime %lu s =====",
       cycle, ESP.getChipModel(), info.revision / 100, info.revision % 100, mac[0], mac[1], mac[2], mac[3], mac[4],
       mac[5], (unsigned long)(millis() / 1000));
  line("[Sistema] arranque #%lu, causa %s, brownouts %lu, %.1f C, heap libre %lu B", bootCount,
       reasonName(resetReason), brownoutCount, temperatureRead(), (unsigned long)ESP.getFreeHeap());
  line("[Wi-Fi] AP %s, IP %s, clientes %d", AP_SSID, WiFi.softAPIP().toString().c_str(), WiFi.softAPgetStationNum());

  testOnboardI2c();
  testMux();
  testIsens();
  testCan();

  line("===== RESUMEN: %d PASS, %d FAIL =====", nPass, nFail);
  cycle++;
}

// ---------- web ----------

String num(float v, int decimals) { return isnan(v) ? String("null") : String(v, decimals); }
const char *tf(bool b) { return b ? "true" : "false"; }

String statusJson() {
  char buf[1600];
  snprintf(buf, sizeof buf,
           "{\"uptime_ms\":%lu,\"boot_count\":%lu,\"reset_reason\":\"%s\",\"brownouts\":%lu,"
           "\"vbus_v\":%s,\"vbus_min_v\":%s,\"vbus_max_v\":%s,\"ina226_ok\":%s,\"pca9685_at_0x41\":%s,"
           "\"isens_mv\":%.1f,\"isens_a\":%.3f,\"isens_max_a\":%.3f,\"isens_zero_mv\":%.1f,\"isens_zero_cal\":%s,"
           "\"chip_temp_c\":%.1f,\"free_heap\":%lu,\"min_free_heap\":%lu,"
           "\"wifi_mode\":\"ap\",\"ip\":\"%s\",\"rssi_dbm\":0,\"ap_clients\":%d,\"samples\":%lu,"
           "\"selftest\":{\"cycle\":%lu,\"pass\":%d,\"fail\":%d,\"ob_scan\":\"%s\",\"ina226_id\":%s,"
           "\"ina226_write\":%s,\"pca9685\":%s,\"pca9685_write\":%s,\"pca9685_allcall\":%s,"
           "\"bno085_mounted\":%s,\"bno085\":%s,\"tca9548a\":%s,\"mux_ch\":[\"%s\",\"%s\",\"%s\",\"%s\"],"
           "\"isens\":%s,\"can\":%s,\"can_tec\":%lu,\"can_rec\":%lu,\"can_bus_errors\":%lu}}",
           (unsigned long)millis(), (unsigned long)bootCount, reasonName(resetReason), (unsigned long)brownoutCount,
           num(vbus, 3).c_str(), num(vbusMin, 3).c_str(), num(vbusMax, 3).c_str(), tf(inaOk), tf(pcaAt41), isensMv,
           isensA, isensMaxA, isensZeroMv, tf(isensZeroCalibrated), temperatureRead(),
           (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getMinFreeHeap(),
           WiFi.softAPIP().toString().c_str(), WiFi.softAPgetStationNum(), (unsigned long)samples,
           (unsigned long)cycle, nPass, nFail, obScan.c_str(), tf(tInaId), tf(tInaWrite), tf(tPcaId), tf(tPcaWrite),
           tf(tAllCall), tf(BNO085_MOUNTED), tf(tBno), tf(tTca), muxScan[0].c_str(), muxScan[1].c_str(),
           muxScan[2].c_str(), muxScan[3].c_str(), tf(tIsens), tf(tCan), (unsigned long)canTec,
           (unsigned long)canRec, (unsigned long)canBusErr);
  return String(buf);
}

const char PAGE[] PROGMEM = R"html(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>ORCA autoprueba</title>
<style>body{font-family:monospace;margin:16px;background:#111;color:#eee}pre{white-space:pre-wrap}
.bad{color:#f66}.ok{color:#6f6}.na{color:#aaa}</style></head><body><h3>ORCA-BOARD — autoprueba</h3>
<div id="st">esperando…</div><pre id="r"></pre><p><a href="/status.json">status.json</a></p><script>
function esc(s){return s.replace(/&/g,'&amp;').replace(/</g,'&lt;')}
async function tick(){try{const r=await fetch('/report.txt',{cache:'no-store'});const t=await r.text();
document.getElementById('st').innerHTML='<span class="ok">EN LÍNEA</span>';
document.getElementById('r').innerHTML=esc(t).replace(/\[PASS\]/g,'<span class="ok">[PASS]</span>')
.replace(/\[FAIL\]/g,'<span class="bad">[FAIL]</span>').replace(/\[N\/A \]/g,'<span class="na">[N/A ]</span>');}
catch(e){document.getElementById('st').innerHTML='<span class="bad">SIN RESPUESTA</span>';}}
setInterval(tick,2000);tick();</script></body></html>)html";

void setup() {
  Serial.begin(115200);
  resetReason = esp_reset_reason();

  prefs.begin("orca", false);
  bootCount = prefs.getUInt("boots", 0) + 1;
  prefs.putUInt("boots", bootCount);
  brownoutCount = prefs.getUInt("brownouts", 0);
  if (resetReason == ESP_RST_BROWNOUT) prefs.putUInt("brownouts", ++brownoutCount);

  pinMode(EXT_RST, OUTPUT);
  digitalWrite(EXT_RST, HIGH);  // RESET del TCA9548A no tiene pull-up en la placa
  obI2c.begin(OB_SDA, OB_SCL, 100000);
  extI2c.begin(EXT_SDA, EXT_SCL, 100000);
  analogSetPinAttenuation(ISENS_PIN, ADC_11db);

  // Cero del ACS725 al arrancar (motores quietos). Fuera de 250-450 mV se usa el nominal.
  float zero = readIsensMv(256);
  if (zero > 250 && zero < 450) {
    isensZeroMv = zero;
    isensZeroCalibrated = true;
  }

  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX, CAN_RX, TWAI_MODE_NO_ACK);
  twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  canDriverOk = twai_driver_install(&g, &t, &f) == ESP_OK && twai_start() == ESP_OK;

  WiFi.setHostname(HOSTNAME);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  WiFi.setSleep(false);
  MDNS.begin(HOSTNAME);

  server.on("/", [] { server.send_P(200, "text/html", PAGE); });
  server.on("/report.txt", [] {
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "text/plain; charset=utf-8", report);
  });
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

  runTests();
}

void loop() {
  static uint32_t lastSample = 0, lastTest = 0;
  server.handleClient();
  uint32_t now = millis();
  if (now - lastSample >= SAMPLE_PERIOD_MS) {
    lastSample = now;
    sample();
  }
  if (now - lastTest >= TEST_PERIOD_MS) {
    lastTest = now;
    runTests();
    Serial.print('\n');
    Serial.print(report);
  }
}
