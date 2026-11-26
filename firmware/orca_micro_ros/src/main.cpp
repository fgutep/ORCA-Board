// ORCA-BOARD Control Rev A — nodo micro-ROS /orca_board (rclc, ROS 2 Jazzy). Firmware de uso general.
//
// Documentación completa (tópicos, parámetros, seguridad, compilación, pruebas): README.md
//
// Transporte (elegido al compilar, ver platformio.ini):
//   orca_usb  -> USB-CDC nativo (Serial). El USB queda ocupado por XRCE-DDS: sin consola de texto.
//   orca_wifi -> UDP por Wi-Fi (estación) hacia un agente en ORCA_AGENT_IP:ORCA_AGENT_PORT.
//
// Interfaz ROS 2:
//   pub /battery_state  sensor_msgs/BatteryState        2 Hz
//   pub /imu            sensor_msgs/Imu                 imu.rate_hz, best effort
//   pub /diagnostics    diagnostic_msgs/DiagnosticArray 1 Hz (power, motors, servos, imu, system)
//   sub /cmd_vel        geometry_msgs/Twist             tracción diferencial en lazo abierto (drive.*)
//   sub /motor_cmd      std_msgs/Float32MultiArray      [M1, M2] en % de duty, -100..100 (sin cinemática)
//   sub /servo_cmd      std_msgs/Float32MultiArray      grados 0..180 por canal del PCA9685; NaN o < 0 = sin torque
//   sub /orca/estop     std_msgs/Bool                   true = HALT enclavado, false = liberar
//   pub /can_rx         can_msgs/Frame                  tramas recibidas por J2 (TWAI, can.bitrate = 25 kbit/s)
//   sub /can_tx         can_msgs/Frame                  tramas a transmitir por J2
//   parámetros          rclc parameter server; cada cambio aceptado se guarda en NVS y sobrevive al reinicio
//
// Seguridad: drivers dormidos y servos sin torque al arrancar; deadman de comandos; motores libres sin agente;
// HALT enclavado por estop o sobrecorriente. Watchdog del loop (5 s): un cuelgue reinicia la placa.

#include <Adafruit_BNO08x.h>
#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>
#include <driver/ledc.h>
#include <driver/twai.h>
#include <micro_ros_platformio.h>

#include <can_msgs/msg/frame.h>
#include <diagnostic_msgs/msg/diagnostic_array.h>
#include <geometry_msgs/msg/twist.h>
#include <rcl/rcl.h>
#include <rclc/executor.h>
#include <rclc/rclc.h>
#include <rclc_parameter/rclc_parameter.h>
#include <rmw_microros/rmw_microros.h>
#include <sensor_msgs/msg/battery_state.h>
#include <sensor_msgs/msg/imu.h>
#include <std_msgs/msg/bool.h>
#include <std_msgs/msg/float32_multi_array.h>

// ---------- opciones de compilación ----------

#define ORCA_FW_VERSION "1.1.0"
#ifndef ORCA_ROS_DOMAIN_ID
#define ORCA_ROS_DOMAIN_ID 0
#endif
#ifndef ORCA_NAMESPACE
#define ORCA_NAMESPACE ""
#endif
#ifdef ORCA_TRANSPORT_WIFI
static_assert(sizeof(ORCA_WIFI_SSID) > 1, "Definir ORCA_WIFI_SSID (variable de entorno, ver README)");
static_assert(sizeof(ORCA_AGENT_IP) > 1, "Definir ORCA_AGENT_IP (variable de entorno, ver README)");
#ifndef ORCA_AGENT_PORT
#define ORCA_AGENT_PORT 8888
#endif
constexpr char TRANSPORT_NAME[] = "wifi-udp";
#else
constexpr char TRANSPORT_NAME[] = "usb-serial";
#endif

constexpr char NODE_NAME[] = "orca_board";
constexpr char IMU_FRAME[] = "imu_link";
constexpr char BASE_FRAME[] = "base_link";
constexpr uint32_t BATTERY_PERIOD_MS = 500, DIAG_PERIOD_MS = 1000, TIME_SYNC_PERIOD_MS = 60000;

// ---------- hardware (netlist de ORCA-BOARD.kicad_sch, ver Hardware/physical_debug) ----------

constexpr int OB_SDA = 18, OB_SCL = 17;  // I2C de placa
constexpr uint32_t I2C_HZ = 400000;
constexpr int ISENS_PIN = 9;             // ACS725 VIOUT: corriente total de VM (no hay medida por canal)
constexpr uint8_t ADDR_INA226 = 0x40, ADDR_PCA = 0x41, ADDR_BNO = 0x4A;
constexpr float ISENS_NOMINAL_ZERO_MV = 330.0f, ISENS_MV_PER_A = 264.0f;
constexpr gpio_num_t CAN_TX = GPIO_NUM_6, CAN_RX = GPIO_NUM_7;  // U7 SN65HVD230 -> J2 (R15 = 120 Ω fija)
constexpr int NSERVO = 16;
constexpr float SERVO_HZ = 50.0f, MIN_DUTY = 1.0f;

// ---------- parámetros (ROS 2 + NVS) ----------

