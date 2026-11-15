#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "DHT.h"
#include "esp_task_wdt.h"
#include <WiFiManager.h>
#include <Preferences.h>
#include "esp_system.h"

#define DHTPIN 27
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

TaskHandle_t TareaCore0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
bool modoSeguro = false;

enum EstadoDemostracion {
  INICIO,
  NORMAL,
  FALLO_TWDT,
  FALLO_TWDT_INFINITO,
  FALLO_DHT11,
  FALLO_SERVIDOR
};

volatile EstadoDemostracion estadoActual = INICIO;

enum EstadoFix {
  SIN_FIX,
  FIX1,
  FIX2,
  FIX3
};

volatile EstadoFix estadoFixActual = SIN_FIX;

esp_task_wdt_config_t twdt_config0 = {
  .timeout_ms = 5000,
  .idle_core_mask = (1 << 0) | (1 << 1),
  .trigger_panic = true,
};

esp_task_wdt_config_t twdt_config1 = {
  .timeout_ms = 10000,
  .idle_core_mask = (1 << 0) | (1 << 1),
  .trigger_panic = true,
};

esp_task_wdt_config_t twdt_config2 = {
  .timeout_ms = 10000,
  .idle_core_mask = (1 << 1), // Solo se vigila el core 1
  .trigger_panic = true,      // Otra forma de deshabilitar es poner esto en false, solo genera Warning
};

float leerTempDHT11(int pin){

  //Según el protocolo del DHT11 Los datos transmitidos son de 40 bits
  uint8_t datos[5] = {0};
  
  //Para que el sensor empieze a medir hay que enviarle un pulso de inicio
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  delayMicroseconds(20000); //La línea de datos debe mantenerse en LOW por 18ms al menos para que el sensor detecte la señal
  digitalWrite(pin, HIGH);
  delayMicroseconds(40);
  pinMode(pin, INPUT); //luego volvemos a poner el pin como entrada para que lleguen las mediciones por acá

  //El sensor responde al pulso enviado con una señal de Acknowledge
  //La misma consiste en mantener la línea en LOW por 80ms y luego en HIGH por otros 80ms
  //Por lo tanto, se esperan ambas señales y se verifica que no superen un timeout de 100us (indicaría que ocurrió un error)
  uint32_t timeout = micros();
  while(digitalRead(pin)==LOW){ 
    if(micros()-timeout>100) 
      return NAN;
  }
  timeout = micros();
  while(digitalRead(pin)==HIGH){
    if(micros()-timeout>100)
      return NAN;
  }

  //Luego de eso ahora sí el DHT está listo para enviar los datos
  //En este bucle de 40 iteraciones se reciben los 40 bits de datos 
  for(int i=0; i<40; i++) {
    //Cada transimisión inicia con un pulso en LOW de 50us, igual que en el caso anterior se espera ese pulso
    timeout = micros();
    while(digitalRead(pin)==LOW){
      if(micros()-timeout>100)
        return NAN;
    }
    //Luego de eso el DHT envía un pulso en HIGH y acá se hace la determinación efectiva del bit
    //Según las especificaciones del sensor si el pulso dura aproximadamente 26 o 28us es un 0, si dura 70us o más es un 1
    uint32_t tAlta = micros();
    while(digitalRead(pin)==HIGH){
      if(micros()-tAlta>100)
        return NAN;
    }
    //Consideramos que si el estado HIGH duró más de 40us es un 1, sino es un 0
    if((micros()-tAlta)>40) {
      datos[i/8] |= (1<<(7-(i%8)));
    }
  }

  //Por último, una vez obtenido el dato se debe validar
  //De los 5 bytes que transmite el DHT11 el último es de paridad (datos[4])
  //Los úlimos 8 bits de la suma de los otros 4 bytes debe ser igual al byte de paridad
  if(datos[4] == ((datos[0]+datos[1]+datos[2]+datos[3]) & 0xFF)) {
    return (float)datos[2]; //Se devuelve el byte 2 que corresponde a la temperatura (entera, sin decimales)
  }
  return NAN;
}

void imprimirTempDHT11(int pin){
  float temp = leerTempDHT11(pin);
  if (!isnan(temp)) {
    Serial.print("Temperatura: ");
    Serial.print(temp);
    Serial.println("°C");
  } else {
    Serial.println("Fallo de lectura");
  }
}

