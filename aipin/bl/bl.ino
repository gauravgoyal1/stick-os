#include <M5StickCPlus2.h>
#include <BluetoothSerial.h>
#include "esp_gap_bt_api.h"

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
#define AUDIO_SAMPLE_RATE    8000      // Standard "Telephone Quality" (stable on Bluetooth)
#define AUDIO_BIT_DEPTH      8         // 8-bit to save 50% bandwidth
#define AUDIO_CHANNELS       1
#define AUDIO_CHUNK_SAMPLES  1024      // Larger buffer = fewer transmission interruptions
#define AUDIO_CHUNK_BYTES    (AUDIO_CHUNK_SAMPLES * 1)  // 1 byte per sample now

#define LIST_TOP_Y 28

// Recording state
bool isRecording = false;
unsigned long recordStartMillis = 0;
// Note: mic captures 16-bit natively; we downsample to 8-bit in captureAndStreamChunk()

// Audio processing settings — tunable at runtime via Serial commands
// Voice fundamentals: 85–255 Hz | Consonants/sibilants: up to 5 kHz
// Target: -12dB to -6dB peaks (~8000-15000 amplitude)
// Serial commands: gain <val> | gate <val> | hpf <val> | lpf <val> | knee <val> | ratio <val> | audio
float audioGain       = 20.0;    // Lowered from 90.0 to prevent clipping
float noiseGateThresh = 200.0;   // Lowered from 800.0 to match new gain
float hpfAlpha        = 0.90;    // Slightly less aggressive high pass (was 0.97)
float lpfAlpha        = 0.30;    // Stronger low pass to remove digital whine (was 0.45)
float softClipKnee    = 25000.0; // Start compressing a bit earlier (was 28000.0)
float softClipRatio   = 0.5;     // Less aggressive compression (was 0.3)

// Filter state variables (separate for left/right channel support)
float hpf_prev_input = 0.0;
float hpf_prev_output = 0.0;
float lpf_prev_output = 0.0;

// Protocol magic bytes
const uint8_t STREAM_START_MAGIC[4] = {0x41, 0x50, 0x53, 0x54}; // "APST"
const uint8_t STREAM_STOP_MAGIC[4]  = {0x41, 0x50, 0x4E, 0x44}; // "APND"

// ==========================================
//          BT OBJECTS
// ==========================================
BluetoothSerial SerialBT;

// Connection state
bool isConnected = false;
String connDeviceName = "";
String connDeviceAddr = "";
int connDeviceRSSI = 0;
String connDeviceClassStr = "";

// ==========================================
//        DRAWING HELPERS
// ==========================================
void drawRSSIBars(int x, int y, int rssi) {
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
    // Microphone capsule (rounded rectangle)
    StickCP2.Display.fillRoundRect(x + 3, y, 6, 9, 2, color);
    // Mic stand
    StickCP2.Display.drawLine(x + 6, y + 9, x + 6, y + 13, color);
    StickCP2.Display.drawLine(x + 3, y + 11, x + 9, y + 11, color);
    // Base
    StickCP2.Display.drawLine(x + 2, y + 13, x + 10, y + 13, color);
}

void drawBluetoothIcon(int x, int y, uint16_t color) {
    // Classic Bluetooth symbol
    StickCP2.Display.drawLine(x + 4, y, x + 4, y + 12, color);
    StickCP2.Display.drawLine(x + 4, y, x + 8, y + 3, color);
    StickCP2.Display.drawLine(x + 8, y + 3, x + 1, y + 6, color);
    StickCP2.Display.drawLine(x + 1, y + 6, x + 8, y + 9, color);
    StickCP2.Display.drawLine(x + 8, y + 9, x + 4, y + 12, color);
}

