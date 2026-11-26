/* ============================================================================
 *  ORCA-BOARD Control Rev A — PCA9685 (U9) como controlador de servos del brazo por gestos.
 *
 *  Ejemplo: sustituto directo de arm_gesture/firmware_arm (tarjeta de servos con
 *  ESP32-S3, fw 3). Mismo protocolo JSON, mismo cuidado de servos (rampa, banda
 *  muerta, límites), homing y E-STOP, así que el GUI web no cambia. Solo cambia la
 *  salida: los servos van por el PCA9685 de la placa (I2C 0x41, SDA IO18 / SCL IO17)
 *  en vez de pines LEDC del ESP32. Pareja de orca_gesture_imu (BNO085 como IMU).
 *
 *    servo        tarjeta vieja   conector ORCA   canal PCA9685
 *    0 base       GPIO 14         J6              LED0
 *    1 hombro     GPIO 13         J7              LED1
 *    2 codo       GPIO 12         J9              LED3
 *    3 pinza      GPIO 11         J8              LED2   (20..180, abierta = 180)
 *  Codo y pinza según el cableado real del brazo (verificado en el hardware el
 *  24-nov-2026). J10 (LED4) queda libre; LED5..LED15 no están ruteados.
 *
 *  50 Hz, 0..180° -> 500..2400 us, contador de 12 bits del PCA9685 (~4.9 us por
 *  paso, ~0.46° por paso: más fino que la banda muerta por defecto de 1°).
 *
 *  Notas de hardware (esquemático Rev A):
 *   - ~OE del PCA9685 está a GND: LIMP = bit FULL_OFF por canal (sin pulsos).
 *     Al encender, el PCA9685 arranca en SLEEP con todo en FULL_OFF: servos sin torque.
 *   - J6..J9 se alimentan de 5V_SERVOS, el buck TPS54331 que sale de VBUS (batería
 *     2S..6S o fuente). Solo con USB los servos NO tienen alimentación. El INA226
 *     (0x40, mismo bus) mide VBUS y se reporta como "vbus" (V).
 *   - Con los cuatro servos llegando a su pose a la vez (al activar el torque) el pico
 *     de corriente es grande: un límite de corriente bajo en la fuente hace caer VBUS
 *     y el buck (probado a 9 V; con el límite bajo, VBUS oscilaba entre 0 y 9 V).
 *   - El oscilador interno del PCA9685 es de 25 MHz nominales pero varía entre chips;
 *     el ancho de pulso escala con él. Ajuste con {"osc":<Hz>} (medir con osciloscopio
 *     un pulso de 1500 us: osc_nuevo = osc * us_medidos / 1500) y luego {"save":1}.
 *
 *  Protocolo: JSON por líneas en ambos sentidos. Se pueden combinar claves en un
 *  mismo objeto, p. ej. {"en":1,"set":[90,90,90,180]}.
 *    host -> placa
 *      {"set":[a0,a1,a2,a3]}        objetivos (°); null = no cambia
 *      {"set":[...],"force":1}      ignora la banda muerta (jog / movimientos manuales)
 *      {"en":1} | {"en":[1,0,1,1]}  torque on/off (0 = limp, sin pulsos)
 *      {"lim":{"min":[..],"max":[..]}}  límites suaves (°)
 *      {"slew":[..]}  {"db":1.0}    rampa (°/s) y banda muerta (°)
 *      {"cal":1}                    calibración: límites = rango de hardware, rampa limitada
 *      {"save":1}                   guarda lim/slew/db/osc en NVS
 *      {"defaults":1}               lim/slew/db de fábrica (no guarda; no toca osc)
 *      {"get":1}                    responde cfg + st
 *      {"home":{"pos":[..],"speed":15}}  torque on, todos los ejes a `pos` (por defecto la
 *                                   pose de arranque), sincronizados para llegar juntos; el
 *                                   eje con más recorrido va a `speed` °/s (1..90). Ignora
 *                                   "set" hasta terminar.
 *      {"stop":1}                   E-STOP: congela cada servo donde está (torque activo) y
 *                                   cancela el homing; el resto del mensaje se ignora
 *      {"osc":25000000}             ajuste del oscilador del PCA9685 (Hz, 20..30 MHz)
 *    placa -> host
 *      {"t":"hello","fw":3,"n":4,"hw":"orca","drv":"pca9685","pca":0|1,"pins":[..],"ch":[0,1,3,2]}
 *      {"t":"cfg","min":[..],"max":[..],"hwmin":[..],"hwmax":[..],"slew":[..],"db":..,"cal":0,"osc":..}
 *      {"t":"st","pos":[..],"tgt":[..],"en":[..],"cal":0,"vbus":..}   20 Hz en movimiento,
 *                                   2 Hz quieto (+ "home":0..1 de progreso durante el homing)
 *      {"t":"ok","save":1} / {"t":"ok","home":1} / {"t":"ok","stop":1} / {"t":"err","msg":".."}
 *      errores propios: "pca9685 not responding" (falla I2C con U9; reintenta cada 1 s),
 *                       "no VBUS: servos unpowered" (torque activado con VBUS < 6 V)
 *  Texto plano "<servo 1..4> <grados>" (p. ej. "4 180") también funciona desde un
 *  monitor serie: activa ese servo y lo mueve (con rampa y límites).
 * ========================================================================== */