enum Param : uint8_t {
  P_I_LIMIT, P_RAMP, P_MAX_DUTY, P_CMD_TIMEOUT, P_IDLE_COAST, P_PWM_HZ, P_INV_M1, P_INV_M2,
  P_WHEEL_SEP, P_MAX_WHEEL_SPEED, P_LEFT_IS_M1,
  P_SERVO_US_MIN, P_SERVO_US_MAX, P_PCA_OSC, P_SERVO_LIMP,
  P_IMU_RATE, P_VBUS_LOW,
  P_CAN_ENABLED, P_CAN_BITRATE, P_CAN_SELF_TEST,
  P_COUNT
};
struct ParamDef {
  const char *name, *key;  // key: clave NVS (máx. 15 caracteres)
  rclc_parameter_type_t type;
  double def, lo, hi;
  const char *desc;        // máx. 99 caracteres (RCLC_PARAMETER_MAX_STRING_LENGTH en colcon.meta)
};
constexpr auto B = RCLC_PARAMETER_BOOL, I = RCLC_PARAMETER_INT, D = RCLC_PARAMETER_DOUBLE;
const ParamDef PARAMS[P_COUNT] = {
    {"motor.i_limit_a", "ilim", D, 2.0, 0.1, 3.0, "HALT if the 100 ms mean of total VM current exceeds this [A]"},
    {"motor.ramp_rate", "ramp", D, 200, 0, 10000, "Duty slew rate [%/s]; 0 = no ramp"},
    {"motor.max_duty", "maxduty", D, 100, 0, 100, "Duty clamp for both motors [%]"},
    {"motor.cmd_timeout_ms", "cmdto", I, 500, 50, 5000, "Deadman: targets go to 0 if no cmd_vel/motor_cmd arrives [ms]"},
    {"motor.idle_coast_ms", "idlecoast", I, 1000, 0, 60000, "Time at 0 duty (brake) before the drivers sleep [ms]"},
    {"motor.pwm_hz", "pwmhz", I, 20000, 1000, 75000, "DRV8874 PWM frequency [Hz]"},
    {"motor.invert_m1", "invm1", B, 0, 0, 1, "Reverse M1 (J3) polarity"},
    {"motor.invert_m2", "invm2", B, 0, 0, 1, "Reverse M2 (J4) polarity"},
    {"drive.wheel_separation", "wsep", D, 0.20, 0.01, 5.0, "cmd_vel: distance between wheel contact points [m]"},
    {"drive.max_wheel_speed", "wmax", D, 0.50, 0.01, 20.0, "cmd_vel: wheel speed at 100 % duty, open loop [m/s]"},
    {"drive.left_is_m1", "leftm1", B, 1, 0, 1, "cmd_vel: true = left wheel on M1 (J3), false = left on M2 (J4)"},
    {"servo.us_min", "usmin", I, 500, 200, 3000, "servo_cmd: pulse width at 0 deg [us]"},
    {"servo.us_max", "usmax", I, 2400, 200, 3000, "servo_cmd: pulse width at 180 deg [us]"},
    {"servo.pca_osc_hz", "pcaosc", I, 25000000, 20000000, 30000000, "PCA9685 oscillator calibration [Hz]"},
    {"servo.limp_on_disconnect", "servolimp", B, 1, 0, 1, "Remove servo torque when the agent is lost"},
    {"imu.rate_hz", "imuhz", I, 50, 1, 200, "BNO085 report and /imu publish rate [Hz]"},
    {"power.vbus_low_v", "vbuslow", D, 6.0, 0, 60, "Diagnostics WARN below this VBUS [V]"},
    {"can.enabled", "canen", B, 1, 0, 1, "Run the TWAI (CAN) controller on J2"},
    {"can.bitrate", "canbr", I, 25000, 25000, 1000000, "CAN bitrate [bit/s]: 25k, 50k, 100k, 125k, 250k, 500k, 800k or 1M"},
    {"can.self_test", "canself", B, 0, 0, 1, "No-ACK mode with self reception: /can_tx frames come back on /can_rx"},
};
double cfg[P_COUNT];
Preferences prefs;

// ---------- estado ----------

struct Motor {
  int pinEn, pinPh, pinSleep;
  ledc_channel_t ledc;
  Param invert;
  float target, duty;  // % -100..100 antes de la inversión de polaridad
  bool awake;
};
Motor motors[2] = {
    {16, 15, 14, LEDC_CHANNEL_0, P_INV_M1, 0, 0, false},  // M1 (U5, J3)
    {12, 13, 11, LEDC_CHANNEL_1, P_INV_M2, 0, 0, false},  // M2 (U6, J4)
};
volatile bool halted = false;
char haltReason[64] = "";
uint32_t lastCmdMs = 0, idleSinceMs = 0;
bool cmdActive = false;
const char *cmdSource = "none";

volatile float iLimit = 2.0f, isensZeroMv = ISENS_NOMINAL_ZERO_MV;
volatile float iMean = 0;  // A, media de 100 ms
float vbus = NAN;
bool inaOk = false, pcaOk = false, bnoOk = false;
float pcaUsPerTick = 1e6f / (SERVO_HZ * 4096.0f);
float servoDeg[NSERVO];    // NaN = sin torque
int servoTicks[NSERVO];    // último valor escrito; 0 = FULL_OFF, -1 = desconocido

enum class Agent { Waiting, Available, Connected, Disconnected };
Agent agent = Agent::Waiting;
uint32_t connections = 0, imuPublished = 0, imuRateHz = 0, bnoInits = 0;
esp_reset_reason_t resetReason;
char hardwareId[24];

// ---------- utilidades ----------

bool every(uint32_t &last, uint32_t ms) {
  if (millis() - last < ms) return false;
  last = millis();
  return true;
}

bool i2cWrite(uint8_t addr, uint8_t reg, const uint8_t *d, size_t n) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(d, n);
  return Wire.endTransmission() == 0;
}
bool i2cWrite8(uint8_t addr, uint8_t reg, uint8_t v) { return i2cWrite(addr, reg, &v, 1); }

const char *resetName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXT";
    case ESP_RST_SW: return "SW";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "WDT";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    default: return "OTHER";
  }
}

// ---------- motores (DRV8874 en modo PH/EN: EN = PWM, PH = dirección, EN = 0 -> freno, nSLEEP bajo -> libre) ----------

void applyOutputs(Motor &m) {
  constexpr uint32_t FULL = 1u << 10;
  uint32_t raw = min<uint32_t>(lroundf(fabsf(m.duty) / 100.0f * FULL), FULL);
  bool fwd = (m.duty > 0) != (cfg[m.invert] != 0);
  digitalWrite(m.pinPh, fwd ? HIGH : LOW);
  ledc_set_duty(LEDC_LOW_SPEED_MODE, m.ledc, raw);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, m.ledc);
}

void coast(Motor &m) {
  m.target = m.duty = 0;
  applyOutputs(m);
  digitalWrite(m.pinSleep, LOW);
  m.awake = false;
}

void wake(Motor &m) {
  if (m.awake || halted) return;
  m.duty = 0;
  applyOutputs(m);
  digitalWrite(m.pinSleep, HIGH);
  delay(2);  // tWAKE máx. 1 ms
  m.awake = true;
}

void coastAll() {
  for (auto &m : motors) coast(m);
  cmdActive = false;
}

void motorsSetup() {
  for (auto &m : motors) {  // el pull-up de nSLEEP los despertó al encender
    pinMode(m.pinSleep, OUTPUT);
    digitalWrite(m.pinSleep, LOW);
    pinMode(m.pinPh, OUTPUT);
    digitalWrite(m.pinPh, LOW);
  }
  ledc_timer_config_t tc = {};
  tc.speed_mode = LEDC_LOW_SPEED_MODE;
  tc.duty_resolution = LEDC_TIMER_10_BIT;
  tc.timer_num = LEDC_TIMER_1;
  tc.freq_hz = (uint32_t)cfg[P_PWM_HZ];
  tc.clk_cfg = LEDC_AUTO_CLK;
  ledc_timer_config(&tc);
  for (auto &m : motors) {
    ledc_channel_config_t cc = {};
    cc.gpio_num = m.pinEn;
    cc.speed_mode = LEDC_LOW_SPEED_MODE;
    cc.channel = m.ledc;
    cc.timer_sel = LEDC_TIMER_1;
    cc.duty = 0;
    ledc_channel_config(&cc);
  }
}

