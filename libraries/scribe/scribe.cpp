#include <M5StickCPlus2.h>
#include <ArduinoWebsockets.h>
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
using namespace websockets;

#include <stick_config.h>
#include <stick_net.h>
#include <stick_store.h>
#include <stick_os.h>
#include <status_strip.h>
#include "scribe.h"

namespace Scribe {

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

// ==========================================
//          TRANSCRIPT HISTORY
// ==========================================
enum Screen {
    S_MAIN,          // connected / disconnected — recording screen when isRecording
    S_HISTORY_LIST,
    S_HISTORY_TEXT,
};
Screen g_screen = S_MAIN;

struct HistoryEntry {
    char name[40];       // "scribe_20260417_181732.txt"
    char timestamp[20];  // "2026-04-17 18:17:32"
    uint32_t size;
};
#define MAX_HISTORY 32
HistoryEntry g_history[MAX_HISTORY];
int g_historyCount = 0;
int g_historyCursor = 0;
bool g_historyFetched = false;

// Transcript viewer state. Full body held in g_textBuffer (capped at
// MAX_TEXT_BYTES); lines pre-wrapped into g_lines offsets so scroll is
// just an index shift.
#define MAX_TEXT_BYTES 65536
#define MAX_TEXT_LINES 1600
String   g_textBuffer;
uint16_t g_lineOffsets[MAX_TEXT_LINES];  // byte offsets into g_textBuffer
uint16_t g_lineLens[MAX_TEXT_LINES];
uint16_t g_lineCount = 0;
uint16_t g_textScroll = 0;
String   g_textHeader;   // timestamp shown above body
bool     g_textOverflow = false;

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
    String url = String(proto) + kStickServerHost + ":" + String(kStickServerPort) + "/services/scribe";
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

