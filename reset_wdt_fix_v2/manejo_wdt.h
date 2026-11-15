#ifndef MANEJO_WDT_H
#define MANEJO_WDT_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_task_wdt.h"
#include "manejo_dht.h"

#define PIN_LED 2

extern volatile char comando;

enum EstadoDemostracion
{
    INICIO,
    NORMAL,
    FALLO_TWDT,
    FALLO_TWDT_INFINITO,
    FALLO_DHT11,
    FALLO_SERVIDOR
};

enum EstadoFix
{
    SIN_FIX,
    FIX1,
    FIX2,
    FIX3
};

extern volatile uint8_t ledEnabled;
extern volatile char comando;
extern volatile int flagComando;

String estadoActualToString();
String motivoReset();
int estadoFix2();
void funcionTareaCore0(void *pvParameters);
void initTareaCore0();
void actualizarEstado();

#endif