// Consignas en % (M1, M2), desde /motor_cmd o /cmd_vel.
void setTargets(float d1, float d2, const char *source) {
  if (halted) return;
  float d[2] = {d1, d2};
  float maxDuty = cfg[P_MAX_DUTY];
  for (int i = 0; i < 2; i++) {
    float v = isnan(d[i]) ? 0 : constrain(d[i], -maxDuty, maxDuty);
    if (fabsf(v) < MIN_DUTY) v = 0;
    if (v != 0) wake(motors[i]);
    motors[i].target = v;
  }
  lastCmdMs = millis();
  cmdActive = true;
  cmdSource = source;
}

// Rampa, deadman y paso a libre. Cada ~5 ms desde loop().
void controlStep(float dt) {
  uint32_t now = millis();
  if (halted) {
    if (motors[0].awake || motors[1].awake) coastAll();
    return;
  }
  if (cmdActive && now - lastCmdMs > (uint32_t)cfg[P_CMD_TIMEOUT]) {
    motors[0].target = motors[1].target = 0;
    cmdActive = false;
  }
  bool idle = true;
  float ramp = cfg[P_RAMP];
  for (auto &m : motors) {
    if (!m.awake) continue;
    float t = m.target;
    if (ramp <= 0) m.duty = t;
    else {
      if (m.duty * t < 0) t = 0;  // al invertir, primero bajar a 0
      float step = ramp * dt;
      m.duty += constrain(t - m.duty, -step, step);
    }
    applyOutputs(m);
    if (m.duty != 0 || m.target != 0) idle = false;
  }
  if (!idle) idleSinceMs = now;
  else if (now - idleSinceMs > (uint32_t)cfg[P_IDLE_COAST] && (motors[0].awake || motors[1].awake)) coastAll();
}

// Se llama también desde la tarea del ACS725: solo bandera y nSLEEP; controlStep() limpia el resto.
void halt(const char *reason) {
  if (!halted) snprintf(haltReason, sizeof haltReason, "%s", reason);
  halted = true;
  for (auto &m : motors) gpio_set_level((gpio_num_t)m.pinSleep, 0);
}

// ---------- medidas ----------

void samplerTask(void *) {  // ACS725: ~4 kmuestras/s con fase aleatoria respecto al PWM -> media real
  TickType_t last = xTaskGetTickCount();
  float sum = 0;
  int n = 0;
  for (;;) {
    for (int i = 0; i < 4; i++) {
      delayMicroseconds(esp_random() % 50);
      sum += (analogReadMilliVolts(ISENS_PIN) - isensZeroMv) / ISENS_MV_PER_A;
      n++;
    }
    if (n >= 400) {  // 100 ms
      iMean = sum / n;
      sum = 0;
      n = 0;
      if (iMean > iLimit && !halted) {
        char r[64];
        snprintf(r, sizeof r, "overcurrent %.2f A > %.2f A", (float)iMean, (float)iLimit);
        halt(r);
      }
    }
    vTaskDelayUntil(&last, 1);
  }
}

void calibrateZero() {  // con los drivers dormidos; fuera de 250–450 mV se deja el nominal
  delay(50);
  uint32_t sum = 0;
  for (int i = 0; i < 512; i++) sum += analogReadMilliVolts(ISENS_PIN);
  float mv = sum / 512.0f;
  if (mv >= 250 && mv <= 450) isensZeroMv = mv;
}

void readVbus() {
  Wire.beginTransmission(ADDR_INA226);
  Wire.write(0x02);  // bus voltage, 1.25 mV/LSB
  inaOk = Wire.endTransmission(false) == 0 && Wire.requestFrom(ADDR_INA226, (uint8_t)2) == 2;
  vbus = inaOk ? ((Wire.read() << 8) | Wire.read()) * 0.00125f : NAN;
}

// ---------- servos (PCA9685, ~OE a GND: sin torque = FULL_OFF por canal) ----------

bool pcaInit() {
  double osc = cfg[P_PCA_OSC];
  int pre = constrain((int)lround(osc / (4096.0 * SERVO_HZ)) - 1, 3, 255);
  pcaUsPerTick = 1e6 * (pre + 1) / osc;
  bool ok = i2cWrite8(ADDR_PCA, 0x00, 0x10)           // SLEEP: PRESCALE escribible
            && i2cWrite8(ADDR_PCA, 0xFE, (uint8_t)pre)
            && i2cWrite8(ADDR_PCA, 0x01, 0x04)        // OUTDRV totem-pole
            && i2cWrite8(ADDR_PCA, 0x00, 0x20);       // despierta, autoincremento
  delayMicroseconds(600);
  if (ok) i2cWrite8(ADDR_PCA, 0x00, 0xA0);            // RESTART
  for (auto &t : servoTicks) t = -1;
  return ok;
}

void servoWriteTicks(int ch, int ticks) {
  if (!pcaOk || servoTicks[ch] == ticks) return;
  uint8_t b[4] = {0, 0, 0, 0};
  if (ticks <= 0) b[3] = 0x10;  // FULL_OFF
  else {
    b[2] = ticks & 0xFF;
    b[3] = (ticks >> 8) & 0x0F;
  }
  if (i2cWrite(ADDR_PCA, 0x06 + 4 * ch, b, 4)) servoTicks[ch] = ticks;
  else pcaOk = false;  // loop() reintenta el init
}

void servoApply(int ch) {
  float d = servoDeg[ch];
  if (isnan(d)) return servoWriteTicks(ch, 0);
  float us = cfg[P_SERVO_US_MIN] + d * (cfg[P_SERVO_US_MAX] - cfg[P_SERVO_US_MIN]) / 180.0f;
  servoWriteTicks(ch, constrain((int)lroundf(us / pcaUsPerTick), 1, 4095));
}

void servosApplyAll() {
  for (int i = 0; i < NSERVO; i++) servoApply(i);
}

void servosLimp() {
  for (int i = 0; i < NSERVO; i++) {
    servoDeg[i] = NAN;
    servoWriteTicks(i, 0);
  }
}

// ---------- IMU (BNO085: INT y RST sin conectar, sondeo y reset por software) ----------

// Lectura SHTP por trozos de I2C_BUFFER_LENGTH (la de Adafruit no maneja paquetes grandes en el ESP32).
bool shtpRead(uint8_t *buf, size_t n) {
  size_t got = Wire.requestFrom((int)ADDR_BNO, (int)n);
  size_t i = 0;
  while (i < n && Wire.available()) buf[i++] = Wire.read();
  return got == n && i == n;
}
int halRead(sh2_Hal_t *, uint8_t *buf, unsigned len, uint32_t *t_us) {
  *t_us = micros();
  uint8_t hdr[4];
  if (!shtpRead(hdr, 4)) return 0;
  uint16_t size = (hdr[0] | hdr[1] << 8) & 0x7FFF;
  if (size == 0 || size > len) return 0;
  uint8_t chunk[I2C_BUFFER_LENGTH];
  uint16_t left = size;
  bool first = true;
  while (left > 0) {  // cada lectura repite el encabezado de 4 B; después de la primera se descarta
    size_t n = min((size_t)I2C_BUFFER_LENGTH, (size_t)left + (first ? 0 : 4));
    if (!shtpRead(chunk, n)) return 0;
    size_t skip = first ? 0 : 4;
    memcpy(buf, chunk + skip, n - skip);
    buf += n - skip;
    left -= n - skip;
    first = false;
  }
  return size;
}