// Compilar con Arduino-ESP32 core 3.x (en core 2.0.x el USB serie dejó de leer datos
// del host tras transmitir con el puerto cerrado), con ArduinoJson 7:
//   FQBN=esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=4M
//   arduino-cli compile --fqbn $FQBN --build-path build .
//   arduino-cli upload  --fqbn $FQBN --build-path build -p /dev/ttyACM0 .
// Después del upload la placa puede quedar en "waiting for download" (el reset por
// USB/RTS no vuelve a leer los straps, ver Hardware/physical_debug/04_retiro_SW2.md). Reiniciar con:
//   esptool --port /dev/ttyACM0 --before no-reset --after watchdog-reset chip-id
// o reconectando el USB. El puerto puede volver con otro número de ttyACM.
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Wire.h>

#define FW_VERSION   3
#define NSERVO       4
#define PWM_HZ       50
#define US_MIN       500        // 0°
#define US_MAX       2400       // 180°
#define TICK_MS      20         // un periodo PWM
#define CAL_SLEW     45.0f      // tope de °/s en modo calibración
#define HOME_SPEED   15.0f      // velocidad de homing por defecto (°/s, eje más largo)
#define VBUS_MIN     6.0f       // por debajo, el buck de 5 V de servos no regula

// ---- I2C de placa ORCA ----
#define OB_SDA       18
#define OB_SCL       17
#define I2C_HZ       400000
#define ADDR_PCA     0x41       // movido de 0x40 en el retrabajo Rev A (el INA226 es 0x40)
#define ADDR_INA     0x40
#define OSC_DEF      25000000.0f

// Registros del PCA9685
#define PCA_MODE1    0x00
#define PCA_MODE2    0x01
#define PCA_LED0     0x06       // LEDn_ON_L = 0x06 + 4n
#define PCA_PRESCALE 0xFE
#define MODE1_AI     0x20
#define MODE1_SLEEP  0x10
#define MODE1_RESTART 0x80
#define MODE2_OUTDRV 0x04       // salidas totem-pole (señal directa a J6..J9)
#define FULL_BIT     0x10       // bit 4 de LEDn_ON_H / LEDn_OFF_H

static const uint8_t CH[NSERVO]     = {0, 1, 3, 2};     // base J6, hombro J7, codo J9, pinza J8
static const float HW_MIN[NSERVO]   = {0, 0, 0, 20};    // rango mecánico absoluto
static const float HW_MAX[NSERVO]   = {180, 180, 180, 180};
static const float DEF_SLEW[NSERVO] = {90, 60, 90, 180};
static const float DEF_DB = 1.0f;
static const float BOOT_POS[NSERVO] = {90, 90, 90, 180}; // pinza abierta

static float lim_min[NSERVO], lim_max[NSERVO], slew[NSERVO], db = DEF_DB;
static float pos[NSERVO], tgt[NSERVO];
static bool  en[NSERVO] = {false, false, false, false};
static int   lastTk[NSERVO] = {-1, -1, -1, -1};          // último valor escrito, 0 = FULL_OFF
static bool  cal = false;
static bool  dirty = true;                               // cambió el estado -> reportar pronto
static bool  homing = false;
static float homeRate[NSERVO], homeSpan = 0;             // °/s por eje, recorrido máximo inicial
static float osc = OSC_DEF, usPerTick = 1e6f / (PWM_HZ * 4096.0f);
static bool  pcaOk = false;
static float vbus = -1;
static Preferences prefs;

