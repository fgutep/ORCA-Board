# Ejemplos — ORCA-Board Control Rev A

Firmware de ejemplo que usa la placa de control en proyectos reales. Para el bring-up y las pruebas de la placa, ver [`Hardware/physical_debug/`](../Hardware/physical_debug/).

## Brazo por gestos (proyecto `arm_gesture`)

La placa reemplaza las dos tarjetas ESP32-S3 del brazo por gestos con el mismo protocolo, así que el GUI web no cambia.

| Ejemplo | Qué hace | Compilación |
|---|---|---|
| [`orca_gesture_imu/`](orca_gesture_imu/) | El BNO085 (U1) como IMU del brazo, mismo protocolo USB y BLE que la tarjeta con MPU6050; nombre BLE "ORCA". | PlatformIO, core 2.0.14 (Bluedroid) |
| [`orca_gesture_arm/`](orca_gesture_arm/orca_gesture_arm.ino) | El PCA9685 (U9, 0x41) maneja los 4 servos del brazo con el protocolo JSON de la tarjeta de servos (fw 3): rampa, banda muerta, límites en NVS, homing sincronizado y E-STOP. | Sketch Arduino, core 3.x + ArduinoJson 7 |

### Servos: conexiones

| Servo | Conector ORCA | Canal PCA9685 |
|---|---|---|
| Base | J6 | LED0 |
| Hombro | J7 | LED1 |
| Codo | J9 | LED3 |
| Pinza | J8 | LED2 |

- **Alimentación de servos:** J6–J9 salen de 5V_SERVOS, el buck TPS54331 alimentado desde VBUS. **Con USB solo los servos no se mueven.** El sketch reporta VBUS (INA226) en cada estado y avisa (`no VBUS: servos unpowered`) si se activa el torque por debajo de 6 V.
- **Límite de corriente:** al activar el torque los cuatro servos van a su pose a la vez. Con un límite bajo en la fuente, VBUS cae y oscila entre 0 y 9 V.
- **Limp:** bit FULL_OFF por canal (~OE del PCA9685 está a GND). La placa arranca con los servos sin torque.
- **Oscilador del PCA9685:** 25 MHz nominales, varía entre chips y escala el ancho de pulso. Ajuste con `{"osc":<Hz>}` midiendo un pulso de 1500 us con osciloscopio, luego `{"save":1}`.
- **Después de flashear:** la placa puede quedar en `waiting for download` (el reset por USB no vuelve a leer los straps). Reiniciar con `esptool --port /dev/ttyACM0 --before no-reset --after watchdog-reset chip-id` o reconectando el USB.

Probado el 24-nov-2026 con el brazo real a 9 V: homing y pick & place hasta 90 °/s.