class OrcaBno : public Adafruit_BNO08x {
 public:
  using Adafruit_BNO08x::Adafruit_BNO08x;
  void useHalRead() { _HAL.read = halRead; }
};
OrcaBno bno(-1);
bool bnoOpen = false, haveQuat = false;
float quat[4] = {1, 0, 0, 0}, accel[3], gyro[3];  // w x y z; m/s²; rad/s
uint32_t bnoLastData = 0, bnoLastInit = 0;

void onSensor(void *, sh2_SensorEvent_t *ev) {
  sh2_SensorValue_t v;
  if (sh2_decodeSensorEvent(&v, ev) != SH2_OK) return;
  bnoLastData = millis();
  switch (v.sensorId) {
    case SH2_GAME_ROTATION_VECTOR:
      quat[0] = v.un.gameRotationVector.real;
      quat[1] = v.un.gameRotationVector.i;
      quat[2] = v.un.gameRotationVector.j;
      quat[3] = v.un.gameRotationVector.k;
      haveQuat = true;
      break;
    case SH2_ACCELEROMETER:
      accel[0] = v.un.accelerometer.x, accel[1] = v.un.accelerometer.y, accel[2] = v.un.accelerometer.z;
      break;
    case SH2_GYROSCOPE_CALIBRATED:
      gyro[0] = v.un.gyroscope.x, gyro[1] = v.un.gyroscope.y, gyro[2] = v.un.gyroscope.z;
      break;
  }
}

bool bnoInit() {
  bnoLastInit = millis();
  bnoOk = false;
  bnoInits++;
  if (bnoOpen) {
    sh2_close();
    bnoOpen = false;
  }
  if (!bno.begin_I2C(ADDR_BNO, &Wire)) return false;
  bnoOpen = true;
  bno.useHalRead();
  sh2_setSensorCallback(onSensor, nullptr);  // todos los reportes de cada paquete, no solo el último
  uint32_t periodUs = 1000000UL / (uint32_t)cfg[P_IMU_RATE];
  bool ok = bno.enableReport(SH2_GAME_ROTATION_VECTOR, periodUs);
  ok &= bno.enableReport(SH2_ACCELEROMETER, periodUs);
  ok &= bno.enableReport(SH2_GYROSCOPE_CALIBRATED, periodUs);
  bno.wasReset();
  bnoLastData = millis();
  return bnoOk = ok;
}

// ---------- CAN (TWAI del ESP32-S3 + SN65HVD230) ----------

bool canInstalled = false;
uint32_t canRxFrames = 0, canTxFrames = 0, canTxFailed = 0, canRxUnsent = 0, tCanRecover = 0;
twai_status_info_t canStatus = {};

#define CAN_RATE(br, MACRO)                \
  case br: {                               \
    twai_timing_config_t c = MACRO();      \
    t = c;                                 \
    return true;                           \
  }
bool canTiming(uint32_t bitrate, twai_timing_config_t &t) {
  switch (bitrate) {
    CAN_RATE(25000, TWAI_TIMING_CONFIG_25KBITS)
    CAN_RATE(50000, TWAI_TIMING_CONFIG_50KBITS)
    CAN_RATE(100000, TWAI_TIMING_CONFIG_100KBITS)
    CAN_RATE(125000, TWAI_TIMING_CONFIG_125KBITS)
    CAN_RATE(250000, TWAI_TIMING_CONFIG_250KBITS)
    CAN_RATE(500000, TWAI_TIMING_CONFIG_500KBITS)
    CAN_RATE(800000, TWAI_TIMING_CONFIG_800KBITS)
    CAN_RATE(1000000, TWAI_TIMING_CONFIG_1MBITS)
  }
  return false;
}

void canStop() {
  if (!canInstalled) return;
  twai_stop();
  twai_driver_uninstall();
  canInstalled = false;
}

// (Re)instala el driver con los parámetros can.*. Normal: hace falta al menos otro nodo que dé ACK.
void canStart() {
  canStop();
  memset(&canStatus, 0, sizeof canStatus);
  if (!cfg[P_CAN_ENABLED]) return;
  twai_timing_config_t t;
  if (!canTiming((uint32_t)cfg[P_CAN_BITRATE], t)) return;
  twai_general_config_t g =
      TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX, CAN_RX, cfg[P_CAN_SELF_TEST] ? TWAI_MODE_NO_ACK : TWAI_MODE_NORMAL);
  g.rx_queue_len = 32;
  g.tx_queue_len = 16;
  twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  if (twai_driver_install(&g, &t, &f) != ESP_OK) return;
  canInstalled = twai_start() == ESP_OK;
  if (!canInstalled) twai_driver_uninstall();
}

// ---------- parámetros: carga, validación y aplicación ----------

int findParam(const char *name) {
  for (int i = 0; i < P_COUNT; i++)
    if (!strcmp(PARAMS[i].name, name)) return i;
  return -1;
}

bool validParam(int i, double v) {
  const ParamDef &p = PARAMS[i];
  if (isnan(v) || v < p.lo || v > p.hi) return false;
  if (i == P_SERVO_US_MIN) return v + 100 <= cfg[P_SERVO_US_MAX];
  if (i == P_SERVO_US_MAX) return v >= cfg[P_SERVO_US_MIN] + 100;
  if (i == P_CAN_BITRATE) {
    twai_timing_config_t t;
    return canTiming((uint32_t)v, t);
  }
  return true;
}

void loadConfig() {
  prefs.begin("orca_ros", false);
  for (int i = 0; i < P_COUNT; i++) cfg[i] = PARAMS[i].def;
  for (int i = 0; i < P_COUNT; i++) {
    double v = prefs.getDouble(PARAMS[i].key, PARAMS[i].def);
    if (validParam(i, v)) cfg[i] = v;
  }
  iLimit = cfg[P_I_LIMIT];
}

void applyParam(int i) {
  switch (i) {
    case P_I_LIMIT: iLimit = cfg[i]; break;
    case P_PWM_HZ: ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_1, (uint32_t)cfg[i]); break;
    case P_INV_M1:
    case P_INV_M2:
    case P_LEFT_IS_M1: coastAll(); break;  // nunca cambiar el sentido con el motor andando
    case P_PCA_OSC:
      if ((pcaOk = pcaInit())) servosApplyAll();
      break;
    case P_SERVO_US_MIN:
    case P_SERVO_US_MAX: servosApplyAll(); break;
    case P_IMU_RATE: bnoInit(); break;
    case P_CAN_ENABLED:
    case P_CAN_BITRATE:
    case P_CAN_SELF_TEST: canStart(); break;
  }
}