    // Button hints
    StickCP2.Display.setTextColor(C_DARKGRAY);
    StickCP2.Display.setCursor(4, SCREEN_H - 12);
    StickCP2.Display.print("A:rec  B:history");
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
//        TRANSCRIPT HISTORY
// ==========================================

static String buildUrl(const char* path) {
    const char* proto = (kStickServerPort == 443) ? "https://" : "http://";
    String url = String(proto) + kStickServerHost;
    if (kStickServerPort != 80 && kStickServerPort != 443) {
        url += ":"; url += String(kStickServerPort);
    }
    if (path[0] != '/') url += "/";
    url += path;
    return url;
}

// GET a URL into `out`. Limits body length to `maxBytes` (caller-enforced,
// used to prevent OOM on huge transcripts).
static bool httpGet(const String& url, String& out, size_t maxBytes) {
    NetworkClientSecure tls;
    tls.setInsecure();
    HTTPClient http;
    Serial.printf("[Scribe/http] begin %s\n", url.c_str());
    if (!http.begin(tls, url)) {
        Serial.println("[Scribe/http] begin failed");
        return false;
    }
    http.setTimeout(10000);
    int code = http.GET();
    Serial.printf("[Scribe/http] GET -> %d\n", code);
    if (code != 200) { http.end(); return false; }
    int total = http.getSize();  // Content-Length; -1 when unknown
    WiFiClient* stream = http.getStreamPtr();
    out = "";
    out.reserve(total > 0 && (size_t)total < maxBytes ? total : 4096);
    char buf[513];
    uint32_t deadline = millis() + 10000;
    while (millis() < deadline && out.length() < maxBytes) {
        // Known-length fast path: stop as soon as we have the full body.
        // Keep-alive means the connection stays open after the response;
        // without this check, we'd spin until the deadline.
        if (total > 0 && (int)out.length() >= total) break;
        size_t avail = stream->available();
        if (avail == 0) {
            if (!http.connected() && !stream->available()) break;
            delay(5); continue;
        }
        size_t want = avail > sizeof(buf) - 1 ? sizeof(buf) - 1 : avail;
        int n = stream->readBytes((uint8_t*)buf, want);
        if (n <= 0) break;
        size_t room = maxBytes - out.length();
        size_t take = (size_t)n > room ? room : (size_t)n;
        buf[take] = '\0';
        out += buf;
    }
    http.end();
    return out.length() > 0;
}

// Tiny JSON field extractor (same shape as app_store's).
static bool jsonScanString(const String& body, int from, const char* key,
                            char* out, size_t outSize, int* endPos) {
    String pat = String("\"") + key + "\"";
    int k = body.indexOf(pat, from);
    if (k < 0) return false;
    int colon = body.indexOf(':', k);
    if (colon < 0) return false;
    int q1 = body.indexOf('"', colon);
    if (q1 < 0) return false;
    int q2 = body.indexOf('"', q1 + 1);
    if (q2 < 0) return false;
    size_t n = (size_t)(q2 - q1 - 1);
    if (n + 1 > outSize) n = outSize - 1;
    memcpy(out, body.c_str() + q1 + 1, n);
    out[n] = '\0';
    if (endPos) *endPos = q2 + 1;
    return true;
}

static uint32_t jsonScanInt(const String& body, int from, const char* key,
                             int* endPos) {
    String pat = String("\"") + key + "\"";
    int k = body.indexOf(pat, from);
    if (k < 0) return 0;
    int colon = body.indexOf(':', k);
    if (colon < 0) return 0;
    int p = colon + 1;
    while (p < (int)body.length() && (body[p] == ' ' || body[p] == '\t')) p++;
    uint32_t v = 0;
    while (p < (int)body.length() && body[p] >= '0' && body[p] <= '9') {
        v = v * 10 + (body[p] - '0');
        p++;
    }
    if (endPos) *endPos = p;
    return v;
}

static bool fetchTranscriptList() {
    g_historyCount = 0;
    g_historyCursor = 0;
    String body;
    if (!httpGet(buildUrl("/api/transcripts"), body, 16 * 1024)) return false;

    // Walk the transcripts array object-by-object using brace matching.
    int arr = body.indexOf('[');
    if (arr < 0) return false;
    int pos = arr + 1;
    while (g_historyCount < MAX_HISTORY) {
        int objStart = body.indexOf('{', pos);
        if (objStart < 0) break;
        int depth = 1, p = objStart + 1;
        while (p < (int)body.length() && depth > 0) {
            char c = body[p];
            if (c == '"') {
                p++;
                while (p < (int)body.length() && body[p] != '"') {
                    if (body[p] == '\\' && p + 1 < (int)body.length()) p++;
                    p++;
                }
            } else if (c == '{') depth++;
            else if (c == '}') depth--;
            p++;
        }
        if (depth != 0) break;
        String obj = body.substring(objStart, p);

        HistoryEntry& e = g_history[g_historyCount];
        memset(&e, 0, sizeof(e));
        int tmp;
        if (!jsonScanString(obj, 0, "name", e.name, sizeof(e.name), &tmp)) {
            pos = p; continue;
        }
        jsonScanString(obj, 0, "timestamp", e.timestamp, sizeof(e.timestamp), &tmp);
        e.size = jsonScanInt(obj, 0, "size", &tmp);
        g_historyCount++;
        pos = p;
    }
    g_historyFetched = true;
    return true;
}

// Word-wrap the current g_textBuffer into g_lineOffsets/g_lineLens for
// a fixed character width. Breaks on spaces and on explicit '\n'. Falls
// back to hard breaks for over-long words so nothing is lost.
static void wrapText(uint16_t charsPerLine) {
    g_lineCount = 0;
    const size_t len = g_textBuffer.length();
    const char* s = g_textBuffer.c_str();
    size_t i = 0;
    while (i < len && g_lineCount < MAX_TEXT_LINES) {
        // Strip leading spaces on a new line (but preserve explicit blanks).
        size_t lineStart = i;
        size_t lastSpace = (size_t)-1;
        size_t col = 0;
        while (i < len) {
            char c = s[i];
            if (c == '\n') { break; }
            if (c == ' ') lastSpace = i;
            col++;
            if (col > charsPerLine) {
                // Break at last space if we have one past lineStart;
                // otherwise hard break at current index.
                if (lastSpace != (size_t)-1 && lastSpace > lineStart) {
                    i = lastSpace;
                } else {
                    // hard break — step back one so the over-long char ends up
                    // on the next line.
                    i--;
                }
                break;
            }
            i++;
        }
        uint16_t lineLen = (uint16_t)(i - lineStart);
        g_lineOffsets[g_lineCount] = (uint16_t)lineStart;
        g_lineLens[g_lineCount] = lineLen;
        g_lineCount++;
        // Skip the delimiter (space or newline).
        if (i < len && (s[i] == ' ' || s[i] == '\n')) i++;
    }
}

static bool fetchTranscriptBody(const HistoryEntry& e) {
    g_textBuffer = "";
    g_textScroll = 0;
    g_lineCount  = 0;
    g_textOverflow = false;
    g_textHeader = String(e.timestamp);

    String url = buildUrl((String("/api/transcripts/") + e.name).c_str());
    if (!httpGet(url, g_textBuffer, MAX_TEXT_BYTES)) return false;
    g_textOverflow = (e.size > MAX_TEXT_BYTES);

    // Screen width 135, size-1 font is 6 px/char. Leave a 6px margin each
    // side → 123 px usable ≈ 20 chars per line.
    wrapText(20);
    return true;
}

static void drawHistoryHeader() {
    auto& d = StickCP2.Display;
    d.fillRect(0, CONTENT_Y, d.width(), 20, C_BLACK);
    d.setTextSize(2);
    d.setTextColor(C_CYAN, C_BLACK);
    d.setCursor(6, CONTENT_Y + 2);
    d.print("History");
    d.drawFastHLine(0, CONTENT_Y + 20, d.width(), C_DARKGRAY);
}

static void drawHistoryList() {
    auto& d = StickCP2.Display;
    drawHistoryHeader();
    const int firstY  = CONTENT_Y + 26;
    const int rowH    = 26;
    const int maxRows = 7;
    const int footerY = d.height() - 12;

    d.fillRect(0, firstY, d.width(), footerY - firstY, C_BLACK);

    if (g_historyCount == 0) {
        d.setTextSize(1);
        d.setTextColor(C_GRAY, C_BLACK);
        d.setCursor(10, firstY + 20);
        d.print(g_historyFetched ? "No transcripts" : "Fetching...");
    } else {
        int start = g_historyCursor - 3;
        if (start < 0) start = 0;
        if (start > g_historyCount - maxRows) start = g_historyCount - maxRows;
        if (start < 0) start = 0;

        for (int i = 0; i < maxRows && (start + i) < g_historyCount; i++) {
            const HistoryEntry& e = g_history[start + i];
            const bool sel = (start + i) == g_historyCursor;
            const int y = firstY + i * rowH;
            const uint16_t fg = sel ? C_CYAN : C_GRAY;
            const uint16_t bg = sel ? d.color565(10, 25, 30) : C_BLACK;

            d.drawRoundRect(4, y, d.width() - 8, rowH - 3, 3, fg);
            if (sel) d.fillRoundRect(5, y + 1, d.width() - 10, rowH - 5, 3, bg);

            // Timestamp split: date on top line, time on second.
            d.setTextSize(1);
            d.setTextColor(sel ? C_WHITE : fg, bg);
            d.setCursor(10, y + 3);
            // e.timestamp = "2026-04-17 18:17:32"
            char date[12] = {0}, tm[12] = {0};
            int sp = 0;
            while (e.timestamp[sp] && e.timestamp[sp] != ' ' && sp < 11) {
                date[sp] = e.timestamp[sp]; sp++;
            }
            int ts = sp + 1;
            int k = 0;
            while (e.timestamp[ts] && k < 11) { tm[k++] = e.timestamp[ts++]; }
            d.print(date[0] ? date : e.name);

            d.setTextColor(sel ? C_CYAN : C_DARKGRAY, bg);
            d.setCursor(10, y + 14);
            d.print(tm);

            // Size on the right.
            d.setTextColor(C_DARKGRAY, bg);
            d.setCursor(d.width() - 38, y + 14);
            if (e.size >= 1024) d.printf("%uk", (unsigned)(e.size / 1024));
            else                d.printf("%uB", (unsigned)e.size);
        }
    }

    d.setTextSize(1);
    d.setTextColor(C_DARKGRAY, C_BLACK);
    d.setCursor(4, footerY);
    d.print("A:open B:next PWR:back");
}

static void drawHistoryText() {
    auto& d = StickCP2.Display;
    d.fillRect(0, CONTENT_Y, d.width(), d.height() - CONTENT_Y, C_BLACK);

    // Header: timestamp + scroll indicator.
    d.setTextSize(1);
    d.setTextColor(C_CYAN, C_BLACK);
    d.setCursor(4, CONTENT_Y + 2);
    d.print(g_textHeader);
    if (g_lineCount > 0) {
        d.setTextColor(C_DARKGRAY, C_BLACK);
        d.setCursor(d.width() - 46, CONTENT_Y + 2);
        d.printf("%u/%u", (unsigned)(g_textScroll + 1),
                 (unsigned)g_lineCount);
    }
    d.drawFastHLine(0, CONTENT_Y + 14, d.width(), C_DARKGRAY);

    const int firstY   = CONTENT_Y + 18;
    const int lineH    = 10;
    const int footerY  = d.height() - 12;
    const int maxLines = (footerY - firstY) / lineH;

    d.setTextColor(C_WHITE, C_BLACK);
    for (int i = 0; i < maxLines && (g_textScroll + i) < g_lineCount; i++) {
        uint16_t off = g_lineOffsets[g_textScroll + i];
        uint16_t len = g_lineLens[g_textScroll + i];
        char tmp[32];
        if (len >= sizeof(tmp)) len = sizeof(tmp) - 1;
        memcpy(tmp, g_textBuffer.c_str() + off, len);
        tmp[len] = '\0';
        d.setCursor(4, firstY + i * lineH);
        d.print(tmp);
    }

    if (g_lineCount == 0) {
        d.setTextColor(C_GRAY, C_BLACK);
        d.setCursor(10, firstY + 20);
        d.print("(empty)");
    }
    if (g_textOverflow) {
        d.setTextColor(C_ORANGE, C_BLACK);
        d.setCursor(4, footerY - 12);
        d.print("-- truncated --");
    }

    d.setTextColor(C_DARKGRAY, C_BLACK);
    d.setCursor(4, footerY);
    d.print("A:down B:up PWR:back");
}

static void drawFetchProgress(const char* label) {
    auto& d = StickCP2.Display;
    drawHistoryHeader();
    // Clear everything from the header down to the bottom edge so the
    // previous screen's footer ("A:rec B:history") doesn't bleed through.
    d.fillRect(0, CONTENT_Y + 26, d.width(),
               d.height() - (CONTENT_Y + 26), C_BLACK);
    d.setTextSize(1);
    d.setTextColor(C_CYAN, C_BLACK);
    d.setCursor(10, CONTENT_Y + 60);
    d.print(label);
    d.setTextColor(C_DARKGRAY, C_BLACK);
    d.setCursor(4, d.height() - 12);
    d.print("PWR:back");
}

static void enterHistoryList() {
    g_screen = S_HISTORY_LIST;
    g_historyFetched = false;
    drawFetchProgress("Fetching history...");

    if (!StickNet::isConnected()) {
        auto& d = StickCP2.Display;
        d.setTextColor(C_ORANGE, C_BLACK);
        d.setCursor(10, CONTENT_Y + 40);
        d.print("Offline");
        g_historyFetched = true;
        return;
    }

    // Free the WebSocket's TLS session before spinning up an HTTPClient —
    // ESP32 mbedTLS can't reliably keep two independent contexts open on
    // the ~200 KB free heap we have at scribe launch.
    uint32_t t0 = millis();
    Serial.printf("[Scribe/History] heap before close: %u\n",
                  (unsigned)ESP.getFreeHeap());
    wsClient.close();
    isServerConnected = false;
    delay(100);
    Serial.printf("[Scribe/History] heap after close:  %u (%ums)\n",
                  (unsigned)ESP.getFreeHeap(),
                  (unsigned)(millis() - t0));

    Serial.println("[Scribe/History] fetching /api/transcripts");
    uint32_t tFetch = millis();
    bool ok = fetchTranscriptList();
    Serial.printf("[Scribe/History] fetch %s in %ums\n",
                  ok ? "ok" : "FAILED",
                  (unsigned)(millis() - tFetch));
    if (ok) {
        drawHistoryList();
    } else {
        auto& d = StickCP2.Display;
        d.setTextColor(C_RED, C_BLACK);
        d.setCursor(10, CONTENT_Y + 40);
        d.print("Fetch failed");
        g_historyFetched = true;
    }
}

static void enterHistoryText(int idx) {
    if (idx < 0 || idx >= g_historyCount) return;
    g_screen = S_HISTORY_TEXT;
    drawFetchProgress("Loading transcript...");

    uint32_t t0 = millis();
    Serial.printf("[Scribe/History] loading %s (size=%u)\n",
                  g_history[idx].name, (unsigned)g_history[idx].size);
    bool ok = fetchTranscriptBody(g_history[idx]);
    Serial.printf("[Scribe/History] body %s in %ums (%u chars, %u lines)\n",
                  ok ? "ok" : "FAILED",
                  (unsigned)(millis() - t0),
                  (unsigned)g_textBuffer.length(),
                  (unsigned)g_lineCount);
    if (!ok) {
        auto& d = StickCP2.Display;
        d.fillRect(0, CONTENT_Y + 16, d.width(), 32, C_BLACK);
        d.setTextColor(C_RED, C_BLACK);
        d.setCursor(10, CONTENT_Y + 20);
        d.print("Fetch failed");
        return;
    }
    drawHistoryText();
}

// ==========================================
//              SETUP
// ==========================================
void init() {
    Serial.println("\n[Scribe] Booting...");

    g_screen = S_MAIN;
    g_historyFetched = false;
    g_textBuffer = "";
    g_textScroll = 0;
    g_lineCount = 0;

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
static void tickHistoryList() {
    if (StickCP2.BtnB.wasPressed() && g_historyCount > 0) {
        g_historyCursor = (g_historyCursor + 1) % g_historyCount;
        drawHistoryList();
    }
    if (StickCP2.BtnA.wasPressed() && g_historyCount > 0) {
        enterHistoryText(g_historyCursor);
    }
}

static void tickHistoryText() {
    auto& d = StickCP2.Display;
    const int firstY   = CONTENT_Y + 18;
    const int lineH    = 10;
    const int footerY  = d.height() - 12;
    const int visible  = (footerY - firstY) / lineH;

    bool changed = false;
    if (StickCP2.BtnA.wasPressed() && g_lineCount > 0) {
        if (g_textScroll + visible < g_lineCount) {
            g_textScroll++;
            changed = true;
        }
    }
    if (StickCP2.BtnB.wasPressed() && g_lineCount > 0) {
        if (g_textScroll > 0) {
            g_textScroll--;
            changed = true;
        }
    }
    if (changed) drawHistoryText();
}

void tick() {
    // History screens own the display while active; network status is
    // still tracked via the shared flags but we don't clobber the UI.
    if (g_screen == S_HISTORY_LIST) {
        tickHistoryList();
        delay(20);
        return;
    }
    if (g_screen == S_HISTORY_TEXT) {
        tickHistoryText();
        delay(20);
        return;
    }

    // ---- S_MAIN: existing connected / recording / disconnected flow ----

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

    if (!wsClient.available()) {
        if (isServerConnected) {
            isServerConnected = false;
            if (isRecording) {
                stopRecording();
            }
            Serial.println("[Scribe] Server connection lost");
            drawDisconnectedScreen(DISC_SERVER);
        }

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
        // B is a no-op while recording — history is reachable only from
        // the idle connected screen.

    } else {
        if (StickCP2.BtnA.wasPressed()) {
            StickCP2.Speaker.tone(1500, 50);
            delay(50);
            startRecording();
        }
        if (StickCP2.BtnB.wasPressed()) {
            enterHistoryList();
        }
    }

    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat > 5000) {
        Serial.printf("[Scribe] heartbeat: wifi=%d server=%d rec=%d rssi=%d\n",
                      isWiFiConnected, isServerConnected, isRecording, StickNet::rssi());
        lastHeartbeat = millis();
    }

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

}  // namespace Scribe

static const stick_os::AppDescriptor kDesc = {
    /*id=*/       "scribe",
    /*name=*/     "Scribe",
    /*version=*/  "1.0.0",
    /*category=*/ stick_os::CAT_UTILITY,
    /*flags=*/    stick_os::APP_NEEDS_NET | stick_os::APP_NEEDS_MIC,
    /*icon=*/     &Scribe::icon,
    /*runtime=*/  stick_os::RUNTIME_NATIVE,
    /*native=*/   { &Scribe::init, &Scribe::tick, nullptr, nullptr },
    /*script=*/   { nullptr, nullptr },
};
STICK_REGISTER_APP(kDesc);
