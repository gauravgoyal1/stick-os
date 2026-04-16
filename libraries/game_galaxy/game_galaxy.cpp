#include <M5StickCPlus2.h>
#include <arcade_common.h>
#include <stick_os.h>
#include "game_galaxy.h"

namespace GameGalaxy {

using namespace ArcadeCommon;

void init() {
    initColors();
    StickCP2.Speaker.begin();
    StickCP2.Speaker.setVolume(160);
    StickCP2.Imu.init();

    StickCP2.Display.setRotation(1);
    StickCP2.Display.fillScreen(C_BLACK);

    float playerX = 110;
    int score = 0;

    struct Meteor { float x, y, speed; bool active; };
    Meteor meteors[5];
    for(int i=0; i<5; i++) meteors[i].active = false;

    StickCP2.Display.setCursor(50, 50); StickCP2.Display.setTextColor(C_CYAN); StickCP2.Display.print("TILT TO MOVE");
    delay(1500);
    StickCP2.Display.fillScreen(C_BLACK);

    bool running = true;
    while(running) {
        if (ArcadeCommon::updateAndCheckExit()) return;
        StickCP2.Imu.update();
        auto imuData = StickCP2.Imu.getImuData();

        // --- UPDATED TILT AXIS ---
        // Switched from accel.x to accel.y to fix tilt direction
        // Multiplied by -10.0 or 10.0 depending on desired inversion
        float tilt = imuData.accel.y;

        StickCP2.Display.fillRect((int)playerX, 115, 16, 16, C_BLACK);

        // Move Player (Sensitivity 12.0)
        // If it moves opposite to tilt, change '+' to '-'
        playerX += (tilt * 12.0);

        // Bounds
        if(playerX < 0) playerX = 0;
        if(playerX > 224) playerX = 224;

        if(random(0, 20) == 0) {
            for(int i=0; i<5; i++) {
                if(!meteors[i].active) {
                    meteors[i].active = true;
                    meteors[i].x = random(0, 230);
                    meteors[i].y = -10;
                    meteors[i].speed = random(2, 6);
                    break;
                }
            }
        }

        for(int i=0; i<5; i++) {
            if(meteors[i].active) {
                StickCP2.Display.fillRect((int)meteors[i].x, (int)meteors[i].y, 10, 10, C_BLACK);
                meteors[i].y += meteors[i].speed;

                if(meteors[i].y > 135) {
                    meteors[i].active = false;
                    score++;
                    StickCP2.Speaker.tone(1000, 20);
                } else {
                    StickCP2.Display.fillRect((int)meteors[i].x, (int)meteors[i].y, 10, 10, C_RED);
                }

                if(playerX < meteors[i].x + 10 && playerX + 16 > meteors[i].x &&
                   115 < meteors[i].y + 10 && 115 + 16 > meteors[i].y) {
                       running = false;
                   }
            }
        }

        StickCP2.Display.fillRect((int)playerX, 115, 16, 16, C_BLUE);
        StickCP2.Display.setCursor(5, 5); StickCP2.Display.setTextColor(C_WHITE, C_BLACK); StickCP2.Display.print(score);

        if(StickCP2.BtnA.wasPressed()) running = false;
        delay(15);
    }

    int highScore = loadHighScore("galaxy");
    if(saveHighScoreIfBetter("galaxy", score)) highScore = score;

    StickCP2.Speaker.tone(150, 500);
    StickCP2.Display.fillScreen(C_BLACK);
    StickCP2.Display.setCursor(60, 50); StickCP2.Display.setTextColor(C_RED); StickCP2.Display.print("CRASHED!");
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
    // Ship triangle pointing up + two dots (stars)
    d.fillTriangle(x + 18, y + 4, x + 10, y + 24, x + 26, y + 24, color);
    d.fillCircle(x + 6, y + 8, 2, color);
    d.fillCircle(x + 30, y + 14, 2, color);
}

}  // namespace GameGalaxy

static const stick_os::AppDescriptor kDesc = {
    "galaxy", "Galaxy", "1.0.0",
    stick_os::CAT_GAME, stick_os::APP_NONE,
    &GameGalaxy::icon, stick_os::RUNTIME_NATIVE,
    { &GameGalaxy::init, &GameGalaxy::tick, nullptr, nullptr },
    { nullptr, nullptr },
};
STICK_REGISTER_APP(kDesc);
