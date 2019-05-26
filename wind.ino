/*
  Wind - NMEA Wind Instrument
  Copyright (c) 2018 Tom K
  Originally written for Arduino Pro Mini 328
  v3c

  MIT License

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#define VERSION PSTR("Wind v3 18-Sep-2015")

#include <PString.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "IPAddress.h"
#include "peetbros.h"

const char *ssid = "LaBiserica";
const char *password = "Macanache";

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(windSpeedPin, INPUT);
    pinMode(windDirPin, INPUT);

    attachInterrupt(windSpeedPin, readWindSpeed, FALLING);
    attachInterrupt(windDirPin, readWindDir, FALLING);

    Serial.begin(115200, SERIAL_8N1);
    if(Serial.availableForWrite()) {
        Serial.println(VERSION);
        Serial.print("Direction Filter: ");
        Serial.println(filterGain);
    }
    
    // delete old config
    WiFi.disconnect(true);
    
    WiFi.mode(WIFI_STA);
    //register event handler
    WiFi.onEvent(WiFiEvent);
    
    //Initiate connection
    WiFi.begin(ssid, password);

    Serial.print("Waiting for WIFI connection...");

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    interrupts();
}

void loop()
{
    int i;
    const unsigned int LOOP_DELAY = 50;
    const unsigned int LOOP_TIME = TIMEOUT / LOOP_DELAY;

    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); // Toggle LED

    i = 0;
    // If there is new data, process it, otherwise wait for LOOP_TIME to pass
    while ((newData != true) && (i < LOOP_TIME))
    {
        i++;
        delayMicroseconds(LOOP_DELAY);
    }

    calcWindSpeedAndDir(); // Process new data
    newData = false;
}
