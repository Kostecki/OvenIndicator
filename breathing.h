/*
  Breathing effect
  https://github.com/marmilicious/FastLED_examples/blob/master/breath_effect_v2.ino
*/
static float pulseSpeed = 0.50; // Larger value gives faster pulse.

uint8_t hueA = 255;     // Start hue at valueMin.
uint8_t satA = 0;       // Start saturation at valueMin.
float valueMin = 150.0; // Pulse minimum value (Should be less then valueMax).

uint8_t hueB = 255;     // End hue at valueMax.
uint8_t satB = 0;       // End saturation at valueMax.
float valueMax = 255.0; // Pulse maximum value (Should be larger then valueMin).

uint8_t hue = hueA;                                      // Do Not Edit
uint8_t sat = satA;                                      // Do Not Edit
float val = valueMin;                                    // Do Not Edit
uint8_t hueDelta = hueA - hueB;                          // Do Not Edit
static float delta = (valueMax - valueMin) / 2.35040238; // Do Not Edit