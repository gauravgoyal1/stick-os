#pragma once

#include <M5StickCPlus2.h>
#include <stdint.h>

namespace ArcadeCommon {

// Screen dimensions for the two orientations.
#define SCREEN_W_LANDSCAPE 240
#define SCREEN_H_LANDSCAPE 135
#define SCREEN_W_PORTRAIT  135
#define SCREEN_H_PORTRAIT  240

// Color palette -- call initColors() once before use.
extern uint16_t C_BLACK, C_WHITE, C_GREEN, C_RED, C_BLUE, C_CYAN,
                C_YELLOW, C_ORANGE, C_GRAY;
void initColors();

// Microphone noise level (average absolute amplitude over 256 samples).
// Used by Scream and Panic.
int getNoiseLevel();

// High-score persistence via StickStore. Each game passes its own
// slug ("flappy", "dino", ...) as the key.
uint32_t loadHighScore(const char* gameId);
bool     saveHighScoreIfBetter(const char* gameId, uint32_t score);

// Call this instead of StickCP2.update() inside game loops. Updates
// button state AND checks if the user pressed PWR to exit. Returns
// true if the game should immediately return from init().
bool updateAndCheckExit();

}  // namespace ArcadeCommon
