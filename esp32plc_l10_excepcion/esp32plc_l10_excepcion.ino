// Practica L10 - Reto 1: transmitir por excepcion. El nodo lee el Controllino continuamente,
// pero solo transmite si la temperatura cambia +-2 C, el nivel +-2 % o cambia una alarma,
// y ademas envia un latido cada 30 min para saber que sigue vivo. SF9 (DR1), US915 FSB 2.
// En DR0 (SF10) la carga maxima de US915 es 11 B: los 9 B mas las respuestas MAC que el
// servidor pide en la bajada no caben y el RN2903 contesta invalid_data_len.
// Autor: Diego Caceres
#include <ModbusRTU.h>              // modbus-esp8266 (maestro, como en L6)
#include <RTClib.h>                 // DS3231 por I2C
#include <rn2xx3.h>                 // RN2483-Arduino-Library (jpmeijers)

#define LORA_SERIAL loraSerial      // nombre del puerto del slot LORA en este paquete de placas
#define SLAVE_ID 1
#define N_REGS 10                   // [0..7] canonicos + [8] A1 + [9] estado

// Identidades OTAA de ChirpStack. NO subir al repositorio con las claves reales.
const char* JOIN_EUI = "0000000000000000";              // 16 caracteres hex
const char* APP_KEY  = "00000000000000000000000000000000";               // 32 caracteres hex

const int DR = 1;                                // SF9: 205,8 ms por trama de 22 B
const uint16_t BANDA_T = 20;                     // +-2,0 C en decimas
const uint16_t BANDA_N = 20;                     // +-2,0 % en decimas
const unsigned long T_LATIDO = 1800000UL;        // 30 min sin cambios -> latido
const unsigned long T_MIN = 20700UL;             // 1 % de ciclo de trabajo con 205,8 ms

ModbusRTU mb;
RTC_DS3231 rtc;
rn2xx3 lora(LORA_SERIAL);
uint16_t reg[N_REGS];
uint8_t trama[9];
volatile bool listo = false;

bool cbLectura(Modbus::ResultCode ev, uint16_t, void*) {
  listo = (ev == Modbus::EX_SUCCESS);
  if (!listo) Serial.printf("Modbus error 0x%02X\n", ev);
  return true;
}

uint16_t aDecimas(uint16_t c) { return (uint32_t)c * 1000UL / 1023UL; }

// Fig. L10.4: byte 0 = VER (b7-b4) = 1 | A_T (b3) | A_N (b2); bytes 1-2 TEMP y 3-4 NIV
// (decimas, big-endian); bytes 5-8 marca de tiempo Unix del DS3231.
void empaquetar(uint16_t temp, uint16_t niv, uint8_t aT, uint8_t aN) {
  uint32_t ts = rtc.now().unixtime();
  trama[0] = (1 << 4) | (aT << 3) | (aN << 2);
  trama[1] = temp >> 8;  trama[2] = temp & 0xFF;
  trama[3] = niv >> 8;   trama[4] = niv & 0xFF;
  trama[5] = ts >> 24;   trama[6] = ts >> 16;  trama[7] = ts >> 8;  trama[8] = ts;
}

void setup() {
  Serial.begin(115200);
  RS485.begin(19200, SERIAL_8N1);
  mb.begin(&RS485); mb.master();
  rtc.begin();
  LORA_SERIAL.begin(57600);
  delay(1000);
  lora.autobaud();
  Serial.println(lora.hweui());
  Serial.println(lora.sysver());

  lora.setFrequencyPlan(TTN_US);
  while (!lora.initOTAA(JOIN_EUI, APP_KEY)) {
    Serial.println("Join rechazado; reintento en 30 s");
    delay(30000);
  }
  Serial.println("Union completada: sesion activa");
  lora.setDR(DR);
}

void loop() {
  if (!mb.slave()) {
    listo = false;
    mb.readHreg(SLAVE_ID, 0, reg, N_REGS, cbLectura);       // FC3, base 0
  }
  mb.task(); yield();
  if (!listo) return;

  uint16_t temp = aDecimas(reg[0]);
  uint16_t niv  = aDecimas(reg[8]);
  uint8_t aT = reg[9] & 0x01;
  uint8_t aN = (reg[9] >> 1) & 0x01;

  static bool primera = true;
  static uint16_t t_env = 0, n_env = 0;          // ultimo valor ENVIADO, no el ultimo leido
  static uint8_t alm_env = 0;
  static uint32_t ultimo = 0, n_tx = 0;

  const char* motivo = nullptr;
  if (primera) motivo = "inicio";
  else if (abs((int)temp - (int)t_env) >= BANDA_T) motivo = "temperatura";
  else if (abs((int)niv - (int)n_env) >= BANDA_N) motivo = "nivel";
  else if (((aT << 1) | aN) != alm_env) motivo = "alarma";
  else if (millis() - ultimo >= T_LATIDO) motivo = "latido";
  if (!motivo) return;
  if (!primera && millis() - ultimo < T_MIN) return;      // espera al 1 % antes de transmitir

  primera = false;
  ultimo = millis();
  t_env = temp; n_env = niv; alm_env = (aT << 1) | aN;
  empaquetar(temp, niv, aT, aN);
  TX_RETURN_TYPE r = lora.txBytes(trama, sizeof(trama));
  n_tx++;
  Serial.printf("TX #%lu %s por %s: TEMP %u NIV %u ALM_T %u ALM_N %u\n", (unsigned long)n_tx,
                r == TX_FAIL ? "fallida" : "enviada", motivo, temp, niv, aT, aN);
}
