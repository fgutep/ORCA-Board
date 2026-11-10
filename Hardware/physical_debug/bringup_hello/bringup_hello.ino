// ORCA-BOARD Control Rev A — firmware de bring-up
// Imprime la identidad del chip por el USB nativo (USB-Serial/JTAG) cada segundo.
// Si se ve esta salida, el bootloader, la tabla de particiones y la app arrancan bien.

#include <Arduino.h>
#include <esp_chip_info.h>
#include <esp_flash.h>
#include <esp_mac.h>

void setup() {
  Serial.begin(115200);
  delay(1500);  // dar tiempo a que el host abra /dev/ttyACM0
}

void loop() {
  esp_chip_info_t info;
  esp_chip_info(&info);

  uint32_t flash_size = 0;
  esp_flash_get_size(NULL, &flash_size);

  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);

  Serial.printf("[ORCA bring-up] %s rev v%d.%d, %d cores, %lu MHz | flash %lu MB | PSRAM %s | "
                "MAC %02x:%02x:%02x:%02x:%02x:%02x | uptime %lu s\n",
                ESP.getChipModel(), info.revision / 100, info.revision % 100, info.cores,
                (unsigned long)ESP.getCpuFreqMHz(), (unsigned long)(flash_size >> 20),
                psramFound() ? "si" : "no", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                (unsigned long)(millis() / 1000));
  delay(1000);
}
