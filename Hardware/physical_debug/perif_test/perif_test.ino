// ORCA-BOARD Control Rev A — prueba de periféricos
// Cada 5 s imprime por el USB nativo el estado de: I2C de placa (INA226, PCA9685, BNO085),
// mux I2C externo (TCA9548A), Isens (ACS725 en IO9) y CAN (SN65HVD230 en autorrecepción).
// Todas las pruebas I2C son de solo lectura: mientras INA226 y PCA9685 compartan 0x40,
// cualquier escritura de registro llegaría a los dos.

#include <Arduino.h>
#include <Wire.h>
#include <driver/twai.h>
#include <esp_chip_info.h>
#include <esp_mac.h>

// Pines (ver 05_revision_CAN_I2C_sensores.md)
constexpr int OB_SDA = 18, OB_SCL = 17;                  // I2C de placa
constexpr int EXT_SDA = 39, EXT_SCL = 38, EXT_RST = 40;  // I2C externo (TCA9548A)
constexpr int ISENS_PIN = 9;                             // ACS725 VIOUT
constexpr gpio_num_t CAN_TX = GPIO_NUM_6, CAN_RX = GPIO_NUM_7;

constexpr uint8_t ADDR_INA226 = 0x40, ADDR_PCA9685_ALT = 0x41, ADDR_BNO085 = 0x4A,
                  ADDR_BNO085_ALT = 0x4B, ADDR_ALLCALL = 0x70, ADDR_TCA9548A = 0x70;

TwoWire &obI2c = Wire;
TwoWire &extI2c = Wire1;
bool canOk = false;
uint32_t cycle = 0;

bool present(TwoWire &bus, uint8_t addr) {
  bus.beginTransmission(addr);
  return bus.endTransmission() == 0;
}

void scan(TwoWire &bus, const char *label, uint8_t skip = 0) {
  Serial.printf("  %s:", label);
  int n = 0;
  for (uint8_t a = 1; a < 127; a++) {
    if (a != skip && present(bus, a)) {
      Serial.printf(" 0x%02X", a);
      n++;
    }
  }
  Serial.println(n ? "" : " (ninguno)");
}

// Lee un registro de 16 bits big-endian (formato INA226). -1 si no hay respuesta.
int32_t read16(TwoWire &bus, uint8_t addr, uint8_t reg) {
  bus.beginTransmission(addr);
  bus.write(reg);
  if (bus.endTransmission(false) != 0) return -1;
  if (bus.requestFrom(addr, (uint8_t)2) != 2) return -1;
  return (bus.read() << 8) | bus.read();
}

int32_t read8(TwoWire &bus, uint8_t addr, uint8_t reg) {
  bus.beginTransmission(addr);
  bus.write(reg);
  if (bus.endTransmission(false) != 0) return -1;
  if (bus.requestFrom(addr, (uint8_t)1) != 1) return -1;
  return bus.read();
}

void testOnboardI2c() {
  Serial.println("[I2C placa] IO18 SDA / IO17 SCL");
  scan(obI2c, "dispositivos");

  // INA226: Manufacturer ID (0xFE) = 0x5449. Con el PCA9685 también en 0x40 se lee
  // el AND de ambos (PRE_SCALE = 0x1E repetido) → ~0x1408.
  int32_t id = read16(obI2c, ADDR_INA226, 0xFE);
  int32_t bus = read16(obI2c, ADDR_INA226, 0x02);
  if (id == 0x5449) {
    Serial.printf("  [PASS] INA226 en 0x40 (ID 0x5449) | Vbus = %.3f V\n", bus * 0.00125f);
  } else if (id < 0) {
    Serial.println("  [FAIL] Nada responde en 0x40");
  } else {
    Serial.printf("  [FAIL] 0x40 responde pero ID = 0x%04lX (esperado 0x5449): "
                  "colision INA226/PCA9685 | Vbus crudo = 0x%04lX\n", id, bus);
  }

  // PCA9685 tras el retrabajo (A0 a VDD): MODE1 al encender = 0x11 (SLEEP + ALLCALL)
  if (present(obI2c, ADDR_PCA9685_ALT)) {
    Serial.printf("  [PASS] PCA9685 en 0x41 | MODE1 = 0x%02lX (0x11 al encender)\n",
                  read8(obI2c, ADDR_PCA9685_ALT, 0x00));
  } else {
    Serial.println("  [INFO] Nada en 0x41 (PCA9685 sigue en 0x40 hasta el retrabajo)");
  }
  Serial.printf("  [%s] PCA9685 LED All Call en 0x70\n",
                present(obI2c, ADDR_ALLCALL) ? "PASS" : "FAIL");

  bool bno = present(obI2c, ADDR_BNO085) || present(obI2c, ADDR_BNO085_ALT);
  Serial.printf("  [%s] BNO085 en 0x4A/0x4B\n", bno ? "PASS" : "FAIL");
}

