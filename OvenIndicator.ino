// 12V, 1.6A @ Full White, 144 LEDs

#include <FastLED.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>

#include "config.h"
#include "breathing.h"

// Enable/Disable debug-mode
#define DEBUG false
#define DEBUG_SERIAL \
  if (DEBUG)         \
  Serial

#define DATA_PIN 16
#define NUM_LEDS 84

WiFiClient client;
WiFiClientSecure clientSecure;
HTTPClient http;

CRGB hardwareLeds[NUM_LEDS];

DynamicJsonDocument configDoc(4096);
DynamicJsonDocument smartThingsDoc(24576);

// Configs
const char *smartthingsApiUrl;
const char *bearerToken;
int brightness = 32;
long onInterval = 0;
long offInterval = 0;

unsigned long runtime = 0;
long checkInterval = offInterval;

int usableLeds[] = {84, 42};

// Oven State
int mode = 0;
int mainTempTarget;
int mainTempCurrent;
int mainTimerStatus;
int bottomTempTarget;
int bottomTempCurrent;
int bottomTimerStatus;

void setupWiFi()
{
  WiFi.setHostname(hostname);
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
void setupOTA()
{
  // Initialize OTA
  ArduinoOTA.begin();
  ArduinoOTA.setPassword(ota_password);

  // Set up OTA event handlers
  ArduinoOTA.onStart(handleOTAStart);
  ArduinoOTA.onProgress(handleOTAProgress);
  ArduinoOTA.onEnd(handleOTAEnd);
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
            smartthingsApiUrl = elem["value"];
            DEBUG_SERIAL.println("smartthings-api-url: " + String(smartthingsApiUrl));
          }
        }
        else if (strcmp(key, "bearer-token") == 0)
        {
          if (enabled)
          {
            bearerToken = elem["value"];
            DEBUG_SERIAL.println("bearer-token: " + String(bearerToken));
          }
        }
        else if (strcmp(key, "on-interval") == 0)
        {
          if (enabled)
          {
            onInterval = elem["value"];
            DEBUG_SERIAL.println("on-interval: " + String(onInterval));
          }
        }
        else if (strcmp(key, "off-interval") == 0)
        {
          if (enabled)
          {
            offInterval = elem["value"];
            DEBUG_SERIAL.println("off-interval: " + String(offInterval));
          }
        }
        else if (strcmp(key, "brightness") == 0)
        {
          if (enabled)
          {
            brightness = elem["value"];
            DEBUG_SERIAL.println("brightness: " + String(brightness));
          }
        }
        else if (strcmp(key, "pulse-speed") == 0)
        {
          if (enabled)
          {
            pulseSpeed = elem["value"];
            DEBUG_SERIAL.println("pulse speed: " + String(pulseSpeed));
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
  FastLED.setBrightness(brightness);
  FastLED.clear();
  FastLED.show();
}

void handleOTAStart()
{
  DEBUG_SERIAL.println("OTA update starting..");
}
void handleOTAProgress(unsigned int progress, unsigned int total)
{
  DEBUG_SERIAL.printf("Progress: %u%%\r", (progress / (total / 100)));
}
void handleOTAEnd()
{
  DEBUG_SERIAL.println("\nOTA update complete!");
  ESP.restart();
}

void doBreathing(int index, uint8_t hue, uint8_t sat, uint8_t val)
{
  hardwareLeds[index] = CHSV(hue, sat, val);

  // You can experiment with commenting out these dim8_video lines
  // to get a different sort of look.
  hardwareLeds[index].r = dim8_video(hardwareLeds[index].r);
  hardwareLeds[index].g = dim8_video(hardwareLeds[index].g);
  hardwareLeds[index].b = dim8_video(hardwareLeds[index].b);
}

void setLEDData(int progress, bool reversed = false)
{
  // Total number of leds in use
  int leds = usableLeds[mode];

  float dV = ((exp(sin(pulseSpeed * millis() / 2000.0 * PI)) - 0.36787944) * delta);
  val = valueMin + dV;
  hue = map(val, valueMin, valueMax, hueA, hueB); // Map hue based on current val
  sat = map(val, valueMin, valueMax, satA, satB); // Map sat based on current val

  if (reversed)
  {
    for (int i = NUM_LEDS; i >= NUM_LEDS - progress; i--)
    {
      doBreathing(i, hue, sat, val);
    }

    // Fixed on for last led
    hardwareLeds[leds] = CRGB::White;
    FastLED.show();
  }
  else
  {
    for (int i = 0; i < progress; i++)
    {
      doBreathing(i, hue, sat, val);
    }

    // Fixed on for last led
    hardwareLeds[leds - 1] = CRGB::White;
    FastLED.show();
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

void fetchOvenNumbers()
{
  DEBUG_SERIAL.println();
  DEBUG_SERIAL.println("Fetching oven numbers from SmartThings..");

  if (http.begin(clientSecure, smartthingsApiUrl))
  {
    http.addHeader("Authorization", bearerToken);
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
      const int mTempTarget = main["ovenSetpoint"]["ovenSetpoint"]["value"];
      const int mTempCurrent = main["temperatureMeasurement"]["temperature"]["value"];
      const int mTimerStatus = main["samsungce.ovenOperatingState"]["progress"]["value"];

      // Bottom Oven
      JsonObject bottom = smartThingsDoc["components"]["cavity-01"];
      const char *bottomState = bottom["samsungce.ovenOperatingState"]["operatingState"]["value"];
      const int bTempTarget = bottom["ovenSetpoint"]["ovenSetpoint"]["value"];
      const int bTempCurrent = bottom["temperatureMeasurement"]["temperature"]["value"];
      const int bTimerStatus = bottom["samsungce.ovenOperatingState"]["progress"]["value"];

      // // Test Data
      // const char *mainState = "running";
      // const int mTempTarget = 250;
      // const int mTempCurrent = 200;
      // const int mTimerStatus = 0;

      // const char *bottomState = "running";
      // const int bTempTarget = 250;
      // const int bTempCurrent = 200;
      // const int bTimerStatus = 0;

      const bool topActive = strcmp(mainState, "running") == 0 && mTempCurrent < mTempTarget - 1;
      const bool bottomActive = strcmp(mainState, "running") == 0 && bTempCurrent < bTempTarget - 1;

      if (topActive || bottomActive)
      {
        DEBUG_SERIAL.println("Oven is running");
        checkInterval = onInterval;
        mode = 0;

        if (mainTempTarget && bottomTempTarget)
        {
          mode = 1;
          FastLED.clear();
          FastLED.show();
        }

        mainTempTarget = mTempTarget;
        mainTempCurrent = mTempCurrent;
        mainTimerStatus = mTimerStatus;

        bottomTempTarget = bTempTarget;
        bottomTempCurrent = bTempCurrent;
        bottomTimerStatus = bTimerStatus;
      }
      else
      {
        DEBUG_SERIAL.println("Oven is off");

        checkInterval = offInterval;
        mode = -1; // Off

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
  setupOTA();
  setupConfig();
  setupFastLED();
  fetchOvenNumbers();
}

void loop()
{
  ArduinoOTA.handle();

  unsigned long currentMillis = millis();

  if (currentMillis - runtime >= checkInterval)
  {
    DEBUG_SERIAL.println();
    DEBUG_SERIAL.println("Checking runtime..");
    runtime = currentMillis;
    fetchOvenNumbers();
  }
  else if (mode > -1) // Not Off
  {
    DEBUG_SERIAL.println();
    DEBUG_SERIAL.println("Generating LED state..");

    int leds = usableLeds[mode];
    int mainTempProgress = map(mainTempCurrent + 1, 0, mainTempTarget, 0, leds);
    int bottomTempProgress = map(bottomTempCurrent + 1, 0, bottomTempTarget, 0, leds);
    int mainTimerProgress = map(mainTimerStatus, 0, 100, 0, leds);
    int bottomTimerProgress = map(bottomTimerStatus, 0, 100, 0, leds);

    updateLEDs(mainTempProgress, bottomTempProgress, mainTimerProgress, bottomTimerProgress);
  }

  if (Serial.available())
  {
    String cmd = Serial.readString();
    Serial.println(cmd);
    if (cmd.equals("reset\n"))
    {
      ESP.restart();
    }
  }
}