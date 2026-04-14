#include <M5StickCPlus2.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <time.h>
#include <wifi_config.h>
#include "arcade_app.h"

namespace ArcadeApp {

// ==========================================
//            WIFI CONFIGURATION
// ==========================================

// WiFi State
bool wifiConnected = false;
int wifiSignalStrength = 0;  // 0-4 bars

// ==========================================
//               GLOBAL SETTINGS
// ==========================================
#define SCREEN_W_LANDSCAPE 240
#define SCREEN_H_LANDSCAPE 135
#define SCREEN_W_PORTRAIT  135
#define SCREEN_H_PORTRAIT  240

// EEPROM Addresses
#define ADDR_SCORE_FLAPPY  0
#define ADDR_SCORE_DINO    4
#define ADDR_SCORE_SCREAM  8
#define ADDR_SCORE_GALAXY  12
#define ADDR_SCORE_BALANCE 16
#define ADDR_SCORE_SIMON   20
#define ADDR_SCORE_PANIC   24

// Menu State
int selectedGameIndex = 0;
const int NUM_GAMES = 7;
const char* gameTitles[] = { "FLAPPY BIRD", "DINO RUN", "SCREAM BIRD", "GALAXY DODGE", "BALANCE BALL", "SIMON SAYS", "PANIC MODE" };

// Shared Color Definitions
uint16_t C_BLACK, C_WHITE, C_GREEN, C_RED, C_BLUE, C_CYAN, C_YELLOW, C_ORANGE, C_GRAY;
uint16_t C_FLAPPY_BG, C_FLAPPY_PIPE, C_FLAPPY_BIRD;

// Flappy Bird Sprite (8x8 pixels)
uint16_t birdSprite[64];

// ==========================================
//           HELPER FUNCTIONS
// ==========================================

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
    C_FLAPPY_BG = StickCP2.Display.color565(138, 235, 244);
    C_FLAPPY_PIPE = StickCP2.Display.color565(99, 255, 78);
    C_FLAPPY_BIRD = StickCP2.Display.color565(255, 254, 174); // Classic Bird Color
    
    // Create simple Bird Sprite (Yellow block with an eye)
    for(int i=0; i<64; i++) birdSprite[i] = C_FLAPPY_BIRD;
    // Add Eye (Black pixel at index 18 and 19 approx)
    birdSprite[13] = C_BLACK; birdSprite[14] = C_BLACK;
    birdSprite[21] = C_BLACK; birdSprite[22] = C_BLACK; 
    // Add Beak (Red)
    birdSprite[23] = C_RED; birdSprite[31] = C_RED; 
}

// WiFi Connection Helper
void connectWiFi() {
    StickCP2.Display.fillScreen(C_BLACK);
    StickCP2.Display.setTextColor(C_WHITE);
    StickCP2.Display.setCursor(20, 50);
    StickCP2.Display.print("Connecting WiFi...");
    
    // Scan for visible SSIDs and connect to the first known one.
    int numScanned = WiFi.scanNetworks();
    bool attempted = false;
    for (int i = 0; i < numScanned && !attempted; i++) {
      String found = WiFi.SSID(i);
      for (size_t j = 0; j < kWiFiNetworkCount; j++) {
        if (found == kWiFiNetworks[j].ssid) {
          WiFi.begin(kWiFiNetworks[j].ssid, kWiFiNetworks[j].password);
          attempted = true;
          break;
        }
      }
    }
    // If nothing matched, WiFi stays disconnected. The surrounding code
    // already polls WiFi.status() so the existing retry logic handles it.
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        StickCP2.Display.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        StickCP2.Display.fillScreen(C_BLACK);
        StickCP2.Display.setCursor(20, 50);
        StickCP2.Display.setTextColor(C_GREEN);
        StickCP2.Display.print("WiFi Connected!");
        delay(500);
    } else {
        wifiConnected = false;
        StickCP2.Display.fillScreen(C_BLACK);
        StickCP2.Display.setCursor(20, 50);
        StickCP2.Display.setTextColor(C_RED);
        StickCP2.Display.print("WiFi Failed");
        delay(1000);
    }
}