static inline float lo(int i) { return cal ? HW_MIN[i] : lim_min[i]; }
static inline float hi(int i) { return cal ? HW_MAX[i] : lim_max[i]; }

static void send(JsonDocument &d);
static void err(const char *m);

// ---------------------------------------------------------------- I2C
static bool i2cWrite(uint8_t addr, uint8_t reg, const uint8_t *b, size_t n) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(b, n);
  return Wire.endTransmission() == 0;
}
static bool i2cWrite8(uint8_t addr, uint8_t reg, uint8_t v) { return i2cWrite(addr, reg, &v, 1); }
static int32_t i2cRead16(uint8_t addr, uint8_t reg) {    // big endian, -1 si falla
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return -1;
  if (Wire.requestFrom(addr, (uint8_t)2) != 2) return -1;
  int32_t hi8 = Wire.read();
  return (hi8 << 8) | Wire.read();
}

// ---------------------------------------------------------------- PCA9685
// Prescaler a partir del oscilador (ajustado); la conversión us -> cuentas usa la
// frecuencia que el chip produce de verdad, así el redondeo del prescaler no
// distorsiona el ancho de pulso.
static bool pcaInit() {
  int pre = (int)lroundf(osc / (4096.0f * PWM_HZ)) - 1;
  pre = constrain(pre, 3, 255);
  usPerTick = 1e6f * (pre + 1) / osc;                    // = periodo / 4096
  bool ok = i2cWrite8(ADDR_PCA, PCA_MODE1, MODE1_SLEEP)  // oscilador apagado: PRESCALE es escribible
         && i2cWrite8(ADDR_PCA, PCA_PRESCALE, (uint8_t)pre)
         && i2cWrite8(ADDR_PCA, PCA_MODE2, MODE2_OUTDRV)
         && i2cWrite8(ADDR_PCA, PCA_MODE1, MODE1_AI);    // despierta, autoincremento, ALLCALL apagado
  delayMicroseconds(600);                                // arranque del oscilador (500 us máx.)
  if (ok) i2cWrite8(ADDR_PCA, PCA_MODE1, MODE1_AI | MODE1_RESTART);
  for (int i = 0; i < NSERVO; i++) lastTk[i] = -1;       // fuerza reescribir todos los canales
  return ok;
}

// Un canal, los cuatro registros en una ráfaga con autoincremento (las salidas cambian en el STOP).
static void pcaChannel(uint8_t ch, int ticks) {
  uint8_t b[4] = {0, 0, 0, 0};
  if (ticks <= 0) b[3] = FULL_BIT;                       // FULL_OFF: sin pulsos -> limp
  else { b[2] = ticks & 0xFF; b[3] = (ticks >> 8) & 0x0F; }
  if (!i2cWrite(ADDR_PCA, PCA_LED0 + 4 * ch, b, 4)) {   // loop() lo reinicializa (el prescaler pudo perderse)
    pcaOk = false;
    err("pca9685 not responding");
  }
}

static int degToTicks(float d) {
  float us = US_MIN + d * (US_MAX - US_MIN) / 180.0f;
  return constrain((int)lroundf(us / usPerTick), 1, 4095);
}

static void writeServo(int i) {
  int tk = en[i] ? degToTicks(pos[i]) : 0;
  if (!pcaOk || tk == lastTk[i]) return;
  pcaChannel(CH[i], tk);
  if (pcaOk) lastTk[i] = tk;                             // escritura fallida: reintenta en el siguiente tick
}

// ---------------------------------------------------------------- INA226
static void inaInit() { uint8_t c[2] = {0x41, 0x27}; i2cWrite(ADDR_INA, 0x00, c, 2); } // valor de reset: continuo
static void readVbus() {
  int32_t r = i2cRead16(ADDR_INA, 0x02);                 // tensión de bus, 1.25 mV/LSB
  vbus = r < 0 ? -1 : r * 0.00125f;
}

