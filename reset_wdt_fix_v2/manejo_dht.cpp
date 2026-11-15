#include "manejo_dht.h"

DHT dht(DHTPIN, DHTTYPE);

void initDHT()
{
    dht.begin();
}

float leerTempDHT11(int pin)
{

    // Según el protocolo del DHT11 Los datos transmitidos son de 40 bits
    uint8_t datos[5] = {0};

    // Para que el sensor empieze a medir hay que enviarle un pulso de inicio
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    delayMicroseconds(20000); // La línea de datos debe mantenerse en LOW por 18ms al menos para que el sensor detecte la señal
    digitalWrite(pin, HIGH);
    delayMicroseconds(40);
    pinMode(pin, INPUT); // luego volvemos a poner el pin como entrada para que lleguen las mediciones por acá

    // El sensor responde al pulso enviado con una señal de Acknowledge
    // La misma consiste en mantener la línea en LOW por 80ms y luego en HIGH por otros 80ms
    // Por lo tanto, se esperan ambas señales y se verifica que no superen un timeout de 100us (indicaría que ocurrió un error)
    uint32_t timeout = micros();
    while (digitalRead(pin) == LOW)
    {
        if (micros() - timeout > 100)
            return NAN;
    }
    timeout = micros();
    while (digitalRead(pin) == HIGH)
    {
        if (micros() - timeout > 100)
            return NAN;
    }

    // Luego de eso ahora sí el DHT está listo para enviar los datos
    // En este bucle de 40 iteraciones se reciben los 40 bits de datos
    for (int i = 0; i < 40; i++)
    {
        // Cada transimisión inicia con un pulso en LOW de 50us, igual que en el caso anterior se espera ese pulso
        timeout = micros();
        while (digitalRead(pin) == LOW)
        {
            if (micros() - timeout > 100)
                return NAN;
        }
        // Luego de eso el DHT envía un pulso en HIGH y acá se hace la determinación efectiva del bit
        // Según las especificaciones del sensor si el pulso dura aproximadamente 26 o 28us es un 0, si dura 70us o más es un 1
        uint32_t tAlta = micros();
        while (digitalRead(pin) == HIGH)
        {
            if (micros() - tAlta > 100)
                return NAN;
        }
        // Consideramos que si el estado HIGH duró más de 40us es un 1, sino es un 0
        if ((micros() - tAlta) > 40)
        {
            datos[i / 8] |= (1 << (7 - (i % 8)));
        }
    }

    // Por último, una vez obtenido el dato se debe validar
    // De los 5 bytes que transmite el DHT11 el último es de paridad (datos[4])
    // Los úlimos 8 bits de la suma de los otros 4 bytes debe ser igual al byte de paridad
    if (datos[4] == ((datos[0] + datos[1] + datos[2] + datos[3]) & 0xFF))
    {
        return (float)datos[2]; // Se devuelve el byte 2 que corresponde a la temperatura (entera, sin decimales)
    }
    return NAN;
}

void imprimirTempDHT11(int pin)
{
    float temp = leerTempDHT11(pin);
    if (!isnan(temp))
    {
        Serial.print("Temperatura: ");
        Serial.print(temp);
        Serial.println("°C");
    }
    else
    {
        Serial.println("Fallo de lectura");
    }
}