// Wemos lolin s2 mini usb keyboard med HC-SR501, SK6812, Onboard LED og Keep-Alive for å hindre dvale over natten
#include "USB.h"
#include "USBHIDKeyboard.h"
#include <Adafruit_NeoPixel.h>
#include <Ticker.h> 
#include <WiFi.h>
#include <ArduinoOTA.h>

// DEFINE PINNER
const int PIR_PIN = 18;        
const int LED_STRIP_PIN = 16;  
const int ONBOARD_LED = 15;    
const char* ssid = "SSID";
const char* password = "password";

#define NUM_LEDS 8
Adafruit_NeoPixel strip(NUM_LEDS, LED_STRIP_PIN, NEO_GRBW + NEO_KHZ800);

USBHIDKeyboard Keyboard;
Ticker ledTimer; 

enum State { CYLON, MOTION_TRIGGERED, GREEN_HOLD, FADE_OUT };
volatile State currentState = CYLON; 

unsigned long lastTriggerTime = 0;
const unsigned long COOLDOWN_TIME = 10000; 
bool isTriggered = false;

// Tidsstyring for å holde USB-porten våken på PC-en
unsigned long lastKeepAliveTime = 0;
const unsigned long KEEP_ALIVE_INTERVAL = 300000; // 5 minutter i millisekunder

int cylonPos = 0;
int cylonDir = 1;
unsigned long stateStartTime = 0;
int fadeBrightness = 255;

volatile bool readyToUpdateLEDs = false;

void onTimer() {
  readyToUpdateLEDs = true;
}

void updateLEDs() {
  unsigned long currentTime = millis();

  switch (currentState) {
    case CYLON:
      static int speedDivider = 0;
      speedDivider++;
      if (speedDivider >= 6) { 
        speedDivider = 0;
        strip.clear();
        strip.setPixelColor(cylonPos, strip.Color(255, 0, 0, 0)); 
        if (cylonPos - cylonDir >= 0 && cylonPos - cylonDir < NUM_LEDS) {
          strip.setPixelColor(cylonPos - cylonDir, strip.Color(80, 0, 0, 0)); 
        }
        strip.show();
        
        cylonPos += cylonDir;
        if (cylonPos == NUM_LEDS - 1 || cylonPos == 0) {
          cylonDir = -cylonDir;
        }
      }
      break;

    case MOTION_TRIGGERED:
      for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, strip.Color(0, 255, 0, 0)); 
      }
      strip.show();
      stateStartTime = currentTime;
      currentState = GREEN_HOLD;
      break;

    case GREEN_HOLD:
      if (currentTime - stateStartTime >= 1000) { 
        stateStartTime = currentTime;
        fadeBrightness = 255;
        currentState = FADE_OUT;
      }
      break;

    case FADE_OUT:
      if (fadeBrightness > 0) {
        fadeBrightness -= 8; 
        if (fadeBrightness < 0) fadeBrightness = 0;
        
        for (int i = 0; i < NUM_LEDS; i++) {
          strip.setPixelColor(i, strip.Color(0, fadeBrightness, 0, 0));
        }
        strip.show();
      } else {
        currentState = CYLON;
        strip.clear();
        strip.show(); 
        cylonPos = 0;
        cylonDir = 1;
      }
      break;
  }
}

void setup() {

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  ArduinoOTA.begin();

  // Bruk INPUT_PULLDOWN for å sikre at pinnen har en bunnsolid referanse til jord (0V) når sensoren er stille
  pinMode(PIR_PIN, INPUT_PULLDOWN);

  pinMode(ONBOARD_LED, OUTPUT);
  digitalWrite(ONBOARD_LED, LOW);

  strip.begin();
  strip.setBrightness(10);
  strip.show(); 

  Keyboard.begin();
  USB.begin();

  ledTimer.attach(0.03, onTimer);
}

void loop() {
  ArduinoOTA.handle();
  unsigned long currentTime = millis();
  int pirState = digitalRead(PIR_PIN);

  // 1. HÅNDTER PIR-SENSOR, USB & ONBOARD LED
  if (pirState == HIGH) {
    digitalWrite(ONBOARD_LED, HIGH); 

    if (!isTriggered && currentState == CYLON) {
      if (currentTime - lastTriggerTime >= COOLDOWN_TIME) {
        Keyboard.press(KEY_F13);
        delay(50); 
        Keyboard.release(KEY_F13);
        lastTriggerTime = currentTime;
        lastKeepAliveTime = currentTime; // Nullstill keep-alive siden vi akkurat sendte data
      }
      isTriggered = true;
      currentState = MOTION_TRIGGERED; 
    }
  } else {
    digitalWrite(ONBOARD_LED, LOW); 
    isTriggered = false;
  }

  // 2. KEEP-ALIVE MEKANISME: Sørger for at PC-en ikke deaktiverer USB-enheten
  if (currentTime - lastKeepAliveTime >= KEEP_ALIVE_INTERVAL) {
    lastKeepAliveTime = currentTime;
    Keyboard.releaseAll(); // Sender en tom "slipp alle taster"-kommando. Dette trigger ingen tast, men holder USB-porten aktiv!
  }

  // 3. KJØR LED-ANIMALISERING
  if (readyToUpdateLEDs) {
    readyToUpdateLEDs = false; 
    updateLEDs();              
  }
}