void testMux() {
  Serial.println("[Mux I2C] IO39 SDA / IO38 SCL / IO40 RESET");
  if (!present(extI2c, ADDR_TCA9548A)) {
    Serial.println("  [FAIL] TCA9548A no responde en 0x70");
    return;
  }
  extI2c.beginTransmission(ADDR_TCA9548A);
  extI2c.write(0x01);
  extI2c.endTransmission();
  extI2c.requestFrom(ADDR_TCA9548A, (uint8_t)1);
  int ctrl = extI2c.available() ? extI2c.read() : -1;
  Serial.printf("  [%s] TCA9548A en 0x70 | control escrito 0x01, leido 0x%02X\n",
                ctrl == 0x01 ? "PASS" : "FAIL", ctrl);

  // Canales 0-3 = J14-J17. Sin pull-ups en la placa, solo aparecen módulos que traigan las suyas.
  static const char *conn[] = {"CH0 J14 (5V)", "CH1 J15 (5V)", "CH2 J16 (3V3)", "CH3 J17 (3V3)"};
  for (uint8_t ch = 0; ch < 4; ch++) {
    extI2c.beginTransmission(ADDR_TCA9548A);
    extI2c.write(1 << ch);
    extI2c.endTransmission();
    scan(extI2c, conn[ch], ADDR_TCA9548A);
  }
  extI2c.beginTransmission(ADDR_TCA9548A);
  extI2c.write(0x00);
  extI2c.endTransmission();
}

void testIsens() {
  // ACS725LLCTR-10AU: 0 A = 0.1 x VCC (330 mV), 264 mV/A
  uint32_t sum = 0;
  for (int i = 0; i < 64; i++) sum += analogReadMilliVolts(ISENS_PIN);
  float mv = sum / 64.0f;
  float amps = (mv - 330.0f) / 264.0f;
  bool zeroOk = mv > 250 && mv < 450;
  Serial.println("[Isens] ACS725 en IO9");
  Serial.printf("  [%s] VIOUT = %.0f mV -> I = %.2f A%s\n", zeroOk ? "PASS" : "INFO", mv, amps,
                zeroOk ? " (cero esperado ~330 mV)" : " (fuera de 250-450 mV: hay carga o falla)");
}

void testCan() {
  Serial.println("[CAN] SN65HVD230, TX IO6 / RX IO7, 500 kbit/s, NO_ACK + autorrecepcion");
  if (!canOk) {
    Serial.println("  [FAIL] No se pudo iniciar el driver TWAI");
    return;
  }
  // La trama sale por IO6, pasa por el transceptor al bus (R15 120 ohm) y vuelve por IO7.
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
  static const char *states[] = {"STOPPED", "RUNNING", "BUS_OFF", "RECOVERING"};
  Serial.printf("  [%s] eco %s | estado %s, TEC %lu, REC %lu, errores de bus %lu\n", got ? "PASS" : "FAIL",
                got ? "recibido" : (sent ? "no recibido" : "no transmitido"), states[st.state],
                st.tx_error_counter, st.rx_error_counter, st.bus_error_count);
  if (st.state == TWAI_STATE_BUS_OFF) twai_initiate_recovery();
  if (st.state == TWAI_STATE_STOPPED) twai_start();
}

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 3000) delay(10);

  pinMode(EXT_RST, OUTPUT);
  digitalWrite(EXT_RST, HIGH);  // RESET del TCA9548A no tiene pull-up en la placa
  obI2c.begin(OB_SDA, OB_SCL, 100000);
  extI2c.begin(EXT_SDA, EXT_SCL, 100000);
  analogSetPinAttenuation(ISENS_PIN, ADC_11db);

  twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX, CAN_RX, TWAI_MODE_NO_ACK);
  twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  canOk = twai_driver_install(&g, &t, &f) == ESP_OK && twai_start() == ESP_OK;
}

void loop() {
  esp_chip_info_t info;
  esp_chip_info(&info);
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  Serial.printf("\n===== ORCA perif_test #%lu | %s rev v%d.%d | MAC %02x:%02x:%02x:%02x:%02x:%02x | "
                "uptime %lu s =====\n", cycle, ESP.getChipModel(), info.revision / 100,
                info.revision % 100, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                (unsigned long)(millis() / 1000));

  testOnboardI2c();
  testMux();
  testIsens();
  testCan();

  cycle++;
  delay(5000);
}
