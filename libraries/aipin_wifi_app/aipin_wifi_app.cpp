#include <M5StickCPlus2.h>
#include <ArduinoWebsockets.h>
using namespace websockets;

#include <stick_config.h>
#include <stick_net.h>
#include <stick_store.h>
#include <status_strip.h>
#include "aipin_wifi_app.h"

namespace AiPinWifiApp {

WebsocketsClient wsClient;

// ==========================================
//          DISPLAY CONFIGURATION
// ==========================================
#define SCREEN_W 135
#define SCREEN_H 240
#define CONTENT_Y 18  // below OS status strip

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
#define WAVEFORM_Y       140

uint8_t waveformBuffer[WAVEFORM_BARS] = {0};
int waveformIndex = 0;

// Protocol magic bytes
const uint8_t STREAM_START_MAGIC[4] = {0x41, 0x50, 0x53, 0x54}; // "APST"
const uint8_t STREAM_STOP_MAGIC[4]  = {0x41, 0x50, 0x4E, 0x44}; // "APND"

// ==========================================
//        DRAWING HELPERS
// ==========================================
void clearContent() {
    StickCP2.Display.fillRect(0, CONTENT_Y, SCREEN_W, SCREEN_H - CONTENT_Y, C_BLACK);
}

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
void drawWiFiIcon(int x, int y, uint16_t color) {
    StickCP2.Display.drawArc(x + 6, y + 10, 9, 7, 220, 320, color);
    StickCP2.Display.drawArc(x + 6, y + 10, 6, 5, 220, 320, color);
    StickCP2.Display.drawArc(x + 6, y + 10, 3, 2, 220, 320, color);
    StickCP2.Display.fillCircle(x + 6, y + 10, 1, color);
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

// ==========================================
//        SERVER CONNECTION
// ==========================================
bool connectToServer() {
    const char* proto = (kStickServerPort == 443) ? "wss://" : "ws://";
    String url = String(proto) + kStickServerHost + ":" + String(kStickServerPort) + "/services/aipin";
    char apiKey[65];
    if (stick_os::getApiKey(apiKey, sizeof(apiKey))) {
        url += "?key=" + String(apiKey);
    }
    Serial.printf("[Scribe] Connecting to %s\n", url.c_str());
    bool ok = wsClient.connect(url.c_str());
    if (ok) Serial.println("[Scribe] WebSocket connected");
    else    Serial.println("[Scribe] WebSocket failed");
    return ok;
}

// ==========================================
//        DISCONNECTED SCREEN
// ==========================================
void drawAvailableNetworks() {
    constexpr size_t kMaxScan = 8;
    StickNet::ScanResult results[kMaxScan];
    size_t n = StickNet::scanNetworks(results, kMaxScan);

    int startY = 96;
    int maxNetworks = 5;

    StickCP2.Display.setTextSize(1);
    StickCP2.Display.setTextColor(C_GRAY);
    StickCP2.Display.setCursor(6, startY - 15);
    StickCP2.Display.print("Available Networks:");

    if (n == 0) {
        StickCP2.Display.setTextColor(C_DARKGRAY);
        StickCP2.Display.setCursor(6, startY);
        StickCP2.Display.print("No networks found");
    } else {
        int displayCount = (n < (size_t)maxNetworks) ? n : maxNetworks;
        for (int i = 0; i < displayCount; i++) {
            int y = startY + (i * 18);
            const StickNet::ScanResult& r = results[i];

            drawWiFiBars(6, y, r.rssi);

            String ssid = r.ssid;
            if (ssid.length() > 14) ssid = ssid.substring(0, 12) + "..";

            StickCP2.Display.setTextColor(r.known ? C_CYAN : C_WHITE);
            StickCP2.Display.setCursor(22, y + 2);
            StickCP2.Display.print(ssid.c_str());

            if (r.known) {
                StickCP2.Display.setCursor(SCREEN_W - 12, y + 2);
                StickCP2.Display.print("*");
            }
        }

        if (n > (size_t)maxNetworks) {
            StickCP2.Display.setTextColor(C_DARKGRAY);
            StickCP2.Display.setCursor(6, startY + (maxNetworks * 18));
            StickCP2.Display.printf("+%d more", (int)(n - maxNetworks));
        }
    }
}

void drawDisconnectedScreen(int reason) {
    clearContent();
    int cx = SCREEN_W / 2;

    if (reason == DISC_WIFI) {
        StickCP2.Display.setTextSize(1);
        StickCP2.Display.setTextColor(C_RED);
        StickCP2.Display.setCursor(6, 24);
        StickCP2.Display.print("WiFi Disconnected");

        drawWiFiIcon(SCREEN_W - 20, 20, C_GRAY);
        StickCP2.Display.drawLine(SCREEN_W - 20, 32, SCREEN_W - 8, 20, C_RED);

        StickCP2.Display.setTextColor(C_DARKGRAY);
        StickCP2.Display.setCursor(6, 44);
        StickCP2.Display.print("Scanning...");

        drawAvailableNetworks();

    } else if (reason == DISC_SERVER) {
        drawWiFiIcon(cx - 6, 56, C_GREEN);

        drawLargeServerIcon(cx, 96, C_RED);
        drawSlash(cx, 96, 14, C_RED);

        StickCP2.Display.setTextSize(1);
        StickCP2.Display.setTextColor(C_RED);
        StickCP2.Display.setCursor(cx - 27, 126);
        StickCP2.Display.print("No Server");

        StickCP2.Display.setTextColor(C_DARKGRAY);
        StickCP2.Display.setCursor(cx - 42, 151);
        StickCP2.Display.print("Reconnecting...");
    } else {
        StickCP2.Display.setTextSize(1);
        StickCP2.Display.setTextColor(C_GRAY);
        StickCP2.Display.setCursor(cx - 36, 96);
        StickCP2.Display.print("Disconnected");
    }
}

// ==========================================
//        CONNECTED SCREEN
// ==========================================
void drawConnectedScreen() {
    clearContent();
    int cx = SCREEN_W / 2;

    // Green circle with checkmark
    StickCP2.Display.fillCircle(cx, 68, 18, C_GREEN);
    StickCP2.Display.drawLine(cx - 8, 68, cx - 2, 76, C_BLACK);
    StickCP2.Display.drawLine(cx - 7, 68, cx - 1, 76, C_BLACK);
    StickCP2.Display.drawLine(cx - 2, 76, cx + 10, 60, C_BLACK);
    StickCP2.Display.drawLine(cx - 1, 76, cx + 11, 60, C_BLACK);

    // "Ready"
    StickCP2.Display.setTextSize(2);
    StickCP2.Display.setTextColor(C_WHITE);
    StickCP2.Display.setCursor(cx - 30, 96);
    StickCP2.Display.print("Ready");

    // "Press to Record"
    StickCP2.Display.setTextSize(1);
    StickCP2.Display.setTextColor(C_GRAY);
    StickCP2.Display.setCursor(cx - 42, 118);
    StickCP2.Display.print("Press to Record");

    // WiFi SSID
    String ssid = StickNet::ssid();
    if (ssid.length() > 20) ssid = ssid.substring(0, 18) + "..";
    int nameW = ssid.length() * 6;
    StickCP2.Display.setTextColor(C_DARKGRAY);
    StickCP2.Display.setCursor(cx - nameW / 2, 140);
    StickCP2.Display.print(ssid.c_str());

    // Signal bars
    drawWiFiBars(cx - 6, 156, StickNet::rssi());
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

    Serial.printf("[Scribe] Sending APST header (connected=%d)\n", wsClient.available());

    // Pack into a single 12-byte message (server expects one receive)
    uint8_t header[12];
    memcpy(header,     STREAM_START_MAGIC, 4);
    memcpy(header + 4, &sampleRate, 4);
    memcpy(header + 8, &bitDepth, 2);
    memcpy(header + 10, &channels, 2);
    wsClient.sendBinary((const char*)header, 12);

    Serial.println("[Scribe] Header sent");
}

void sendStreamStop() {
    wsClient.sendBinary((const char*)STREAM_STOP_MAGIC, 4);
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
    clearContent();
    int cx = SCREEN_W / 2;

    // Duration timer
    StickCP2.Display.setTextSize(2);
    StickCP2.Display.setTextColor(C_WHITE);
    StickCP2.Display.setCursor(cx - 30, 26);
    StickCP2.Display.print("00:00");

    // Initial flat waveform
    drawWaveform();
}

void updateRecordingDisplay() {
    unsigned long elapsed = (millis() - recordStartMillis) / 1000;
    unsigned int mins = elapsed / 60;
    unsigned int secs = elapsed % 60;

    int cx = SCREEN_W / 2;

    // Update timer
    StickCP2.Display.fillRect(cx - 30, 26, 60, 16, C_BLACK);
    StickCP2.Display.setTextSize(2);
    StickCP2.Display.setTextColor(C_WHITE);
    StickCP2.Display.setCursor(cx - 30, 26);
    StickCP2.Display.printf("%02d:%02d", mins, secs);

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
        bool written = wsClient.sendBinary((const char*)txBuffer, AUDIO_CHUNK_SAMPLES);

        // Debug logging
        chunkCount++;
        if (millis() - lastChunkLog > 2000) {
            Serial.printf("[Scribe] chunks=%d last_write=%s\n",
                          chunkCount, written ? "ok" : "fail");
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

    wsClient.close();
    isServerConnected = false;

    StickCP2.Speaker.tone(800, 100);
    delay(50);
    StickCP2.Speaker.tone(400, 100);

    drawDisconnectedScreen(DISC_MANUAL);
}

// ==========================================
//              SETUP
// ==========================================
void init() {
    Serial.println("\n[Scribe] Booting...");

    StickCP2.Speaker.begin();
    StickCP2.Speaker.setVolume(120);
    StickCP2.Display.setRotation(0);
    StickCP2.Display.setTextSize(1);

    initColors();

    // LED indicator
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    StickCP2.Display.fillScreen(C_BLACK);
    stick_os::statusStripDrawFull();

    StickNet::waitForReady();
    isWiFiConnected = StickNet::isWiFiReady();
    if (!isWiFiConnected) isWiFiConnected = StickNet::connectWiFi();

    if (isWiFiConnected) {
        StickNet::syncNTP();
        drawDisconnectedScreen(DISC_SERVER);
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
void tick() {
    // Check WiFi connection
    if (!StickNet::isConnected()) {
        if (isWiFiConnected) {
            isWiFiConnected = false;
            isServerConnected = false;
            if (isRecording) {
                stopRecording();
            }
            Serial.println("[Scribe] WiFi connection lost");
            drawDisconnectedScreen(DISC_WIFI);
        }

        // Try to reconnect periodically
        static unsigned long lastWiFiRetry = 0;
        if (millis() - lastWiFiRetry > 10000) {
            isWiFiConnected = StickNet::connectWiFi();
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
    if (!wsClient.available()) {
        if (isServerConnected) {
            isServerConnected = false;
            if (isRecording) {
                stopRecording();
            }
            Serial.println("[Scribe] Server connection lost");
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
    }

    // Periodic heartbeat for debug
    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat > 5000) {
        Serial.printf("[Scribe] heartbeat: wifi=%d server=%d rec=%d rssi=%d\n",
                      isWiFiConnected, isServerConnected, isRecording, StickNet::rssi());
        lastHeartbeat = millis();
    }

    // Skip delay during recording
    if (!isRecording) {
        delay(20);
    }
}

void icon(int x, int y, uint16_t color) {
    auto& d = StickCP2.Display;
    d.fillRoundRect(x + 12, y, 12, 18, 6, color);
    d.drawLine(x + 8,  y + 16, x + 8,  y + 22, color);
    d.drawArc(x + 18, y + 16, 12, 11, 180, 360, color);
    d.drawLine(x + 28, y + 16, x + 28, y + 22, color);
    d.drawLine(x + 18, y + 22, x + 18, y + 28, color);
    d.fillRect(x + 10, y + 28, 17, 2, color);
}

}  // namespace AiPinWifiApp

#include <stick_os.h>

static const stick_os::AppDescriptor kAipinDescriptor = {
    /*id=*/       "scribe",
    /*name=*/     "Scribe",
    /*version=*/  "1.0.0",
    /*category=*/ stick_os::CAT_UTILITY,
    /*flags=*/    stick_os::APP_NEEDS_NET | stick_os::APP_NEEDS_MIC,
    /*icon=*/     &AiPinWifiApp::icon,
    /*runtime=*/  stick_os::RUNTIME_NATIVE,
    /*native=*/   { &AiPinWifiApp::init, &AiPinWifiApp::tick, nullptr, nullptr },
    /*script=*/   { nullptr, nullptr },
};
STICK_REGISTER_APP(kAipinDescriptor);