void drawWaveIcon(int x, int y, uint16_t color) {
    // Sound waves (3 curved lines)
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
    // Filter/funnel shape
    StickCP2.Display.drawLine(x, y, x + 10, y, color);
    StickCP2.Display.drawLine(x, y, x + 3, y + 4, color);
    StickCP2.Display.drawLine(x + 10, y, x + 7, y + 4, color);
    StickCP2.Display.drawLine(x + 3, y + 4, x + 3, y + 8, color);
    StickCP2.Display.drawLine(x + 7, y + 4, x + 7, y + 8, color);
    StickCP2.Display.fillRect(x + 3, y + 8, 5, 2, color);
}

void drawHeader(const char* title) {
    StickCP2.Display.fillRect(0, 0, SCREEN_W, 22, StickCP2.Display.color565(20, 20, 60));
    StickCP2.Display.setTextSize(1);
    StickCP2.Display.setTextColor(C_CYAN);
    StickCP2.Display.setCursor(6, 6);
    StickCP2.Display.print(title);

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
//        WAITING SCREEN
// ==========================================
void drawWaitingScreen(const char* message) {
    StickCP2.Display.fillScreen(C_BLACK);
    drawHeader("WAITING");
    StickCP2.Display.setTextSize(1);
    StickCP2.Display.setTextColor(C_YELLOW);
    StickCP2.Display.setCursor(6, LIST_TOP_Y);
    StickCP2.Display.print(message);
    drawFooter("---", "---");
}

// ==========================================
//        CONNECTED DETAILS SCREEN
// ==========================================
void drawConnectedScreen() {
    StickCP2.Display.fillScreen(C_BLACK);
    drawHeader("CONNECTED");

    int y = LIST_TOP_Y + 15;
    int centerX = SCREEN_W / 2;

    // Large Bluetooth icon (connected state)
    drawBluetoothIcon(centerX - 5, y, C_GREEN);

    y += 25;

    // Device name (if available)
    StickCP2.Display.setTextSize(1);
    StickCP2.Display.setTextColor(C_GREEN);
    String dispName = connDeviceName;
    if (dispName.length() > 20) dispName = dispName.substring(0, 18) + "..";
    int nameWidth = dispName.length() * 6;
    StickCP2.Display.setCursor(centerX - nameWidth/2, y);
    StickCP2.Display.print(dispName.c_str());
    y += 15;

    // Device class with icon
    StickCP2.Display.setTextColor(C_GRAY);
    String classText = "SPP";
    int classWidth = classText.length() * 6;
    StickCP2.Display.setCursor(centerX - classWidth/2, y);
    StickCP2.Display.print(classText.c_str());
    y += 20;

    // Signal strength visualization (RSSI bars centered)
    if (connDeviceRSSI != 0) {
        drawRSSIBars(centerX - 6, y, connDeviceRSSI);
        y += 15;
        StickCP2.Display.setTextColor(C_DARKGRAY);
        StickCP2.Display.setCursor(centerX - 15, y);
        StickCP2.Display.printf("%d dBm", connDeviceRSSI);
    }

    drawFooter("Rec", "Disc");
}

// ==========================================
//        AUDIO PROCESSING FUNCTIONS
// ==========================================
void processAudioBuffer(int16_t* buffer, size_t samples) {
    // Multi-stage audio processing pipeline for voice enhancement
    // Pipeline: Gain → HPF (DC removal) → LPF (noise reduction) → Noise Gate → Clip

    for (size_t i = 0; i < samples; i++) {
        float sample = (float)buffer[i];

        // Stage 1: Software amplification
        sample *= audioGain;

        // Stage 2: High-pass filter (1st order IIR) — removes DC offset and low-frequency rumble
        // y[n] = alpha * (y[n-1] + x[n] - x[n-1])
        float hpf_output = hpfAlpha * (hpf_prev_output + sample - hpf_prev_input);
        hpf_prev_input = sample;
        hpf_prev_output = hpf_output;

        // Stage 3: Low-pass filter (1st order IIR) — removes high-frequency noise
        // y[n] = alpha * x[n] + (1 - alpha) * y[n-1]
        float lpf_output = lpfAlpha * hpf_output + (1.0 - lpfAlpha) * lpf_prev_output;
        lpf_prev_output = lpf_output;

        // Stage 4: Noise gate — suppress very quiet samples to reduce background hiss
        if (fabs(lpf_output) < noiseGateThresh) {
            lpf_output = 0;
        }

        // Stage 5: Soft clipping — compress peaks above knee for natural limiting
        float clipped = lpf_output;
        if (clipped > softClipKnee)
            clipped = softClipKnee + (clipped - softClipKnee) * softClipRatio;
        else if (clipped < -softClipKnee)
            clipped = -softClipKnee + (clipped + softClipKnee) * softClipRatio;

        // Stage 6: Hard clipping to prevent int16_t overflow
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

    Serial.printf("[AiPin] Sending APST header (connected=%d, hasClient=%d)\n",
                  SerialBT.connected(), SerialBT.hasClient());
    size_t w1 = SerialBT.write(STREAM_START_MAGIC, 4);
    size_t w2 = SerialBT.write((uint8_t*)&sampleRate, 4);
    size_t w3 = SerialBT.write((uint8_t*)&bitDepth, 2);
    size_t w4 = SerialBT.write((uint8_t*)&channels, 2);
    SerialBT.flush();
    Serial.printf("[AiPin] Header sent: %d+%d+%d+%d = %d bytes\n",
                  w1, w2, w3, w4, w1+w2+w3+w4);
}

void sendStreamStop() {
    SerialBT.write(STREAM_STOP_MAGIC, 4);
    SerialBT.flush();
}

// ==========================================
//        RECORDING SCREEN
// ==========================================
void drawRecordingScreen() {
    StickCP2.Display.fillScreen(C_BLACK);
    drawHeader("RECORDING");

    int y = LIST_TOP_Y + 10;

    // Large recording indicator with icon
    int centerX = SCREEN_W / 2;

    // Red recording circle (pulsing indicator)
    StickCP2.Display.fillCircle(centerX - 25, y + 8, 6, C_RED);

    // Microphone icon
    drawMicIcon(centerX - 6, y + 2, C_RED);

    // Duration timer
    StickCP2.Display.setTextSize(2);
    StickCP2.Display.setTextColor(C_WHITE);
    StickCP2.Display.setCursor(centerX - 30, y + 22);
    StickCP2.Display.print("00:00");

    y += 50;

    // Status icons row
    StickCP2.Display.setTextSize(1);
    int iconY = y;

    // Bluetooth icon + text
    drawBluetoothIcon(12, iconY, C_CYAN);
    StickCP2.Display.setTextColor(C_CYAN);
    StickCP2.Display.setCursor(26, iconY + 2);
    StickCP2.Display.print("SPP");

    // Wave icon + sample rate
    drawWaveIcon(58, iconY, C_GRAY);
    StickCP2.Display.setTextColor(C_GRAY);
    StickCP2.Display.setCursor(72, iconY + 2);
    StickCP2.Display.print("16k");

    // Filter icon (noise reduction)
    drawFilterIcon(104, iconY, C_GREEN);
    StickCP2.Display.setTextColor(C_GREEN);
    StickCP2.Display.setCursor(118, iconY + 2);
    StickCP2.Display.print("NR");

    drawFooter("Stop", "Disc");
}

void updateRecordingTimer() {
    unsigned long elapsed = (millis() - recordStartMillis) / 1000;
    unsigned int mins = elapsed / 60;
    unsigned int secs = elapsed % 60;

    int centerX = SCREEN_W / 2;
    int timerY = LIST_TOP_Y + 32;

    // Overwrite just the timer area
    StickCP2.Display.fillRect(centerX - 30, timerY, 60, 16, C_BLACK);
    StickCP2.Display.setTextSize(2);
    StickCP2.Display.setTextColor(C_WHITE);
    StickCP2.Display.setCursor(centerX - 30, timerY);
    StickCP2.Display.printf("%02d:%02d", mins, secs);

    // Blink the red recording dot
    bool dotVisible = ((millis() / 500) % 2 == 0);
    uint16_t dotColor = dotVisible ? C_RED : C_BLACK;
    int dotY = LIST_TOP_Y + 18;
    StickCP2.Display.fillCircle(centerX - 25, dotY, 6, dotColor);
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

    // Send stream start header over SPP
    sendStreamHeader();

    isRecording = true;
    recordStartMillis = millis();

    drawRecordingScreen();
}

void stopRecording() {
    isRecording = false;

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

    // 1. Record 16-bit audio as usual (StickCP2 mic is natively 16-bit)
    int16_t tempBuffer[AUDIO_CHUNK_SAMPLES];

    if (StickCP2.Mic.record(tempBuffer, AUDIO_CHUNK_SAMPLES, AUDIO_SAMPLE_RATE)) {

        // 2. Process Audio (Gain, Noise Gate, etc.)
        processAudioBuffer(tempBuffer, AUDIO_CHUNK_SAMPLES);

        // 3. Compress to 8-bit (High Byte Only)
        // This discards the fine detail but keeps the core audio, reducing data by 50%
        int8_t txBuffer[AUDIO_CHUNK_SAMPLES];
        for (int i = 0; i < AUDIO_CHUNK_SAMPLES; i++) {
            // Take the top 8 bits of the 16-bit sample
            txBuffer[i] = (int8_t)(tempBuffer[i] >> 8);
        }

        // 4. Send the smaller 8-bit packet
        size_t written = SerialBT.write((uint8_t*)txBuffer, AUDIO_CHUNK_SAMPLES);

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
        while (StickCP2.Mic.isRecording()) { delay(1); }
        StickCP2.Mic.end();
        StickCP2.Speaker.begin();
        StickCP2.Speaker.setVolume(120);
        sendStreamStop();
        delay(50);
    }

    SerialBT.disconnect();
    isConnected = false;

    StickCP2.Speaker.tone(800, 100);
    delay(50);
    StickCP2.Speaker.tone(400, 100);

    drawWaitingScreen("Disconnected.");
}

// ==========================================
//              SETUP
// ==========================================
void setup() {
    Serial.begin(115200);
    Serial.println("\n[AiPin] Booting...");

    StickCP2.begin();
    StickCP2.Speaker.begin();
    StickCP2.Speaker.setVolume(120);
    StickCP2.Display.setRotation(0);
    StickCP2.Display.setTextSize(1);

    initColors();

    // Splash screen
    StickCP2.Display.fillScreen(C_BLACK);
    StickCP2.Display.setTextSize(2);
    StickCP2.Display.setTextColor(C_CYAN);
    StickCP2.Display.setCursor(37, 70);
    StickCP2.Display.print("AiPin");
    StickCP2.Display.setTextSize(1);
    StickCP2.Display.setTextColor(C_GRAY);
    StickCP2.Display.setCursor(16, 110);
    StickCP2.Display.print("BT Audio Streamer");
    StickCP2.Display.setCursor(13, 140);
    StickCP2.Display.setTextColor(C_WHITE);
    StickCP2.Display.print("Initializing BT...");
    delay(1000);

    // Initialize Classic BT in slave mode (Mac connects to us)
    SerialBT.register_callback([](esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
        Serial.printf("[BT-SPP] event=%d\n", event);
    });
    SerialBT.onConfirmRequest([](uint32_t numVal) {
        Serial.printf("[BT] SSP confirm request: %d — auto-accepting\n", numVal);
        SerialBT.confirmReply(true);
    });
    SerialBT.onAuthComplete([](boolean success) {
        Serial.printf("[BT] Auth complete: %s\n", success ? "SUCCESS" : "FAIL");
    });
    SerialBT.begin("AiPin");  // slave mode (default)
    SerialBT.setPin("1234", 4);

    // Register GAP callback for detailed pairing debug
    esp_bt_gap_register_callback([](esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
        Serial.printf("[GAP] event=%d\n", event);
        if (event == ESP_BT_GAP_AUTH_CMPL_EVT) {
            Serial.printf("[GAP] Auth complete: status=%d name=%s\n",
                          param->auth_cmpl.stat, param->auth_cmpl.device_name);
        }
        if (event == ESP_BT_GAP_PIN_REQ_EVT) {
            Serial.println("[GAP] PIN requested — replying 1234");
            esp_bt_pin_code_t pin = {'1', '2', '3', '4'};
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin);
        }
    });

    // Ensure device is discoverable and connectable
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
    Serial.println("[AiPin] BT initialized (slave mode, PIN=1234)");

    drawWaitingScreen("Run receiver.py");
}

// ==========================================
//        SERIAL COMMAND INTERFACE
// ==========================================
// Tune audio parameters at runtime via Serial Monitor (no reflash needed).
// Commands: gain <val> | gate <val> | hpf <val> | lpf <val> | knee <val> | ratio <val> | audio
String serialCmdBuf = "";

void handleSerialCommands() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            serialCmdBuf.trim();
            if (serialCmdBuf.length() > 0) {
                processSerialCommand(serialCmdBuf);
                serialCmdBuf = "";
            }
        } else {
            serialCmdBuf += c;
        }
    }
}

