// 12V, 1.6A @ Full White, 144 LEDs

#include <FastLED.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "config.h"

// Enable/Disable debug-mode
#define DEBUG true
#define DEBUG_SERIAL \
  if (DEBUG)         \
  Serial

#define DATA_PIN 16
#define NUM_LEDS 86

WiFiClient client;
WiFiClientSecure clientSecure;
HTTPClient http;

CRGB hardwareLeds[NUM_LEDS];

DynamicJsonDocument configDoc(4096);
DynamicJsonDocument smartThingsDoc(24576);

// Configs
const char *smartthings_api_url;
const char *bearer_token;

unsigned long runtime = 0;
const long onInterval = 10000;   // 10 seconds
const long OffInternval = 30000; // 30 seconds
long checkInterval = OffInternval;

int mode = 0;
int usableLeds[] = {86, 43};

void setupWiFi()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  DEBUG_SERIAL.println();
  DEBUG_SERIAL.println();
  DEBUG_SERIAL.print("Connecting to WiFi: ");
  DEBUG_SERIAL.print(wifi_ssid);

  while (WiFi.status() != WL_CONNECTED)
  {
    DEBUG_SERIAL.print('.');
    delay(1000);
  }

  DEBUG_SERIAL.println();
  DEBUG_SERIAL.print("IP Address: ");
  DEBUG_SERIAL.println(WiFi.localIP());

  clientSecure.setInsecure();
}
void setupConfig()
{
  DEBUG_SERIAL.println();
  DEBUG_SERIAL.println("Fetching config from: " + configURL);

  if (http.begin(client, configURL))
  {
    http.addHeader("update-last-seen", "true");
    int httpCode = http.GET();
    DEBUG_SERIAL.println("Response code: " + String(httpCode));
    if (httpCode > 0)
    {
      String JSONConfig = http.getString();
      DeserializationError error = deserializeJson(configDoc, JSONConfig);

      if (error)
      {
        DEBUG_SERIAL.print(F("deserializeJson() failed: "));
        DEBUG_SERIAL.println(error.f_str());
        return;
      }

      for (JsonObject elem : configDoc.as<JsonArray>())
      {
        const char *key = elem["key"];
        const bool enabled = elem["enabled"];

        if (strcmp(key, "smartthings-api-url") == 0)
        {
          if (enabled)
          {
            smartthings_api_url = elem["value"];
          }
        }
        else if (strcmp(key, "bearer-token") == 0)
        {
          if (enabled)
          {
            bearer_token = elem["value"];
          }
        }
      }
    }
    http.end();
  }
  else
  {
    DEBUG_SERIAL.printf("[HTTP] Unable to connect\n");
  }
}
void setupFastLED()
{
  FastLED.addLeds<WS2812B, DATA_PIN>(hardwareLeds, NUM_LEDS);
  FastLED.setBrightness(255);
  FastLED.clear();
  FastLED.show();
}

void setLEDData(int progress, bool reversed = false)
{
  if (reversed)
  {
    for (int i = NUM_LEDS; i >= NUM_LEDS - progress; i--)
    {
      hardwareLeds[i] = CRGB::White;
    }
  }
  else
  {
    for (int i = 0; i < progress; i++)
    {
      hardwareLeds[i] = CRGB::White;
    }
  }
}

void updateLEDs(int mainTemp, int bottomTemp, int mainTimer, int bottomTimer)
{
  DEBUG_SERIAL.println("Update leds..");

  int leds = usableLeds[mode];

  if (mode == 1)
  {
    if (mainTimer)
    {
      DEBUG_SERIAL.println("updateLEDs, Main Timer: " + String(mainTimer) + " / " + String(leds));
      setLEDData(mainTimer);
    }
    else
    {
      DEBUG_SERIAL.println("updateLEDs, Main Temp: " + String(mainTemp) + " / " + String(leds));
      setLEDData(mainTemp);
    }

    if (bottomTimer)
    {
      DEBUG_SERIAL.println("updateLEDs, Bottom Timer: " + String(bottomTimer) + " / " + String(leds));
      setLEDData(bottomTimer, true);
    }
    else
    {
      DEBUG_SERIAL.println("updateLEDs, Bottom Temp: " + String(bottomTemp) + " / " + String(leds));
      setLEDData(bottomTemp, true);
    }
  }
  else if (mainTemp)
  {
    if (mainTimer)
    {
      DEBUG_SERIAL.println("updateLEDs, Main Timer: " + String(mainTimer) + " / " + String(leds));
      setLEDData(mainTimer);
    }
    else
    {
      DEBUG_SERIAL.println("updateLEDs, Main Temp: " + String(mainTemp) + " / " + String(leds));
      setLEDData(mainTemp);
    }
  }
  else
  {
    if (bottomTimer)
    {
      DEBUG_SERIAL.println("updateLEDs, Bottom Timer: " + String(bottomTimer) + " / " + String(leds));
      setLEDData(bottomTimer);
    }
    else
    {
      DEBUG_SERIAL.println("updateLEDs, Bottom Temp: " + String(bottomTemp) + " / " + String(leds));
      setLEDData(bottomTemp);
    }
  }

  FastLED.show();
}

