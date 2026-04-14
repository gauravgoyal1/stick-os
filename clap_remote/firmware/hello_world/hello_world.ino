#include <M5Unified.h>

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(3);
  M5.Display.fillScreen(BLACK);
  M5.Display.setTextColor(GREEN);
  M5.Display.setTextSize(2);
  M5.Display.setCursor(10, 10);
  M5.Display.print("HELLO");
  M5.Display.setCursor(10, 40);
  M5.Display.print("voice-remote");

  Serial.begin(115200);
  Serial.println("hello_world booted");
}

void loop() {
  M5.update();
  delay(50);
}
