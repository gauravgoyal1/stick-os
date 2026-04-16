#include <M5StickCPlus2.h>
#include <arcade_common.h>
#include <stick_os.h>
#include "game_dino.h"

namespace GameDino {

using namespace ArcadeCommon;

void init() {
    initColors();
    StickCP2.Speaker.begin();
    StickCP2.Speaker.setVolume(160);

    StickCP2.Display.setRotation(1);
    StickCP2.Display.fillScreen(C_BLACK);

    int groundY = 110;
    int dinoX = 20;
    int dinoY = groundY - 20;
    int dinoVel = 0;
    bool jumping = false;

    int cactusX = SCREEN_W_LANDSCAPE;
    int cactusH = 20;
    int score = 0;
    int speed = 5;

    StickCP2.Display.drawFastHLine(0, groundY, SCREEN_W_LANDSCAPE, C_WHITE);
    StickCP2.Display.setCursor(80, 50); StickCP2.Display.print("PRESS A");
    if (ArcadeCommon::updateAndCheckExit()) return;
    while(!StickCP2.BtnA.wasPressed()) { if (ArcadeCommon::updateAndCheckExit()) return; delay(10); }
    StickCP2.Display.fillScreen(C_BLACK);
    StickCP2.Display.drawFastHLine(0, groundY, SCREEN_W_LANDSCAPE, C_WHITE);

    bool running = true;
    while(running) {
        if (ArcadeCommon::updateAndCheckExit()) return;

        StickCP2.Display.fillRect(dinoX, dinoY, 15, 20, C_BLACK);
        StickCP2.Display.fillRect(cactusX, groundY - cactusH, 12, cactusH, C_BLACK);

        if(StickCP2.BtnA.wasPressed() && !jumping) {
            jumping = true;
            dinoVel = -12;
            StickCP2.Speaker.tone(1200, 30);
        }

        if(jumping) {
            dinoY += dinoVel;
            dinoVel += 2;
            if(dinoY >= groundY - 20) {
                dinoY = groundY - 20;
                jumping = false;
                dinoVel = 0;
            }
        }

        cactusX -= speed;
        if(cactusX < -15) {
            cactusX = SCREEN_W_LANDSCAPE;
            score++;
            if(score % 5 == 0 && speed < 15) speed++;
            StickCP2.Speaker.tone(500, 20);
        }

        if (dinoX + 15 > cactusX && dinoX < cactusX + 12) {
            if (dinoY + 20 > groundY - cactusH) running = false;
        }

        StickCP2.Display.fillRect(dinoX, dinoY, 15, 20, C_GREEN);
        StickCP2.Display.fillRect(cactusX, groundY - cactusH, 12, cactusH, C_RED);
        StickCP2.Display.drawFastHLine(0, groundY, SCREEN_W_LANDSCAPE, C_WHITE);

        StickCP2.Display.setCursor(200, 5);
        StickCP2.Display.setTextColor(C_WHITE, C_BLACK);
        StickCP2.Display.print(score);

        delay(20);
    }

    int highScore = loadHighScore("dino");
    if(saveHighScoreIfBetter("dino", score)) highScore = score;

    StickCP2.Speaker.tone(100, 500);
    StickCP2.Display.setCursor(80, 50); StickCP2.Display.setTextColor(C_RED); StickCP2.Display.print("GAME OVER");
    StickCP2.Display.setCursor(80, 70); StickCP2.Display.setTextColor(C_WHITE); StickCP2.Display.printf("Score: %d", score);
    StickCP2.Display.setCursor(80, 90); StickCP2.Display.printf("Best: %d", highScore);
    delay(1000);
    while(!StickCP2.BtnA.wasPressed()) { if (ArcadeCommon::updateAndCheckExit()) return; delay(10); }
}

void tick() {
    // No-op. The game runs to completion inside init().
}

void icon(int x, int y, uint16_t color) {
    auto& d = StickCP2.Display;
    // T-Rex silhouette: body + head
    d.fillRect(x + 6, y + 10, 18, 14, color);
    d.fillRect(x + 18, y + 4, 12, 10, color);
}

}  // namespace GameDino

static const stick_os::AppDescriptor kDesc = {
    "dino", "Dino", "1.0.0",
    stick_os::CAT_GAME, stick_os::APP_NONE,
    &GameDino::icon, stick_os::RUNTIME_NATIVE,
    { &GameDino::init, &GameDino::tick, nullptr, nullptr },
    { nullptr, nullptr },
};
STICK_REGISTER_APP(kDesc);
