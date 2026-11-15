#ifndef MANEJO_DHT_H
#define MANEJO_DHT_H

#include <Arduino.h>
#include "DHT.h"

#define DHTPIN 17
#define DHTTYPE DHT11

void initDHT();
float leerTempDHT11(int pin);
void imprimirTempDHT11(int pin);

#endif