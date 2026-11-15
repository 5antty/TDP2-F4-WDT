#include "manejo_webserver.h"

String paginaIndex()
{
    String h = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>ESP32 WDT Monitor</title></head><body>";
    h += "<h1>Taller de Proyecto II - Grupo F4 - Watchdog Timer Monitor and Solver</h1>";
    // h += "<h2>Ultimo valor medido por DHT11: " + String(getDHT11Value()) + "</h2>";
    h += "<h2>Contador de reinicios por WDT</h2>";
    h += "<p>Resets por computo intensivo: " + String(cnt1) + " veces</p>";
    h += "<p>Resets por lecturas DHT11 sin delay: " + String(cnt2) + " veces</p>";

    h += "<form action='/test' method='get'>";
    h += "<button name='f' value='1'>Reset por computo intensivo</button> ";
    h += "<button name='f' value='2'>Reset por DHT11</button> ";
    h += "</form>";

    h += "<h2>Estado del WDT</h2>";
    h += "<table border='1' cellpadding='4'>";
    h += "<tr><td>Modo activo</td><td>" + estadoActualToString() + "</td></tr>";
    // h += "<tr><td>Timeout WDT</td><td>" + "hola" + "</td></tr>";
    h += "<tr><td>WDT core 0</td><td>" + String(estadoFix2() ? "deshabilitado" : "habilitado") + "</td></tr>";
    h += "<tr><td>Último reset</td><td>" + motivoReset() + "</td></tr>";
    h += "</table>";
    h += "<form action='/modo' method='get'>";
    h += "<button name='m' value='0'>Por defecto</button> ";
    h += "<button name='m' value='1'>Timeout aumentado</button> ";
    h += "<button name='m' value='2'>WDT core 0 off</button>";
    h += "</form></body></html>";
    return h;
}

// Instancia del servidor HTTP
AsyncWebServer server(80);

void initServer()
{
    // El servidor entrega la página, cada vez que se accede a /, se genera una página HTML con el estado actual del ESP32.
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/html", paginaIndex()); });

    // El firmware recibe esa petición y ejecuta la función correspondiente.
    server.on("/test", HTTP_GET, [](AsyncWebServerRequest *req)
              {
    if (!req->hasParam("f")) { req->redirect("/"); return; }
    int f = req->getParam("f")->value().toInt();

    // 1) Guardar ANTES del reset
    String choice;
    if (f == 1) { 
        cnt1++; 
        prefs.putUInt("f1", cnt1); 
        comando = '2'; // Provoca reset por WDT de tarea
        choice = "Reset por computo intensivo";
    }
    if (f == 2) { cnt2++; prefs.putUInt("f2", cnt2); 
        comando = '4'; // Provoca reset por DHT11
        choice = "Reset por DHT11";
    }
    flagComando = 1; // Indica que hay un comando pendiente

    // 2) Responder y recién después ejecutar el test desde loop()
    //pendingTest = f;
    req->send(200, "text/html",
              "<meta http-equiv='refresh' content='8;url=/'>"
              "<p>Ejecutando " +
                  choice + ", el ESP se va a reiniciar...</p>"); });

    server.on("/modo", HTTP_GET, [](AsyncWebServerRequest *req)
              {
        if (req->hasParam("m")) {
            int m = req->getParam("m")->value().toInt();
            prefs.putInt("modo", m);
            switch (m)
            {
            case 0:
                /* code */
                comando = 'a'; // Por defecto
                break;
            case 1:
                /* code */
                comando = 'b'; // FIX1
                break;
            case 2:
                /* code */
                comando = 'c'; // FIX2
                break;

            default:
                break;
            }
            flagComando = 1; // Indica que hay un comando pendiente
        }
        req->send(200, "text/html",paginaIndex()); });
    server.begin();
}