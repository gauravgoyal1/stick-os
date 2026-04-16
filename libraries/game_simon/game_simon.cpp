#include <M5StickCPlus2.h>
#include <arcade_common.h>
#include <stick_os.h>
#include "game_simon.h"

namespace GameSimon {

using namespace ArcadeCommon;

void init() {
    initColors();
    StickCP2.Speaker.begin();
    StickCP2.Speaker.setVolume(160);

    StickCP2.Display.setRotation(1);
    StickCP2.Display.fillScreen(C_BLACK);

    int sequence[20];
    int seqLen = 1;
    int score = 0;

    uint16_t colorA = StickCP2.Display.color565(255, 100, 100); // Red for A
    uint16_t colorB = StickCP2.Display.color565(100, 100, 255); // Blue for B

    StickCP2.Display.setCursor(50, 40);
    StickCP2.Display.setTextColor(C_YELLOW);
    StickCP2.Display.print("SIMON SAYS");
    StickCP2.Display.setCursor(30, 60);
    StickCP2.Display.setTextColor(C_WHITE);
    StickCP2.Display.print("Watch & Repeat!");
    StickCP2.Display.setCursor(40, 90);
    StickCP2.Display.setTextColor(colorA);
    StickCP2.Display.print("A");
    StickCP2.Display.setTextColor(C_WHITE);
    StickCP2.Display.print(" = Left  ");
    StickCP2.Display.setTextColor(colorB);
    StickCP2.Display.print("B");
    StickCP2.Display.setTextColor(C_WHITE);
    StickCP2.Display.print(" = Right");
    delay(2500);

    // Generate initial sequence
    for(int i = 0; i < 20; i++) sequence[i] = random(0, 2); // 0=A, 1=B

    auto showButton = [](int btn, bool lit) {
        int x = (btn == 0) ? 50 : 140;
        uint16_t col = lit ? ((btn == 0) ? RED : BLUE) : StickCP2.Display.color565(50, 50, 50);
        StickCP2.Display.fillRoundRect(x, 40, 50, 60, 8, col);
        StickCP2.Display.setTextColor(C_WHITE);
        StickCP2.Display.setCursor(x + 18, 60);
        StickCP2.Display.setTextSize(2);
        StickCP2.Display.print(btn == 0 ? "A" : "B");
        StickCP2.Display.setTextSize(1);
    };

    auto playSequence = [&]() {
        StickCP2.Display.fillScreen(C_BLACK);
        StickCP2.Display.setCursor(80, 10);
        StickCP2.Display.setTextColor(C_YELLOW);
        StickCP2.Display.print("WATCH!");
        showButton(0, false);
        showButton(1, false);
        delay(500);

        for(int i = 0; i < seqLen; i++) {
            showButton(sequence[i], true);
            StickCP2.Speaker.tone(sequence[i] == 0 ? 440 : 880, 200);
            delay(400);
            showButton(sequence[i], false);
            delay(200);
        }
    };

    bool running = true;
    while(running && seqLen <= 20) {
        playSequence();

        StickCP2.Display.fillRect(0, 0, 240, 30, C_BLACK);
        StickCP2.Display.setCursor(70, 10);
        StickCP2.Display.setTextColor(C_GREEN);
        StickCP2.Display.print("YOUR TURN!");

        // Clear button states
        if (ArcadeCommon::updateAndCheckExit()) return;

        for(int i = 0; i < seqLen; i++) {
            bool gotInput = false;
            while(!gotInput) {
                if (ArcadeCommon::updateAndCheckExit()) return;

                if(StickCP2.BtnA.wasPressed()) {
                    showButton(0, true);
                    StickCP2.Speaker.tone(440, 100);
                    delay(150);
                    showButton(0, false);

                    if(sequence[i] != 0) {
                        running = false;
                        break;
                    }
                    gotInput = true;
                }

                if(StickCP2.BtnB.wasPressed()) {
                    showButton(1, true);
                    StickCP2.Speaker.tone(880, 100);
                    delay(150);
                    showButton(1, false);

                    if(sequence[i] != 1) {
                        running = false;
                        break;
                    }
                    gotInput = true;
                }
                delay(10);
            }
            if(!running) break;
        }

        if(running) {
            score = seqLen;
            seqLen++;
            StickCP2.Display.fillRect(0, 0, 240, 30, C_BLACK);
            StickCP2.Display.setCursor(70, 10);
            StickCP2.Display.setTextColor(C_CYAN);
            StickCP2.Display.printf("SCORE: %d", score);
            StickCP2.Speaker.tone(1500, 100);
            delay(300);
            StickCP2.Speaker.tone(2000, 100);
            delay(800);
        }
    }

    int highScore = loadHighScore("simon");
    if(saveHighScoreIfBetter("simon", score)) highScore = score;

    StickCP2.Speaker.tone(200, 300);
    delay(100);
    StickCP2.Speaker.tone(150, 300);
    StickCP2.Display.fillScreen(C_BLACK);
    StickCP2.Display.setCursor(60, 50); StickCP2.Display.setTextColor(C_RED); StickCP2.Display.print("WRONG!");
    StickCP2.Display.setCursor(60, 70); StickCP2.Display.setTextColor(C_WHITE); StickCP2.Display.printf("Score: %d", score);
    StickCP2.Display.setCursor(60, 90); StickCP2.Display.printf("Best: %d", highScore);
    delay(1000);
    while(!StickCP2.BtnA.wasPressed()) { if (ArcadeCommon::updateAndCheckExit()) return; delay(10); }
}

void tick() {
    // No-op. The game runs to completion inside init().
}

void icon(int x, int y, uint16_t color) {
    auto& d = StickCP2.Display;
    // Four small colored squares in a 2x2 grid
    d.fillRect(x + 6,  y + 4,  12, 10, color);
    d.fillRect(x + 20, y + 4,  12, 10, color);
    d.fillRect(x + 6,  y + 16, 12, 10, color);
    d.fillRect(x + 20, y + 16, 12, 10, color);
}

}  // namespace GameSimon

static const stick_os::AppDescriptor kDesc = {
    "simon", "Simon", "1.0.0",
    stick_os::CAT_GAME, stick_os::APP_NONE,
    &GameSimon::icon, stick_os::RUNTIME_NATIVE,
    { &GameSimon::init, &GameSimon::tick, nullptr, nullptr },
    { nullptr, nullptr },
};
STICK_REGISTER_APP(kDesc);