// Llamado por el parameter server (ros2 param set/load, rqt_reconfigure). false = rechazado.
bool onParam(const Parameter *oldp, const Parameter *newp, void *) {
  if (!oldp || !newp) return false;  // ni borrar ni crear parámetros
  int i = findParam(newp->name.data);
  if (i < 0 || newp->value.type != PARAMS[i].type) return false;
  double v;
  switch (newp->value.type) {
    case RCLC_PARAMETER_BOOL: v = newp->value.bool_value; break;
    case RCLC_PARAMETER_INT: v = (double)newp->value.integer_value; break;
    case RCLC_PARAMETER_DOUBLE: v = newp->value.double_value; break;
    default: return false;
  }
  if (!validParam(i, v)) return false;
  cfg[i] = v;
  prefs.putDouble(PARAMS[i].key, v);
  applyParam(i);
  return true;
}

// ---------- micro-ROS ----------

rclc_support_t support;
rcl_allocator_t allocator;
rcl_init_options_t initOptions;
rcl_node_t node;
rclc_executor_t executor;
rclc_parameter_server_t paramServer;
rcl_publisher_t pubBattery, pubImu, pubDiag, pubCan;
rcl_subscription_t subCmdVel, subMotor, subServo, subEstop, subCanTx;
rcl_timer_t timerBattery, timerDiag;

sensor_msgs__msg__BatteryState msgBattery;
sensor_msgs__msg__Imu msgImu;
geometry_msgs__msg__Twist msgCmdVel;
std_msgs__msg__Float32MultiArray msgMotor, msgServo;
std_msgs__msg__Bool msgEstop;
can_msgs__msg__Frame msgCanRx, msgCanTx;

// Memoria estática para los arreglos entrantes (sin malloc por mensaje)
struct ArrayBuf {
  float data[NSERVO + 8];
  std_msgs__msg__MultiArrayDimension dims[2];
  char labels[2][16];
};
ArrayBuf motorBuf, servoBuf;

// /diagnostics: 5 estados con hasta 8 pares clave-valor cada uno
constexpr int NDIAG = 6, NKV = 8;
struct DiagBuf {
  diagnostic_msgs__msg__KeyValue kv[NKV];
  char key[NKV][24], val[NKV][40];
  char message[64];
};
diagnostic_msgs__msg__DiagnosticArray msgDiag;
diagnostic_msgs__msg__DiagnosticStatus diagStatus[NDIAG];
DiagBuf diagBuf[NDIAG];

#define RCCHECK(fn) \
  if ((fn) != RCL_RET_OK) return false

void setString(rosidl_runtime_c__String &s, const char *v) {
  s.data = (char *)v;
  s.size = strlen(v);
  s.capacity = s.size + 1;
}

void stamp(builtin_interfaces__msg__Time &t) {
  int64_t ns = rmw_uros_epoch_synchronized() ? rmw_uros_epoch_nanos() : (int64_t)esp_timer_get_time() * 1000;
  t.sec = ns / 1000000000LL;
  t.nanosec = ns % 1000000000LL;
}

void initArrayMsg(std_msgs__msg__Float32MultiArray &m, ArrayBuf &b) {
  memset(&m, 0, sizeof m);
  m.data.data = b.data;
  m.data.capacity = sizeof b.data / sizeof b.data[0];
  for (int i = 0; i < 2; i++) {
    b.dims[i].label.data = b.labels[i];
    b.dims[i].label.capacity = sizeof b.labels[i];
  }
  m.layout.dim.data = b.dims;
  m.layout.dim.capacity = 2;
}

void initMessages() {
  memset(&msgBattery, 0, sizeof msgBattery);
  setString(msgBattery.header.frame_id, BASE_FRAME);
  setString(msgBattery.location, "J20");
  setString(msgBattery.serial_number, "");
  msgBattery.temperature = msgBattery.charge = msgBattery.capacity = msgBattery.design_capacity = NAN;
  msgBattery.percentage = NAN;
  msgBattery.power_supply_status = sensor_msgs__msg__BatteryState__POWER_SUPPLY_STATUS_UNKNOWN;
  msgBattery.power_supply_health = sensor_msgs__msg__BatteryState__POWER_SUPPLY_HEALTH_UNKNOWN;
  msgBattery.power_supply_technology = sensor_msgs__msg__BatteryState__POWER_SUPPLY_TECHNOLOGY_UNKNOWN;

  memset(&msgImu, 0, sizeof msgImu);
  setString(msgImu.header.frame_id, IMU_FRAME);
  for (int i : {0, 4, 8}) {  // aproximadas (hoja de datos del BNO085, orientación sin magnetómetro)
    msgImu.orientation_covariance[i] = 0.0012;  // ~2°
    msgImu.angular_velocity_covariance[i] = 0.0003;
    msgImu.linear_acceleration_covariance[i] = 0.01;
  }

  initArrayMsg(msgMotor, motorBuf);
  initArrayMsg(msgServo, servoBuf);

  memset(&msgCanRx, 0, sizeof msgCanRx);
  setString(msgCanRx.header.frame_id, "can");
  memset(&msgCanTx, 0, sizeof msgCanTx);
  static char canTxFrame[16];  // frame_id entrante
  msgCanTx.header.frame_id.data = canTxFrame;
  msgCanTx.header.frame_id.capacity = sizeof canTxFrame;

  static const char *names[NDIAG] = {"orca_board: power", "orca_board: motors", "orca_board: servos",
                                     "orca_board: imu", "orca_board: system", "orca_board: can"};
  memset(&msgDiag, 0, sizeof msgDiag);
  msgDiag.status.data = diagStatus;
  msgDiag.status.size = msgDiag.status.capacity = NDIAG;
  for (int s = 0; s < NDIAG; s++) {
    auto &st = diagStatus[s];
    auto &b = diagBuf[s];
    memset(&st, 0, sizeof st);
    setString(st.name, names[s]);
    setString(st.hardware_id, hardwareId);
    st.message.data = b.message;
    st.message.capacity = sizeof b.message;
    for (int k = 0; k < NKV; k++) {
      b.kv[k].key.data = b.key[k];
      b.kv[k].key.capacity = sizeof b.key[k];
      b.kv[k].value.data = b.val[k];
      b.kv[k].value.capacity = sizeof b.val[k];
    }
    st.values.data = b.kv;
    st.values.capacity = NKV;
  }
}

// ----- callbacks -----

