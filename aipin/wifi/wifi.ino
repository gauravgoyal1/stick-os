#include <M5StickCPlus2.h>
#include <WiFi.h>
#include <time.h>

#include <wifi_config.h>
#include <secrets_config.h>

WiFiClient client;

// ==========================================
//          DISPLAY CONFIGURATION
// ==========================================
#define SCREEN_W 135
#define SCREEN_H 240

uint16_t C_BLACK, C_WHITE, C_GREEN, C_RED, C_CYAN, C_YELLOW, C_ORANGE, C_GRAY, C_DARKGRAY;

void initColors() {
    C_BLACK    = BLACK;
    C_WHITE    = WHITE;
    C_GREEN    = GREEN;
    C_RED      = RED;
    C_CYAN     = CYAN;
    C_YELLOW   = YELLOW;
    C_ORANGE   = ORANGE;
    C_GRAY     = StickCP2.Display.color565(128, 128, 128);
    C_DARKGRAY = StickCP2.Display.color565(40, 40, 40);
}

// ==========================================
//          AUDIO STREAMING CONFIG
// ==========================================
#define AUDIO_SAMPLE_RATE    8000
#define AUDIO_BIT_DEPTH      8
#define AUDIO_CHANNELS       1
#define AUDIO_CHUNK_SAMPLES  1024
#define AUDIO_CHUNK_BYTES    (AUDIO_CHUNK_SAMPLES * 1)

#define LIST_TOP_Y 28
#define LED_PIN 19

// Disconnect reason constants
#define DISC_WIFI   0
#define DISC_SERVER 1
#define DISC_MANUAL 2

// Recording state
bool isRecording = false;
unsigned long recordStartMillis = 0;
bool isWiFiConnected = false;
bool isServerConnected = false;

// Audio processing settings
float audioGain       = 20.0;
float noiseGateThresh = 200.0;
float hpfAlpha        = 0.90;
float lpfAlpha        = 0.30;
float softClipKnee    = 25000.0;
float softClipRatio   = 0.5;

// Filter state variables
float hpf_prev_input = 0.0;
float hpf_prev_output = 0.0;
float lpf_prev_output = 0.0;

// Waveform visualization
#define WAVEFORM_BARS    45
#define WAVEFORM_BAR_W   2
#define WAVEFORM_BAR_GAP 1
#define WAVEFORM_HEIGHT  100
#define WAVEFORM_Y       130

uint8_t waveformBuffer[WAVEFORM_BARS] = {0};
int waveformIndex = 0;

// Protocol magic bytes
const uint8_t STREAM_START_MAGIC[4] = {0x41, 0x50, 0x53, 0x54}; // "APST"
const uint8_t STREAM_STOP_MAGIC[4]  = {0x41, 0x50, 0x4E, 0x44}; // "APND"

// ==========================================
//        DRAWING HELPERS
// ==========================================
void drawWiFiBars(int x, int y, int rssi) {
    int bars = 0;
    if (rssi >= -50) bars = 4;
    else if (rssi >= -65) bars = 3;
    else if (rssi >= -80) bars = 2;
    else if (rssi >= -90) bars = 1;

    int barW = 2, spacing = 1;
    int heights[] = {3, 5, 7, 9};
    for (int i = 0; i < 4; i++) {
        int bx = x + i * (barW + spacing);
        int by = y + 9 - heights[i];
        uint16_t col = (i < bars) ? C_GREEN : C_DARKGRAY;
        StickCP2.Display.fillRect(bx, by, barW, heights[i], col);
    }
}

// ==========================================
//        ICON DRAWING FUNCTIONS
// ==========================================
void drawMicIcon(int x, int y, uint16_t color) {
    StickCP2.Display.fillRoundRect(x + 3, y, 6, 9, 2, color);
    StickCP2.Display.drawLine(x + 6, y + 9, x + 6, y + 13, color);
    StickCP2.Display.drawLine(x + 3, y + 11, x + 9, y + 11, color);
    StickCP2.Display.drawLine(x + 2, y + 13, x + 10, y + 13, color);
}

