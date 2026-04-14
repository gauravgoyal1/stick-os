// IR pin sweep diagnostic.
// Turns each candidate GPIO on for 500 ms, one after another, and shows
// which pin is currently hot on the LCD. Point the phone camera at the
// Stick; whichever pin makes the camera show a purple/white glow is the
// real IR LED pin.

#include <M5Unified.h>

// Candidate pins we want to try. We include 9 and 19 (the two most
// commonly cited), plus a handful of nearby free GPIO for safety.
static const int kPins[] = { 9, 19, 10, 25, 26, 32, 33 };
static const size_t kPinCount = sizeof(kPins) / sizeof(kPins[0]);

static size_t gIdx = 0;

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(3);
  Serial.begin(115200);
  delay(200);
  Serial.println("ir_sweep booted");

  // Release anything M5Unified may have attached to these pins so we
  // can drive them cleanly.
  for (size_t i = 0; i < kPinCount; i++) {
    // ledcDetach is available in ESP32 Arduino core v3 (which is what
    // m5stack:esp32 3.2.5 uses). If compilation fails with "not
    // declared", fall back to ledcDetachPin in the else branch.
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcDetach(kPins[i]);
#else
    ledcDetachPin(kPins[i]);
#endif
    pinMode(kPins[i], OUTPUT);
    digitalWrite(kPins[i], LOW);
  }
  // Also explicitly disable the M5 power LED handler, which owns GPIO 19.
  M5.Power.setLed(0);
}

void loop() {
  M5.update();

  const int pin = kPins[gIdx];

  M5.Display.fillScreen(BLACK);
  M5.Display.setCursor(4, 4);
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(YELLOW);
  M5.Display.printf("PIN %d\n", pin);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(WHITE);
  M5.Display.printf("\nON 500ms");

  Serial.printf("pin %d ON\n", pin);
  digitalWrite(pin, HIGH);
  delay(500);
  digitalWrite(pin, LOW);
  Serial.printf("pin %d OFF\n", pin);

  // Short gap so the user can tell one pin apart from the next.
  M5.Display.fillScreen(BLACK);
  M5.Display.setCursor(4, 4);
  M5.Display.setTextSize(3);
  M5.Display.setTextColor(DARKGREY);
  M5.Display.printf("PIN %d\n", pin);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(WHITE);
  M5.Display.printf("\nOFF");
  delay(700);

  gIdx = (gIdx + 1) % kPinCount;
}
