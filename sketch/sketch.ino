// 12V, 1.6A @ Full White, 144 LEDs

#include <FastLED.h>
#include <WiFi.h>
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

WiFiClient espClient;
HTTPClient http;

CRGB leds[NUM_LEDS];

DynamicJsonDocument doc(4096);

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
}
void setupConfig()
{
  DEBUG_SERIAL.println();
  DEBUG_SERIAL.println("Fetching config from: " + configURL);

  if (http.begin(espClient, configURL))
  {
    http.addHeader("update-last-seen", "true");
    int httpCode = http.GET();
    DEBUG_SERIAL.println("Response code: " + String(httpCode));
    if (httpCode > 0)
    {
      String JSONConfig = http.getString();
      DeserializationError error = deserializeJson(doc, JSONConfig);

      if (error)
      {
        DEBUG_SERIAL.print(F("deserializeJson() failed: "));
        DEBUG_SERIAL.println(error.f_str());
        return;
      }

      for (JsonObject elem : doc.as<JsonArray>())
      {
        const int id = elem["id"];
        const bool enabled = elem["enabled"];
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
  FastLED.addLeds<WS2812B, DATA_PIN>(leds, NUM_LEDS);
}

void setup()
{
#ifdef DEBUG
  Serial.begin(9600);
  delay(500);
#endif

  setupWiFi();
  setupConfig();
  // setupFastLED();
}

// uint8_t gHue = 0;

void loop()
{
  // int pos = beatsin16(5, 0, 192);
  // fill_solid(leds, NUM_LEDS, CHSV(gHue, 255, pos));
  // FastLED.show();
  // EVERY_N_MILLISECONDS(100) { gHue++; }
  // fill_solid(leds, NUM_LEDS, CHSV(gHue, 255, 255));
  // FastLED.show();
  // EVERY_N_MILLISECONDS(100) { gHue++; }

  // fill_solid(leds, NUM_LEDS, CRGB::White);
  // FastLED.show();
}