void onCmdVel(const void *msg) {
  auto *m = (const geometry_msgs__msg__Twist *)msg;
  float half = m->angular.z * cfg[P_WHEEL_SEP] / 2;
  float left = (m->linear.x - half) / cfg[P_MAX_WHEEL_SPEED] * 100.0f;
  float right = (m->linear.x + half) / cfg[P_MAX_WHEEL_SPEED] * 100.0f;
  if (cfg[P_LEFT_IS_M1]) setTargets(left, right, "cmd_vel");
  else setTargets(right, left, "cmd_vel");
}

void onMotor(const void *msg) {
  auto *m = (const std_msgs__msg__Float32MultiArray *)msg;
  if (m->data.size < 2) return;
  setTargets(m->data.data[0], m->data.data[1], "motor_cmd");
}

void onServo(const void *msg) {
  auto *m = (const std_msgs__msg__Float32MultiArray *)msg;
  if (halted) return;
  for (size_t i = 0; i < m->data.size && i < NSERVO; i++) {
    float d = m->data.data[i];
    servoDeg[i] = (isnan(d) || d < 0) ? NAN : min(d, 180.0f);
    servoApply(i);
  }
}

void onEstop(const void *msg) {
  if (((const std_msgs__msg__Bool *)msg)->data) halt("estop");
  else {
    halted = false;
    haltReason[0] = 0;
  }
}

// /can_tx -> bus. Sin espera: si la cola de 16 tramas está llena (p. ej. nadie da ACK), se descarta y se cuenta.
void onCanTx(const void *msg) {
  auto *f = (const can_msgs__msg__Frame *)msg;
  if (!canInstalled) {
    canTxFailed++;
    return;
  }
  twai_message_t m = {};
  m.extd = f->is_extended;
  m.rtr = f->is_rtr;
  m.self = cfg[P_CAN_SELF_TEST] != 0;
  m.identifier = f->id & (f->is_extended ? 0x1FFFFFFF : 0x7FF);
  m.data_length_code = min<uint8_t>(f->dlc, 8);
  memcpy(m.data, f->data, 8);
  if (twai_transmit(&m, 0) == ESP_OK) canTxFrames++;
  else canTxFailed++;
}

void publishCan(const twai_message_t &m) {
  stamp(msgCanRx.header.stamp);
  msgCanRx.id = m.identifier;
  msgCanRx.is_extended = m.extd;
  msgCanRx.is_rtr = m.rtr;
  msgCanRx.is_error = false;  // el TWAI no entrega tramas de error; los errores van en /diagnostics
  msgCanRx.dlc = m.data_length_code;
  memset(msgCanRx.data, 0, 8);
  memcpy(msgCanRx.data, m.data, min<uint8_t>(m.data_length_code, 8));
  rcl_publish(&pubCan, &msgCanRx, nullptr);
}

// Recepción, recuperación de bus-off y estado. Cada loop().
void canService(bool connected) {
  if (!canInstalled) return;
  twai_get_status_info(&canStatus);
  if (canStatus.state == TWAI_STATE_BUS_OFF && every(tCanRecover, 1000)) twai_initiate_recovery();
  else if (canStatus.state == TWAI_STATE_STOPPED) twai_start();  // fin de la recuperación
  twai_message_t m;
  for (int n = 0; n < 16 && twai_receive(&m, 0) == ESP_OK; n++) {
    canRxFrames++;
    if (connected) publishCan(m);
    else canRxUnsent++;
  }
}

void onBatteryTimer(rcl_timer_t *, int64_t) {
  readVbus();
  stamp(msgBattery.header.stamp);
  msgBattery.voltage = vbus;
  msgBattery.current = -iMean;  // convención de BatteryState: negativo = descarga
  msgBattery.present = inaOk && vbus > 5.0f;
  rcl_publish(&pubBattery, &msgBattery, nullptr);
}

// ----- /diagnostics -----

void diagBegin(int s, uint8_t level, const char *fmt, ...) {
  diagStatus[s].level = level;
  diagStatus[s].values.size = 0;
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(diagBuf[s].message, sizeof diagBuf[s].message, fmt, ap);
  va_end(ap);
  diagStatus[s].message.size = constrain(n, 0, (int)sizeof diagBuf[s].message - 1);
}

void diagKv(int s, const char *key, const char *fmt, ...) {
  auto &st = diagStatus[s];
  if (st.values.size >= NKV) return;
  int k = st.values.size++;
  auto &b = diagBuf[s];
  snprintf(b.key[k], sizeof b.key[k], "%s", key);
  b.kv[k].key.size = strlen(b.key[k]);
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(b.val[k], sizeof b.val[k], fmt, ap);
  va_end(ap);
  b.kv[k].value.size = strlen(b.val[k]);
}

