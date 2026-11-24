// ORCA-BOARD Control Rev A — banco de prueba de motores (U5/U6 DRV8874)
//
// Maneja los dos puentes H con rampa, límite de corriente por software y deadman, mide la corriente de VM
// con el ACS725 y corre secuencias de prueba en la placa. Todo se opera desde el panel web:
//   - Wi-Fi (AP "ORCA-Board" / "orca1234"): http://192.168.4.1/ (panel), /cmd?c=<comando>, /status.json
//     (incluye los campos de wifi_telemetry: orca_monitor.py sigue sirviendo), /seq.txt (informe de la
//     secuencia), /log.csv (lecturas a 10 Hz de la última secuencia)
//   - USB: los mismos comandos a 115200 ("help"), salvo iniciar secuencias
// Las secuencias solo se inician con el botón del panel. Ver 10_banco_motores.md.
//
// Hardware (netlist de ORCA-BOARD.kicad_sch):
//   - PMODE = GND -> modo PH/EN: EN = PWM, PH = dirección, EN = 0 -> freno (low-side). Libre = nSLEEP bajo.
//   - IMODE = GND -> off-time fijo y reintento automático de OCP. ITRIP ≈ 3.06 A por driver (VREF 3.3 V, 2.4 kΩ).
//   - nFAULT e IPROPI NO llegan al ESP32: no hay corriente por canal ni aviso de falla. La única medida es la
//     corriente total de VM (ACS725, IO9). Para atribuir corriente a un motor, probar uno a la vez.
//   - nSLEEP tiene pull-up de 10 k: el driver despierta al encender, con EN en bajo por su pull-down (freno)
//     hasta que setup() lo pone a dormir.
//
// Seguridad: al arrancar ambos drivers duermen. STOP (panel, barra espaciadora o "stop" por USB), la
// sobrecorriente (media de 100 ms > ilim) y el deadman (nadie consulta /status.json ni manda comandos en
// "deadman" ms) dejan los dos motores libres, abortan la secuencia y enclavan un HALT: se rechaza toda orden
// de movimiento hasta "release" (botón Liberar del panel).

#include <Arduino.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Wire.h>
#include <driver/ledc.h>
#include <stdarg.h>

constexpr char AP_SSID[] = "ORCA-Board";
constexpr char AP_PASS[] = "orca1234";
constexpr char HOSTNAME[] = "orca-board";

struct Motor {
  const char *name;
  int pinEn, pinPh, pinSleep;  // EN/IN1 (PWM), PH/IN2 (dirección), nSLEEP
  ledc_channel_t ledc;
  float target, duty;  // % de -100 a 100; positivo = adelante (OUT1 -> OUT2, PH alto)
  bool awake;
};
Motor motors[2] = {
    {"M1 (U5, J3)", 16, 15, 14, LEDC_CHANNEL_0, 0, 0, false},
    {"M2 (U6, J4)", 12, 13, 11, LEDC_CHANNEL_1, 0, 0, false},
};

constexpr int OB_SDA = 18, OB_SCL = 17;  // I2C de placa (INA226)
constexpr int ISENS_PIN = 9;             // ACS725 VIOUT
constexpr uint8_t ADDR_INA226 = 0x40;
constexpr float ISENS_NOMINAL_ZERO_MV = 330.0f, ISENS_MV_PER_A = 264.0f;
constexpr ledc_timer_t PWM_TIMER = LEDC_TIMER_1;
constexpr ledc_timer_bit_t PWM_BITS = LEDC_TIMER_10_BIT;
constexpr uint32_t PWM_FULL = 1u << 10;  // con 10 bits, 1024 = 100 % (siempre encendido)
constexpr uint32_t CONTROL_PERIOD_MS = 5, VBUS_PERIOD_MS = 100, TELEMETRY_PERIOD_MS = 1000, LOG_PERIOD_MS = 100;

// Ajustes (comandos ramp, ilim, deadman, freq)
float rampRate = 200;    // %/s; 0 = sin rampa (escalón)
float iLimit = 2.0f;     // A, media de 100 ms. Pistas VM/OUT2 de U5/U6: ~1.5–1.8 A para ΔT = 20 °C (07, P3/P4)
uint32_t deadmanMs = 2000;
uint32_t pwmHz = 20000;  // DRV8874 admite hasta 100 kHz; con 10 bits el LEDC llega a 78 kHz

WebServer server(80);
Preferences prefs;
TwoWire &obI2c = Wire;

// Protege motores, informe y etiquetas entre loop() y la tarea de la secuencia. Nunca se retiene en una espera.
SemaphoreHandle_t stateMux;
struct Lock {
  Lock() { xSemaphoreTakeRecursive(stateMux, portMAX_DELAY); }
  ~Lock() { xSemaphoreGiveRecursive(stateMux); }
};

uint32_t bootCount = 0, brownoutCount = 0, lastContact = 0;
esp_reset_reason_t resetReason;
float vbus = NAN, vbusMin = NAN, vbusMax = NAN;
bool inaOk = false;
String lastEvent = "arranque";

// HALT enclavado: STOP, sobrecorriente o deadman. Solo "release" lo borra.
volatile bool halted = false;
char haltReason[80] = "";

// Corriente de VM. Con PWM en modo PH/EN la corriente del motor recircula por los low-side en el tiempo
// apagado, así que la de alimentación es ≈ duty × I_motor. VIOUT no tiene filtro RC y el ADC toma puntos
// sueltos: el retardo aleatorio de 0–50 µs entre lecturas reparte la fase respecto al PWM para que el
// promedio sea la corriente media.
volatile float isensZeroMv = ISENS_NOMINAL_ZERO_MV;
volatile float iMean = 0, iPeak = 0, iMin = 0;  // A: media de 100 ms; máx./mín. de medias de 10 ms
volatile bool peakReset = true, isensZeroCal = false;
volatile uint32_t isensSamples = 0;

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
#if ESP_IDF_VERSION_MAJOR >= 5  // el core 2.0.14 (IDF 4.4) no tiene estas causas
    case ESP_RST_USB: return "USB";
    case ESP_RST_JTAG: return "JTAG";
#endif
    default: return "UNKNOWN";
  }
}

String fmt(const char *f, ...) {
  char buf[256];
  va_list ap;
  va_start(ap, f);
  vsnprintf(buf, sizeof buf, f, ap);
  va_end(ap);
  return String(buf);
}

// ---------- drivers ----------

void applyOutputs(Motor &m) {
  uint32_t raw = min<uint32_t>(lroundf(fabsf(m.duty) / 100.0f * PWM_FULL), PWM_FULL);
  digitalWrite(m.pinPh, m.duty > 0 ? HIGH : LOW);
  ledc_set_duty(LEDC_LOW_SPEED_MODE, m.ledc, raw);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, m.ledc);
}