// Update WiFi signal strength (0-4 bars)
void updateWiFiSignal() {
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        int rssi = WiFi.RSSI();
        if (rssi >= -50) wifiSignalStrength = 4;
        else if (rssi >= -60) wifiSignalStrength = 3;
        else if (rssi >= -70) wifiSignalStrength = 2;
        else if (rssi >= -80) wifiSignalStrength = 1;
        else wifiSignalStrength = 0;
    } else {
        wifiConnected = false;
        wifiSignalStrength = 0;
    }
}

// Sync time from NTP server  
void syncNTPTime() {
    if (!wifiConnected) return;
    
    StickCP2.Display.fillScreen(C_BLACK);
    StickCP2.Display.setTextColor(C_CYAN);
    StickCP2.Display.setCursor(20, 40);
    StickCP2.Display.print("Syncing NTP...");
    
    // Reset any previous time settings
    struct timeval tv = { 0, 0 };
    settimeofday(&tv, NULL);
    
    // Configure NTP: GMT offset = 5.5 hours = 19800 seconds for IST
    configTime(19800, 0, "time.google.com", "pool.ntp.org");
    
    // Wait 3 seconds for initial NTP request
    delay(3000);
    
    StickCP2.Display.setCursor(20, 60);
    StickCP2.Display.print("Getting time...");
    
    // Try to get time with timeout
    struct tm timeinfo = { 0 };
    int attempts = 0;
    bool success = false;
    
    while (attempts < 10 && !success) {
        if (getLocalTime(&timeinfo, 2000)) {
            // Verify year is reasonable (2024-2030)
            if (timeinfo.tm_year + 1900 >= 2024 && timeinfo.tm_year + 1900 <= 2030) {
                success = true;
            }
        }
        attempts++;
        delay(500);
    }
    
    if (success) {
        // Display synced time briefly
        StickCP2.Display.fillScreen(C_BLACK);
        StickCP2.Display.setCursor(20, 40);
        StickCP2.Display.setTextColor(C_GREEN);
        StickCP2.Display.printf("Time: %02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
        StickCP2.Display.setCursor(20, 60);
        StickCP2.Display.print("Synced!");
        
        // Set M5's RTC
        m5::rtc_datetime_t dt;
        dt.date.year = timeinfo.tm_year + 1900;
        dt.date.month = timeinfo.tm_mon + 1;
        dt.date.date = timeinfo.tm_mday;
        dt.time.hours = timeinfo.tm_hour;
        dt.time.minutes = timeinfo.tm_min;
        dt.time.seconds = timeinfo.tm_sec;
        StickCP2.Rtc.setDateTime(dt);
        
        delay(1000);
    } else {
        StickCP2.Display.fillScreen(C_BLACK);
        StickCP2.Display.setCursor(20, 50);
        StickCP2.Display.setTextColor(C_RED);
        StickCP2.Display.print("NTP Failed!");
        delay(2000);
    }
}

// Draw WiFi icon at specified position
void drawWiFiIcon(int x, int y) {
    updateWiFiSignal();
    
    if (!wifiConnected) {
        // Draw X for disconnected
        StickCP2.Display.setTextColor(C_RED);
        StickCP2.Display.setCursor(x, y);
        StickCP2.Display.print("X");
        return;
    }
    
    // Draw signal bars
    int barWidth = 3;
    int barSpacing = 2;
    int heights[] = {3, 5, 7, 9};
    
    for (int i = 0; i < 4; i++) {
        int barX = x + i * (barWidth + barSpacing);
        int barY = y + 9 - heights[i];
        uint16_t color = (i < wifiSignalStrength) ? C_GREEN : C_GRAY;
        StickCP2.Display.fillRect(barX, barY, barWidth, heights[i], color);
    }
}

// Microphone Helper
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

// ==========================================
//        GAME 1: FLAPPY BIRD (Restored)
// ==========================================
void playFlappy() {
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
    StickCP2.update(); // Flush inputs
    while(!StickCP2.BtnA.wasPressed()) { StickCP2.update(); delay(10); }
    StickCP2.Display.fillScreen(C_FLAPPY_BG);
    StickCP2.Display.fillRect(0, gameH, SCREEN_W_PORTRAIT, floorH, C_ORANGE);
    StickCP2.Display.drawFastHLine(0, gameH, SCREEN_W_PORTRAIT, C_BLACK);

    double oldTime = millis();
    bool running = true;
    
    // Explicitly using birdOldY to erase correctly
    int birdOldY = birdY;

    while(running) {
        StickCP2.update();
        
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
            StickCP2.Display.fillRect(0,0, 50, 20, C_FLAPPY_BG);
            StickCP2.Display.setCursor(5,5);
            StickCP2.Display.setTextColor(C_WHITE);
            StickCP2.Display.print(score);
        }
        
        delay(20);
    }
    
    int highScore = EEPROM.readInt(ADDR_SCORE_FLAPPY);
    if(score > highScore) { EEPROM.writeInt(ADDR_SCORE_FLAPPY, score); EEPROM.commit(); highScore = score; }
    
    StickCP2.Speaker.tone(200, 500);
    StickCP2.Display.fillScreen(C_BLACK);
    StickCP2.Display.setTextColor(C_RED);
    StickCP2.Display.setCursor(20, 80); StickCP2.Display.print("GAME OVER");
    StickCP2.Display.setTextColor(C_WHITE);
    StickCP2.Display.setCursor(20, 110); StickCP2.Display.printf("Score: %d", score);
    StickCP2.Display.setCursor(20, 130); StickCP2.Display.printf("Best: %d", highScore);
    delay(1000);
    while(!StickCP2.BtnA.wasPressed()) { StickCP2.update(); delay(10); }
}