void funcionTareaCore0(void* pvParameters) {

  EstadoFix fixAplicado = SIN_FIX; 
  esp_task_wdt_reconfigure(&twdt_config0);

  for (;;) {

    if (estadoFixActual != fixAplicado) {
      switch (estadoFixActual) {
        case FIX1:
          esp_task_wdt_reconfigure(&twdt_config1);
          break;
        case FIX2:
          esp_task_wdt_reconfigure(&twdt_config2);
          break;
        default:
          esp_task_wdt_reconfigure(&twdt_config0);
          break;
      }
      fixAplicado = estadoFixActual; // Actualizamos el registro
    }

    switch (estadoActual) {

      case INICIO: // Simplemente para el arranque, no se puede volver a este estado
        vTaskDelay(pdMS_TO_TICKS(100));
        break;

      case NORMAL:
        Serial.print("CORE 0 : Estado Normal. ");
        imprimirTempDHT11(DHTPIN);
        vTaskDelay(pdMS_TO_TICKS(2000));
        break;

      case FALLO_TWDT: {
        Serial.println("\n >>> INTENTANDO BLOQUEO DE SOFTWARE (TWDT) EN CORE 0 <<<");
        uint32_t tiempoInicio = millis();
        while (millis() - tiempoInicio < 7000) { //Se apropia de la CPU por 7 segundos
          int calculo = analogRead(34);

          if (estadoFixActual == FIX3) {
            vTaskDelay(pdMS_TO_TICKS(10)); 
          }

        }
        estadoActual = NORMAL; //Si se llega al final de la prueba, vuelve al estado normal
        break;
      }

      case FALLO_TWDT_INFINITO:
        Serial.println("\n >>> INTENTANDO BLOQUEO DE SOFTWARE (TWDT) EN CORE 0 (BUCLE INFINITO)<<<");
        while (true) {
          int calculo = analogRead(34);

          if (estadoFixActual == FIX3) {
            vTaskDelay(pdMS_TO_TICKS(10)); 

            if (estadoActual != FALLO_TWDT_INFINITO) {
              break;
            }
          }

        }
        break;

      case FALLO_DHT11:
        Serial.println("\n >>> INTENTANDO REINICIAR WDT CON DHT11 EN CORE 0 <<<");
        while (true) {
          imprimirTempDHT11(DHTPIN);
          // ESPERA BLOQUEANTE 
          uint32_t espera = micros();
          while (micros() - espera < 2000000) {}
        }
        break;

      case FALLO_SERVIDOR: {
        Serial.println("\n >>> INTENTANDO REINICIO DE WDT POR  LATENCIA DE RED <<<");
        
        WiFiClient client;
        const char* servidor = "httpbin.org";
        
        int segDemora = 7; //Tiempo de respuesta del servidor
        
        Serial.print("Conectando a ");
        Serial.println(servidor);

        if(client.connect(servidor, 80)) {
          Serial.println("Enviando petición GET");
          client.print("GET /delay/");
          client.print(segDemora);
          client.print(" HTTP/1.1\r\nHost: ");
          client.print(servidor);
          client.print("\r\nConnection: close\r\n\r\n");
          
          uint32_t tiempo_inicio = millis();
          
          while(!client.available()) {
            //Busy-wait de red, se espera la respuesta reteniendo la CPU
            if (estadoFixActual == FIX3) {
              if (millis() - tiempo_inicio > 3000) {
                Serial.println("Timeout de red alcanzado.");
                break;
              }
              vTaskDelay(pdMS_TO_TICKS(10));
            }
          }
          
          Serial.println("Respuesta:");
          Serial.println(client.readStringUntil('\n'));
          
        } else {
          Serial.println("Fallo de conexión a Internet");
        }
        
        client.stop();
        estadoActual = NORMAL; 
        break;
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  dht.begin();

  Preferences prefs;
  prefs.begin("wdt_mem", false); // Se abre el espacio de memoria en modo lectura/escritura para almacenar los contadores de reseteos
  
  esp_reset_reason_t reason = esp_reset_reason();

  int fix_guardado = prefs.getInt("ult_fix",0);
  switch (fix_guardado) {
        case 0:
          estadoFixActual = SIN_FIX;
          break;
        case 1:
          estadoFixActual = FIX1;
          break;
        case 2:
          estadoFixActual = FIX2;
          break;
        case 3:
          estadoFixActual = FIX3;
          break;
      }

  if (reason == ESP_RST_TASK_WDT || reason == ESP_RST_PANIC) {
    int cont = prefs.getInt("cont_total",0)+1;
    prefs.putInt("cont_total",cont); // Se incrementa el contador de reseteos
    int ult_test = prefs.getInt("ult_test",0); // Se obtiene el motivo de reseteo

    char numContador[6];
    sprintf(numContador,"cont%d",ult_test);
    int contAct = prefs.getInt(numContador,0)+1; // Se incrementa el contador del motivo de reseteo específico
    prefs.putInt(numContador,contAct);
    
    Serial.println("\n=======================================================");
    Serial.printf(" RESETEO POR TWDT DETECTADO (Motivo nro: %d).\n", ult_test);
    Serial.printf(" Reinicios consecutivos: %d\n", cont);
    Serial.printf(" Reinicios por motivo %d: %d\n", ult_test,contAct);
    Serial.println("=======================================================\n");

    if (estadoFixActual == FIX3 && cont >= 3) {
      Serial.println("ENTRANDO EN MODO SEGURO");
      modoSeguro = true;
      estadoActual = NORMAL;
    }    

  } else {
    Serial.println("\n=======================================================");
    Serial.printf(" RESETEO POR MOTIVO AJENO AL TWDT\n");
    Serial.println("=======================================================\n");

    // Si el reinicio no fue por TWDT se reinician los contadores
    prefs.putInt("cont2",0);
    prefs.putInt("cont3",0);
    prefs.putInt("cont4",0);
    prefs.putInt("cont5",0);
    prefs.putInt("cont_total",0);
  }
  
  prefs.end();

  if (!modoSeguro){
    Serial.println("Conectando a Wifi...");
    WiFiManager wm;
    bool res = wm.autoConnect("ESP32_WDT"); // Nombre del Access Point

    if(!res) {
      Serial.println("Fallo al conectar al WiFi");
      ESP.restart();
    } 
    
    Serial.print("WiFi conectado");

    imprimirMenu();
  }

  xTaskCreatePinnedToCore(
    funcionTareaCore0,
    "Tarea_Bloqueante",
    4096,
    NULL,
    1,
    &TareaCore0,
    0);

}

void loop() {

  //TODO setup() y loop() se ejecuta en el core 1

  if(!modoSeguro){
    if (Serial.available() > 0) {
      char comando = Serial.read();
      Preferences prefs;

      if (comando == '1' && estadoActual != NORMAL) {
        estadoActual = NORMAL;
        Serial.println("\nComando: Estado Normal.");
      } else if (comando == '2' && estadoActual != FALLO_TWDT) {
        estadoActual = FALLO_TWDT;
        prefs.begin("wdt_mem", false);
        prefs.putInt("ult_test",2); // En cada caso se almacena el último test realizado para saber el motivo de reseteo
        prefs.end();
      } else if (comando == '3' && estadoActual != FALLO_TWDT_INFINITO) {
        estadoActual = FALLO_TWDT_INFINITO;
        prefs.begin("wdt_mem", false);
        prefs.putInt("ult_test",3);
        prefs.end();
      } else if (comando == '4' && estadoActual != FALLO_DHT11) {
        estadoActual = FALLO_DHT11;
        prefs.begin("wdt_mem", false);
        prefs.putInt("ult_test",4);
        prefs.end();
      } else if (comando == '5' && estadoActual != FALLO_SERVIDOR) {
        estadoActual = FALLO_SERVIDOR;
        prefs.begin("wdt_mem", false);
        prefs.putInt("ult_test",5);
        prefs.end();
      } else if (comando == 'a' && estadoFixActual != SIN_FIX) {
        estadoFixActual = SIN_FIX;
        prefs.begin("wdt_mem", false);
        prefs.putInt("ult_fix",0);
        prefs.end();
        Serial.println("\nFix: sin fix.");
      } else if (comando == 'b' && estadoFixActual != FIX1) {
        estadoFixActual = FIX1;
        prefs.begin("wdt_mem", false);
        prefs.putInt("ult_fix",1);
        prefs.end();
        Serial.println("\nFix 1: incremento timeout WDT.");
      } else if (comando == 'c' && estadoFixActual != FIX2) {
        estadoFixActual = FIX2;
        prefs.begin("wdt_mem", false);
        prefs.putInt("ult_fix",2);
        prefs.end();
        Serial.println("\nFix 2: WDT deshabilitado para core 0.");
      } else if (comando == 'd' && estadoFixActual != FIX3) {
        estadoFixActual = FIX3;
        prefs.begin("wdt_mem", false);
        prefs.putInt("ult_fix",3);
        prefs.end();
        Serial.println("\nFix 3: soluciones sin modificar el WDT");
      }
    }
  }

  if (estadoActual == NORMAL) {
    Serial.println("CORE 1: Bucle Principal");
  } else if (estadoActual != INICIO){
    Serial.println("CORE 1: TESTEANDO EL WDT EN CORE 0");
  }

  delay(1000);
}

void imprimirMenu() {
  Serial.println("\n=================================");
  Serial.println("  Elegir de que manera Provocar el reset de wdt ");
  Serial.println("=================================");
  Serial.println(" [1] - Operacion Normal (leo dht normal)");
  Serial.println(" [2] - Task WDT - Bloqueo por SW (tarea que demora tiempo mayor al timeout)");
  Serial.println(" [3] - Task WDT - Bloqueo por SW (while infinito)");
  Serial.println(" [4] - WDT con DHT11 (Lecturas sin pausa)");
  Serial.println(" [5] - WDT por demora de conexión a un servidor");
  Serial.println("=================================\n");
  Serial.println("  Elegir el fix para TWDT ");
  Serial.println("=================================");
  Serial.println(" [a] - Ejecución normal (sin fix)");
  Serial.println(" [b] - FIX 1: Se incrementa el tiempo de timeout del WDT");
  Serial.println(" [c] - FIX 2: Se deshabilita el watchdog para el core 0, solo vigila la tarea inactiva del core 1");
  Serial.println(" [d] - FIX 3: Se solucionan los problemas de raíz, sin modificar el TWDT (suele ser lo más adecuado)");
  Serial.println("=================================\n");
}