void wake(Motor &m) {
  if (m.awake || halted) return;
  m.target = m.duty = 0;
  applyOutputs(m);
  digitalWrite(m.pinSleep, HIGH);
  delay(2);  // tWAKE máx. 1 ms
  m.awake = true;
}

void coast(Motor &m) {
  m.target = m.duty = 0;
  applyOutputs(m);
  digitalWrite(m.pinSleep, LOW);  // puente en alta impedancia
  m.awake = false;
}

void brake(Motor &m) {
  wake(m);
  m.target = m.duty = 0;  // EN = 0 -> ambos low-side encendidos
  applyOutputs(m);
}

// También se llama desde la tarea del ACS725: solo toca el flag y nSLEEP; controlStep() limpia el estado.
// Conserva la primera causa si ya estaba en HALT.
void halt(const char *reason) {
  if (!halted) snprintf(haltReason, sizeof haltReason, "%s", reason);
  halted = true;
  for (auto &m : motors) gpio_set_level((gpio_num_t)m.pinSleep, 0);
}

void slew(Motor &m, float dt) {
  float t = m.target;
  if (rampRate <= 0) {
    m.duty = t;
    return;
  }
  if (m.duty * t < 0) t = 0;  // al invertir, primero bajar a 0
  float step = rampRate * dt;
  m.duty += constrain(t - m.duty, -step, step);
}

void controlStep(float dt) {
  Lock l;
  for (auto &m : motors) {
    if (halted && m.awake) {
      coast(m);
      lastEvent = fmt("HALT: %s", haltReason);
    }
    if (m.awake) {
      slew(m, dt);
      applyOutputs(m);
    }
  }
}

bool anyAwake() { return motors[0].awake || motors[1].awake; }

// ---------- medidas ----------

void samplerTask(void *) {
  TickType_t last = xTaskGetTickCount();
  float sum10 = 0, sum100 = 0;
  int n10 = 0, n100 = 0;
  for (;;) {
    for (int i = 0; i < 4; i++) {
      delayMicroseconds(esp_random() % 50);  // un período de PWM a 20 kHz
      sum10 += (analogReadMilliVolts(ISENS_PIN) - isensZeroMv) / ISENS_MV_PER_A;
      n10++;
    }
    if (n10 >= 40) {  // 10 ms
      float m = sum10 / n10;
      sum100 += sum10;
      n100 += n10;
      isensSamples += n10;
      sum10 = 0;
      n10 = 0;
      if (peakReset) {
        iPeak = iMin = m;
        peakReset = false;
      }
      if (m > iPeak) iPeak = m;
      if (m < iMin) iMin = m;
      if (n100 >= 400) {  // 100 ms
        iMean = sum100 / n100;
        sum100 = 0;
        n100 = 0;
        if (iMean > iLimit && !halted) {
          char r[80];
          snprintf(r, sizeof r, "sobrecorriente %.2f A > ilim %.2f A", iMean, iLimit);
          halt(r);
        }
      }
    }
    vTaskDelayUntil(&last, 1);
  }
}

int32_t read16(TwoWire &bus, uint8_t addr, uint8_t reg) {
  bus.beginTransmission(addr);
  bus.write(reg);
  if (bus.endTransmission(false) != 0 || bus.requestFrom(addr, (uint8_t)2) != 2) return -1;
  return (bus.read() << 8) | bus.read();
}

void readVbus() {
  int32_t raw = read16(obI2c, ADDR_INA226, 0x02);
  inaOk = raw >= 0;
  if (!inaOk) {
    vbus = NAN;
    return;
  }
  vbus = raw * 0.00125f;  // registro de bus voltage, 1.25 mV/LSB
  vbusMin = isnan(vbusMin) ? vbus : min(vbusMin, vbus);
  vbusMax = isnan(vbusMax) ? vbus : max(vbusMax, vbus);
}

// Cero del ACS725 con los dos drivers dormidos. Fuera de 250–450 mV se mantiene el valor anterior.
String calibrateZero() {
  Lock l;
  if (anyAwake()) return "err zero: poner ambos motores en libre primero (coast all)";
  delay(50);
  uint32_t sum = 0;
  for (int i = 0; i < 512; i++) sum += analogReadMilliVolts(ISENS_PIN);
  float mv = sum / 512.0f;
  if (mv < 250 || mv > 450) return fmt("err zero: VIOUT = %.0f mV fuera de 250-450 mV", mv);
  isensZeroMv = mv;
  isensZeroCal = true;
  peakReset = true;
  return fmt("ok zero %.1f mV", mv);
}

// ---------- secuencias (tarea propia; solo se inician desde el panel) ----------

struct SeqParams {
  char test[12];
  int ch, duty, step, max, cycles;
  float time, settle, dwell, ramp, ilim;  // ramp/ilim NAN = dejar los de la placa
};
SeqParams seq;
volatile bool seqRunning = false;
String seqReport;
char seqError[96] = "";
constexpr int MAX_LABELS = 64;
char seqLabels[MAX_LABELS][24];
volatile int seqStep = 0;
uint32_t seqStartMs = 0, seqEndMs = 0;
int seqPass = 0, seqWarn = 0, seqFail = 0;

// Registro a 10 Hz de la secuencia (68 kB, ~400 s). Solo se escribe y se lee desde loop().
struct __attribute__((packed)) LogRow {
  uint32_t tMs;
  uint8_t step;
  int16_t duty1, duty2;  // décimas de %
  int16_t iMa, pkMa, minMa;
  uint16_t vbusMv;
};
constexpr int LOG_ROWS = 4000;
LogRow logRows[LOG_ROWS];
int logCount = 0;

void rep(const char *f, ...) {
  char buf[200];
  va_list ap;
  va_start(ap, f);
  vsnprintf(buf, sizeof buf, f, ap);
  va_end(ap);
  {
    Lock l;
    if (seqReport.length() < 16000) {
      seqReport += buf;
      seqReport += '\n';
    }
  }
  Serial.println(buf);
}

void note(const char *level, const char *f, ...) {
  char buf[160];
  va_list ap;
  va_start(ap, f);
  vsnprintf(buf, sizeof buf, f, ap);
  va_end(ap);
  rep("  [%s] %s", level, buf);
  if (!strcmp(level, "PASS")) seqPass++;
  else if (!strcmp(level, "WARN")) seqWarn++;
  else seqFail++;
}

void label(const char *f, ...) {
  Lock l;
  if (seqStep < MAX_LABELS - 1) seqStep++;
  va_list ap;
  va_start(ap, f);
  vsnprintf(seqLabels[seqStep], sizeof seqLabels[0], f, ap);
  va_end(ap);
}

bool fail(const char *f, ...) {
  va_list ap;
  va_start(ap, f);
  vsnprintf(seqError, sizeof seqError, f, ap);
  va_end(ap);
  return false;
}

