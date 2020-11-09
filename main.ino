/*
  WOT Mozilla IoT smart Plug

  This program is to connect esp01 module with WOT gateway

  Created 09 11 2020
  Abhishek Rana

*/

#include <Arduino.h>
#include "Thing.h"
#include "WebThingAdapter.h"
#include <ESP8266WiFi.h>

#define Relay 0 // Relay connected to GPIO 0

byte relON[] = {0xA0, 0x01, 0x01, 0xA2};  //Hex command to send to serial for open relay
byte relOFF[] = {0xA0, 0x01, 0x00, 0xA1}; //Hex command to send to serial for close relay

//TODO: Hardcode your wifi credentials here (and keep it private)
const char* ssid = "SSID";
const char* password = "";

WebThingAdapter* adapter;

const char* ledTypes[] = {"RelaySwitch", "Light", nullptr};
ThingDevice led("relay", "Smart Plug", ledTypes);
ThingProperty ledOn("on", "", BOOLEAN, "OnOffProperty");

bool lastOn = false;
int timer = 0;

void setup(void) {
  pinMode(Relay, OUTPUT);
  digitalWrite(Relay, LOW);

  #if defined(ESP8266) || defined(ESP32)
    WiFi.mode(WIFI_STA);
  #endif
    WiFi.begin(ssid, password);
  //Serial.println("");

  // Wait for connection
  bool blink = true;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    timer = timer + 1;
    if (timer == 100) {
      ESP.restart();
    }
  }
  adapter = new WebThingAdapter("w26", WiFi.localIP());

  led.addProperty(&ledOn);
  adapter->addDevice(&led);
  adapter->begin();
}

void loop(void) {
  adapter->update();
  bool acceso = ledOn.getValue().boolean;
  if (acceso != lastOn) {
    if (acceso) {
      digitalWrite(Relay, HIGH);
    }
    else {
      digitalWrite(Relay, LOW);
    }
  }
  lastOn = acceso;
}