// ---------------------------------------------------------------- configuración / NVS
static void defaults() {
  for (int i = 0; i < NSERVO; i++) { lim_min[i] = HW_MIN[i]; lim_max[i] = HW_MAX[i]; slew[i] = DEF_SLEW[i]; }
  db = DEF_DB;
}

static void load() {
  defaults();
  prefs.begin("armorca", true);
  if (prefs.getBytesLength("min") == sizeof lim_min) prefs.getBytes("min", lim_min, sizeof lim_min);
  if (prefs.getBytesLength("max") == sizeof lim_max) prefs.getBytes("max", lim_max, sizeof lim_max);
  if (prefs.getBytesLength("slew") == sizeof slew) prefs.getBytes("slew", slew, sizeof slew);
  db = prefs.getFloat("db", DEF_DB);
  osc = prefs.getFloat("osc", OSC_DEF);
  prefs.end();
  for (int i = 0; i < NSERVO; i++) {                     // sanea lo que haya guardado
    lim_min[i] = constrain(lim_min[i], HW_MIN[i], HW_MAX[i]);
    lim_max[i] = constrain(lim_max[i], lim_min[i], HW_MAX[i]);
    if (!(slew[i] > 0)) slew[i] = DEF_SLEW[i];
  }
  if (!(osc >= 20e6f && osc <= 30e6f)) osc = OSC_DEF;
}

static void save() {
  prefs.begin("armorca", false);
  prefs.putBytes("min", lim_min, sizeof lim_min);
  prefs.putBytes("max", lim_max, sizeof lim_max);
  prefs.putBytes("slew", slew, sizeof slew);
  prefs.putFloat("db", db);
  prefs.putFloat("osc", osc);
  prefs.end();
}

// ---------------------------------------------------------------- enlace con el host
static float r1(float v) { return roundf(v * 10) / 10; }

// Escribe la línea completa o nada: se salta si ningún host tiene el puerto abierto o
// si falta espacio en TX (una cola HW-CDC llena nunca debe frenar el lazo de control).
static void out(const char *s, size_t n) {
  if (!Serial || Serial.availableForWrite() < (int)n) return;
  Serial.write((const uint8_t *)s, n);
}

static void send(JsonDocument &d) {
  char line[400];
  size_t n = serializeJson(d, line, sizeof line - 1);
  if (!n || n >= sizeof line - 1) return;
  line[n++] = '\n';
  out(line, n);
}

static void sendCfg() {
  JsonDocument d;
  d["t"] = "cfg"; d["fw"] = FW_VERSION;
  for (int i = 0; i < NSERVO; i++) {
    d["min"].add(r1(lim_min[i])); d["max"].add(r1(lim_max[i]));
    d["hwmin"].add(HW_MIN[i]);    d["hwmax"].add(HW_MAX[i]);
    d["slew"].add(r1(slew[i]));
  }
  d["db"] = r1(db); d["cal"] = cal ? 1 : 0;
  d["osc"] = (long)lroundf(osc);
  send(d);
}

static void sendSt() {
  JsonDocument d;
  d["t"] = "st";
  for (int i = 0; i < NSERVO; i++) { d["pos"].add(r1(pos[i])); d["tgt"].add(r1(tgt[i])); d["en"].add(en[i] ? 1 : 0); }
  d["cal"] = cal ? 1 : 0;
  d["vbus"] = vbus < 0 ? -1 : r1(vbus);
  if (homing) {
    float rem = 0;
    for (int i = 0; i < NSERVO; i++) rem = max(rem, fabsf(tgt[i] - pos[i]));
    d["home"] = homeSpan > 0 ? roundf((1 - rem / homeSpan) * 1000) / 1000 : 1;
  }
  send(d);
}

static void err(const char *m) { JsonDocument d; d["t"] = "err"; d["msg"] = m; send(d); }

static void sendHello() {
  JsonDocument h;
  h["t"] = "hello"; h["fw"] = FW_VERSION; h["n"] = NSERVO;
  h["hw"] = "orca"; h["drv"] = "pca9685"; h["pca"] = pcaOk ? 1 : 0;
  for (int i = 0; i < NSERVO; i++) { h["pins"].add(CH[i]); h["ch"].add(CH[i]); }
  send(h);
}