void onDiagTimer(rcl_timer_t *, int64_t) {
  constexpr uint8_t OK = diagnostic_msgs__msg__DiagnosticStatus__OK, WARN = diagnostic_msgs__msg__DiagnosticStatus__WARN,
                    ERROR = diagnostic_msgs__msg__DiagnosticStatus__ERROR;
  static uint32_t lastImuCount = 0, lastMs = 0;
  uint32_t now = millis();
  if (now > lastMs) imuRateHz = lroundf((imuPublished - lastImuCount) * 1000.0f / (now - lastMs));
  lastImuCount = imuPublished;
  lastMs = now;

  if (!inaOk) diagBegin(0, ERROR, "INA226 not responding");
  else if (vbus < cfg[P_VBUS_LOW]) diagBegin(0, WARN, "VBUS low: motors and servos unpowered");
  else diagBegin(0, OK, "OK");
  diagKv(0, "vbus_v", "%.2f", vbus);
  diagKv(0, "i_vm_a", "%.3f", (float)iMean);
  diagKv(0, "i_zero_mv", "%.1f", (float)isensZeroMv);

  if (halted) diagBegin(1, ERROR, "HALT: %s (publish false on /orca/estop)", haltReason);
  else if (motors[0].awake || motors[1].awake) diagBegin(1, OK, "driving (%s)", cmdSource);
  else diagBegin(1, OK, "idle (drivers asleep)");
  diagKv(1, "duty_m1", "%.1f", motors[0].duty);
  diagKv(1, "duty_m2", "%.1f", motors[1].duty);
  diagKv(1, "target_m1", "%.1f", motors[0].target);
  diagKv(1, "target_m2", "%.1f", motors[1].target);
  diagKv(1, "awake", "%d,%d", motors[0].awake, motors[1].awake);
  diagKv(1, "last_source", "%s", cmdSource);
  diagKv(1, "i_limit_a", "%.2f", (float)iLimit);

  int active = 0;
  for (float d : servoDeg) active += !isnan(d);
  if (!pcaOk) diagBegin(2, ERROR, "PCA9685 (0x41) not responding");
  else diagBegin(2, OK, active ? "%d channel(s) with torque" : "all channels limp", active);
  diagKv(2, "active_channels", "%d", active);
  diagKv(2, "us_per_tick", "%.3f", pcaUsPerTick);

  if (!bnoOk) diagBegin(3, ERROR, "BNO085 (0x4A) not responding, retrying");
  else diagBegin(3, OK, "OK");
  diagKv(3, "rate_hz", "%lu", (unsigned long)imuRateHz);
  diagKv(3, "inits", "%lu", (unsigned long)bnoInits);

  diagBegin(4, OK, "fw %s, %s", ORCA_FW_VERSION, TRANSPORT_NAME);
  diagKv(4, "uptime_s", "%lu", (unsigned long)(now / 1000));
  diagKv(4, "reset_reason", "%s", resetName(resetReason));
  diagKv(4, "free_heap", "%lu", (unsigned long)ESP.getFreeHeap());
  diagKv(4, "min_free_heap", "%lu", (unsigned long)ESP.getMinFreeHeap());
  diagKv(4, "agent_sessions", "%lu", (unsigned long)connections);
  diagKv(4, "time_synced", "%d", rmw_uros_epoch_synchronized());
  diagKv(4, "build", "%s %s", __DATE__, __TIME__);

  const char *canState = !canInstalled                               ? "off"
                         : canStatus.state == TWAI_STATE_RUNNING     ? "running"
                         : canStatus.state == TWAI_STATE_BUS_OFF     ? "bus-off"
                         : canStatus.state == TWAI_STATE_RECOVERING  ? "recovering"
                                                                     : "stopped";
  if (!cfg[P_CAN_ENABLED]) diagBegin(5, OK, "disabled (can.enabled = false)");
  else if (!canInstalled) diagBegin(5, ERROR, "TWAI driver failed to start");
  else if (canStatus.state != TWAI_STATE_RUNNING) diagBegin(5, ERROR, "%s: check wiring, termination, bitrate", canState);
  else if (cfg[P_CAN_SELF_TEST]) diagBegin(5, WARN, "self-test mode: no ACK, frames looped back");
  else if (canStatus.tx_error_counter >= 128 || canStatus.rx_error_counter >= 128)
    diagBegin(5, WARN, "error passive: no ACK or bus errors (another node? bitrate?)");
  else diagBegin(5, OK, "running");
  diagKv(5, "bitrate", "%lu", (unsigned long)cfg[P_CAN_BITRATE]);
  diagKv(5, "state", "%s", canState);
  diagKv(5, "rx_frames", "%lu", (unsigned long)canRxFrames);
  diagKv(5, "tx_frames", "%lu", (unsigned long)canTxFrames);
  diagKv(5, "tx_failed", "%lu", (unsigned long)canTxFailed);
  diagKv(5, "rx_lost", "%lu", (unsigned long)(canStatus.rx_missed_count + canStatus.rx_overrun_count + canRxUnsent));
  diagKv(5, "tec_rec", "%lu,%lu", (unsigned long)canStatus.tx_error_counter, (unsigned long)canStatus.rx_error_counter);
  diagKv(5, "bus_errors", "%lu", (unsigned long)canStatus.bus_error_count);

  stamp(msgDiag.header.stamp);
  rcl_publish(&pubDiag, &msgDiag, nullptr);
}

void publishImu() {
  stamp(msgImu.header.stamp);
  msgImu.orientation.w = quat[0];
  msgImu.orientation.x = quat[1];
  msgImu.orientation.y = quat[2];
  msgImu.orientation.z = quat[3];
  msgImu.angular_velocity.x = gyro[0];
  msgImu.angular_velocity.y = gyro[1];
  msgImu.angular_velocity.z = gyro[2];
  msgImu.linear_acceleration.x = accel[0];
  msgImu.linear_acceleration.y = accel[1];
  msgImu.linear_acceleration.z = accel[2];
  if (rcl_publish(&pubImu, &msgImu, nullptr) == RCL_RET_OK) imuPublished++;
}

// ----- entidades -----

bool createParams() {
  rclc_parameter_options_t opt = {};
  opt.notify_changed_over_dds = true;  // /parameter_events
  opt.max_params = P_COUNT;
  opt.allow_undeclared_parameters = false;
  opt.low_mem_mode = false;
  RCCHECK(rclc_parameter_server_init_with_option(&paramServer, &node, &opt));
  for (int i = 0; i < P_COUNT; i++) {
    const ParamDef &p = PARAMS[i];
    RCCHECK(rclc_add_parameter(&paramServer, p.name, p.type));
    RCCHECK(rclc_add_parameter_description(&paramServer, p.name, p.desc, ""));
    switch (p.type) {
      case RCLC_PARAMETER_BOOL: RCCHECK(rclc_parameter_set_bool(&paramServer, p.name, cfg[i] != 0)); break;
      case RCLC_PARAMETER_INT:
        RCCHECK(rclc_add_parameter_constraint_integer(&paramServer, p.name, (int64_t)p.lo, (int64_t)p.hi, 0));
        RCCHECK(rclc_parameter_set_int(&paramServer, p.name, (int64_t)cfg[i]));
        break;
      default:
        RCCHECK(rclc_add_parameter_constraint_double(&paramServer, p.name, p.lo, p.hi, 0));
        RCCHECK(rclc_parameter_set_double(&paramServer, p.name, cfg[i]));
    }
  }
  return true;
}