bool sleepMs(uint32_t ms) {
  uint32_t t0 = millis();
  while (millis() - t0 < ms) {
    if (halted) return false;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  return !halted;
}

// mask: bit 0 = M1, bit 1 = M2
bool setDuty(int mask, float d) {
  Lock l;
  if (halted) return false;
  for (int i = 0; i < 2; i++) {
    if (!(mask & (1 << i))) continue;
    wake(motors[i]);
    motors[i].target = d;
  }
  return !halted;
}

void coastMask(int mask) {
  Lock l;
  for (int i = 0; i < 2; i++)
    if (mask & (1 << i)) coast(motors[i]);
}

bool waitRamp(int mask, uint32_t limitMs = 30000) {
  uint32_t t0 = millis();
  for (;;) {
    bool done = true;
    {
      Lock l;
      for (int i = 0; i < 2; i++)
        if ((mask & (1 << i)) && fabsf(motors[i].duty - motors[i].target) >= 0.05f) done = false;
    }
    if (halted) return false;
    if (done) return true;
    if (millis() - t0 > limitMs) return fail("no llego al duty objetivo en %lu s", (unsigned long)(limitMs / 1000));
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

bool rampDownAndCoast(int mask) {
  if (!setDuty(mask, 0) || !waitRamp(mask)) return false;
  coastMask(mask);
  return true;
}

void resetPeaks() {
  peakReset = true;
  vTaskDelay(pdMS_TO_TICKS(20));  // la tarea del ACS725 lo aplica en su próxima ventana de 10 ms
}

struct Meas {
  float i, sd, peak, min;
};

// Promedia la media de 100 ms durante `ms`; el pico y el mínimo son los de ese intervalo
bool measureWindow(uint32_t ms, Meas &r) {
  resetPeaks();
  float sum = 0, sum2 = 0;
  int n = 0;
  uint32_t t0 = millis();
  do {
    if (!sleepMs(100)) return false;
    float x = iMean;
    sum += x;
    sum2 += x * x;
    n++;
  } while (millis() - t0 < ms);
  r.i = sum / n;
  r.sd = sqrtf(fmaxf(0, sum2 / n - r.i * r.i));
  r.peak = iPeak;
  r.min = iMin;
  return true;
}

bool measureAt(int ch, float d, Meas &r) {
  return setDuty(1 << (ch - 1), d) && waitRamp(1 << (ch - 1)) && sleepMs(seq.settle * 1000) &&
         measureWindow(seq.dwell * 1000, r);
}

String iMotorText(float i, float d) { return fabsf(d) >= 20 ? String(i / (fabsf(d) / 100), 3) : String("--"); }

bool tZero() {
  label("zero");
  rep("== Cero del ACS725 ==");
  coastMask(3);
  if (!sleepMs(300)) return false;
  String r = calibrateZero();
  rep("  %s", r.c_str());
  if (r.startsWith("err")) return fail("%s", r.c_str());
  return true;
}

bool tSweep(int ch) {
  int mask = 1 << (ch - 1);
  int duties[21], nd = 0;
  for (int d = seq.step; d <= seq.max && nd < 21; d += seq.step) duties[nd++] = d;
  float iFwd[21], iRev[21];
  rep("== Barrido M%d: %d..%d %% en ambos sentidos, %.1f s de asentamiento + %.1f s de medida ==", ch, duties[0],
      duties[nd - 1], seq.settle, seq.dwell);
  rep("    duty   I_alim       ±    pico  I_motor");
  for (int sign = 1; sign >= -1; sign -= 2) {
    for (int k = 0; k < nd; k++) {
      int d = sign * duties[k];
      label("sweep M%d %+d%%", ch, d);
      Meas r;
      if (!measureAt(ch, d, r)) return false;
      (sign > 0 ? iFwd : iRev)[k] = r.i;
      rep("  %+5d%%  %7.3f  %6.3f  %6.3f  %7s", d, r.i, r.sd, r.peak, iMotorText(r.i, d).c_str());
    }
    if (!setDuty(mask, 0) || !waitRamp(mask)) return false;
  }
  coastMask(mask);

  int top = duties[nd - 1];
  float f = iFwd[nd - 1], b = iRev[nd - 1];
  if (fmaxf(f, b) < 0.03f) {
    note("WARN", "M%d: a +/-%d %% la corriente es < 30 mA: motor desconectado, driver sin VM o sin conmutar", ch, top);
  } else {
    note("PASS", "M%d conduce en ambos sentidos: %.3f A (+%d %%) / %.3f A (-%d %%)", ch, f, top, b, top);
  }
  float worst = -1;
  int worstK = -1;
  for (int k = 0; k < nd; k++) {
    float hi = fmaxf(iFwd[k], iRev[k]);
    if (hi <= 0.1f) continue;
    float rel = fabsf(iFwd[k] - iRev[k]) / hi;
    if (rel > worst) {
      worst = rel;
      worstK = k;
    }
  }
  if (worstK >= 0) {
    note(worst > 0.25f ? "WARN" : "PASS", "M%d asimetria adelante/atras max. %.0f %% a %d %% (%.3f / %.3f A)", ch,
         worst * 100, duties[worstK], iFwd[worstK], iRev[worstK]);
  }
  return true;
}

bool tStep(int ch) {
  int mask = 1 << (ch - 1);
  rep("== Escalon M%d: 0 -> %d %% sin rampa, %.1f s ==", ch, seq.duty, seq.time);
  label("step M%d freno", ch);
  {
    Lock l;
    if (halted) return false;
    brake(motors[ch - 1]);
  }
  if (!sleepMs(500)) return false;
  label("step M%d %d%%", ch, seq.duty);
  float ramp0 = rampRate;
  rampRate = 0;
  resetPeaks();
  bool ok = setDuty(mask, seq.duty);
  float sum = 0;
  int n = 0, total = max(2, (int)(seq.time * 10));
  for (int k = 0; ok && k < total; k++) {
    ok = sleepMs(100);
    if (k >= total / 2) {
      sum += iMean;
      n++;
    }
  }
  float peak = iPeak;
  coastMask(mask);
  rampRate = ramp0;
  if (!ok) return false;
  float steady = sum / max(n, 1);
  rep("  pico (media de 10 ms) %.3f A | regimen %.3f A | relacion %.1fx", peak, steady, peak / fmaxf(steady, 1e-3f));
  note("PASS", "M%d escalon a %d %%: pico %.3f A, regimen %.3f A", ch, seq.duty, peak, steady);
  return true;
}

bool tReverse(int ch) {
  int mask = 1 << (ch - 1);
  rep("== Inversiones M%d: +/-%d %%, %d ciclos, %.1f s por sentido, rampa %.0f %%/s ==", ch, seq.duty, seq.cycles,
      seq.time, rampRate);
  float maxPeak = 0, minI = 0;
  for (int c = 1; c <= seq.cycles; c++) {
    for (int sign = 1; sign >= -1; sign -= 2) {
      int d = sign * seq.duty;
      label("reverse M%d c%d %+d%%", ch, c, d);
      resetPeaks();
      if (!setDuty(mask, d) || !sleepMs(seq.time * 1000)) return false;
      float pk = iPeak, mn = iMin;
      maxPeak = fmaxf(maxPeak, pk);
      minI = fminf(minI, mn);
      rep("  ciclo %d %+4d%%: pico %.3f A, minimo %+.3f A", c, d, pk, mn);
    }
  }
  if (!rampDownAndCoast(mask)) return false;
  note("PASS", "M%d: %d inversiones sin disparo; pico max. %.3f A", ch, 2 * seq.cycles, maxPeak);
  if (minI < -0.1f) {
    note("WARN", "M%d: corriente negativa hasta %.3f A: el motor devuelve energia a VM. Con una fuente que no "
                 "absorbe corriente VM sube; bajar la rampa o revisar el bulk de VM", ch, minI);
  }
  return true;
}

bool tSoak(int ch) {
  int mask = 1 << (ch - 1);
  rep("== Marcha continua M%d a %d %% durante %.0f s ==", ch, seq.duty, seq.time);
  rep("  Revisar con camara termica o termopar: U5/U6, pista VM (U5.11/U6.11) y OUT2 (07, P3/P4).");
  label("soak M%d %d%%", ch, seq.duty);
  if (!setDuty(mask, seq.duty) || !waitRamp(mask)) return false;
  resetPeaks();
  int total = seq.time * 10;
  float sum = 0, first = 0, last = 0;
  for (int k = 1; k <= total; k++) {
    if (!sleepMs(100)) return false;
    float x = iMean;
    sum += x;
    if (k <= 20) first += x / 20;
    if (k > total - 20) last += x / 20;
    if (k % 50 == 0) {
      rep("  %6d s  I %6.3f A  pico %6.3f A  I_motor ~ %s A  Tchip %.1f C", k / 10, x, (float)iPeak,
          iMotorText(x, seq.duty).c_str(), temperatureRead());
    }
  }
  if (!rampDownAndCoast(mask)) return false;
  note("PASS", "M%d %.0f s a %d %%: I media %.3f A, deriva %+.3f A, sin disparo", ch, seq.time, seq.duty,
       sum / max(total, 1), total >= 40 ? last - first : 0.0f);
  return true;
}

bool tBoth() {
  rep("== M1 y M2 a %d %%: cada uno solo y luego juntos ==", seq.duty);
  float alone[2];
  for (int ch = 1; ch <= 2; ch++) {
    label("both M%d solo", ch);
    Meas r;
    if (!measureAt(ch, seq.duty, r) || !rampDownAndCoast(1 << (ch - 1))) return false;
    alone[ch - 1] = r.i;
    rep("  M%d solo: %.3f A", ch, r.i);
  }
  label("both M1+M2");
  Meas r;
  if (!setDuty(3, seq.duty) || !waitRamp(3) || !sleepMs(seq.settle * 1000) || !measureWindow(seq.dwell * 1000, r) ||
      !rampDownAndCoast(3))
    return false;
  float expected = alone[0] + alone[1], rel = fabsf(r.i - expected) / fmaxf(expected, 1e-3f);
  rep("  juntos: %.3f A (suma de cada uno solo %.3f A), pico %.3f A", r.i, expected, r.peak);
  note(expected > 0.1f && rel > 0.25f ? "WARN" : "PASS", "juntos %.3f A contra %.3f A esperados (%.0f %% de diferencia)",
       r.i, expected, rel * 100);
  return true;
}

bool runSelected() {
  const char *t = seq.test;
  if (!strcmp(t, "zero")) return tZero();
  if (!strcmp(t, "sweep")) return tSweep(seq.ch);
  if (!strcmp(t, "step")) return tStep(seq.ch);
  if (!strcmp(t, "reverse")) return tReverse(seq.ch);
  if (!strcmp(t, "soak")) return tSoak(seq.ch);
  if (!strcmp(t, "both")) return tBoth();
  // full
  if (!tZero()) return false;
  for (int ch = 1; ch <= 2; ch++)
    if (!tSweep(ch) || !tStep(ch) || !tReverse(ch)) return false;
  return tBoth();
}

void seqTask(void *) {
  float ramp0 = rampRate, ilim0 = iLimit;
  if (!isnan(seq.ramp)) rampRate = seq.ramp;
  if (!isnan(seq.ilim)) iLimit = seq.ilim;
  rep("===== Secuencia %s | rampa %.0f %%/s | ilim %.2f A | PWM %lu Hz | Vbus (J19) %s V =====", seq.test, rampRate,
      iLimit, (unsigned long)pwmHz, isnan(vbus) ? "--" : String(vbus, 2).c_str());
  rep("VM (J20) no se mide en la placa: anotar el voltaje de la fuente de VM.");
  if (!runSelected()) note("FAIL", "abortada: %s", halted ? haltReason : seqError);
  coastMask(3);
  rampRate = ramp0;
  iLimit = ilim0;
  seqEndMs = millis();
  rep("===== RESUMEN: %d PASS, %d WARN, %d FAIL | %.0f s =====", seqPass, seqWarn, seqFail,
      (seqEndMs - seqStartMs) / 1000.0f);
  label("terminada");
  seqRunning = false;
  vTaskDelete(nullptr);
}

const char *const SEQ_TESTS[] = {"full", "sweep", "step", "reverse", "soak", "both", "zero"};

// "run <prueba> [ch=1 duty=50 step=20 max=100 time=2 cycles=3 settle=1 dwell=1 ramp=200 ilim=2]"
String startSequence(const String &line) {
  if (halted) return fmt("err HALT activo (%s): Liberar primero", haltReason);
  if (seqRunning) return "err ya hay una secuencia en curso";
  SeqParams p = {"", 1, 50, 20, 100, 3, NAN, 1.0f, 1.0f, NAN, NAN};
  char buf[160];
  snprintf(buf, sizeof buf, "%s", line.c_str());
  strtok(buf, " ");  // "run"
  char *tok = strtok(nullptr, " ");
  if (!tok) return "err run: falta la prueba";
  for (const char *t : SEQ_TESTS)
    if (!strcmp(tok, t)) snprintf(p.test, sizeof p.test, "%s", t);
  if (!p.test[0]) return fmt("err run: prueba '%s' desconocida", tok);
  while ((tok = strtok(nullptr, " "))) {
    char *eq = strchr(tok, '=');
    if (!eq) return fmt("err run: '%s' no es clave=valor", tok);
    *eq = 0;
    char *end;
    float x = strtof(eq + 1, &end);
    if (end == eq + 1 || *end) return fmt("err run: valor no numerico en '%s'", tok);
    struct { const char *key; float lo, hi; } lim[] = {{"ch", 1, 2},       {"duty", 5, 100},  {"step", 5, 100},
                                                      {"max", 5, 100},    {"time", 0.5, 600}, {"cycles", 1, 20},
                                                      {"settle", 0, 10},  {"dwell", 0.3, 10}, {"ramp", 0, 10000},
                                                      {"ilim", 0.1, 9}};
    bool known = false;
    for (auto &k : lim) {
      if (strcmp(tok, k.key)) continue;
      known = true;
      if (x < k.lo || x > k.hi) return fmt("err run: %s fuera de %g-%g", tok, k.lo, k.hi);
    }
    if (!known) return fmt("err run: clave '%s' desconocida", tok);
    if (!strcmp(tok, "ch")) p.ch = x;
    else if (!strcmp(tok, "duty")) p.duty = x;
    else if (!strcmp(tok, "step")) p.step = x;
    else if (!strcmp(tok, "max")) p.max = x;
    else if (!strcmp(tok, "time")) p.time = x;
    else if (!strcmp(tok, "cycles")) p.cycles = x;
    else if (!strcmp(tok, "settle")) p.settle = x;
    else if (!strcmp(tok, "dwell")) p.dwell = x;
    else if (!strcmp(tok, "ramp")) p.ramp = x;
    else p.ilim = x;
  }
  if (p.step > p.max) return "err run: step debe ser <= max";
  if (isnan(p.time)) p.time = strcmp(p.test, "soak") ? 2.0f : 60.0f;

  coastMask(3);
  seq = p;
  seqReport = "";
  seqError[0] = 0;
  seqStep = 0;
  snprintf(seqLabels[0], sizeof seqLabels[0], "inicio");
  seqPass = seqWarn = seqFail = 0;
  logCount = 0;
  seqStartMs = millis();
  seqEndMs = 0;
  seqRunning = true;
  if (xTaskCreatePinnedToCore(seqTask, "seq", 8192, nullptr, 1, nullptr, 1) != pdPASS) {
    seqRunning = false;
    return "err run: no se pudo crear la tarea";
  }
  lastEvent = fmt("secuencia %s iniciada", p.test);
  return fmt("ok run %s", p.test);
}

void logSample() {
  if (!seqRunning || logCount >= LOG_ROWS) return;
  LogRow &r = logRows[logCount++];
  r.tMs = millis() - seqStartMs;
  r.step = seqStep;
  r.duty1 = lroundf(motors[0].duty * 10);
  r.duty2 = lroundf(motors[1].duty * 10);
  r.iMa = constrain(lroundf(iMean * 1000), -32768, 32767);
  r.pkMa = constrain(lroundf(iPeak * 1000), -32768, 32767);
  r.minMa = constrain(lroundf(iMin * 1000), -32768, 32767);
  r.vbusMv = isnan(vbus) ? 0 : lroundf(vbus * 1000);
}

// ---------- comandos ----------

const char HELP[] =
    "Comandos (canal: 1, 2 o all):\n"
    "  m <canal> <duty>   duty de -100 a 100 %; positivo = OUT1 -> OUT2. Despierta el driver\n"
    "  brake <canal>      EN = 0: freno por los low-side (duty 0 inmediato)\n"
    "  coast <canal>      nSLEEP = 0: puente libre (alta impedancia)\n"
    "  stop               HALT: ambos libres, aborta la secuencia y bloquea toda orden hasta release\n"
    "  release            borra el HALT (boton Liberar del panel)\n"
    "  ramp <%/s>         rampa de duty (0 = escalon)\n"
    "  ilim <A>           HALT por corriente media de 100 ms (0.1-9 A)\n"
    "  deadman <ms>       HALT si no hay comandos ni /status.json en ese tiempo (0 = apagado)\n"
    "  freq <Hz>          frecuencia de PWM (1000-50000)\n"
    "  zero               calibra el cero del ACS725 (motores en libre)\n"
    "  peaks              reinicia el pico y el minimo de corriente\n"
    "  status             estado en JSON\n"
    "  ping               mantiene vivo el deadman\n"
    "Las secuencias de prueba se inician desde el panel web (http://192.168.4.1/).";

// Máscara de canales: bit 0 = M1, bit 1 = M2. 0 si no es válido.
int parseChannels(const char *s) {
  if (!strcmp(s, "1")) return 1;
  if (!strcmp(s, "2")) return 2;
  if (!strcmp(s, "all") || !strcmp(s, "*")) return 3;
  return 0;
}

String statusJson();
enum class Src { Serial, Web };

String handleCommand(String line, Src src) {
  line.trim();
  line.toLowerCase();
  lastContact = millis();
  char cmd[16] = "", a[16] = "";
  float v = NAN;
  int n = sscanf(line.c_str(), "%15s %15s %f", cmd, a, &v);
  if (n < 1) return "";
  Lock l;

  // Siempre disponibles, también durante una secuencia o un HALT
  if (!strcmp(cmd, "help")) return HELP;
  if (!strcmp(cmd, "status")) return statusJson();
  if (!strcmp(cmd, "ping")) return "ok";
  if (!strcmp(cmd, "stop")) {
    halt(src == Src::Web ? "STOP desde el panel" : "STOP por USB");
    for (auto &m : motors) coast(m);
    lastEvent = fmt("HALT: %s", haltReason);
    return "ok stop: HALT activo, usar release (Liberar) para volver a mover";
  }

  if (seqRunning) return "err secuencia en curso: solo se acepta STOP";

  if (!strcmp(cmd, "run")) {
    if (src != Src::Web) return "err las secuencias se inician desde el panel web (http://192.168.4.1/)";
    return startSequence(line);
  }
  if (!strcmp(cmd, "release")) {
    if (!halted) return "ok release (no habia HALT)";
    halted = false;
    haltReason[0] = 0;
    lastEvent = "HALT liberado";
    return "ok release";
  }
  if (!strcmp(cmd, "zero")) return calibrateZero();
  if (!strcmp(cmd, "peaks")) {
    peakReset = true;
    vbusMin = vbusMax = vbus;
    return "ok peaks";
  }

  // Ajustes con un valor numérico en el segundo campo
  if (!strcmp(cmd, "ramp") || !strcmp(cmd, "ilim") || !strcmp(cmd, "deadman") || !strcmp(cmd, "freq")) {
    char *end = a;
    float x = n >= 2 ? strtof(a, &end) : NAN;
    if (n < 2 || end == a || *end) return fmt("err %s: falta el valor numerico", cmd);
    if (!strcmp(cmd, "ramp")) {
      if (x < 0 || x > 10000) return "err ramp: 0-10000 %/s";
      rampRate = x;
      return fmt("ok ramp %.0f %%/s", rampRate);
    }
    if (!strcmp(cmd, "ilim")) {
      if (x < 0.1f || x > 9) return "err ilim: 0.1-9 A";
      iLimit = x;
      return fmt("ok ilim %.2f A", iLimit);
    }
    if (!strcmp(cmd, "deadman")) {
      if (x < 0 || x > 600000) return "err deadman: 0-600000 ms";
      deadmanMs = x;
      return fmt("ok deadman %lu ms", (unsigned long)deadmanMs);
    }
    if (x < 1000 || x > 50000) return "err freq: 1000-50000 Hz";
    if (ledc_set_freq(LEDC_LOW_SPEED_MODE, PWM_TIMER, (uint32_t)x) != ESP_OK) return "err freq: ledc rechazo";
    pwmHz = x;
    return fmt("ok freq %lu Hz", (unsigned long)pwmHz);
  }

  // Comandos por canal
  int mask = n >= 2 ? parseChannels(a) : 0;
  if (!strcmp(cmd, "m") || !strcmp(cmd, "brake") || !strcmp(cmd, "coast")) {
    if (!mask) return fmt("err %s: canal 1, 2 o all", cmd);
    if (!strcmp(cmd, "m") && (n < 3 || v < -100 || v > 100)) return "err m: duty de -100 a 100";
    if (halted && strcmp(cmd, "coast")) return fmt("err HALT activo (%s): Liberar primero", haltReason);
    for (int i = 0; i < 2; i++) {
      if (!(mask & (1 << i))) continue;
      Motor &m = motors[i];
      if (!strcmp(cmd, "m")) {
        wake(m);
        m.target = v;
      } else if (!strcmp(cmd, "brake")) {
        brake(m);
      } else {
        coast(m);
      }
    }
    return n >= 3 ? fmt("ok %s %s %.1f", cmd, a, v) : fmt("ok %s %s", cmd, a);
  }
  return fmt("err comando desconocido '%s' (help)", cmd);
}

// ---------- estado ----------

String num(float v, int decimals) { return isnan(v) ? String("null") : String(v, decimals); }
const char *tf(bool b) { return b ? "true" : "false"; }

String jsonEscape(const char *s) {
  String o;
  for (; *s; s++) {
    if (*s == '"' || *s == '\\') o += '\\';
    o += *s;
  }
  return o;
}

String statusJson() {
  Lock l;
  char buf[1800];
  float zero = isensZeroMv, mean = iMean;
  uint32_t seqElapsed = seqStartMs ? ((seqRunning ? millis() : seqEndMs) - seqStartMs) : 0;
  snprintf(buf, sizeof buf,
           "{\"uptime_ms\":%lu,\"boot_count\":%lu,\"reset_reason\":\"%s\",\"brownouts\":%lu,"
           "\"vbus_v\":%s,\"vbus_min_v\":%s,\"vbus_max_v\":%s,\"ina226_ok\":%s,"
           "\"isens_mv\":%.1f,\"isens_a\":%.3f,\"isens_max_a\":%.3f,\"isens_min_a\":%.3f,"
           "\"isens_zero_mv\":%.1f,\"isens_zero_cal\":%s,\"isens_samples\":%lu,"
           "\"chip_temp_c\":%.1f,\"free_heap\":%lu,\"min_free_heap\":%lu,"
           "\"wifi_mode\":\"ap\",\"ip\":\"%s\",\"rssi_dbm\":0,\"ap_clients\":%d,"
           "\"motors\":[{\"name\":\"%s\",\"target\":%.1f,\"duty\":%.1f,\"awake\":%s},"
           "{\"name\":\"%s\",\"target\":%.1f,\"duty\":%.1f,\"awake\":%s}],"
           "\"ramp_pct_s\":%.0f,\"ilim_a\":%.2f,\"deadman_ms\":%lu,\"pwm_hz\":%lu,"
           "\"halted\":%s,\"halt_reason\":\"%s\",\"last_event\":\"%s\","
           "\"seq\":{\"running\":%s,\"test\":\"%s\",\"step\":\"%s\",\"elapsed_s\":%.1f,"
           "\"pass\":%d,\"warn\":%d,\"fail\":%d,\"log_rows\":%d}}",
           (unsigned long)millis(), (unsigned long)bootCount, reasonName(resetReason), (unsigned long)brownoutCount,
           num(vbus, 3).c_str(), num(vbusMin, 3).c_str(), num(vbusMax, 3).c_str(), tf(inaOk),
           zero + mean * ISENS_MV_PER_A, mean, (float)iPeak, (float)iMin, zero, tf(isensZeroCal),
           (unsigned long)isensSamples, temperatureRead(), (unsigned long)ESP.getFreeHeap(),
           (unsigned long)ESP.getMinFreeHeap(), WiFi.softAPIP().toString().c_str(), WiFi.softAPgetStationNum(),
           motors[0].name, motors[0].target, motors[0].duty, tf(motors[0].awake), motors[1].name, motors[1].target,
           motors[1].duty, tf(motors[1].awake), rampRate, iLimit, (unsigned long)deadmanMs, (unsigned long)pwmHz,
           tf(halted), jsonEscape(haltReason).c_str(), jsonEscape(lastEvent.c_str()).c_str(), tf(seqRunning), seq.test,
           jsonEscape(seqLabels[seqStep]).c_str(), seqElapsed / 1000.0f, seqPass, seqWarn, seqFail, logCount);
  return String(buf);
}

String motorText(const Motor &m) {
  if (!m.awake) return "libre";
  return fmt("%+6.1f%% (obj %+.0f)", m.duty, m.target);
}

// Mientras se envía, loop() no corre: por eso no se permite durante una secuencia
void sendLogCsv() {
  if (seqRunning) {
    server.send(409, "text/plain; charset=utf-8", "secuencia en curso: descargar al terminar");
    return;
  }
  server.sendHeader("Content-Disposition", "attachment; filename=orca_motor_log.csv");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/csv", "t_s,step,label,duty1,duty2,isens_a,isens_max_a,isens_min_a,vbus_v\n");
  String chunk;
  for (int i = 0; i < logCount; i++) {
    const LogRow &r = logRows[i];
    char lab[24];
    {
      Lock l;
      snprintf(lab, sizeof lab, "%s", seqLabels[r.step]);
    }
    chunk += fmt("%.1f,%u,%s,%.1f,%.1f,%.3f,%.3f,%.3f,%.3f\n", r.tMs / 1000.0f, r.step, lab, r.duty1 / 10.0f,
                 r.duty2 / 10.0f, r.iMa / 1000.0f, r.pkMa / 1000.0f, r.minMa / 1000.0f, r.vbusMv / 1000.0f);
    if (chunk.length() > 1200) {
      server.sendContent(chunk);
      chunk = "";
    }
  }
  if (chunk.length()) server.sendContent(chunk);
  server.sendContent("");
  lastContact = millis();
}

// ---------- web ----------

const char PAGE[] PROGMEM = R"html(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>ORCA motores</title>
<style>body{font-family:monospace;margin:16px;background:#111;color:#eee;max-width:720px}
.card{border:1px solid #333;border-radius:6px;padding:10px;margin:10px 0}input[type=range]{width:100%}
fieldset{border:1px solid #333;border-radius:6px;margin:10px 0;padding:10px}fieldset:disabled{opacity:.45}
legend{color:#aaa}button{font:inherit;padding:6px 10px;margin:2px;background:#222;color:#eee;border:1px solid #555;border-radius:4px}
button:disabled{opacity:.4}#stop{width:100%;padding:16px;background:#a00;border-color:#f44;font-weight:bold;font-size:1.2em}
#halt{display:none;background:#400;border-color:#f44}#start{background:#063;border-color:#0a5;font-weight:bold}
.bad{color:#f66}.ok{color:#6f6}.warn{color:#fc4}.dim{color:#999}input[type=number],select{width:70px;font:inherit;background:#222;color:#eee;border:1px solid #555}
label{display:inline-block;margin:3px 8px 3px 0}pre{white-space:pre-wrap;background:#0a0a0a;padding:8px;max-height:420px;overflow:auto}
</style></head><body>
<h3>ORCA-BOARD — banco de motores</h3><div id="st" class="dim">esperando…</div>
<button id="stop" onclick="cmd('stop')">STOP (barra espaciadora)</button>
<div class="card" id="halt"><b class="bad">HALT</b> <span id="haltr"></span><br>
<span class="dim">Toda orden de movimiento está bloqueada.</span> <button onclick="cmd('release')">Liberar</button></div>
<div class="card">I media <b id="im">-</b> A · pico <span id="ip">-</span> · min <span id="in">-</span>
<br>Vbus (J19) <span id="vb">-</span> V · cero ACS725 <span id="z">-</span> mV<br>
<span class="dim" id="ev"></span> · <span class="dim" id="resp"></span></div>

<fieldset id="fseq"><legend>Secuencia de prueba</legend>
<label>prueba <select id="test"><option>full</option><option>sweep</option><option>step</option><option>reverse</option>
<option>soak</option><option>both</option><option>zero</option></select></label>
<label>canal <select id="ch"><option>1</option><option>2</option></select></label>
<label>duty <input type="number" id="duty" value="50"> %</label>
<label>paso <input type="number" id="step" value="20"> %</label>
<label>máx <input type="number" id="max" value="100"> %</label>
<label>tiempo <input type="number" id="time" placeholder="auto" step="0.5"> s</label>
<label>ciclos <input type="number" id="cycles" value="3"></label>
<label>asentar <input type="number" id="settle" value="1" step="0.1"> s</label>
<label>medir <input type="number" id="dwell" value="1" step="0.1"> s</label>
<label>rampa <input type="number" id="sramp" placeholder="placa"> %/s</label>
<label>ilim <input type="number" id="silim" placeholder="placa" step="0.1"> A</label><br>
<button id="start" onclick="startSeq()">Iniciar secuencia</button>
<span class="dim">canal no aplica a full ni both · tiempo: step 2 s, reverse 2 s por sentido, soak 60 s</span>
</fieldset>
<div class="card"><span id="seqst" class="dim">sin secuencia</span> · <a href="/log.csv" style="color:#8cf">descargar CSV</a>
<pre id="rep"></pre></div>

<fieldset id="fman"><legend>Control manual</legend><div id="motors"></div>
rampa <input type="number" id="ramp"> %/s · ilim <input type="number" id="ilim" step="0.1"> A ·
deadman <input type="number" id="deadman"> ms · PWM <input type="number" id="freq"> Hz
<button onclick="applyCfg()">aplicar</button> <button onclick="cmd('zero')">zero</button>
<button onclick="cmd('peaks')">reiniciar pico</button></fieldset>
<script>
const $=id=>document.getElementById(id);let dragging=[false,false],cfgLoaded=false,sendT=[0,0],lastRep=0,wasRunning=false;
async function cmd(c){try{const r=await fetch('/cmd?c='+encodeURIComponent(c));const t=await r.text();$('resp').textContent=t;return t}
catch(e){$('resp').textContent='sin respuesta'}}
for(let i=0;i<2;i++){$('motors').insertAdjacentHTML('beforeend',`<div class="card"><b id="n${i}"></b> <span id="s${i}"></span>
<input type="range" min="-100" max="100" step="1" value="0" id="r${i}"><div>
<button onclick="setM(${i},0)">0</button><button onclick="cmd('brake ${i+1}')">freno</button>
<button onclick="cmd('coast ${i+1}')">libre</button>
${[-100,-50,-25,25,50,100].map(d=>`<button onclick="setM(${i},${d})">${d}</button>`).join('')}</div></div>`);
const r=$('r'+i);r.oninput=()=>{dragging[i]=true;const now=Date.now();if(now-sendT[i]>100){sendT[i]=now;cmd(`m ${i+1} ${r.value}`)}};
r.onchange=()=>{cmd(`m ${i+1} ${r.value}`);dragging[i]=false}}
function setM(i,d){$('r'+i).value=d;cmd(`m ${i+1} ${d}`)}
function applyCfg(){['ramp','ilim','deadman','freq'].forEach(k=>cmd(k+' '+$(k).value))}
function startSeq(){let c='run '+$('test').value;
[['ch','ch'],['duty','duty'],['step','step'],['max','max'],['time','time'],['cycles','cycles'],['settle','settle'],
['dwell','dwell'],['ramp','sramp'],['ilim','silim']].forEach(([k,id])=>{const v=$(id).value.trim();if(v!=='')c+=` ${k}=${v}`});
$('rep').textContent='';cmd(c).then(()=>loadRep())}
// Espacio = STOP. Se anula también el keyup: si no, el botón con foco (p. ej. Liberar) recibiría el clic y anularía el STOP
const isSpace=e=>e.code==='Space'&&!['INPUT','SELECT'].includes(e.target.tagName);
document.addEventListener('keydown',e=>{if(isSpace(e)){e.preventDefault();document.activeElement.blur();if(!e.repeat)cmd('stop')}});
document.addEventListener('keyup',e=>{if(isSpace(e))e.preventDefault()});
function esc(s){return s.replace(/&/g,'&amp;').replace(/</g,'&lt;')}
async function loadRep(){try{const t=await (await fetch('/seq.txt',{cache:'no-store'})).text();const p=$('rep');
const atEnd=p.scrollTop+p.clientHeight>=p.scrollHeight-20;
p.innerHTML=esc(t).replace(/\[PASS\]/g,'<span class="ok">[PASS]</span>').replace(/\[WARN\]/g,'<span class="warn">[WARN]</span>')
.replace(/\[FAIL\]/g,'<span class="bad">[FAIL]</span>');if(atEnd)p.scrollTop=p.scrollHeight}catch(e){}}
async function tick(){try{const j=await (await fetch('/status.json',{cache:'no-store'})).json();const s=j.seq;
$('st').innerHTML='<span class="ok">EN LÍNEA</span> · PWM '+j.pwm_hz+' Hz · arranque #'+j.boot_count+' ('+j.reset_reason+')';
j.motors.forEach((m,i)=>{$('n'+i).textContent=m.name;$('s'+i).textContent=m.awake?`duty ${m.duty.toFixed(1)} % (obj ${m.target})`:'libre';
if(!dragging[i])$('r'+i).value=m.target});
$('im').textContent=j.isens_a.toFixed(3);$('ip').textContent=j.isens_max_a.toFixed(3);$('in').textContent=j.isens_min_a.toFixed(3);
$('vb').textContent=j.vbus_v===null?'-':j.vbus_v.toFixed(2);$('z').textContent=j.isens_zero_mv.toFixed(0)+(j.isens_zero_cal?'':' (nominal)');
$('ev').textContent='último evento: '+j.last_event;$('halt').style.display=j.halted?'block':'none';$('haltr').textContent=j.halt_reason;
$('fseq').disabled=s.running||j.halted;$('fman').disabled=s.running||j.halted;
const res=`${s.pass} PASS, ${s.warn} WARN, ${s.fail} FAIL`;
$('seqst').innerHTML=s.running?`<span class="warn">EN CURSO</span> ${s.test} · ${esc(s.step)} · ${s.elapsed_s.toFixed(0)} s · ${res}`
:(s.test?`terminada: ${s.test} · ${res} · ${s.elapsed_s.toFixed(0)} s · ${s.log_rows} filas`:'sin secuencia');
const now=Date.now();if(s.running&&now-lastRep>1000||wasRunning&&!s.running){lastRep=now;loadRep()}wasRunning=s.running;
if(!cfgLoaded){$('ramp').value=j.ramp_pct_s;$('ilim').value=j.ilim_a;$('deadman').value=j.deadman_ms;$('freq').value=j.pwm_hz;cfgLoaded=true;loadRep()}}
catch(e){$('st').innerHTML='<span class="bad">SIN RESPUESTA</span> (el deadman de la placa los deja libres y en HALT)'}}
setInterval(tick,250);tick();</script></body></html>)html";

void setup() {
  // Primero los drivers: el pull-up de nSLEEP los despertó al encender
  for (auto &m : motors) {
    pinMode(m.pinSleep, OUTPUT);
    digitalWrite(m.pinSleep, LOW);
    pinMode(m.pinPh, OUTPUT);
    digitalWrite(m.pinPh, LOW);
  }
  ledc_timer_config_t tc = {};
  tc.speed_mode = LEDC_LOW_SPEED_MODE;
  tc.duty_resolution = PWM_BITS;
  tc.timer_num = PWM_TIMER;
  tc.freq_hz = pwmHz;
  tc.clk_cfg = LEDC_AUTO_CLK;
  ledc_timer_config(&tc);
  for (auto &m : motors) {
    ledc_channel_config_t cc = {};
    cc.gpio_num = m.pinEn;
    cc.speed_mode = LEDC_LOW_SPEED_MODE;
    cc.channel = m.ledc;
    cc.timer_sel = PWM_TIMER;
    cc.duty = 0;
    ledc_channel_config(&cc);
  }
  stateMux = xSemaphoreCreateRecursiveMutex();
  snprintf(seqLabels[0], sizeof seqLabels[0], "-");

  Serial.begin(115200);
  // Con el USB conectado pero sin nadie leyendo, el core bloquea 100 ms por escritura: frenaba la secuencia
  // y loop(). Con 0 se descarta lo que no cabe (el informe completo está en el panel).
  Serial.setTxTimeoutMs(0);
  resetReason = esp_reset_reason();
  prefs.begin("orca", false);
  bootCount = prefs.getUInt("boots", 0) + 1;
  prefs.putUInt("boots", bootCount);
  brownoutCount = prefs.getUInt("brownouts", 0);
  if (resetReason == ESP_RST_BROWNOUT) prefs.putUInt("brownouts", ++brownoutCount);

  obI2c.begin(OB_SDA, OB_SCL, 400000);
  analogSetPinAttenuation(ISENS_PIN, ADC_11db);
  calibrateZero();
  xTaskCreatePinnedToCore(samplerTask, "isens", 4096, nullptr, 2, nullptr, 1);

  WiFi.setHostname(HOSTNAME);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  WiFi.setSleep(false);
  MDNS.begin(HOSTNAME);

  server.on("/", [] { server.send_P(200, "text/html", PAGE); });
  server.on("/cmd", [] {
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "text/plain; charset=utf-8", handleCommand(server.arg("c"), Src::Web));
  });
  server.on("/status.json", [] {
    lastContact = millis();
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", statusJson());
  });
  server.on("/seq.txt", [] {
    String r;
    {
      Lock l;
      r = seqReport;
    }
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "text/plain; charset=utf-8", r);
  });
  server.on("/log.csv", sendLogCsv);
  server.on("/reset_max", [] {  // compatibilidad con wifi_telemetry
    handleCommand("peaks", Src::Web);
    server.send(200, "text/plain", "ok");
  });
  server.begin();

  Serial.printf("\nORCA banco de motores | arranque #%lu (%s) | AP %s -> http://%s/ | cero ACS725 %.1f mV%s\n",
                (unsigned long)bootCount, reasonName(resetReason), AP_SSID, WiFi.softAPIP().toString().c_str(),
                (float)isensZeroMv, isensZeroCal ? "" : " (nominal)");
  Serial.println("Escribir 'help'. Deadman: HALT tras 2 s sin comandos con un motor activo ('deadman 0' lo apaga).");
}

void loop() {
  static uint32_t lastControl = 0, lastVbus = 0, lastTele = 0, lastLog = 0;
  static String rx;
  server.handleClient();

  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (rx.length()) Serial.println(handleCommand(rx, Src::Serial));
      rx = "";
    } else if (rx.length() < 80) {
      rx += c;
    }
  }

  uint32_t now = millis();
  if (now - lastControl >= CONTROL_PERIOD_MS) {
    controlStep((now - lastControl) / 1000.0f);
    lastControl = now;
  }
  if (deadmanMs && anyAwake() && !halted && now - lastContact > deadmanMs) {
    Lock l;
    halt(fmt("deadman: %lu ms sin comandos", (unsigned long)deadmanMs).c_str());
    for (auto &m : motors) coast(m);
    lastEvent = fmt("HALT: %s", haltReason);
    Serial.println(lastEvent);
  }
  if (now - lastVbus >= VBUS_PERIOD_MS) {
    lastVbus = now;
    readVbus();
  }
  if (now - lastLog >= LOG_PERIOD_MS) {
    lastLog = now;
    logSample();
  }
  if (now - lastTele >= TELEMETRY_PERIOD_MS) {
    lastTele = now;
    if (anyAwake()) {
      Serial.printf("M1 %s | M2 %s | I %.3f A (pico %.3f, min %.3f) | Vbus %s V\n", motorText(motors[0]).c_str(),
                    motorText(motors[1]).c_str(), (float)iMean, (float)iPeak, (float)iMin, num(vbus, 2).c_str());
    }
  }
}
