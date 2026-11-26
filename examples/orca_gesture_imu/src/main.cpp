/* ============================================================================
 *  ORCA-BOARD Control Rev A — BNO085 (U1) como IMU del brazo por gestos.
 *
 *  Sustituto directo de arm_gesture/firmware (MPU6050, fw 2): mismo protocolo USB
 *  y BLE byte a byte, así que el GUI web, ble_record.py y replay.mjs no cambian.
 *  Solo cambia el nombre BLE: "ORCA" (el GUI filtra por el UUID del servicio).
 *
 *  El BNO085 entrega acelerómetro y giróscopo calibrados (SH-2) a 400 Hz; cada 4
 *  reportes del giróscopo se publica una muestra (100 Hz) con el promedio de ambos,
 *  pasado a las escalas del MPU6050 (+-4 g = 8192 LSB/g, +-500 dps = 65.5 LSB/dps,
 *  saturando igual que él). La fusión sigue en el navegador.
 *
 *  Protocolo serie (líneas de texto; el USB nativo ignora el baud):
 *    I,fw=2,who=0x68,ok=1,acc_lsb=8192,gyr_lsb=65.5,rate=100,sda=18,scl=17,ble=0,imu=BNO085,sw=3.x.y
 *    D,<seq>,<t_us>,<ax>,<ay>,<az>,<temp>,<gx>,<gy>,<gz>                 int16 en escala MPU6050
 *    F,<reason>                                                         falla
 *    S,<i2c_fails>,<max_txn_us>,<polls>,<samples>                       diagnóstico a 1 Hz
 *  Comandos del host (un carácter): 'i' = info, 'r' = reinicia el IMU.
 *    who=0x68 se mantiene para que el GUI no avise "possible clone"; imu=BNO085 dice
 *    qué sensor es. temp: el BNO085 no publica temperatura; va fija a 25 °C
 *    (raw -3920 = (25 - 36.53) * 340), así el modelo de deriva térmica del GUI no actúa.
 *
 *  BLE, nombre "ORCA":
 *    servicio a7e10001-5d2c-4f1a-9b3e-6c0d8e2f4a11
 *    ...0002 DATA notify  uint32 seq del primer registro, uint8 n, n registros de
 *                         uint32 t_us, int16 ax,ay,az,temp,gx,gy,gz   (LE, 18 B c/u;
 *                         n = 5 -> 95 B a 20 Hz, necesita ATT MTU >= 98)
 *    ...0003 LOG  notify  las líneas I / F / S (sin salto de línea)
 *    ...0004 CMD  write   los mismos comandos ('i', 'r')
 *
 *  Placa: BNO085 de Adafruit en el I2C de placa (IO18 SDA / IO17 SCL), 0x4A. INT y RST
 *  no están conectados: se sondea cada 1 ms y el reset es por software (SHTP).
 *  Fallas: sin giróscopo durante 50 ms (1 s tras el init), reset espontáneo del BNO085, init fallido.
 *  Luego reintenta el init cada 500 ms. El resto de la placa (PCA9685 en SLEEP, INA226,
 *  CAN, mux) no se toca.
 * ========================================================================== */
#include <Arduino.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <Adafruit_BNO08x.h>

#define FW_VERSION      2          // versión del protocolo de arm_gesture/firmware
#define I2C_SDA         18         // OB_I2C (ver 05_revision_CAN_I2C_sensores.md)
#define I2C_SCL         17
#define I2C_HZ          400000
#define BNO_ADDR        0x4A
#define OVERSAMPLE      4          // el BNO085 corre a 400 Hz; se promedian 4 -> 100 Hz
#define REPORT_US       (10000 / OVERSAMPLE)

#define ACC_LSB         8192.0f    // escalas del MPU6050 que espera el GUI
#define GYR_LSB         65.5f
#define TEMP_RAW        (-3920)    // 25 °C en la escala del MPU6050

#define DATA_TIMEOUT_MS 50
#define START_TIMEOUT_MS 1000    // el BNO085 tarda en dar el primer reporte tras activarlo
#define REINIT_MS       500