void drawWiFiIcon(int x, int y, uint16_t color) {
    // WiFi symbol - concentric arcs
    StickCP2.Display.drawArc(x + 6, y + 10, 9, 7, 220, 320, color);
    StickCP2.Display.drawArc(x + 6, y + 10, 6, 5, 220, 320, color);
    StickCP2.Display.drawArc(x + 6, y + 10, 3, 2, 220, 320, color);
    StickCP2.Display.fillCircle(x + 6, y + 10, 1, color);
}

void drawWaveIcon(int x, int y, uint16_t color) {
    StickCP2.Display.drawLine(x, y + 4, x + 1, y + 2, color);
    StickCP2.Display.drawLine(x + 1, y + 2, x + 2, y, color);
    StickCP2.Display.drawLine(x, y + 4, x + 1, y + 6, color);
    StickCP2.Display.drawLine(x + 1, y + 6, x + 2, y + 8, color);
    StickCP2.Display.drawLine(x + 4, y + 4, x + 5, y + 1, color);
    StickCP2.Display.drawLine(x + 5, y + 1, x + 6, y, color);
    StickCP2.Display.drawLine(x + 4, y + 4, x + 5, y + 7, color);
    StickCP2.Display.drawLine(x + 5, y + 7, x + 6, y + 8, color);
}

void drawFilterIcon(int x, int y, uint16_t color) {
    StickCP2.Display.drawLine(x, y, x + 10, y, color);
    StickCP2.Display.drawLine(x, y, x + 3, y + 4, color);
    StickCP2.Display.drawLine(x + 10, y, x + 7, y + 4, color);
    StickCP2.Display.drawLine(x + 3, y + 4, x + 3, y + 8, color);
    StickCP2.Display.drawLine(x + 7, y + 4, x + 7, y + 8, color);
    StickCP2.Display.fillRect(x + 3, y + 8, 5, 2, color);
}

void drawHeader() {
    StickCP2.Display.fillRect(0, 0, SCREEN_W, 22, StickCP2.Display.color565(20, 20, 60));
    StickCP2.Display.setTextSize(1);

    // Time on the left
    auto dt = StickCP2.Rtc.getDateTime();
    StickCP2.Display.setTextColor(C_WHITE);
    StickCP2.Display.setCursor(6, 6);
    StickCP2.Display.printf("%02d:%02d", dt.time.hours, dt.time.minutes);

    // Battery on the right
    int batt = StickCP2.Power.getBatteryLevel();
    StickCP2.Display.setCursor(100, 6);
    if (batt > 50) StickCP2.Display.setTextColor(C_GREEN);
    else if (batt > 20) StickCP2.Display.setTextColor(C_YELLOW);
    else StickCP2.Display.setTextColor(C_RED);
    StickCP2.Display.printf("%d%%", batt);
}

void drawFooter(const char* btnALabel, const char* btnBLabel) {
    StickCP2.Display.fillRect(0, SCREEN_H - 14, SCREEN_W, 14, StickCP2.Display.color565(20, 20, 60));
    StickCP2.Display.setTextSize(1);
    StickCP2.Display.setTextColor(C_WHITE);
    StickCP2.Display.setCursor(6, SCREEN_H - 11);
    StickCP2.Display.printf("A:%s", btnALabel);
    StickCP2.Display.setCursor(75, SCREEN_H - 11);
    StickCP2.Display.printf("B:%s", btnBLabel);
}