void checkStatus()
{
  DEBUG_SERIAL.println();
  DEBUG_SERIAL.println("Fetching oven status from Samartthings..");

  if (http.begin(clientSecure, smartthings_api_url))
  {
    http.addHeader("Authorization", bearer_token);
    int httpCode = http.GET();
    DEBUG_SERIAL.println("Response code: " + String(httpCode));

    if (httpCode > 0)
    {
      String JSONConfig = http.getString();
      DeserializationError error = deserializeJson(smartThingsDoc, JSONConfig, DeserializationOption::NestingLimit(11));

      if (error)
      {
        DEBUG_SERIAL.print(F("deserializeJson() failed: "));
        DEBUG_SERIAL.println(error.f_str());
        return;
      }

      // Top/Main Oven
      JsonObject main = smartThingsDoc["components"]["main"];
      const char *mainState = main["samsungce.ovenOperatingState"]["operatingState"]["value"];
      const int mainTempTarget = main["ovenSetpoint"]["ovenSetpoint"]["value"];
      const int mainTempCurrent = main["temperatureMeasurement"]["temperature"]["value"];
      const int mainTimerStatus = main["samsungce.ovenOperatingState"]["progress"]["value"];

      // Bottom Oven
      JsonObject bottom = smartThingsDoc["components"]["cavity-01"];
      const char *bottomState = bottom["samsungce.ovenOperatingState"]["operatingState"]["value"];
      const int bottomTempTarget = bottom["ovenSetpoint"]["ovenSetpoint"]["value"];
      const int bottomTempCurrent = bottom["temperatureMeasurement"]["temperature"]["value"];
      const int bottomTimerStatus = bottom["samsungce.ovenOperatingState"]["progress"]["value"];

      // // Top/Main Oven
      // const char *mainState = "running";
      // const int mainTempTarget = 250;
      // const int mainTempCurrent = 249;
      // const int mainTimerStatus = 0;

      // // Bottom Oven
      // const char *bottomState = "running";
      // const int bottomTempTarget = 250;
      // const int bottomTempCurrent = 250;
      // const int bottomTimerStatus = 50;

      if (strcmp(mainState, "running") == 0 || strcmp(bottomState, "running") == 0)
      {
        checkInterval = onInterval;

        if (mainTempTarget && bottomTempTarget)
        {
          mode = 1;
        }

        int leds = usableLeds[mode];
        int mainTempProgress = map(mainTempCurrent, 0, mainTempTarget, 0, leds);
        int bottomTempProgress = map(bottomTempCurrent, 0, bottomTempTarget, 0, leds);
        int mainTimerProgress = map(mainTimerStatus, 0, 100, 0, leds);
        int bottomTimerProgress = map(bottomTimerStatus, 0, 100, 0, leds);

        updateLEDs(mainTempProgress, bottomTempProgress, mainTimerProgress, bottomTimerProgress);
      }
      else
      {
        DEBUG_SERIAL.println("Ovn er slukket");
        checkInterval = OffInternval;
        FastLED.clear();
        FastLED.show();
      }
    }
    http.end();
  }
  else
  {
    DEBUG_SERIAL.println("[HTTP] Unable to connect");
  }
}

void setup()
{
#ifdef DEBUG
  Serial.begin(9600);
  delay(500);
#endif

  setupWiFi();
  setupConfig();
  setupFastLED();
  checkStatus();
}

void loop()
{
  unsigned long currentMillis = millis();

  if (currentMillis - runtime >= checkInterval)
  {
    runtime = currentMillis;
    checkStatus();
  }
}