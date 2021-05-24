#include <ESP8266WiFi.h>

#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>

#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <Timezone.h>

#include <Adafruit_NeoPixel.h>

#include "NixieLib.h"

#define GPIO_LATCH D0
#define GPIO_PWM D8

#define GPIO_MOSI D7
#define GPIO_SCK D5

#define GPIO_NEOPIXEL D3

//D1 SCL, D2 SDA

#define NAME "Nixie ESP Clock"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
WiFiManager wifiManager;

WiFiServer telnetServer(23);
WiFiClient debugClient;

Stream *debug = &Serial;

TimeChangeRule winter = {"CET", Last, Sun, Oct, 2, 60};    //UTC + 1 hours
TimeChangeRule summer = {"CEST", Last, Sun, Mar, 2, 120};     //UTC + 2 hours
Timezone timeZone(winter, summer);

#define NUMMODULES 3
NixieLib nixies = NixieLib(NUMMODULES, NIXIE_MODULE_TYPE_TPIC6592, GPIO_LATCH, GPIO_PWM, 0);

Adafruit_NeoPixel leds = Adafruit_NeoPixel(NUMMODULES*2, GPIO_NEOPIXEL, NEO_GRB + NEO_KHZ800);


void setup() {
  Serial.begin(115200);


  nixies.setBrightness(PWMRANGE); //5 is practical minimum

  //blank tubes, but light all the dots
  nixies.setNixieModule(0, 00, BLANKBOTHDIGITS, true, true, true, true);
  nixies.setNixieModule(1, 00, BLANKBOTHDIGITS, true, true, true, true);
  nixies.setNixieModule(2, 00, BLANKBOTHDIGITS, true, true, true, true);
  nixies.updateNixies();

  //The LEDs do not work, except sometimes the first one. So turn it off for uniformity
  leds.begin();
  leds.fill(0);
  leds.show();

  wifiManager.autoConnect("AutoConnectAP"); //TODO this is blocking. Add RTC and non-blocking config
  timeClient.begin();
  telnetServer.begin();
  telnetServer.setNoDelay(true);

  ArduinoOTA.setHostname ( NAME ) ;                  // Set the hostname
  ArduinoOTA.begin() ;




  Serial.println("booted");

}

uint32_t number;
float fnumber;

void secondLoop(){
  timeClient.update(); //this only updates if the interval set in timeClient is reached
  time_t local = timeZone.toLocal(timeClient.getEpochTime());


  // nixies.setNixies(number);
  // debug->printf("nixies set to %dL\n", number);
  // number = random(999999);

  // fnumber = 12.3;
  // nixies.setNixies(fnumber, 3, false);
  // debug->printf("nixies set to %f\n", fnumber);
  // fnumber = (float)rand()/(RAND_MAX/9999);

  // nixies.setNixies(" 11..34^5.6");

  nixies.setNixieModule(0, (uint8_t)second(local), SHOWALLZEROS, false, false, true, true);
  nixies.setNixieModule(1, (uint8_t)minute(local), SHOWALLZEROS, false, false, true, true);
  nixies.setNixieModule(2, (uint8_t)hour(local), SHOWLONEZERO, false, false, false, false);

  nixies.updateNixies();


  // debug->print("UTC: ");
  // debug->print(timeClient.getFormattedTime());
  // debug->println();
  // debug->print("Loc: ");
  // debug->print(hour(local));
  // debug->print(minute(local)<10 ? ":0" : ":");
  // debug->print(minute(local));
  // debug->print(second(local)<10 ? ":0" : ":");
  // debug->print(second(local));
  // debug->println();
}

uint32_t secondLoopLastRun = 0;

void telnetLoop(){
  if (telnetServer.hasClient()) {
    debugClient = telnetServer.available();
    Serial.println("New Telnet client");
    debugClient.flush();  // clear input buffer, else you get strange characters
    debug = &debugClient;
  }

  //drop data if available
  while(debugClient.available()) {
    debugClient.read();
  }

  if ( debug == &debugClient && !debugClient.connected() ){
    debug = &Serial;
    debugClient.stop();
    Serial.println("Telnet Client Stop");
  }
}

void loop() {
  ArduinoOTA.handle();
  telnetLoop();


  if ( millis () - secondLoopLastRun > 1000 ){
    secondLoopLastRun = millis();
    secondLoop();
  }
}
