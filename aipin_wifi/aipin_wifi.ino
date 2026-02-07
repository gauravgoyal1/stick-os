#include <M5StickCPlus2.h>
#include <WiFi.h>

// ==========================================
//          WIFI CONFIGURATION
// ==========================================
// List of allowed WiFi networks (SSID, Password)
struct WiFiCredentials {
    const char* ssid;
    const char* password;
};

// Add your WiFi networks here
WiFiCredentials wifiNetworks[] = {
    {"REDACTED-WIFI", "REDACTED-WIFI-PW"},
    {"REDACTED-WIFI", "REDACTED-WIFI"},
    {"Mischief Managed", "Alohamora@404"},
    // Add more networks as needed
};
const int numNetworks = sizeof(wifiNetworks) / sizeof(wifiNetworks[0]);

// Server configuration
const char* serverHost = "192.168.100.17";  // Change to your server IP
const uint16_t serverPort = 8765;

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
        
        for (int j = 0; j < numNetworks; j++) {
            if (foundSSID == wifiNetworks[j].ssid) {
                Serial.printf("[AiPin] Attempting to connect to: %s\n", wifiNetworks[j].ssid);
                
                WiFi.begin(wifiNetworks[j].ssid, wifiNetworks[j].password);
                
                int attempts = 0;
                while (WiFi.status() != WL_CONNECTED && attempts < 20) {
                    delay(500);
                    Serial.print(".");
                    attempts++;
                }
                
                if (WiFi.status() == WL_CONNECTED) {
                    Serial.printf("\n[AiPin] Connected to: %s\n", wifiNetworks[j].ssid);
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
    Serial.printf("[AiPin] Connecting to server %s:%d\n", serverHost, serverPort);
    
    if (client.connect(serverHost, serverPort)) {
        Serial.println("[AiPin] Connected to server!");
        return true;
    }
    
    Serial.println("[AiPin] Server connection failed");
    return false;
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

    // WiFi icon (connected state)
    drawWiFiIcon(centerX - 6, y, C_GREEN);

    y += 25;

    // WiFi SSID
    StickCP2.Display.setTextSize(1);
    StickCP2.Display.setTextColor(C_GREEN);
    String ssid = WiFi.SSID();
    if (ssid.length() > 20) ssid = ssid.substring(0, 18) + "..";
    int nameWidth = ssid.length() * 6;
    StickCP2.Display.setCursor(centerX - nameWidth/2, y);
    StickCP2.Display.print(ssid.c_str());
    y += 15;

    // Server status
    StickCP2.Display.setTextColor(isServerConnected ? C_GREEN : C_YELLOW);
    String serverStatus = isServerConnected ? "Server: OK" : "Server: --";
    int statusWidth = serverStatus.length() * 6;
    StickCP2.Display.setCursor(centerX - statusWidth/2, y);
    StickCP2.Display.print(serverStatus.c_str());
    y += 20;

    // Signal strength visualization
    int rssi = WiFi.RSSI();
    drawWiFiBars(centerX - 6, y, rssi);
    y += 15;
    StickCP2.Display.setTextColor(C_DARKGRAY);
    StickCP2.Display.setCursor(centerX - 15, y);
    StickCP2.Display.printf("%d dBm", rssi);

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
void drawRecordingScreen() {
    StickCP2.Display.fillScreen(C_BLACK);
    drawHeader("RECORDING");

    int y = LIST_TOP_Y + 10;
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

    // WiFi icon + text
    drawWiFiIcon(12, iconY, C_CYAN);
    StickCP2.Display.setTextColor(C_CYAN);
    StickCP2.Display.setCursor(26, iconY + 2);
    StickCP2.Display.print("TCP");

    // Wave icon + sample rate
    drawWaveIcon(58, iconY, C_GRAY);
    StickCP2.Display.setTextColor(C_GRAY);
    StickCP2.Display.setCursor(72, iconY + 2);
    StickCP2.Display.print("8k");

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

    // Send stream start header over TCP
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

        // 4. Send over TCP
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

    drawWaitingScreen("Disconnected.");
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
        StickCP2.Display.print("Connecting server...");
        isServerConnected = connectToServer();
        
        if (isServerConnected) {
            StickCP2.Speaker.tone(1500, 80);
            delay(80);
            StickCP2.Speaker.tone(2000, 80);
            drawConnectedScreen();
        } else {
            drawWaitingScreen("Server offline");
        }
    } else {
        drawWaitingScreen("WiFi not found");
    }
}

// ==========================================
//        SERIAL COMMAND INTERFACE
// ==========================================
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
    
    if (key == "server") {
        // Allow changing server at runtime: "server 192.168.1.50:8765"
        String val = cmd.substring(spaceIdx + 1);
        int colonIdx = val.indexOf(':');
        if (colonIdx > 0) {
            String host = val.substring(0, colonIdx);
            int port = val.substring(colonIdx + 1).toInt();
            Serial.printf("[AiPin] New server: %s:%d\n", host.c_str(), port);
            // Note: Would need to reconnect to apply
        }
    } else if (key == "reconnect") {
        if (isWiFiConnected) {
            client.stop();
            isServerConnected = connectToServer();
            if (isServerConnected) {
                drawConnectedScreen();
            }
        }
    } else if (key == "status") {
        Serial.printf("[AiPin] WiFi: %s, Server: %s, Recording: %s\n",
                      isWiFiConnected ? "connected" : "disconnected",
                      isServerConnected ? "connected" : "disconnected",
                      isRecording ? "yes" : "no");
    } else {
        float val = (spaceIdx > 0) ? cmd.substring(spaceIdx + 1).toFloat() : 0;
        
        if (key == "gain")       { audioGain = val; }
        else if (key == "gate")  { noiseGateThresh = val; }
        else if (key == "hpf")   { hpfAlpha = val; }
        else if (key == "lpf")   { lpfAlpha = val; }
        else if (key == "knee")  { softClipKnee = val; }
        else if (key == "ratio") { softClipRatio = val; }
        else if (key == "audio") { /* just print */ }
        else {
            Serial.println("[AiPin] Commands: gain|gate|hpf|lpf|knee|ratio|audio|server|reconnect|status");
            return;
        }

        Serial.printf("[AiPin] Audio: gain=%.1f gate=%.0f hpf=%.3f lpf=%.3f knee=%.0f ratio=%.2f\n",
                      audioGain, noiseGateThresh, hpfAlpha, lpfAlpha, softClipKnee, softClipRatio);
    }
}

// ==========================================
//              MAIN LOOP
// ==========================================
void loop() {
    StickCP2.update();
    handleSerialCommands();

    // Check WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
        if (isWiFiConnected) {
            isWiFiConnected = false;
            isServerConnected = false;
            if (isRecording) {
                stopRecording();
            }
            Serial.println("[AiPin] WiFi connection lost");
            drawWaitingScreen("WiFi lost");
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
            drawWaitingScreen("Server lost");
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
