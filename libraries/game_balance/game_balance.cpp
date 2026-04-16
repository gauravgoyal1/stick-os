#include <M5StickCPlus2.h>
#include <arcade_common.h>
#include <stick_os.h>
#include "game_balance.h"

namespace GameBalance {

using namespace ArcadeCommon;

void init() {
    initColors();
    StickCP2.Speaker.begin();
    StickCP2.Speaker.setVolume(160);
    StickCP2.Imu.init();

    StickCP2.Display.setRotation(1);
    StickCP2.Display.fillScreen(C_BLACK);

    float ballX = 120, ballY = 67;
    float velX = 0, velY = 0;
    int score = 0;
    int level = 1;

    // Holes - obstacles to avoid
    struct Hole { int x, y, r; bool active; };
    Hole holes[6];
    for(int i = 0; i < 6; i++) holes[i].active = false;

    // Goal position
    int goalX = 200, goalY = 100;
    int goalR = 12;

    StickCP2.Display.setCursor(40, 50);
    StickCP2.Display.setTextColor(C_YELLOW);
    StickCP2.Display.print("TILT TO ROLL");
    StickCP2.Display.setCursor(40, 70);
    StickCP2.Display.print("REACH THE GOAL!");
    delay(2000);

    auto spawnHoles = [&](int count) {
        for(int i = 0; i < 6; i++) holes[i].active = false;
        for(int i = 0; i < count && i < 6; i++) {
            holes[i].active = true;
            holes[i].x = random(40, 200);
            holes[i].y = random(30, 100);
            holes[i].r = random(8, 15);
        }
        goalX = random(180, 220);
        goalY = random(20, 110);
    };

    spawnHoles(level + 1);

    bool running = true;
    while(running) {
        if (ArcadeCommon::updateAndCheckExit()) return;
        StickCP2.Imu.update();
        auto imuData = StickCP2.Imu.getImuData();

        // Erase ball
        StickCP2.Display.fillCircle((int)ballX, (int)ballY, 6, C_BLACK);

        // Physics
        float tiltX = imuData.accel.y * 0.5;
        float tiltY = imuData.accel.x * 0.5;
        velX += tiltX;
        velY += tiltY;
        velX *= 0.95; velY *= 0.95; // Friction
        ballX += velX;
        ballY += velY;

        // Bounds
        if(ballX < 8) { ballX = 8; velX = 0; }
        if(ballX > 232) { ballX = 232; velX = 0; }
        if(ballY < 8) { ballY = 8; velY = 0; }
        if(ballY > 127) { ballY = 127; velY = 0; }

        // Redraw everything
        StickCP2.Display.fillScreen(C_BLACK);

        // Draw holes
        for(int i = 0; i < 6; i++) {
            if(holes[i].active) {
                StickCP2.Display.fillCircle(holes[i].x, holes[i].y, holes[i].r, C_RED);
                // Check collision
                float dx = ballX - holes[i].x;
                float dy = ballY - holes[i].y;
                if(sqrt(dx*dx + dy*dy) < holes[i].r - 2) {
                    running = false;
                }
            }
        }

        // Draw goal
        StickCP2.Display.fillCircle(goalX, goalY, goalR, C_GREEN);
        StickCP2.Display.drawCircle(goalX, goalY, goalR, C_WHITE);

        // Check goal reached
        float dgx = ballX - goalX;
        float dgy = ballY - goalY;
        if(sqrt(dgx*dgx + dgy*dgy) < goalR) {
            score++;
            level++;
            StickCP2.Speaker.tone(2000, 100);
            ballX = 30; ballY = 67;
            velX = 0; velY = 0;
            spawnHoles(min(level + 1, 6));
        }

        // Draw ball
        StickCP2.Display.fillCircle((int)ballX, (int)ballY, 6, C_CYAN);
        StickCP2.Display.drawCircle((int)ballX, (int)ballY, 6, C_WHITE);

        // Score
        StickCP2.Display.setCursor(5, 5);
        StickCP2.Display.setTextColor(C_WHITE);
        StickCP2.Display.printf("LVL:%d", level);

        if(StickCP2.BtnA.wasPressed()) running = false;
        delay(20);
    }

    // NOTE: Balance saves level, not score
    int highScore = loadHighScore("balance");
    if(saveHighScoreIfBetter("balance", level)) highScore = level;

    StickCP2.Speaker.tone(150, 500);
    StickCP2.Display.fillScreen(C_BLACK);
    StickCP2.Display.setCursor(60, 50); StickCP2.Display.setTextColor(C_RED); StickCP2.Display.print("FELL IN!");
    StickCP2.Display.setCursor(60, 70); StickCP2.Display.setTextColor(C_WHITE); StickCP2.Display.printf("Level: %d", level);
    StickCP2.Display.setCursor(60, 90); StickCP2.Display.printf("Best: %d", highScore);
    delay(1000);
    while(!StickCP2.BtnA.wasPressed()) { if (ArcadeCommon::updateAndCheckExit()) return; delay(10); }
}

void tick() {
    // No-op. The game runs to completion inside init().
}

void icon(int x, int y, uint16_t color) {
    auto& d = StickCP2.Display;
    // Horizontal bar + circle on top
    d.fillRect(x + 4, y + 20, 28, 4, color);
    d.fillCircle(x + 18, y + 14, 5, color);
}

}  // namespace GameBalance

static const stick_os::AppDescriptor kDesc = {
    "balance", "Balance", "1.0.0",
    stick_os::CAT_GAME, stick_os::APP_NONE,
    &GameBalance::icon, stick_os::RUNTIME_NATIVE,
    { &GameBalance::init, &GameBalance::tick, nullptr, nullptr },
    { nullptr, nullptr },
};
STICK_REGISTER_APP(kDesc);