// ---------------------------------------------------------------- lógica de servos (igual que fw 3)
static void setTarget(int i, float a, bool force) {
  a = constrain(a, lo(i), hi(i));
  if (!force && fabsf(a - tgt[i]) < db) return;          // banda muerta / histéresis
  tgt[i] = a; dirty = true;
}

static void warnVbus() {
  static uint32_t warned = 0;
  static bool once = false;
  if (vbus >= 0 && vbus < VBUS_MIN && (!once || millis() - warned > 2000)) {
    once = true; warned = millis(); err("no VBUS: servos unpowered");
  }
}

static void enable(int i, bool on) {
  if (on == en[i]) return;
  // Sin realimentación de posición: al volver el torque, el servo salta desde donde
  // lo dejó la mano hasta `pos`. El host debe poner el objetivo cerca de la pose real antes.
  if (on) { pos[i] = tgt[i] = constrain(tgt[i], lo(i), hi(i)); warnVbus(); }
  en[i] = on; dirty = true;
  writeServo(i);
}

static void handleJson(const char *line) {
  JsonDocument d;
  if (deserializeJson(d, line)) { err("bad json"); return; }

  if (d["stop"].is<int>() && d["stop"]) {              // el E-STOP gana sobre todo
    homing = false;
    for (int i = 0; i < NSERVO; i++) tgt[i] = pos[i];
    dirty = true;
    JsonDocument a; a["t"] = "ok"; a["stop"] = 1; send(a);
    return;
  }
  bool cfgChanged = false;

  if (d["defaults"].is<int>() && d["defaults"]) { defaults(); cfgChanged = true; }
  if (d["cal"].is<int>()) { cal = d["cal"].as<int>() != 0; cfgChanged = true; }
  if (d["lim"].is<JsonObject>()) {
    JsonArray mn = d["lim"]["min"], mx = d["lim"]["max"];
    for (int i = 0; i < NSERVO; i++) {
      float a = mn && !mn[i].isNull() ? mn[i].as<float>() : lim_min[i];
      float b = mx && !mx[i].isNull() ? mx[i].as<float>() : lim_max[i];
      a = constrain(a, HW_MIN[i], HW_MAX[i]); b = constrain(b, HW_MIN[i], HW_MAX[i]);
      if (a > b) { err("min > max"); continue; }
      lim_min[i] = a; lim_max[i] = b;
    }
    cfgChanged = true;
  }
  if (d["slew"].is<JsonArray>()) {
    JsonArray s = d["slew"];
    for (int i = 0; i < NSERVO && i < (int)s.size(); i++)
      if (!s[i].isNull()) slew[i] = constrain(s[i].as<float>(), 1.0f, 600.0f);
    cfgChanged = true;
  }
  if (d["db"].is<float>()) { db = constrain(d["db"].as<float>(), 0.0f, 10.0f); cfgChanged = true; }
  if (d["osc"].is<float>()) {
    float o = d["osc"].as<float>();
    if (o >= 20e6f && o <= 30e6f) { osc = o; pcaOk = pcaInit(); cfgChanged = true; }
    else err("osc out of range (20e6..30e6)");
  }
  if (cfgChanged) for (int i = 0; i < NSERVO; i++) tgt[i] = constrain(tgt[i], lo(i), hi(i));

  // objetivos antes que el torque: {"en":1,"set":[...]} activa directo en la pose nueva
  if (d["set"].is<JsonArray>() && homing) {
    static uint32_t warned = 0;
    if (millis() - warned > 1000) { warned = millis(); err("homing: set ignored (send stop to cancel)"); }
  } else if (d["set"].is<JsonArray>()) {
    JsonArray s = d["set"];
    bool force = d["force"].is<int>() && d["force"];
    for (int i = 0; i < NSERVO && i < (int)s.size(); i++)
      if (!s[i].isNull()) setTarget(i, s[i].as<float>(), force || !en[i]);
  }
  if (d["en"].is<JsonArray>()) {
    JsonArray e = d["en"];
    for (int i = 0; i < NSERVO && i < (int)e.size(); i++) if (!e[i].isNull()) enable(i, e[i].as<int>() != 0);
  } else if (d["en"].is<int>()) {
    for (int i = 0; i < NSERVO; i++) enable(i, d["en"].as<int>() != 0);
  }

  if (d["home"].is<JsonObject>()) {
    JsonArray hp = d["home"]["pos"];
    float speed = d["home"]["speed"].is<float>() ? d["home"]["speed"].as<float>() : HOME_SPEED;
    speed = constrain(speed, 1.0f, 90.0f);
    for (int i = 0; i < NSERVO; i++) enable(i, true);  // los servos limp parten de su último objetivo
    homeSpan = 0;
    for (int i = 0; i < NSERVO; i++) {
      float h = hp && !hp[i].isNull() ? hp[i].as<float>() : BOOT_POS[i];
      tgt[i] = constrain(h, lo(i), hi(i));
      homeSpan = max(homeSpan, fabsf(tgt[i] - pos[i]));
    }
    for (int i = 0; i < NSERVO; i++)                    // mismo tiempo de llegada en todos los ejes
      homeRate[i] = homeSpan > 0 ? max(0.5f, speed * fabsf(tgt[i] - pos[i]) / homeSpan) : speed;
    homing = true; dirty = true;
  }

  if (d["save"].is<int>() && d["save"]) {
    save();
    JsonDocument a; a["t"] = "ok"; a["save"] = 1; send(a);
  }
  if (cfgChanged || (d["get"].is<int>() && d["get"])) { sendCfg(); dirty = true; }
}

