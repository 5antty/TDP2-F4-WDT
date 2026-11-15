#ifndef MANEJO_WIFI_H
#define MANEJO_WIFI_H

#include <Arduino.h>
#include <WiFi.h>

static const char *ssid = "TEST_WIFI_WDT";
static const char *password = "password123";

void initWifi();

#endif