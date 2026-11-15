#ifndef MANEJO_WEBSERVER_H
#define MANEJO_WEBSERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include "manejo_wdt.h"

extern Preferences prefs;
extern uint32_t cnt1, cnt2;
extern volatile char comando;
extern volatile int flagComando;

void initServer();

#endif