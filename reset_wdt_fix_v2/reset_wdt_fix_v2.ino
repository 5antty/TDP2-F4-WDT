#include <Preferences.h>

#include "manejo_wdt.h"
#include "manejo_dht.h"
#include "manejo_wifi.h"
#include "manejo_webserver.h"

volatile uint8_t ledEnabled;
uint32_t cnt1, cnt2;
volatile char comando;
volatile int flagComando = 0;
Preferences prefs;

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- ACA ARRANCA ---");
  prefs.begin("wdt", false);
  cnt1 = prefs.getUInt("f1", 0);
  cnt2 = prefs.getUInt("f2", 0);

  initDHT();

  pinMode(PIN_LED, OUTPUT);
  ledEnabled = 0;
  digitalWrite(PIN_LED, LOW);

  initWifi();
  initTareaCore0();
  imprimirMenu();
  initServer();
}

void loop()
{

  // TODO setup() y loop() se ejecuta en el core 1
  actualizarEstado();
  delay(1000);
}

void imprimirMenu()
{
  Serial.println("\n=================================");
  Serial.println("  Elegir de que manera Provocar el reset de wdt ");
  Serial.println("=================================");
  Serial.println(" [1] - Operacion Normal (leo dht normal)");
  Serial.println(" [2] - Task WDT - Bloqueo por SW (tarea que demora tiempo mayor al timeout)");
  Serial.println(" [3] - Task WDT - Bloqueo por SW (while infinito)");
  Serial.println(" [4] - WDT con DHT11 (Lecturas sin pausa)");
  Serial.println("=================================\n");
  Serial.println("  Elegir el fix para TWDT ");
  Serial.println("=================================");
  Serial.println(" [a] - Ejecución normal (sin fix)");
  Serial.println(" [b] - FIX 1: Se incrementa el tiempo de timeout del WDT");
  Serial.println(" [c] - FIX 2: Se deshabilita el watchdog para el core 0, solo vigila la tarea inactiva del core 1");
  Serial.println("=================================\n");
}