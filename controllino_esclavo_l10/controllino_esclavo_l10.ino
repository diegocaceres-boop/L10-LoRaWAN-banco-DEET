// Practica L10 - Esclavo Modbus RTU en el Controllino 1 (direccion 1, 19200 8N1).
// Mapa holding (base 0): [0] = A0 (P1, temperatura), [8] = A1 (P2, nivel),
// [9] = palabra de estado: b0 alarma de temperatura (fuera de 5-30 C),
//                          b1 alarma de nivel (fuera de 20-90 %).
// [1]-[7] quedan reservados para el mapa de la L6.
// Autor: Diego Caceres
#include <Controllino.h>
#include <ArduinoRS485.h>
#include <ArduinoModbus.h>

const int ID = 1;                 // direccion del esclavo en el bus
const int N_REGS = 10;            // [0..9]

// Misma escala que el nodo: cuentas del ADC 0-1023 -> decimas 0-1000.
uint16_t aDecimas(int c) { return (uint32_t)c * 1000UL / 1023UL; }

void setup() {
  Serial.begin(9600);
  ModbusRTUServer.begin(RS485, ID, 19200, SERIAL_8N1);
  ModbusRTUServer.configureHoldingRegisters(0, N_REGS);
  Serial.println("Esclavo L10 listo: dir 1, 19200 8N1, [0]=A0 [8]=A1 [9]=estado");
}

void loop() {
  int a0 = analogRead(CONTROLLINO_A0);
  int a1 = analogRead(CONTROLLINO_A1);
  uint16_t t = aDecimas(a0);      // temperatura en decimas de grado
  uint16_t n = aDecimas(a1);      // nivel en decimas de %

  uint16_t estado = 0;
  if (t < 50 || t > 300) estado |= 0x01;    // b0: fuera de [5, 30] C
  if (n < 200 || n > 900) estado |= 0x02;   // b1: fuera de [20, 90] %

  ModbusRTUServer.holdingRegisterWrite(0, a0);
  ModbusRTUServer.holdingRegisterWrite(8, a1);
  ModbusRTUServer.holdingRegisterWrite(9, estado);
  ModbusRTUServer.poll();         // atiende una peticion del maestro, si llego

  static unsigned long t_print = 0;
  if (millis() - t_print >= 2000) {
    t_print = millis();
    Serial.print("A0="); Serial.print(a0);
    Serial.print("  A1="); Serial.print(a1);
    Serial.print("  estado="); Serial.println(estado, BIN);
  }
}
