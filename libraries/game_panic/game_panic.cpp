#include <M5StickCPlus2.h>
#include <arcade_common.h>
#include <stick_os.h>
#include "game_panic.h"

namespace GamePanic {

using namespace ArcadeCommon;

void init() {
    initColors();
    StickCP2.Speaker.begin();
    StickCP2.Speaker.setVolume(160);
    StickCP2.Mic.begin();
    StickCP2.Imu.init();

    StickCP2.Display.setRotation(1);
    StickCP2.Display.fillScreen(C_BLACK);

    int score = 0;
    int timeLimit = 2000; // ms to react

    StickCP2.Display.setCursor(40, 40);
    StickCP2.Display.setTextColor(C_YELLOW);
    StickCP2.Display.print("PANIC MODE!");
    StickCP2.Display.setCursor(20, 70);
    StickCP2.Display.setTextColor(C_WHITE);
    StickCP2.Display.print("React to prompts!");
    delay(2000);

    const char* prompts[] = { "PRESS A!", "PRESS B!", "SHAKE!", "SCREAM!" };
    uint16_t promptColors[] = { C_RED, C_BLUE, C_GREEN, C_YELLOW };

    bool running = true;
    while(running) {
        int challenge = random(0, 4);

        StickCP2.Display.fillScreen(C_BLACK);
        StickCP2.Display.setTextSize(2);
        StickCP2.Display.setCursor(30, 55);
        StickCP2.Display.setTextColor(promptColors[challenge]);
        StickCP2.Display.print(prompts[challenge]);
        StickCP2.Display.setTextSize(1);

        unsigned long startTime = millis();
        bool success = false;

        // Clear inputs
        if (ArcadeCommon::updateAndCheckExit()) return;

        while(millis() - startTime < (unsigned long)timeLimit) {
            if (ArcadeCommon::updateAndCheckExit()) return;
            StickCP2.Imu.update();

            if(challenge == 0 && StickCP2.BtnA.wasPressed()) {
                success = true;
                break;
            }
            if(challenge == 1 && StickCP2.BtnB.wasPressed()) {
                success = true;
                break;
            }
            if(challenge == 2) {
                auto imu = StickCP2.Imu.getImuData();
                float mag = sqrt(imu.accel.x*imu.accel.x + imu.accel.y*imu.accel.y + imu.accel.z*imu.accel.z);
                if(mag > 2.0) { // Shake threshold
                    success = true;
                    break;
                }
            }
            if(challenge == 3) {
                int noise = getNoiseLevel();
                if(noise > 300) { // Scream threshold
                    success = true;
                    break;
                }
            }

            // Progress bar (y=125 is well below the OS strip)
            int elapsed = millis() - startTime;
            int barW = map(elapsed, 0, timeLimit, 240, 0);
            StickCP2.Display.fillRect(0, 125, 240, 10, C_BLACK);
            StickCP2.Display.fillRect(0, 125, barW, 10, C_RED);

            delay(15);
        }

        if(success) {
            score++;
            StickCP2.Speaker.tone(2000, 50);
            StickCP2.Display.fillScreen(C_GREEN);
            StickCP2.Display.setTextSize(2);
            StickCP2.Display.setCursor(70, 55);
            StickCP2.Display.setTextColor(C_BLACK);
            StickCP2.Display.print("NICE!");
            StickCP2.Display.setTextSize(1);
            delay(400);

            // Speed up every 5 points
            if(score % 5 == 0 && timeLimit > 800) {
                timeLimit -= 150;
            }
        } else {
            running = false;
        }
    }

    int highScore = loadHighScore("panic");
    if(saveHighScoreIfBetter("panic", score)) highScore = score;

    StickCP2.Speaker.tone(100, 300);
    StickCP2.Display.fillScreen(C_BLACK);
    StickCP2.Display.setTextSize(2);
    StickCP2.Display.setCursor(40, 40); StickCP2.Display.setTextColor(C_RED); StickCP2.Display.print("TOO SLOW!");
    StickCP2.Display.setTextSize(1);
    StickCP2.Display.setCursor(60, 80); StickCP2.Display.setTextColor(C_WHITE); StickCP2.Display.printf("Score: %d", score);
    StickCP2.Display.setCursor(60, 100); StickCP2.Display.printf("Best: %d", highScore);
    delay(1000);
    while(!StickCP2.BtnA.wasPressed()) { if (ArcadeCommon::updateAndCheckExit()) return; delay(10); }
}

void tick() {
    // No-op. The game runs to completion inside init().
}

void icon(int x, int y, uint16_t color) {
    auto& d = StickCP2.Display;
    // Exclamation mark: tall rect + dot below
    d.fillRect(x + 14, y + 4, 8, 14, color);
    d.fillRect(x + 14, y + 22, 8, 6, color);
}

}  // namespace GamePanic

static const stick_os::AppDescriptor kDesc = {
    "panic", "Panic", "1.0.0",
    stick_os::CAT_GAME, stick_os::APP_NEEDS_MIC,
    &GamePanic::icon, stick_os::RUNTIME_NATIVE,
    { &GamePanic::init, &GamePanic::tick, nullptr, nullptr },
    { nullptr, nullptr },
};
STICK_REGISTER_APP(kDesc);