bool createEntities() {
  allocator = rcl_get_default_allocator();
  initOptions = rcl_get_zero_initialized_init_options();
  RCCHECK(rcl_init_options_init(&initOptions, allocator));
  RCCHECK(rcl_init_options_set_domain_id(&initOptions, ORCA_ROS_DOMAIN_ID));
  RCCHECK(rclc_support_init_with_options(&support, 0, nullptr, &initOptions, &allocator));
  RCCHECK(rclc_node_init_default(&node, NODE_NAME, ORCA_NAMESPACE, &support));

  RCCHECK(rclc_publisher_init_default(&pubBattery, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, BatteryState),
                                      "battery_state"));
  RCCHECK(rclc_publisher_init_best_effort(&pubImu, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(sensor_msgs, msg, Imu), "imu"));
  RCCHECK(rclc_publisher_init_default(&pubDiag, &node,
                                      ROSIDL_GET_MSG_TYPE_SUPPORT(diagnostic_msgs, msg, DiagnosticArray), "diagnostics"));
  RCCHECK(rclc_subscription_init_default(&subCmdVel, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist),
                                         "cmd_vel"));
  RCCHECK(rclc_subscription_init_default(&subMotor, &node,
                                         ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32MultiArray), "motor_cmd"));
  RCCHECK(rclc_subscription_init_default(&subServo, &node,
                                         ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Float32MultiArray), "servo_cmd"));
  RCCHECK(rclc_subscription_init_default(&subEstop, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool),
                                         "orca/estop"));
  RCCHECK(rclc_publisher_init_default(&pubCan, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(can_msgs, msg, Frame), "can_rx"));
  RCCHECK(rclc_subscription_init_default(&subCanTx, &node, ROSIDL_GET_MSG_TYPE_SUPPORT(can_msgs, msg, Frame),
                                         "can_tx"));
  RCCHECK(rclc_timer_init_default2(&timerBattery, &support, RCL_MS_TO_NS(BATTERY_PERIOD_MS), onBatteryTimer, true));
  RCCHECK(rclc_timer_init_default2(&timerDiag, &support, RCL_MS_TO_NS(DIAG_PERIOD_MS), onDiagTimer, true));
  if (!createParams()) return false;  // valores desde NVS; el callback aún no está registrado

  executor = rclc_executor_get_zero_initialized_executor();
  RCCHECK(rclc_executor_init(&executor, &support.context, 7 + RCLC_EXECUTOR_PARAMETER_SERVER_HANDLES, &allocator));
  RCCHECK(rclc_executor_add_subscription(&executor, &subEstop, &msgEstop, onEstop, ON_NEW_DATA));  // estop primero
  RCCHECK(rclc_executor_add_subscription(&executor, &subCmdVel, &msgCmdVel, onCmdVel, ON_NEW_DATA));
  RCCHECK(rclc_executor_add_subscription(&executor, &subMotor, &msgMotor, onMotor, ON_NEW_DATA));
  RCCHECK(rclc_executor_add_subscription(&executor, &subServo, &msgServo, onServo, ON_NEW_DATA));
  RCCHECK(rclc_executor_add_subscription(&executor, &subCanTx, &msgCanTx, onCanTx, ON_NEW_DATA));
  RCCHECK(rclc_executor_add_timer(&executor, &timerBattery));
  RCCHECK(rclc_executor_add_timer(&executor, &timerDiag));
  RCCHECK(rclc_executor_add_parameter_server(&executor, &paramServer, onParam));

  rmw_uros_sync_session(1000);  // hora del agente para los header.stamp
  connections++;
  return true;
}

void destroyEntities() {
  rmw_context_t *ctx = rcl_context_get_rmw_context(&support.context);
  (void)rmw_uros_set_context_entity_destroy_session_timeout(ctx, 0);
  rclc_parameter_server_fini(&paramServer, &node);
  rcl_publisher_fini(&pubBattery, &node);
  rcl_publisher_fini(&pubImu, &node);
  rcl_publisher_fini(&pubDiag, &node);
  rcl_publisher_fini(&pubCan, &node);
  rcl_subscription_fini(&subCanTx, &node);
  rcl_subscription_fini(&subCmdVel, &node);
  rcl_subscription_fini(&subMotor, &node);
  rcl_subscription_fini(&subServo, &node);
  rcl_subscription_fini(&subEstop, &node);
  rcl_timer_fini(&timerBattery);
  rcl_timer_fini(&timerDiag);
  rclc_executor_fini(&executor);
  rcl_node_fini(&node);
  rclc_support_fini(&support);
  rcl_init_options_fini(&initOptions);
}

// ---------- Arduino ----------

void setup() {
  resetReason = esp_reset_reason();
  loadConfig();
  motorsSetup();  // primero: deja los drivers dormidos
  for (auto &d : servoDeg) d = NAN;
  uint64_t mac = ESP.getEfuseMac();
  snprintf(hardwareId, sizeof hardwareId, "orca-revA-%06llx", (unsigned long long)(mac >> 24));

#ifdef ORCA_TRANSPORT_WIFI
  Serial.begin(115200);
  IPAddress agentIp;
  agentIp.fromString(ORCA_AGENT_IP);
  set_microros_wifi_transports((char *)ORCA_WIFI_SSID, (char *)ORCA_WIFI_PASS, agentIp, ORCA_AGENT_PORT);
  WiFi.setSleep(false);  // sin ahorro de energía: latencia estable
  Serial.printf("orca_micro_ros %s: Wi-Fi %s, IP %s, agent %s:%d\n", ORCA_FW_VERSION, ORCA_WIFI_SSID,
                WiFi.localIP().toString().c_str(), ORCA_AGENT_IP, ORCA_AGENT_PORT);
#else
  Serial.setRxBufferSize(4096);
  Serial.setTxBufferSize(4096);
  Serial.begin(115200);
  set_microros_serial_transports(Serial);
#endif

  Wire.begin(OB_SDA, OB_SCL, I2C_HZ);
  analogSetPinAttenuation(ISENS_PIN, ADC_11db);
  calibrateZero();
  xTaskCreatePinnedToCore(samplerTask, "isens", 4096, nullptr, 2, nullptr, 1);
  if ((pcaOk = pcaInit())) servosLimp();
  readVbus();
  bnoInit();
  canStart();
  initMessages();
  enableLoopWDT();  // el core alimenta el watchdog al final de cada loop()
}

uint32_t tPcaRetry = 0, tPing = 0, tSync = 0;

void loop() {
  static uint32_t lastCtrlUs = micros();
  uint32_t nowUs = micros();
  if (nowUs - lastCtrlUs >= 5000) {
    controlStep((nowUs - lastCtrlUs) * 1e-6f);
    lastCtrlUs = nowUs;
  }

  if (!pcaOk && every(tPcaRetry, 1000) && (pcaOk = pcaInit())) servosApplyAll();
  if (bnoOpen) {
    sh2_service();
    if (bno.wasReset() || millis() - bnoLastData > 1000) bnoOk = false;
  }
  if (!bnoOk && millis() - bnoLastInit > 2000) bnoInit();
  if (halted) servosLimp();  // servoWriteTicks solo escribe si cambia
  canService(agent == Agent::Connected);

  switch (agent) {
    case Agent::Waiting:
      if (every(tPing, 500)) agent = rmw_uros_ping_agent(50, 1) == RMW_RET_OK ? Agent::Available : Agent::Waiting;
      break;
    case Agent::Available:
      agent = createEntities() ? Agent::Connected : Agent::Waiting;
      if (agent == Agent::Waiting) destroyEntities();
      break;
    case Agent::Connected:
      if (every(tPing, 1000) && rmw_uros_ping_agent(50, 3) != RMW_RET_OK) {
        agent = Agent::Disconnected;
        break;
      }
      if (every(tSync, TIME_SYNC_PERIOD_MS)) rmw_uros_sync_session(50);
      rclc_executor_spin_some(&executor, RCL_MS_TO_NS(1));
      if (bnoOk && haveQuat) {
        haveQuat = false;
        publishImu();
      }
      break;
    case Agent::Disconnected:
      coastAll();
      if (cfg[P_SERVO_LIMP]) servosLimp();
      destroyEntities();
      agent = Agent::Waiting;
      break;
  }
}
