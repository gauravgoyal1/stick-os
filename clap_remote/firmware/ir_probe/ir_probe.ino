// IR probe: cycles through candidate TCL NEC power codes.
// Button A: send current candidate (twice: once as NEC raw, once as NEC MSB).
// Button B: advance to next candidate.
//
// Setup / IR pin follow the official M5StickCPlus2 ir_nec example.

#define DISABLE_CODE_FOR_RECEIVER
#define SEND_PWM_BY_TIMER
#define IR_TX_PIN 19

#include <M5Unified.h>
#include <IRremote.hpp>
#include "../common/ir_codes.h"

static size_t gIndex = 0;

static void drawScreen() {
  M5.Display.fillScreen(BLACK);
  M5.Display.setCursor(4, 4);
  M5.Display.setTextSize(2);
  M5.Display.setTextColor(WHITE);
  M5.Display.println("IR PROBE");

  M5.Display.setTextSize(1);
  M5.Display.setCursor(4, 36);
  M5.Display.printf("%zu / %zu\n", gIndex + 1, kTclCandidateCount);
  M5.Display.setCursor(4, 48);
  M5.Display.println(kTclCandidates[gIndex].label);

  M5.Display.setCursor(4, 80);
  M5.Display.setTextColor(GREEN);
  M5.Display.println("A: send");
  M5.Display.setTextColor(CYAN);
  M5.Display.println("B: next");
  M5.Display.setTextColor(WHITE);
}

static void flashSent() {
  M5.Display.fillRect(0, 100, 240, 30, DARKGREEN);
  M5.Display.setTextColor(WHITE, DARKGREEN);
  M5.Display.setCursor(4, 108);
  M5.Display.print("sent!");
  delay(300);
  drawScreen();
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(3);

  Serial.begin(115200);
  delay(200);
  Serial.println("ir_probe booted");

  IrSender.begin(DISABLE_LED_FEEDBACK);
  IrSender.setSendPin(IR_TX_PIN);

  drawScreen();
}

void loop() {
  M5.update();

  if (M5.BtnA.wasPressed()) {
    const auto& c = kTclCandidates[gIndex];
    Serial.printf("SEND %s raw=0x%08lX (raw+msb variants)\n",
                  c.label, (unsigned long)c.raw);
    // Try both bit orderings so at least one matches the receiver.
    IrSender.sendNECRaw(c.raw, 0);
    delay(80);
    IrSender.sendNECMSB(c.raw, 32, false);
    flashSent();
  }

  if (M5.BtnB.wasPressed()) {
    gIndex = (gIndex + 1) % kTclCandidateCount;
    Serial.printf("SELECT %s\n", kTclCandidates[gIndex].label);
    drawScreen();
  }

  delay(10);
}
