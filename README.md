# Práctica L10 · IIoT de largo alcance con LoRaWAN

Informática Industrial · MCIE · Universidad de Cuenca

**Autores:** Diego Armando Cáceres Lamar (diego.caceres@ucuenca.edu.ec)

Lleva por LoRaWAN el registro Modbus que el potenciómetro deja en un Controllino. El ESP32 PLC 14 es maestro Modbus RTU del Controllino y nodo LoRaWAN de clase A; ChirpStack decodifica la trama y Node-RED la publica en los tópicos de planta. Se hizo en el banco virtual 3D de la DEET (modalidad remota).

## Hardware (banco virtual)

| Equipo | Papel | Dirección |
|---|---|---|
| Controllino MEGA (tablero 1) | Esclavo Modbus RTU, potenciómetros P1 → A0 y P2 → A1 | dir. 1, 19200 8N1 |
| ESP32 PLC 14 + RN2903 | Maestro Modbus + nodo LoRaWAN clase A (OTAA) | DevEUI `0004A30B001F7A01` |
| Pasarela HT-M7603 | Packet forwarder Semtech UDP, 8 canales | 192.168.1.70 → 192.168.1.80:1700 |
| ChirpStack v4 (Docker) | Servidor de red y de aplicación | localhost:8089 |
| Broker Mosquitto / Node-RED | Aplicación y tablero | 192.168.1.50 / localhost:1890 |

## Mapa de registros (holding, base 0)

| Reg. | Fuente | Contenido | Escala |
|---|---|---|---|
| [0] | A0, P1 | Temperatura | 0–1023 → 0–100 °C |
| [1]–[7] | — | Reservados | — |
| [8] | A1, P2 | Nivel | 0–1023 → 0–100 % |
| [9] | Estado | b0: T fuera de [5, 30] °C · b1: nivel fuera de [20, 90] % | 0/1 |

## Formato de la trama (9 B, big-endian, FPort 1)

| Campo | Posición | Codificación |
|---|---|---|
| VER | byte 0, b7–b4 | versión = 1 |
| ALM_T | byte 0, b3 | 1 = alarma de temperatura |
| ALM_N | byte 0, b2 | 1 = alarma de nivel |
| Reservados | byte 0, b1–b0 | 0 |
| TEMP | bytes 1–2 | décimas de °C |
| NIV | bytes 3–4 | décimas de % |
| TS | bytes 5–8 | segundos Unix UTC (DS3231) |

Ejemplo: `18 01 58 02 1A 6A FF D8 E8` → 34,4 °C, 53,8 %, alarma de temperatura, TS 1795152104. Con los 13 B de LoRaWAN, la trama física ocupa 22 B (370,7 ms en SF10).

## Plan de frecuencias

US915, sub-banda 2 (`us915_1`): canales de subida 8–15 (903,9–905,3 MHz, 125 kHz) y 65 (904,6 MHz, 500 kHz); bajada RX1 en 923,3 + 0,6·(canal mod 8) MHz y RX2 en 923,3 MHz. El nodo usa `setFrequencyPlan(TTN_US)`; tras el JoinAccept la máscara queda en 8–15,65. ADR desactivado; SF fijado a mano (DR3 = SF7, DR1 = SF9, DR0 = SF10). Carga útil máxima: 11 B en DR0, 53 B en DR1.

## Archivos

| Archivo | Equipo | Qué hace |
|---|---|---|
| `controllino_esclavo_l10/` | Controllino 1 | Esclavo Modbus RTU con el mapa de arriba |
| `esp32plc_l10_nodo/` | ESP32 PLC 1 | Unión OTAA + maestro Modbus + trama de 9 B (Partes I–IV) |
| `esp32plc_l10_excepcion/` | ESP32 PLC 1 | Reto: transmite por excepción (±2 °C, ±2 %, alarma) + latido de 30 min, en SF9 |
| `esp32plc_lora_sf12/`, `esp32plc_lora_rx/` | ESP32 PLC 1 y 2 | SF12 en modo radio directo (Paso 13), emisor y receptor |
| `decodificador.js` | ChirpStack | Códec del perfil de dispositivo |
| `flujo_nodered_l10.json` | Node-RED | `application/+/device/+/event/up` → `planta/lorawan/...` + tablero |

## Claves

`JOIN_EUI` y `APP_KEY` están en ceros en este repositorio. Para cargar los programas, copie los valores de su dispositivo en ChirpStack. No suba nunca las claves reales: en un proyecto propio, póngalas en `credenciales.h`, que `.gitignore` excluye.