#define BLE_NAME        "ORCA"
#define SVC_UUID        "a7e10001-5d2c-4f1a-9b3e-6c0d8e2f4a11"
#define DATA_UUID       "a7e10002-5d2c-4f1a-9b3e-6c0d8e2f4a11"
#define LOG_UUID        "a7e10003-5d2c-4f1a-9b3e-6c0d8e2f4a11"
#define CMD_UUID        "a7e10004-5d2c-4f1a-9b3e-6c0d8e2f4a11"
#define REC_PER_NOTIFY  5
#define REC_SIZE        18
#define DATA_SIZE       (5 + REC_SIZE * REC_PER_NOTIFY)

static bool     g_ok = false;
static bool     g_open = false;     // SH-2 abierto (hay que cerrarlo antes de reabrir)
static uint32_t g_seq = 0;
static uint32_t g_lastData = 0;
static uint32_t g_lastInit = 0;
static bool     g_haveAcc = false;
static float    g_aSum[3], g_gSum[3];   // promedios del bloque de OVERSAMPLE reportes
static int      g_aN = 0, g_gN = 0;
static uint64_t g_tSum = 0;
static bool     g_streaming = false;  // ya llegó el primer giróscopo desde el init
static int16_t  g_acc[3];
static char     g_sw[16] = "?";

// diagnóstico, una vez por segundo como  S,<i2c_fails>,<max_txn_us>,<polls>,<samples>
static uint32_t d_fails = 0, d_maxUs = 0, d_polls = 0, d_samples = 0, d_last = 0;

// ------------------------------------------------------------- I2C (SHTP) ----
// Lectura SHTP propia en lugar de la de Adafruit: cuenta fallas y tiempos para la
// línea S y marca la hora de cada transferencia (la de Adafruit deja t_us en 0, y
// sin eso las marcas de tiempo de los reportes no sirven).
static bool i2cRead(uint8_t *buf, size_t n) {
  uint32_t t0 = micros();
  size_t got = Wire.requestFrom((int)BNO_ADDR, (int)n);
  size_t i = 0;
  while (i < n && Wire.available()) buf[i++] = Wire.read();
  uint32_t us = micros() - t0;
  if (us > d_maxUs) d_maxUs = us;
  bool ok = got == n && i == n;
  if (!ok) d_fails++;
  return ok;
}

static int halRead(sh2_Hal_t *, uint8_t *buf, unsigned len, uint32_t *t_us) {
  *t_us = micros();
  d_polls++;
  uint8_t hdr[4];
  if (!i2cRead(hdr, 4)) return 0;
  uint16_t size = (hdr[0] | hdr[1] << 8) & 0x7FFF;   // sin el bit de continuación
  if (size == 0 || size > len) return 0;
  // Cada lectura empieza con el encabezado de 4 B; después de la primera se descarta.
  uint8_t chunk[I2C_BUFFER_LENGTH];
  uint16_t left = size;
  bool first = true;
  while (left > 0) {
    size_t n = first ? min((size_t)I2C_BUFFER_LENGTH, (size_t)left)
                     : min((size_t)I2C_BUFFER_LENGTH, (size_t)left + 4);
    if (!i2cRead(chunk, n)) return 0;
    size_t skip = first ? 0 : 4;
    memcpy(buf, chunk + skip, n - skip);
    buf += n - skip;
    left -= n - skip;
    first = false;
  }
  return size;
}

// Da acceso al HAL de la clase para cambiar la lectura después de begin_I2C().
class OrcaBno : public Adafruit_BNO08x {
 public:
  using Adafruit_BNO08x::Adafruit_BNO08x;
  void useHalRead() { _HAL.read = halRead; }
};
static OrcaBno bno(-1);             // sin pin de reset

// ------------------------------------------------------------------- BLE ----
static BLECharacteristic *g_data = nullptr, *g_log = nullptr;
static volatile bool g_bleOn = false;
static volatile uint16_t g_mtu = 23;
static volatile char g_cmd = 0;            // lo pone la tarea BLE, se atiende en loop()
static uint8_t g_pkt[DATA_SIZE];
static int g_pktN = 0;

class ServerCB : public BLEServerCallbacks {
  void onConnect(BLEServer *srv, esp_ble_gatts_cb_param_t *p) override {
    g_bleOn = true; g_pktN = 0;
    // intervalo de 7.5..15 ms: 20 notificaciones/s nunca se encolan
    srv->updateConnParams(p->connect.remote_bda, 6, 12, 0, 400);
  }
  void onDisconnect(BLEServer *) override { g_bleOn = false; BLEDevice::startAdvertising(); }
  void onMtuChanged(BLEServer *, esp_ble_gatts_cb_param_t *p) override { g_mtu = p->mtu.mtu; }
};
class CmdCB : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) override {
    std::string v = c->getValue();
    if (!v.empty()) g_cmd = v[0];
  }
};

