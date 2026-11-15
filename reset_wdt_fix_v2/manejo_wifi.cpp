#include "manejo_wifi.h"

void initWifi()
{
    Serial.println("Encendiendo módulo Wi-Fi (Core 0)");
    WiFi.softAP(ssid, password);
    Serial.println("Red WiFi creada");
    Serial.print("Dirección IP: ");
    Serial.println(WiFi.softAPIP());
}