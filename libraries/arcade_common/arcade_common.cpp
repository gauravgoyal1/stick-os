#include "arcade_common.h"
#include <stick_os.h>

namespace ArcadeCommon {

uint16_t C_BLACK, C_WHITE, C_GREEN, C_RED, C_BLUE, C_CYAN,
         C_YELLOW, C_ORANGE, C_GRAY;

void initColors() {
    C_BLACK  = BLACK;
    C_WHITE  = WHITE;
    C_GREEN  = GREEN;
    C_RED    = RED;
    C_BLUE   = BLUE;
    C_CYAN   = CYAN;
    C_YELLOW = YELLOW;
    C_ORANGE = ORANGE;
    C_GRAY   = StickCP2.Display.color565(128, 128, 128);
}

int getNoiseLevel() {
    static int16_t soundData[256];  // Static to prevent stack overflow
    memset(soundData, 0, sizeof(soundData));  // Clear buffer

    if (!StickCP2.Mic.isEnabled()) {
        StickCP2.Mic.begin();
        delay(10);  // Small delay for mic to initialize
    }

    StickCP2.Mic.record(soundData, 256, 16000);

    // Add timeout to prevent infinite loop if mic fails
    unsigned long startTime = millis();
    while(StickCP2.Mic.isRecording()) {
        if (millis() - startTime > 100) { // 100ms timeout
            break;
        }
        delay(1);
    }

    int32_t sum = 0;
    for (int i = 0; i < 256; i++) sum += abs(soundData[i]);
    return sum / 256;
}

uint32_t loadHighScore(const char* gameId) {
    stick_os::StickStore s("arcade_hs");
    return s.getU32(gameId, 0);
}

bool saveHighScoreIfBetter(const char* gameId, uint32_t score) {
    stick_os::StickStore s("arcade_hs");
    uint32_t cur = s.getU32(gameId, 0);
    if (score <= cur) return false;
    s.putU32(gameId, score);
    return true;
}

bool updateAndCheckExit() {
    StickCP2.update();
    return stick_os::checkAppExit();
}

}  // namespace ArcadeCommon