static void handleText(const char *line) {
  int n, ang;
  if (sscanf(line, "%d %d", &n, &ang) != 2 || n < 1 || n > NSERVO) { err("use: <servo 1..4> <deg> or JSON"); return; }
  int i = n - 1;
  setTarget(i, ang, true);
  enable(i, true);
  char msg[40];
  int k = snprintf(msg, sizeof msg, "Servo %d -> %.0f grados\n", n, tgt[i]);
  if (k > 0) out(msg, min(k, (int)sizeof msg - 1));
}

// ---------------------------------------------------------------- main
void setup() {
  Serial.setTxBufferSize(2048);
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);                 // nunca bloquear si ningún host lee
  Wire.begin(OB_SDA, OB_SCL, I2C_HZ);
  Wire.setTimeOut(5);                       // ms: un bus colgado no debe frenar el lazo
  load();
  pcaOk = pcaInit();
  inaInit();
  readVbus();
  for (int i = 0; i < NSERVO; i++) {
    pos[i] = tgt[i] = constrain(BOOT_POS[i], lim_min[i], lim_max[i]);
    writeServo(i);                          // limp (FULL_OFF)
  }
  delay(300);
  sendHello();
  if (!pcaOk) err("pca9685 not responding");
  sendCfg();
}

void loop() {
  static char buf[512];
  static size_t len = 0;
  static uint32_t lastTick = 0, lastSt = 0, lastV = 0, lastRetry = 0;
  static bool moving = false;

  while (Serial.available()) {
    int c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      buf[len] = 0;
      if (len) (buf[0] == '{' ? handleJson : handleText)(buf);
      len = 0;
    } else if (len < sizeof buf - 1) buf[len++] = (char)c;
    else len = 0;                           // línea demasiado larga: se descarta
  }

  uint32_t now = millis();
  if (!pcaOk && now - lastRetry >= 1000) {  // PCA9685 perdido (glitch del bus / no responde): reinicializar
    lastRetry = now;
    pcaOk = pcaInit();
    if (pcaOk) sendHello();
  }
  if (now - lastTick >= TICK_MS) {
    float dt = (now - lastTick) / 1000.0f;
    if (dt > 0.1f) dt = 0.1f;
    lastTick = now;
    moving = false;
    for (int i = 0; i < NSERVO; i++) {
      if (en[i] && pos[i] != tgt[i]) {
        float rate = homing ? homeRate[i] : cal ? min(slew[i], CAL_SLEW) : slew[i];
        float step = rate * dt, e = tgt[i] - pos[i];
        pos[i] = fabsf(e) <= step ? tgt[i] : pos[i] + (e > 0 ? step : -step);
        moving = true;
      }
      writeServo(i);
    }
    if (homing && !moving) {
      homing = false; dirty = true;
      JsonDocument a; a["t"] = "ok"; a["home"] = 1; send(a);
    }
  }

  if (now - lastV >= 250) { lastV = now; readVbus(); }

  if (now - lastSt >= (moving || dirty ? 50u : 500u)) {
    lastSt = now; dirty = false;
    sendSt();
  }
}