// ==========================================
//        WIFI CONNECTION
// ==========================================
bool connectToWiFi() {
    Serial.println("[AiPin] Scanning for known WiFi networks...");
    
    int n = WiFi.scanNetworks();
    if (n == 0) {
        Serial.println("[AiPin] No networks found");
        return false;
    }
    
    Serial.printf("[AiPin] Found %d networks\n", n);
    
    // Try to connect to a known network
    for (int i = 0; i < n; i++) {
        String foundSSID = WiFi.SSID(i);
        Serial.printf("[AiPin] Checking: %s\n", foundSSID.c_str());
        
        for (size_t j = 0; j < kWiFiNetworkCount; j++) {
            if (foundSSID == kWiFiNetworks[j].ssid) {
                Serial.printf("[AiPin] Attempting to connect to: %s\n", kWiFiNetworks[j].ssid);
                
                WiFi.begin(kWiFiNetworks[j].ssid, kWiFiNetworks[j].password);
                
                int attempts = 0;
                while (WiFi.status() != WL_CONNECTED && attempts < 20) {
                    delay(500);
                    Serial.print(".");
                    attempts++;
                }
                
                if (WiFi.status() == WL_CONNECTED) {
                    Serial.printf("\n[AiPin] Connected to: %s\n", kWiFiNetworks[j].ssid);
                    Serial.printf("[AiPin] IP: %s\n", WiFi.localIP().toString().c_str());
                    return true;
                }
                
                Serial.println("\n[AiPin] Connection failed, trying next...");
            }
        }
    }
    
    return false;
}

bool connectToServer() {
    Serial.printf("[AiPin] Connecting to server %s:%d\n", kAiPinServerHost, kAiPinServerPort);
    
    if (client.connect(kAiPinServerHost, kAiPinServerPort)) {
        Serial.println("[AiPin] Connected to server!");
        return true;
    }
    
    Serial.println("[AiPin] Server connection failed");
    return false;
}