void processSerialCommand(const String& cmd) {
    int spaceIdx = cmd.indexOf(' ');
    String key = (spaceIdx > 0) ? cmd.substring(0, spaceIdx) : cmd;
    float val  = (spaceIdx > 0) ? cmd.substring(spaceIdx + 1).toFloat() : 0;

    if (key == "gain")       { audioGain = val; }
    else if (key == "gate")  { noiseGateThresh = val; }
    else if (key == "hpf")   { hpfAlpha = val; }
    else if (key == "lpf")   { lpfAlpha = val; }
    else if (key == "knee")  { softClipKnee = val; }
    else if (key == "ratio") { softClipRatio = val; }
    else if (key == "audio") { /* just print */ }
    else {
        Serial.println("[AiPin] Unknown command. Try: gain|gate|hpf|lpf|knee|ratio|audio");
        return;
    }

    Serial.printf("[AiPin] Audio settings: gain=%.1f gate=%.0f hpf=%.3f lpf=%.3f knee=%.0f ratio=%.2f\n",
                  audioGain, noiseGateThresh, hpfAlpha, lpfAlpha, softClipKnee, softClipRatio);
}

// ==========================================
//              MAIN LOOP
// ==========================================
void loop() {
    StickCP2.update();
    handleSerialCommands();

    // Detect Mac connecting via SPP
    if (!isConnected && SerialBT.connected()) {
        isConnected = true;
        connDeviceName = "Mac (SPP)";
        connDeviceAddr = "";
        connDeviceClassStr = "Computer";
        connDeviceRSSI = 0;

        Serial.println("[AiPin] Mac connected via SPP!");
        StickCP2.Speaker.tone(1500, 80);
        delay(80);
        StickCP2.Speaker.tone(2000, 80);
        drawConnectedScreen();
    }

    // Detect lost connection
    if (isConnected && !SerialBT.connected()) {
        if (isRecording) {
            isRecording = false;
            while (StickCP2.Mic.isRecording()) { delay(1); }
            StickCP2.Mic.end();
            StickCP2.Speaker.begin();
            StickCP2.Speaker.setVolume(120);
        }

        isConnected = false;
        Serial.println("[AiPin] Connection lost, waiting for reconnect...");
        StickCP2.Speaker.tone(300, 200);
        drawWaitingScreen("Connection lost.");
    }

    // Handle connected state
    if (isConnected) {
        if (isRecording) {
            captureAndStreamChunk();

            static unsigned long lastTimerUpdate = 0;
            if (millis() - lastTimerUpdate > 1000) {
                updateRecordingTimer();
                lastTimerUpdate = millis();
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
    }

    // Periodic heartbeat for debug
    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat > 5000) {
        Serial.printf("[AiPin] heartbeat: conn=%d btConn=%d hasClient=%d rec=%d\n",
                      isConnected, SerialBT.connected(), SerialBT.hasClient(), isRecording);
        lastHeartbeat = millis();
    }

    // Skip delay during recording — captureAndStreamChunk() already paces the loop
    if (!isRecording) {
        delay(20);
    }
}
