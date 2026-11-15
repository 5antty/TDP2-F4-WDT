#include "manejo_wdt.h"

TaskHandle_t TareaCore0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
volatile EstadoDemostracion estadoActual = INICIO;
volatile EstadoFix estadoFixActual = SIN_FIX;

bool modoSeguro = false;

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

void testTaskWDT()
{
    uint32_t tiempoInicio = millis();
    while (millis() - tiempoInicio < 7000)
    { // Se apropia de la CPU por 7 segundos
        int calculo = analogRead(34);
        esp_task_wdt_reset();
    }
    estadoActual = NORMAL; // Si se llega al final de la prueba, vuelve al estado normal
}

void testWDT_Inf()
{
    while (true)
    {
        int calculo = analogRead(34);
        esp_task_wdt_reset();
    }
}

void testWDT_DHT11()
{
    while (true)
    {
        imprimirTempDHT11(DHTPIN);
        // EN ESTE CASO NO HAY ESPERA, DESCOMENTAR SI SE QUIERE USAR UNA ESPERA BLOQUEANTE (también genera reseteo por WDT)
        // uint32_t espera = micros();
        // while (micros() - espera < 2000000) {}
        esp_task_wdt_reset();
    }
}

String estadoActualToString()
{
    switch (estadoActual)
    {
    case NORMAL:
        return "NORMAL";
    case FALLO_TWDT:
        return "FALLO_TWDT";
    case FALLO_TWDT_INFINITO:
        return "FALLO_TWDT_INFINITO";
    case FALLO_DHT11:
        return "FALLO_DHT11";
    default:
        return "DESCONOCIDO";
    }
}

String motivoReset()
{
    switch (esp_reset_reason())
    {
    case ESP_RST_POWERON:
        return "Power on";
    case ESP_RST_SW:
        return "Software";
    case ESP_RST_PANIC:
        return "Panic";
    case ESP_RST_INT_WDT:
        return "WDT de interrupciones";
    case ESP_RST_TASK_WDT:
        return "WDT de tareas";
    case ESP_RST_WDT:
        return "Otro WDT";
    case ESP_RST_BROWNOUT:
        return "Brownout";
    default:
        return "Otro";
    }
}
int estadoFix2()
{
    if (estadoFixActual == FIX2)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

void funcionTareaCore0(void *pvParameters)
{
    Serial.print("Tarea de prueba iniciada en el Core: ");
    Serial.println(xPortGetCoreID());

    EstadoFix fixAplicado = SIN_FIX;
    esp_task_wdt_reconfigure(&twdt_config0);
    // Por defecto el TWDT controla la tarea inactiva del core, pero se pueden suscribir otras tareas
    esp_task_wdt_add(NULL); // Se suscribe la tarea para que sea controlada por el TWDT

    for (;;)
    {

        if (estadoFixActual != fixAplicado)
        {
            switch (estadoFixActual)
            {
            case SIN_FIX:
                esp_task_wdt_reconfigure(&twdt_config0);
                break;
            case FIX1:
                esp_task_wdt_reconfigure(&twdt_config1);
                ;
                break;
            case FIX2:
                esp_task_wdt_reconfigure(&twdt_config2);
                break;
            }
            fixAplicado = estadoFixActual; // Actualizamos el registro
        }

        switch (estadoActual)
        {

        case NORMAL:
            Serial.print("CORE 0 : Estado Normal. Temperatura: ");
            if (!ledEnabled)
            {
                digitalWrite(PIN_LED, HIGH);
                ledEnabled = 1;
            }
            imprimirTempDHT11(DHTPIN);
            vTaskDelay(pdMS_TO_TICKS(2000));
            esp_task_wdt_reset();
            // Como se suscribió esta tarea al TWDT, se debe reiniciar el contador del timer dentro de la misma
            break;

        case FALLO_TWDT:
        {
            Serial.println("\n >>> INTENTANDO BLOQUEO DE SOFTWARE (TWDT) EN CORE 0 <<<");
            testTaskWDT();
            break;
        }

        case FALLO_TWDT_INFINITO:
            Serial.println("\n >>> INTENTANDO BLOQUEO DE SOFTWARE (TWDT) EN CORE 0 (BUCLE INFINITO)<<<");
            testWDT_Inf();
            break;

        case FALLO_DHT11:
            Serial.println("\n >>> INTENTANDO REINICIAR WDT CON DHT11 EN CORE 0 <<<");
            testWDT_DHT11();
            break;
        }
    }
}

void initTareaCore0()
{
    xTaskCreatePinnedToCore(
        funcionTareaCore0,  /* Function to implement the task */
        "Tarea_Bloqueante", /* Name of the task */
        4096,               /* Stack size in words */
        NULL,               /* Task input parameter */
        1,                  /* Priority of the task */
        &TareaCore0,        /* Task handle. */
        0);                 /* Core where the task should run */
}

void actualizarEstado()
{
    // leer comando de un buffer
    if (flagComando)
    {
        flagComando = 0;
        if (comando == '1')
        {
            estadoActual = NORMAL;
            Serial.println("\nComando: Estado Normal.");
        }
        else if (comando == '2')
        {
            estadoActual = FALLO_TWDT;
        }
        else if (comando == '3')
        {
            estadoActual = FALLO_TWDT_INFINITO;
        }
        else if (comando == '4')
        {
            estadoActual = FALLO_DHT11;
        }
        else if (comando == 'a')
        {
            estadoFixActual = SIN_FIX;
            Serial.println("\nFix: sin fix.");
        }
        else if (comando == 'b')
        {
            estadoFixActual = FIX1;
            Serial.println("\nFix 1: incremento timeout WDT.");
        }
        else if (comando == 'c')
        {
            estadoFixActual = FIX2;
            Serial.println("\nFix 2: WDT deshabilitado para core 0.");
        }
    }

    if (estadoActual == NORMAL)
    {
        Serial.println("CORE 1: Bucle Principal");
    }
    else
    {
        Serial.println("CORE 1: TESTEANDO EL WDT EN CORE 0");
    }
}