static void bleBegin() {
  BLEDevice::init(BLE_NAME);
  BLEDevice::setMTU(247);
  BLEServer *srv = BLEDevice::createServer();
  srv->setCallbacks(new ServerCB());
  BLEService *svc = srv->createService(SVC_UUID);
  g_data = svc->createCharacteristic(DATA_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  g_log  = svc->createCharacteristic(LOG_UUID,  BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
  BLECharacteristic *cmd = svc->createCharacteristic(CMD_UUID,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  g_data->addDescriptor(new BLE2902());      // Bluedroid (core 2.x) necesita el CCCD explícito
  g_log->addDescriptor(new BLE2902());
  cmd->setCallbacks(new CmdCB());
  svc->start();
  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SVC_UUID);
  adv->setScanResponse(true);               // el nombre va en la respuesta al escaneo
  BLEDevice::startAdvertising();
}

// una línea de texto al USB y (si hay conexión) a la característica LOG
static void emit(const char *fmt, ...) {
  char line[128];
  va_list ap; va_start(ap, fmt);
  int n = vsnprintf(line, sizeof line, fmt, ap);
  va_end(ap);
  if (n <= 0) return;
  if (n >= (int)sizeof line) n = sizeof line - 1;
  Serial.write((const uint8_t *)line, n);
  Serial.write('\n');
  if (g_bleOn && g_log) { g_log->setValue((uint8_t *)line, n); g_log->notify(); }
}

static void bleSample(uint32_t seq, uint32_t t, const int16_t *v) {
  if (!g_bleOn) return;
  if (g_pktN == 0) { memcpy(g_pkt, &seq, 4); }
  uint8_t *r = g_pkt + 5 + REC_SIZE * g_pktN;
  memcpy(r, &t, 4);
  memcpy(r + 4, v, 14);                     // ax,ay,az,temp,gx,gy,gz (ESP32 es little-endian)
  if (++g_pktN < REC_PER_NOTIFY) return;
  g_pkt[4] = (uint8_t)g_pktN; g_pktN = 0;
  if (g_mtu < DATA_SIZE + 3) {              // se truncaría: avisarlo (máximo 1/s)
    static uint32_t warned = 0;
    if (millis() - warned > 1000) { warned = millis(); emit("F,mtu%u", (unsigned)g_mtu); }
    return;
  }
  g_data->setValue(g_pkt, DATA_SIZE);
  g_data->notify();
}

// ------------------------------------------------------------------- IMU ----
static void printInfo() {
  emit("I,fw=%d,who=0x68,ok=%d,acc_lsb=8192,gyr_lsb=65.5,rate=100,sda=%d,scl=%d,ble=%d,imu=BNO085,sw=%s",
       FW_VERSION, g_ok ? 1 : 0, I2C_SDA, I2C_SCL, g_bleOn ? 1 : 0, g_sw);
}

static void fault(const char *why) {
  if (g_ok) emit("F,%s", why);
  g_ok = false;
  g_lastInit = millis();
}

static int16_t sat(float x) {
  return x >= 32767.0f ? 32767 : x <= -32768.0f ? -32768 : (int16_t)lroundf(x);
}

static void sample(uint32_t t, const int16_t *gyr) {
  int16_t v[7] = {g_acc[0], g_acc[1], g_acc[2], TEMP_RAW, gyr[0], gyr[1], gyr[2]};
  g_lastData = millis();
  g_streaming = true;
  d_samples++;
  bleSample(g_seq, t, v);
  char line[96];
  int n = snprintf(line, sizeof line, "D,%lu,%lu,%d,%d,%d,%d,%d,%d,%d\n",
                   (unsigned long)g_seq++, (unsigned long)t,
                   v[0], v[1], v[2], v[3], v[4], v[5], v[6]);
  Serial.write((const uint8_t *)line, n);
}

// Llamado dentro de sh2_service() por cada reporte (una transferencia puede traer varios).
static void onSensor(void *, sh2_SensorEvent_t *ev) {
  sh2_SensorValue_t sv;
  if (sh2_decodeSensorEvent(&sv, ev) != SH2_OK) return;
  if (sv.sensorId == SH2_ACCELEROMETER) {
    g_aSum[0] += sv.un.accelerometer.x;
    g_aSum[1] += sv.un.accelerometer.y;
    g_aSum[2] += sv.un.accelerometer.z;
    g_aN++;
  } else if (sv.sensorId == SH2_GYROSCOPE_CALIBRATED && g_ok) {
    g_gSum[0] += sv.un.gyroscope.x;
    g_gSum[1] += sv.un.gyroscope.y;
    g_gSum[2] += sv.un.gyroscope.z;
    g_tSum += sv.timestamp;                // marca del BNO085 en la base de micros()
    if (++g_gN < OVERSAMPLE) return;
    if (g_aN > 0) {                        // m/s^2 -> LSB
      const float k = ACC_LSB / 9.80665f / g_aN;
      for (int i = 0; i < 3; i++) g_acc[i] = sat(g_aSum[i] * k);
      g_haveAcc = true;
    }
    const float k = GYR_LSB * 57.2957795f / OVERSAMPLE;   // rad/s -> LSB
    int16_t gyr[3] = {sat(g_gSum[0] * k), sat(g_gSum[1] * k), sat(g_gSum[2] * k)};
    uint32_t t = (uint32_t)(g_tSum / OVERSAMPLE);         // centro del bloque
    g_aSum[0] = g_aSum[1] = g_aSum[2] = g_gSum[0] = g_gSum[1] = g_gSum[2] = 0;
    g_aN = g_gN = 0; g_tSum = 0;
    if (g_haveAcc) sample(t, gyr);
  }
}

static bool bnoInit() {
  g_lastInit = millis();
  g_ok = false;
  g_haveAcc = false;
  g_streaming = false;
  g_aSum[0] = g_aSum[1] = g_aSum[2] = g_gSum[0] = g_gSum[1] = g_gSum[2] = 0;
  g_aN = g_gN = 0; g_tSum = 0;
  if (g_open) { sh2_close(); g_open = false; }
  if (!bno.begin_I2C(BNO_ADDR, &Wire)) return false;   // reset por software + IDs
  g_open = true;
  bno.useHalRead();
  sh2_setSensorCallback(onSensor, nullptr);           // todos los reportes, no solo el último
  const sh2_ProductId_t &id = bno.prodIds.entry[0];
  snprintf(g_sw, sizeof g_sw, "%u.%u.%u", id.swVersionMajor, id.swVersionMinor, id.swVersionPatch);
  bool ok = sh2_setCalConfig(SH2_CAL_ACCEL | SH2_CAL_GYRO) == SH2_OK;  // calibración dinámica
  ok &= bno.enableReport(SH2_ACCELEROMETER, REPORT_US);
  ok &= bno.enableReport(SH2_GYROSCOPE_CALIBRATED, REPORT_US);
  bno.wasReset();                                      // descarta el reset de este init
  g_ok = ok;
  g_lastData = millis();
  printInfo();
  return ok;
}

static void command(int c) {
  if (c == 'i') printInfo();
  else if (c == 'r') { g_ok = false; bnoInit(); }
}

void setup() {
  Serial.setTxBufferSize(4096);
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);                 // nunca bloquear si nadie lee el USB
  Wire.begin(I2C_SDA, I2C_SCL, I2C_HZ);
  delay(200);
  bleBegin();
  if (!bnoInit()) emit("F,init");
}

void loop() {
  while (Serial.available()) command(Serial.read());
  if (g_cmd) { char c = g_cmd; g_cmd = 0; command(c); }

  if (millis() - d_last >= 1000) {
    d_last = millis();
    emit("S,%lu,%lu,%lu,%lu", (unsigned long)d_fails, (unsigned long)d_maxUs,
         (unsigned long)d_polls, (unsigned long)d_samples);
    d_fails = d_maxUs = d_polls = d_samples = 0;
  }

  if (!g_ok) {
    if (millis() - g_lastInit >= REINIT_MS && !bnoInit()) emit("F,init");
    delay(1);
    return;
  }

  uint32_t before = d_samples;
  sh2_service();                            // una transferencia SHTP -> onSensor()
  if (bno.wasReset()) { fault("reset"); return; }
  if (millis() - g_lastData > (g_streaming ? DATA_TIMEOUT_MS : START_TIMEOUT_MS)) { fault("timeout"); return; }
  if (d_samples == before) delay(1);        // sondeo cada 1 ms -> <= 1 ms de latencia
}