// ==========================================
//        GAME 2: DINO RUN (Landscape)
// ==========================================
void playDino() {
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
    StickCP2.update();
    while(!StickCP2.BtnA.wasPressed()) { StickCP2.update(); delay(10); }
    StickCP2.Display.fillScreen(C_BLACK);
    StickCP2.Display.drawFastHLine(0, groundY, SCREEN_W_LANDSCAPE, C_WHITE);

    bool running = true;
    while(running) {
        StickCP2.update();
        
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
    
    int highScore = EEPROM.readInt(ADDR_SCORE_DINO);
    if(score > highScore) { EEPROM.writeInt(ADDR_SCORE_DINO, score); EEPROM.commit(); highScore = score; }

    StickCP2.Speaker.tone(100, 500);
    StickCP2.Display.setCursor(80, 50); StickCP2.Display.setTextColor(C_RED); StickCP2.Display.print("GAME OVER");
    StickCP2.Display.setCursor(80, 70); StickCP2.Display.setTextColor(C_WHITE); StickCP2.Display.printf("Score: %d", score);
    StickCP2.Display.setCursor(80, 90); StickCP2.Display.printf("Best: %d", highScore);
    delay(1000);
    while(!StickCP2.BtnA.wasPressed()) { StickCP2.update(); delay(10); }
}

// ==========================================
//      GAME 3: SCREAM BIRD (Mic)
// ==========================================
void playScream() {
    StickCP2.Display.setRotation(1);
    StickCP2.Display.fillScreen(C_BLACK);
    
    float birdY = SCREEN_H_LANDSCAPE / 2;
    int score = 0;
    int wallX = SCREEN_W_LANDSCAPE;
    int gapY = 50;
    
    // --- UPDATED SENSITIVITY ---
    // Lowered threshold to 250 for easier play
    int NOISE_THRESHOLD = 250; 

    StickCP2.Display.setCursor(30, 50); StickCP2.Display.setTextColor(C_YELLOW); StickCP2.Display.print("SCREAM TO FLY!");
    delay(1500);
    StickCP2.Display.fillScreen(C_BLACK);
    
    bool running = true;
    while(running) {
        StickCP2.update();
        
        int noise = getNoiseLevel();
        // Visual Bar
        int barLen = map(noise, 0, 1000, 0, SCREEN_W_LANDSCAPE);
        StickCP2.Display.drawFastHLine(0, 134, SCREEN_W_LANDSCAPE, C_BLACK);
        StickCP2.Display.drawFastHLine(0, 134, barLen, (noise > NOISE_THRESHOLD ? C_GREEN : C_RED));

        StickCP2.Display.fillRect(40, (int)birdY, 15, 15, C_BLACK); 
        StickCP2.Display.fillRect(wallX, 0, 20, 135, C_BLACK);      

        if(noise > NOISE_THRESHOLD) birdY -= 6; // Flap Strength
        else birdY += 2;                        // Fall Speed (Slower than before)
        
        // --- UPDATED SPEED ---
        // Wall moves slower (2 pixels per frame instead of 4)
        wallX -= 2; 
        if(wallX < -20) {
            wallX = SCREEN_W_LANDSCAPE;
            gapY = random(10, 80);
            score++;
            StickCP2.Speaker.tone(4000, 50);
        }
        
        StickCP2.Display.fillRect(40, (int)birdY, 15, 15, C_CYAN);
        StickCP2.Display.fillRect(wallX, 0, 20, gapY, C_GREEN); 
        StickCP2.Display.fillRect(wallX, gapY + 50, 20, 135, C_GREEN); 
        
        StickCP2.Display.setCursor(5, 5); StickCP2.Display.setTextColor(C_WHITE, C_BLACK); StickCP2.Display.print(score);

        if(birdY < 0 || birdY > 135) running = false;
        if(40 + 15 > wallX && 40 < wallX + 20) {
            if(birdY < gapY || birdY + 15 > gapY + 50) running = false;
        }
        
        if(StickCP2.BtnA.wasPressed()) running = false; 
        
        // Added slight delay to slow down the whole game
        delay(30);
    }
    
    int highScore = EEPROM.readInt(ADDR_SCORE_SCREAM);
    if(score > highScore) { EEPROM.writeInt(ADDR_SCORE_SCREAM, score); EEPROM.commit(); highScore = score; }

    StickCP2.Speaker.tone(150, 500);
    StickCP2.Display.fillScreen(C_BLACK);
    StickCP2.Display.setCursor(60, 50); StickCP2.Display.setTextColor(C_RED); StickCP2.Display.print("GAME OVER");
    StickCP2.Display.setCursor(60, 70); StickCP2.Display.setTextColor(C_WHITE); StickCP2.Display.printf("Score: %d", score);
    StickCP2.Display.setCursor(60, 90); StickCP2.Display.printf("Best: %d", highScore);
    delay(1000);
    while(!StickCP2.BtnA.wasPressed()) { StickCP2.update(); delay(10); }
}

// ==========================================
//      GAME 4: GALAXY DODGE (IMU)
// ==========================================
void playGalaxy() {
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
        StickCP2.update();
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
    
    int highScore = EEPROM.readInt(ADDR_SCORE_GALAXY);
    if(score > highScore) { EEPROM.writeInt(ADDR_SCORE_GALAXY, score); EEPROM.commit(); highScore = score; }

    StickCP2.Speaker.tone(150, 500);
    StickCP2.Display.fillScreen(C_BLACK);
    StickCP2.Display.setCursor(60, 50); StickCP2.Display.setTextColor(C_RED); StickCP2.Display.print("CRASHED!");
    StickCP2.Display.setCursor(60, 70); StickCP2.Display.setTextColor(C_WHITE); StickCP2.Display.printf("Score: %d", score);
    StickCP2.Display.setCursor(60, 90); StickCP2.Display.printf("Best: %d", highScore);
    delay(1000);
    while(!StickCP2.BtnA.wasPressed()) { StickCP2.update(); delay(10); }
}
// ==========================================
//      GAME 5: BALANCE BALL (IMU Maze)
// ==========================================
void playBalanceBall() {
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
        StickCP2.update();
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
    
    int highScore = EEPROM.readInt(ADDR_SCORE_BALANCE);
    if(level > highScore) { EEPROM.writeInt(ADDR_SCORE_BALANCE, level); EEPROM.commit(); highScore = level; }

    StickCP2.Speaker.tone(150, 500);
    StickCP2.Display.fillScreen(C_BLACK);
    StickCP2.Display.setCursor(60, 50); StickCP2.Display.setTextColor(C_RED); StickCP2.Display.print("FELL IN!");
    StickCP2.Display.setCursor(60, 70); StickCP2.Display.setTextColor(C_WHITE); StickCP2.Display.printf("Level: %d", level);
    StickCP2.Display.setCursor(60, 90); StickCP2.Display.printf("Best: %d", highScore);
    delay(1000);
    while(!StickCP2.BtnA.wasPressed()) { StickCP2.update(); delay(10); }
}

// ==========================================
//      GAME 6: SIMON SAYS (Memory)
// ==========================================
void playSimonSays() {
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
        StickCP2.update();
        
        for(int i = 0; i < seqLen; i++) {
            bool gotInput = false;
            while(!gotInput) {
                StickCP2.update();
                
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
    
    int highScore = EEPROM.readInt(ADDR_SCORE_SIMON);
    if(score > highScore) { EEPROM.writeInt(ADDR_SCORE_SIMON, score); EEPROM.commit(); highScore = score; }

    StickCP2.Speaker.tone(200, 300);
    delay(100);
    StickCP2.Speaker.tone(150, 300);
    StickCP2.Display.fillScreen(C_BLACK);
    StickCP2.Display.setCursor(60, 50); StickCP2.Display.setTextColor(C_RED); StickCP2.Display.print("WRONG!");
    StickCP2.Display.setCursor(60, 70); StickCP2.Display.setTextColor(C_WHITE); StickCP2.Display.printf("Score: %d", score);
    StickCP2.Display.setCursor(60, 90); StickCP2.Display.printf("Best: %d", highScore);
    delay(1000);
    while(!StickCP2.BtnA.wasPressed()) { StickCP2.update(); delay(10); }
}

// ==========================================
//      GAME 7: PANIC MODE (Multi-Sensor)
// ==========================================
void playPanicMode() {
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
        StickCP2.update();
        
        while(millis() - startTime < timeLimit) {
            StickCP2.update();
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
            
            // Progress bar
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
    
    int highScore = EEPROM.readInt(ADDR_SCORE_PANIC);
    if(score > highScore) { EEPROM.writeInt(ADDR_SCORE_PANIC, score); EEPROM.commit(); highScore = score; }

    StickCP2.Speaker.tone(100, 300);
    StickCP2.Display.fillScreen(C_BLACK);
    StickCP2.Display.setTextSize(2);
    StickCP2.Display.setCursor(40, 40); StickCP2.Display.setTextColor(C_RED); StickCP2.Display.print("TOO SLOW!");
    StickCP2.Display.setTextSize(1);
    StickCP2.Display.setCursor(60, 80); StickCP2.Display.setTextColor(C_WHITE); StickCP2.Display.printf("Score: %d", score);
    StickCP2.Display.setCursor(60, 100); StickCP2.Display.printf("Best: %d", highScore);
    delay(1000);
    while(!StickCP2.BtnA.wasPressed()) { StickCP2.update(); delay(10); }
}

// ==========================================
//               MAIN MENU
// ==========================================
void drawMenu() {
    StickCP2.Display.setRotation(1); // Landscape
    StickCP2.Display.fillScreen(C_BLACK);
    
    // Get time from RTC (UTC+5:30 - India Standard Time)
    auto dt = StickCP2.Rtc.getDateTime();
    
    // Get battery percentage
    int batteryLevel = StickCP2.Power.getBatteryLevel();
    
    // Draw time and battery at top
    StickCP2.Display.setTextSize(1);
    StickCP2.Display.setCursor(5, 5);
    StickCP2.Display.setTextColor(C_WHITE);
    StickCP2.Display.printf("%02d:%02d", dt.time.hours, dt.time.minutes);
    
    // WiFi icon (top right area)
    drawWiFiIcon(170, 3);
    
    // Battery indicator on right side (after WiFi icon)
    StickCP2.Display.setCursor(210, 5);
    if(batteryLevel > 50) StickCP2.Display.setTextColor(C_GREEN);
    else if(batteryLevel > 20) StickCP2.Display.setTextColor(C_YELLOW);
    else StickCP2.Display.setTextColor(C_RED);
    StickCP2.Display.printf("%d%%", batteryLevel);
    
    StickCP2.Display.setTextSize(2);
    StickCP2.Display.setCursor(20, 20);
    StickCP2.Display.setTextColor(C_ORANGE);
    StickCP2.Display.print("STICK-C ARCADE");
    
    StickCP2.Display.drawFastHLine(0, 45, 240, C_WHITE);
    
    // Draw Current Selection
    StickCP2.Display.setCursor(30, 65);
    StickCP2.Display.setTextColor(C_GREEN);
    StickCP2.Display.printf("< %s >", gameTitles[selectedGameIndex]);

    int currentHigh = 0;
    switch(selectedGameIndex) {
        case 0: currentHigh = EEPROM.readInt(ADDR_SCORE_FLAPPY); break;
        case 1: currentHigh = EEPROM.readInt(ADDR_SCORE_DINO); break;
        case 2: currentHigh = EEPROM.readInt(ADDR_SCORE_SCREAM); break;
        case 3: currentHigh = EEPROM.readInt(ADDR_SCORE_GALAXY); break;
        case 4: currentHigh = EEPROM.readInt(ADDR_SCORE_BALANCE); break;
        case 5: currentHigh = EEPROM.readInt(ADDR_SCORE_SIMON); break;
        case 6: currentHigh = EEPROM.readInt(ADDR_SCORE_PANIC); break;
    }
    
    StickCP2.Display.setTextSize(1);
    StickCP2.Display.setCursor(80, 95);
    StickCP2.Display.setTextColor(C_WHITE);
    StickCP2.Display.printf("Best: %d", currentHigh);
    
    StickCP2.Display.setCursor(40, 115);
    StickCP2.Display.setTextColor(C_CYAN);
    StickCP2.Display.print("B: Next   A: Play");
}

void init() {
    StickCP2.begin();
    StickCP2.Speaker.begin();
    StickCP2.Speaker.setVolume(160);
    StickCP2.Mic.begin();
    StickCP2.Imu.init();
    EEPROM.begin(64);
    
    initColors();
    
    // Connect to WiFi and sync time from NTP
    connectWiFi();
    if (wifiConnected) {
        syncNTPTime();
    }
    
    drawMenu();
}

void tick() {
    StickCP2.update();

    if (StickCP2.BtnB.wasPressed()) {
        selectedGameIndex++;
        if (selectedGameIndex >= NUM_GAMES) selectedGameIndex = 0;
        StickCP2.Speaker.tone(4000, 50);
        drawMenu();
    }
    
    if (StickCP2.BtnA.wasPressed()) {
        StickCP2.Speaker.tone(2000, 100);
        delay(100);
        StickCP2.Speaker.tone(3000, 100);
        
        switch(selectedGameIndex) {
            case 0: playFlappy(); break;
            case 1: playDino(); break;
            case 2: playScream(); break;
            case 3: playGalaxy(); break;
            case 4: playBalanceBall(); break;
            case 5: playSimonSays(); break;
            case 6: playPanicMode(); break;
        }
        
        drawMenu();
    }

    delay(20);
}

}  // namespace ArcadeApp