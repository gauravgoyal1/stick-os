#include <M5StickCPlus2.h>
#include <arcade_common.h>
#include <stick_os.h>
#include "game_flappy.h"

namespace GameFlappy {

using namespace ArcadeCommon;

void init() {
    initColors();
    StickCP2.Speaker.begin();
    StickCP2.Speaker.setVolume(160);

    // Flappy-specific colors and sprite
    uint16_t C_FLAPPY_BG = StickCP2.Display.color565(138, 235, 244);
    uint16_t C_FLAPPY_PIPE = StickCP2.Display.color565(99, 255, 78);
    uint16_t C_FLAPPY_BIRD = StickCP2.Display.color565(255, 254, 174);

    uint16_t birdSprite[64];
    for(int i=0; i<64; i++) birdSprite[i] = C_FLAPPY_BIRD;
    birdSprite[13] = C_BLACK; birdSprite[14] = C_BLACK;
    birdSprite[21] = C_BLACK; birdSprite[22] = C_BLACK;
    birdSprite[23] = C_RED; birdSprite[31] = C_RED;

    StickCP2.Display.setRotation(0); // Portrait
    StickCP2.Display.fillScreen(C_FLAPPY_BG);

    const float GRAVITY = 9.8;
    const float JUMP = -2.3;

    float birdY = SCREEN_H_PORTRAIT / 2;
    float birdVel = 0;
    int birdX = 30;
    // Sprite size is 8x8
    int birdSize = 8;

    int pipeX = SCREEN_W_PORTRAIT;
    int pipeGapY = 100;
    int pipeGapH = 45;
    int pipeW = 18;

    int score = 0;
    bool passedPipe = false;

    // Initial Draw (Background Floor)
    int floorH = 20;
    int gameH = SCREEN_H_PORTRAIT - floorH;
    StickCP2.Display.fillRect(0, gameH, SCREEN_W_PORTRAIT, floorH, C_ORANGE);
    StickCP2.Display.drawFastHLine(0, gameH, SCREEN_W_PORTRAIT, C_BLACK);

    StickCP2.Display.setTextColor(C_BLACK);
    StickCP2.Display.setTextSize(2);
    StickCP2.Display.setCursor(20, 100);
    StickCP2.Display.print("PRESS A");
    if (ArcadeCommon::updateAndCheckExit()) return; // Flush inputs
    while(!StickCP2.BtnA.wasPressed()) { if (ArcadeCommon::updateAndCheckExit()) return; delay(10); }
    StickCP2.Display.fillScreen(C_FLAPPY_BG);
    StickCP2.Display.fillRect(0, gameH, SCREEN_W_PORTRAIT, floorH, C_ORANGE);
    StickCP2.Display.drawFastHLine(0, gameH, SCREEN_W_PORTRAIT, C_BLACK);

    double oldTime = millis();
    bool running = true;

    // Explicitly using birdOldY to erase correctly
    int birdOldY = birdY;

    while(running) {
        if (ArcadeCommon::updateAndCheckExit()) return;

        // Input
        if(StickCP2.BtnA.wasPressed()) {
            birdVel = JUMP;
            StickCP2.Speaker.tone(1500, 30);
        }

        // Physics Timer
        double currentTime = millis();
        float delta = (currentTime - oldTime) / 1000.0;
        oldTime = currentTime;

        // --- RENDERING ---

        // 1. Erase Bird (Fill rect at old position)
        StickCP2.Display.fillRect(birdX, birdOldY, birdSize, birdSize, C_FLAPPY_BG);

        // 2. Erase Pipe (Redraw BG line)
        if (pipeX <= SCREEN_W_PORTRAIT) {
             StickCP2.Display.drawFastVLine(pipeX + pipeW, 0, gameH, C_FLAPPY_BG);
             StickCP2.Display.drawFastVLine(pipeX + pipeW + 1, 0, gameH, C_FLAPPY_BG); // Extra line for speed cleanup
        }

        // 3. Update Positions
        birdVel += GRAVITY * delta;
        birdY += birdVel;
        birdOldY = (int)birdY;

        pipeX -= 2;
        if(pipeX < -pipeW) {
            pipeX = SCREEN_W_PORTRAIT;
            pipeGapY = random(20, gameH - pipeGapH - 20);
            passedPipe = false;
        }

        // 4. Draw Pipe
        if (pipeX < SCREEN_W_PORTRAIT) {
            StickCP2.Display.fillRect(pipeX, 0, pipeW, pipeGapY, C_FLAPPY_PIPE);
            StickCP2.Display.fillRect(pipeX, pipeGapY + pipeGapH, pipeW, gameH - (pipeGapY + pipeGapH), C_FLAPPY_PIPE);
            // Pipe Outlines
            StickCP2.Display.drawRect(pipeX, 0, pipeW, pipeGapY, C_BLACK);
            StickCP2.Display.drawRect(pipeX, pipeGapY + pipeGapH, pipeW, gameH - (pipeGapY + pipeGapH), C_BLACK);
        }

        // 5. Draw Bird (Using Sprite)
        StickCP2.Display.pushImage(birdX, (int)birdY, birdSize, birdSize, birdSprite);

        // Collision
        if(birdY > gameH - birdSize || birdY < 0) running = false;
        if(birdX + birdSize > pipeX && birdX < pipeX + pipeW) {
            if(birdY < pipeGapY || birdY + birdSize > pipeGapY + pipeGapH) running = false;
        }

        // Score
        if(birdX > pipeX + pipeW && !passedPipe) {
            passedPipe = true;
            score++;
            StickCP2.Speaker.tone(3000, 50);
            StickCP2.Display.fillRect(0, 0, 50, 20, C_FLAPPY_BG);
            StickCP2.Display.setCursor(5, 5);
            StickCP2.Display.setTextColor(C_WHITE);
            StickCP2.Display.print(score);
        }

        delay(20);
    }

    int highScore = loadHighScore("flappy");
    if(saveHighScoreIfBetter("flappy", score)) highScore = score;

    StickCP2.Speaker.tone(200, 500);
    StickCP2.Display.fillScreen(C_BLACK);
    StickCP2.Display.setTextColor(C_RED);
    StickCP2.Display.setCursor(20, 80); StickCP2.Display.print("GAME OVER");
    StickCP2.Display.setTextColor(C_WHITE);
    StickCP2.Display.setCursor(20, 110); StickCP2.Display.printf("Score: %d", score);
    StickCP2.Display.setCursor(20, 130); StickCP2.Display.printf("Best: %d", highScore);
    delay(1000);
    while(!StickCP2.BtnA.wasPressed()) { if (ArcadeCommon::updateAndCheckExit()) return; delay(10); }
}

void tick() {
    // No-op. The game runs to completion inside init().
}

void icon(int x, int y, uint16_t color) {
    auto& d = StickCP2.Display;
    d.fillTriangle(x + 4, y + 16, x + 28, y + 8, x + 28, y + 24, color);
    d.fillCircle(x + 22, y + 14, 2, BLACK);
}

}  // namespace GameFlappy

static const stick_os::AppDescriptor kDesc = {
    "flappy", "Flappy", "1.0.0",
    stick_os::CAT_GAME, stick_os::APP_NONE,
    &GameFlappy::icon, stick_os::RUNTIME_NATIVE,
    { &GameFlappy::init, &GameFlappy::tick, nullptr, nullptr },
    { nullptr, nullptr },
};
STICK_REGISTER_APP(kDesc);