// ==========================================
//        NTP TIME SYNC
// ==========================================
void syncNTPTime() {
    Serial.println("[AiPin] Syncing NTP time...");
    
    // Reset any previous time settings
    struct timeval tv = { 0, 0 };
    settimeofday(&tv, NULL);
    
    configTime(19800, 0, "time.google.com", "pool.ntp.org");
    delay(2000);

    struct tm timeinfo = { 0 };
    int attempts = 0;
    bool success = false;

    while (attempts < 10 && !success) {
        if (getLocalTime(&timeinfo, 2000)) {
            if (timeinfo.tm_year + 1900 >= 2024) {
                success = true;
            }
        }
        attempts++;
        delay(500);
    }

    if (success) {
        m5::rtc_datetime_t dt;
        dt.date.year = timeinfo.tm_year + 1900;
        dt.date.month = timeinfo.tm_mon + 1;
        dt.date.date = timeinfo.tm_mday;
        dt.time.hours = timeinfo.tm_hour;
        dt.time.minutes = timeinfo.tm_min;
        dt.time.seconds = timeinfo.tm_sec;
        StickCP2.Rtc.setDateTime(dt);
        Serial.printf("[AiPin] Time synced: %02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min);
    } else {
        Serial.println("[AiPin] NTP sync failed");
    }
}

// ==========================================
//        DISCONNECTED SCREEN
// ==========================================
void drawLargeWiFiIcon(int cx, int cy, uint16_t color) {
    StickCP2.Display.drawArc(cx, cy, 24, 21, 220, 320, color);
    StickCP2.Display.drawArc(cx, cy, 17, 14, 220, 320, color);
    StickCP2.Display.drawArc(cx, cy, 10, 8, 220, 320, color);
    StickCP2.Display.fillCircle(cx, cy, 3, color);
}

void drawLargeServerIcon(int cx, int cy, uint16_t color) {
    StickCP2.Display.drawRoundRect(cx - 12, cy - 14, 24, 12, 2, color);
    StickCP2.Display.drawRoundRect(cx - 12, cy + 2, 24, 12, 2, color);
    StickCP2.Display.fillCircle(cx + 8, cy - 8, 2, color);
    StickCP2.Display.fillCircle(cx + 8, cy + 8, 2, color);
    StickCP2.Display.drawLine(cx - 6, cy - 8, cx + 2, cy - 8, color);
    StickCP2.Display.drawLine(cx - 6, cy + 8, cx + 2, cy + 8, color);
}

void drawSlash(int cx, int cy, int size, uint16_t color) {
    for (int t = -1; t <= 1; t++) {
        StickCP2.Display.drawLine(cx - size + t, cy + size, cx + size + t, cy - size, color);
    }
}

void drawAvailableNetworks() {
    int n = WiFi.scanNetworks();
    
    int startY = 100;
    int maxNetworks = 5;  // Limit to 5 networks to fit on screen
    
    StickCP2.Display.setTextSize(1);
    StickCP2.Display.setTextColor(C_GRAY);
    StickCP2.Display.setCursor(6, startY - 15);
    StickCP2.Display.print("Available Networks:");
    
    if (n == 0) {
        StickCP2.Display.setTextColor(C_DARKGRAY);
        StickCP2.Display.setCursor(6, startY);
        StickCP2.Display.print("No networks found");
    } else {
        int displayCount = min(n, maxNetworks);
        for (int i = 0; i < displayCount; i++) {
            int y = startY + (i * 18);
            int rssi = WiFi.RSSI(i);
            
            // Check if this is a known network
            bool isKnown = false;
            String foundSSID = WiFi.SSID(i);
            for (size_t j = 0; j < kWiFiNetworkCount; j++) {
                if (foundSSID == kWiFiNetworks[j].ssid) {
                    isKnown = true;
                    break;
                }
            }
            
            // Draw signal strength bars
            drawWiFiBars(6, y, rssi);
            
            // Draw SSID name
            String ssid = foundSSID;
            if (ssid.length() > 14) ssid = ssid.substring(0, 12) + "..";
            
            StickCP2.Display.setTextColor(isKnown ? C_CYAN : C_WHITE);
            StickCP2.Display.setCursor(22, y + 2);
            StickCP2.Display.print(ssid.c_str());
            
            // Show star for known networks
            if (isKnown) {
                StickCP2.Display.setCursor(SCREEN_W - 12, y + 2);
                StickCP2.Display.print("*");
            }
        }
        
        // Show count if there are more networks
        if (n > maxNetworks) {
            StickCP2.Display.setTextColor(C_DARKGRAY);
            StickCP2.Display.setCursor(6, startY + (maxNetworks * 18));
            StickCP2.Display.printf("+%d more", n - maxNetworks);
        }
    }
    
    WiFi.scanDelete();  // Free scan results memory
}

void drawDisconnectedScreen(int reason) {
    StickCP2.Display.fillScreen(C_BLACK);
    drawHeader();

    int cx = SCREEN_W / 2;

    if (reason == DISC_WIFI) {
        // Compact header for WiFi disconnected
        StickCP2.Display.setTextSize(1);
        StickCP2.Display.setTextColor(C_RED);
        StickCP2.Display.setCursor(6, 30);
        StickCP2.Display.print("WiFi Disconnected");
        
        // Draw small WiFi icon with slash
        drawWiFiIcon(SCREEN_W - 20, 26, C_GRAY);
        StickCP2.Display.drawLine(SCREEN_W - 20, 38, SCREEN_W - 8, 26, C_RED);
        
        // Scanning indicator
        StickCP2.Display.setTextColor(C_DARKGRAY);
        StickCP2.Display.setCursor(6, 50);
        StickCP2.Display.print("Scanning...");
        
        // Draw available networks
        StickCP2.Display.setCursor(6, 70);
        drawAvailableNetworks();
        
    } else if (reason == DISC_SERVER) {
        drawWiFiIcon(cx - 6, 60, C_GREEN);

        drawLargeServerIcon(cx, 100, C_RED);
        drawSlash(cx, 100, 14, C_RED);

        StickCP2.Display.setTextSize(1);
        StickCP2.Display.setTextColor(C_RED);
        StickCP2.Display.setCursor(cx - 27, 130);
        StickCP2.Display.print("No Server");

        StickCP2.Display.setTextColor(C_DARKGRAY);
        StickCP2.Display.setCursor(cx - 42, 155);
        StickCP2.Display.print("Reconnecting...");
    } else {
        StickCP2.Display.setTextSize(1);
        StickCP2.Display.setTextColor(C_GRAY);
        StickCP2.Display.setCursor(cx - 36, 100);
        StickCP2.Display.print("Disconnected");
    }

    drawFooter("---", "---");
}

// ==========================================
//        CONNECTED SCREEN
// ==========================================
void drawConnectedScreen() {
    StickCP2.Display.fillScreen(C_BLACK);
    drawHeader();

    int cx = SCREEN_W / 2;

    // Green circle with checkmark
    StickCP2.Display.fillCircle(cx, 72, 18, C_GREEN);
    StickCP2.Display.drawLine(cx - 8, 72, cx - 2, 80, C_BLACK);
    StickCP2.Display.drawLine(cx - 7, 72, cx - 1, 80, C_BLACK);
    StickCP2.Display.drawLine(cx - 2, 80, cx + 10, 64, C_BLACK);
    StickCP2.Display.drawLine(cx - 1, 80, cx + 11, 64, C_BLACK);

    // "Ready"
    StickCP2.Display.setTextSize(2);
    StickCP2.Display.setTextColor(C_WHITE);
    StickCP2.Display.setCursor(cx - 30, 100);
    StickCP2.Display.print("Ready");

    // "Press to Record"
    StickCP2.Display.setTextSize(1);
    StickCP2.Display.setTextColor(C_GRAY);
    StickCP2.Display.setCursor(cx - 42, 122);
    StickCP2.Display.print("Press to Record");

    // WiFi SSID
    String ssid = WiFi.SSID();
    if (ssid.length() > 20) ssid = ssid.substring(0, 18) + "..";
    int nameW = ssid.length() * 6;
    StickCP2.Display.setTextColor(C_DARKGRAY);
    StickCP2.Display.setCursor(cx - nameW / 2, 140);
    StickCP2.Display.print(ssid.c_str());

    // Signal bars
    drawWiFiBars(cx - 6, 156, WiFi.RSSI());

    drawFooter("Rec", "Disc");
}

// ==========================================
//        AUDIO PROCESSING FUNCTIONS
// ==========================================
void processAudioBuffer(int16_t* buffer, size_t samples) {
    for (size_t i = 0; i < samples; i++) {
        float sample = (float)buffer[i];

        // Stage 1: Software amplification
        sample *= audioGain;

        // Stage 2: High-pass filter
        float hpf_output = hpfAlpha * (hpf_prev_output + sample - hpf_prev_input);
        hpf_prev_input = sample;
        hpf_prev_output = hpf_output;

        // Stage 3: Low-pass filter
        float lpf_output = lpfAlpha * hpf_output + (1.0 - lpfAlpha) * lpf_prev_output;
        lpf_prev_output = lpf_output;

        // Stage 4: Noise gate
        if (fabs(lpf_output) < noiseGateThresh) {
            lpf_output = 0;
        }

        // Stage 5: Soft clipping
        float clipped = lpf_output;
        if (clipped > softClipKnee)
            clipped = softClipKnee + (clipped - softClipKnee) * softClipRatio;
        else if (clipped < -softClipKnee)
            clipped = -softClipKnee + (clipped + softClipKnee) * softClipRatio;

        // Stage 6: Hard clipping
        int32_t final_sample = constrain((int32_t)clipped, -32768, 32767);

        buffer[i] = (int16_t)final_sample;
    }
}

// ==========================================
//        AUDIO STREAMING FUNCTIONS
// ==========================================
void sendStreamHeader() {
    uint32_t sampleRate = AUDIO_SAMPLE_RATE;
    uint16_t bitDepth   = AUDIO_BIT_DEPTH;
    uint16_t channels   = AUDIO_CHANNELS;

    Serial.printf("[AiPin] Sending APST header (connected=%d)\n", client.connected());
    
    client.write(STREAM_START_MAGIC, 4);
    client.write((uint8_t*)&sampleRate, 4);
    client.write((uint8_t*)&bitDepth, 2);
    client.write((uint8_t*)&channels, 2);
    client.flush();
    
    Serial.println("[AiPin] Header sent");
}

void sendStreamStop() {
    client.write(STREAM_STOP_MAGIC, 4);
    client.flush();
}

// ==========================================
//        RECORDING SCREEN
// ==========================================
void drawWaveform() {
    int totalW = WAVEFORM_BARS * (WAVEFORM_BAR_W + WAVEFORM_BAR_GAP) - WAVEFORM_BAR_GAP;
    int startX = (SCREEN_W - totalW) / 2;
    int centerY = WAVEFORM_Y;
    int halfH = WAVEFORM_HEIGHT / 2;

    // Clear waveform area
    StickCP2.Display.fillRect(0, centerY - halfH, SCREEN_W, WAVEFORM_HEIGHT, C_BLACK);

    // Center line
    StickCP2.Display.drawLine(startX, centerY, startX + totalW, centerY, C_DARKGRAY);

    for (int i = 0; i < WAVEFORM_BARS; i++) {
        int idx = (waveformIndex + i) % WAVEFORM_BARS;
        int barH = map(waveformBuffer[idx], 0, 255, 1, halfH);
        int x = startX + i * (WAVEFORM_BAR_W + WAVEFORM_BAR_GAP);

        uint16_t color;
        if (waveformBuffer[idx] > 200) color = C_RED;
        else if (waveformBuffer[idx] > 120) color = C_YELLOW;
        else color = C_GREEN;

        // Symmetric bars above and below center
        StickCP2.Display.fillRect(x, centerY - barH, WAVEFORM_BAR_W, barH, color);
        StickCP2.Display.fillRect(x, centerY + 1, WAVEFORM_BAR_W, barH, color);
    }
}

void drawRecordingScreen() {
    StickCP2.Display.fillScreen(C_BLACK);
    drawHeader();

    int cx = SCREEN_W / 2;

    // Red recording dot in header (after time)
    StickCP2.Display.fillCircle(42, 7, 3, C_RED);

    // Duration timer
    StickCP2.Display.setTextSize(2);
    StickCP2.Display.setTextColor(C_WHITE);
    StickCP2.Display.setCursor(cx - 30, 32);
    StickCP2.Display.print("00:00");

    // Initial flat waveform
    drawWaveform();

    drawFooter("Stop", "Disc");
}

void updateRecordingDisplay() {
    unsigned long elapsed = (millis() - recordStartMillis) / 1000;
    unsigned int mins = elapsed / 60;
    unsigned int secs = elapsed % 60;

    int cx = SCREEN_W / 2;

    // Update timer
    StickCP2.Display.fillRect(cx - 30, 32, 60, 16, C_BLACK);
    StickCP2.Display.setTextSize(2);
    StickCP2.Display.setTextColor(C_WHITE);
    StickCP2.Display.setCursor(cx - 30, 32);
    StickCP2.Display.printf("%02d:%02d", mins, secs);

    // Blink recording dot in header
    bool dotVisible = ((millis() / 500) % 2 == 0);
    uint16_t headerBg = StickCP2.Display.color565(20, 20, 60);
    StickCP2.Display.fillCircle(42, 7, 3, dotVisible ? C_RED : headerBg);

    // Update waveform
    drawWaveform();
}

// ==========================================
//        RECORDING CONTROL
// ==========================================
void startRecording() {
    // Stop speaker (shares GPIO 0 with mic)
    StickCP2.Speaker.end();

    // Start microphone
    StickCP2.Mic.begin();
    delay(50);

    // Reset audio processing filter state
    hpf_prev_input = 0.0;
    hpf_prev_output = 0.0;
    lpf_prev_output = 0.0;

    // Reset waveform
    memset(waveformBuffer, 0, sizeof(waveformBuffer));
    waveformIndex = 0;

    // Send stream start header over TCP
    sendStreamHeader();

    isRecording = true;
    recordStartMillis = millis();
    digitalWrite(LED_PIN, HIGH);

    drawRecordingScreen();
}

void stopRecording() {
    isRecording = false;
    digitalWrite(LED_PIN, LOW);

    // Wait for any in-progress capture to finish
    while (StickCP2.Mic.isRecording()) {
        delay(1);
    }

    // Stop mic, restore speaker
    StickCP2.Mic.end();
    StickCP2.Speaker.begin();
    StickCP2.Speaker.setVolume(120);

    // Send stream stop marker
    sendStreamStop();

    // Confirmation tone
    StickCP2.Speaker.tone(2000, 60);
    delay(60);
    StickCP2.Speaker.tone(1500, 60);

    drawConnectedScreen();
}

static unsigned long lastChunkLog = 0;
static int chunkCount = 0;

void captureAndStreamChunk() {
    if (!isRecording || !StickCP2.Mic.isEnabled()) return;

    // 1. Record 16-bit audio
    int16_t tempBuffer[AUDIO_CHUNK_SAMPLES];

    if (StickCP2.Mic.record(tempBuffer, AUDIO_CHUNK_SAMPLES, AUDIO_SAMPLE_RATE)) {

        // 2. Process Audio
        processAudioBuffer(tempBuffer, AUDIO_CHUNK_SAMPLES);

        // 3. Compress to 8-bit
        int8_t txBuffer[AUDIO_CHUNK_SAMPLES];
        for (int i = 0; i < AUDIO_CHUNK_SAMPLES; i++) {
            txBuffer[i] = (int8_t)(tempBuffer[i] >> 8);
        }

        // 4. Compute waveform peak
        int16_t peak = 0;
        for (int i = 0; i < AUDIO_CHUNK_SAMPLES; i++) {
            int16_t absVal = abs(tempBuffer[i]);
            if (absVal > peak) peak = absVal;
        }
        waveformBuffer[waveformIndex] = map(constrain(peak, 0, 32767), 0, 32767, 0, 255);
        waveformIndex = (waveformIndex + 1) % WAVEFORM_BARS;

        // 5. Send over TCP
        size_t written = client.write((uint8_t*)txBuffer, AUDIO_CHUNK_SAMPLES);

        // Debug logging
        chunkCount++;
        if (millis() - lastChunkLog > 2000) {
            Serial.printf("[AiPin] chunks=%d last_write=%d/%d\n",
                          chunkCount, written, AUDIO_CHUNK_SAMPLES);
            lastChunkLog = millis();
        }
    }
}

// ==========================================
//        DISCONNECT
// ==========================================
void disconnectDevice() {
    // If recording, stop mic and send stop marker before disconnecting
    if (isRecording) {
        isRecording = false;
        digitalWrite(LED_PIN, LOW);
        while (StickCP2.Mic.isRecording()) { delay(1); }
        StickCP2.Mic.end();
        StickCP2.Speaker.begin();
        StickCP2.Speaker.setVolume(120);
        sendStreamStop();
        delay(50);
    }

    client.stop();
    isServerConnected = false;

    StickCP2.Speaker.tone(800, 100);
    delay(50);
    StickCP2.Speaker.tone(400, 100);

    drawDisconnectedScreen(DISC_MANUAL);
}

// ==========================================
//              SETUP
// ==========================================
void setup() {
    Serial.begin(115200);
    Serial.println("\n[AiPin] Booting (WiFi mode)...");

    StickCP2.begin();
    StickCP2.Speaker.begin();
    StickCP2.Speaker.setVolume(120);
    StickCP2.Display.setRotation(0);
    StickCP2.Display.setTextSize(1);

    initColors();

    // LED indicator
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Splash screen
    StickCP2.Display.fillScreen(C_BLACK);
    StickCP2.Display.setTextSize(2);
    StickCP2.Display.setTextColor(C_CYAN);
    StickCP2.Display.setCursor(37, 70);
    StickCP2.Display.print("AiPin");
    StickCP2.Display.setTextSize(1);
    StickCP2.Display.setTextColor(C_GRAY);
    StickCP2.Display.setCursor(12, 110);
    StickCP2.Display.print("WiFi Audio Streamer");
    StickCP2.Display.setCursor(13, 140);
    StickCP2.Display.setTextColor(C_WHITE);
    StickCP2.Display.print("Connecting WiFi...");
    delay(1000);

    // Connect to WiFi
    WiFi.mode(WIFI_STA);
    isWiFiConnected = connectToWiFi();

    if (isWiFiConnected) {
        StickCP2.Display.setCursor(13, 155);
        StickCP2.Display.print("Syncing time...");
        syncNTPTime();

        StickCP2.Display.setCursor(13, 170);
        StickCP2.Display.print("Connecting server...");
        isServerConnected = connectToServer();
        
        if (isServerConnected) {
            StickCP2.Speaker.tone(1500, 80);
            delay(80);
            StickCP2.Speaker.tone(2000, 80);
            drawConnectedScreen();
        } else {
            drawDisconnectedScreen(DISC_SERVER);
        }
    } else {
        drawDisconnectedScreen(DISC_WIFI);
    }
}

// ==========================================
//              MAIN LOOP
// ==========================================
void loop() {
    StickCP2.update();

    // Check WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
        if (isWiFiConnected) {
            isWiFiConnected = false;
            isServerConnected = false;
            if (isRecording) {
                stopRecording();
            }
            Serial.println("[AiPin] WiFi connection lost");
            drawDisconnectedScreen(DISC_WIFI);
        }
        
        // Try to reconnect periodically
        static unsigned long lastWiFiRetry = 0;
        if (millis() - lastWiFiRetry > 10000) {
            isWiFiConnected = connectToWiFi();
            if (isWiFiConnected) {
                isServerConnected = connectToServer();
                if (isServerConnected) {
                    drawConnectedScreen();
                }
            }
            lastWiFiRetry = millis();
        }
        return;
    }
    
    isWiFiConnected = true;

    // Check server connection
    if (!client.connected()) {
        if (isServerConnected) {
            isServerConnected = false;
            if (isRecording) {
                stopRecording();
            }
            Serial.println("[AiPin] Server connection lost");
            drawDisconnectedScreen(DISC_SERVER);
        }
        
        // Try to reconnect periodically
        static unsigned long lastServerRetry = 0;
        if (millis() - lastServerRetry > 5000) {
            isServerConnected = connectToServer();
            if (isServerConnected) {
                drawConnectedScreen();
            }
            lastServerRetry = millis();
        }
        return;
    }
    
    isServerConnected = true;

    // Handle connected state
    if (isRecording) {
        captureAndStreamChunk();

        static unsigned long lastDisplayUpdate = 0;
        if (millis() - lastDisplayUpdate > 150) {
            updateRecordingDisplay();
            lastDisplayUpdate = millis();
        }

        if (StickCP2.BtnA.wasPressed()) {
            stopRecording();
        }

        if (StickCP2.BtnB.wasPressed()) {
            stopRecording();
            delay(100);
            disconnectDevice();
        }

    } else {
        if (StickCP2.BtnA.wasPressed()) {
            StickCP2.Speaker.tone(1500, 50);
            delay(50);
            startRecording();
        }

        if (StickCP2.BtnB.wasPressed()) {
            disconnectDevice();
        }

        // Update header time every minute when on connected screen
        static unsigned long lastTimeUpdate = 0;
        if (millis() - lastTimeUpdate >= 60000) {
            drawHeader();
            lastTimeUpdate = millis();
        }
    }

    // Periodic heartbeat for debug
    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat > 5000) {
        Serial.printf("[AiPin] heartbeat: wifi=%d server=%d rec=%d rssi=%d\n",
                      isWiFiConnected, isServerConnected, isRecording, WiFi.RSSI());
        lastHeartbeat = millis();
    }

    // Skip delay during recording
    if (!isRecording) {
        delay(20);
    }
}
