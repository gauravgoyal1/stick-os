#include <M5StickCPlus2.h>
#include <BluetoothSerial.h>

// ==========================================
//          DISPLAY CONFIGURATION
// ==========================================
#define SCREEN_W 240
#define SCREEN_H 135

uint16_t C_BLACK, C_WHITE, C_GREEN, C_RED, C_BLUE, C_CYAN, C_YELLOW, C_ORANGE, C_GRAY, C_DARKGRAY;

void initColors() {
    C_BLACK    = BLACK;
    C_WHITE    = WHITE;
    C_GREEN    = GREEN;
    C_RED      = RED;
    C_BLUE     = BLUE;
    C_CYAN     = CYAN;
    C_YELLOW   = YELLOW;
    C_ORANGE   = ORANGE;
    C_GRAY     = StickCP2.Display.color565(128, 128, 128);
    C_DARKGRAY = StickCP2.Display.color565(40, 40, 40);
}

// ==========================================
//          APPLICATION STATE
// ==========================================
enum AppState {
    STATE_SCANNING,
    STATE_SCAN_RESULTS,
    STATE_CONNECTING,
    STATE_CONNECTED
};

AppState currentState = STATE_SCANNING;

// ==========================================
//          SCANNED DEVICE DATA
// ==========================================
#define MAX_DEVICES 20

struct ScannedDevice {
    String name;
    String address;
    int rssi;
    bool hasName;
    uint32_t cod; // Class of Device
};

ScannedDevice devices[MAX_DEVICES];
int deviceCount = 0;
int selectedIndex = 0;
int scrollOffset = 0;

#define VISIBLE_ITEMS 6
#define ROW_HEIGHT 16
#define LIST_TOP_Y 28

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
//        DEVICE CLASS DECODER
// ==========================================
const char* getDeviceClassName(uint32_t cod) {
    uint8_t major = (cod >> 8) & 0x1F;
    switch (major) {
        case 1:  return "Computer";
        case 2:  return "Phone";
        case 3:  return "Network";
        case 4:  return "Audio/Video";
        case 5:  return "Peripheral";
        case 6:  return "Imaging";
        case 7:  return "Wearable";
        case 8:  return "Toy";
        case 9:  return "Health";
        default: return "Unknown";
    }
}

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

void drawHeader(const char* title) {
    StickCP2.Display.fillRect(0, 0, SCREEN_W, 22, StickCP2.Display.color565(20, 20, 60));
    StickCP2.Display.setTextSize(1);
    StickCP2.Display.setTextColor(C_CYAN);
    StickCP2.Display.setCursor(6, 6);
    StickCP2.Display.print(title);

    int batt = StickCP2.Power.getBatteryLevel();
    StickCP2.Display.setCursor(200, 6);
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
    StickCP2.Display.setCursor(140, SCREEN_H - 11);
    StickCP2.Display.printf("B:%s", btnBLabel);
}

// ==========================================
//        SCANNING SCREEN
// ==========================================
void showScanningScreen(const char* phase) {
    StickCP2.Display.fillScreen(C_BLACK);
    drawHeader("BT SCANNER");
    StickCP2.Display.setTextSize(1);
    StickCP2.Display.setTextColor(C_WHITE);
    StickCP2.Display.setCursor(30, 42);
    StickCP2.Display.print("Scanning for devices");
    StickCP2.Display.setCursor(30, 60);
    StickCP2.Display.setTextColor(C_YELLOW);
    StickCP2.Display.print(phase);
    StickCP2.Display.setCursor(30, 82);
    StickCP2.Display.setTextColor(C_GRAY);
    StickCP2.Display.printf("Found so far: %d", deviceCount);
    drawFooter("---", "---");
}

// ==========================================
//        SCAN RESULTS SCREEN
// ==========================================
void drawScanResults() {
    StickCP2.Display.fillScreen(C_BLACK);

    char headerBuf[32];
    snprintf(headerBuf, sizeof(headerBuf), "DEVICES (%d)", deviceCount);
    drawHeader(headerBuf);

    if (deviceCount == 0) {
        StickCP2.Display.setTextSize(1);
        StickCP2.Display.setTextColor(C_GRAY);
        StickCP2.Display.setCursor(40, 50);
        StickCP2.Display.print("No devices found");
        StickCP2.Display.setCursor(15, 70);
        StickCP2.Display.setTextColor(C_YELLOW);
        StickCP2.Display.print("Open BT settings on");
        StickCP2.Display.setCursor(15, 82);
        StickCP2.Display.print("target device to pair");
        drawFooter("Rescan", "---");
        return;
    }

    if (selectedIndex >= deviceCount) selectedIndex = deviceCount - 1;
    if (selectedIndex < 0) selectedIndex = 0;
    if (selectedIndex < scrollOffset) scrollOffset = selectedIndex;
    if (selectedIndex >= scrollOffset + VISIBLE_ITEMS) scrollOffset = selectedIndex - VISIBLE_ITEMS + 1;

    StickCP2.Display.setTextSize(1);

    int endIdx = min(scrollOffset + VISIBLE_ITEMS, deviceCount);
    for (int i = scrollOffset; i < endIdx; i++) {
        int row = i - scrollOffset;
        int y = LIST_TOP_Y + row * ROW_HEIGHT;
        bool isSel = (i == selectedIndex);

        if (isSel) {
            StickCP2.Display.fillRect(0, y, SCREEN_W, ROW_HEIGHT, StickCP2.Display.color565(0, 40, 80));
        }

        // Device name
        StickCP2.Display.setTextColor(isSel ? C_WHITE : C_GRAY);
        StickCP2.Display.setCursor(6, y + 4);
        String displayName = devices[i].name;
        if (displayName.length() > 20) displayName = displayName.substring(0, 18) + "..";
        StickCP2.Display.print(displayName.c_str());

        // RSSI bars
        drawRSSIBars(200, y + 3, devices[i].rssi);
        StickCP2.Display.setCursor(215, y + 4);
        StickCP2.Display.setTextColor(C_GRAY);
        StickCP2.Display.printf("%d", devices[i].rssi);
    }

    // Scroll indicators
    if (scrollOffset > 0) {
        StickCP2.Display.setTextColor(C_CYAN);
        StickCP2.Display.setCursor(234, LIST_TOP_Y);
        StickCP2.Display.print("^");
    }
    if (scrollOffset + VISIBLE_ITEMS < deviceCount) {
        StickCP2.Display.setTextColor(C_CYAN);
        StickCP2.Display.setCursor(234, LIST_TOP_Y + (VISIBLE_ITEMS - 1) * ROW_HEIGHT);
        StickCP2.Display.print("v");
    }

    drawFooter("Connect", "Next");
}

// ==========================================
//        CONNECTING SCREEN
// ==========================================
void showConnectingScreen(const char* name) {
    StickCP2.Display.fillScreen(C_BLACK);
    drawHeader("CONNECTING");
    StickCP2.Display.setTextSize(1);
    StickCP2.Display.setTextColor(C_WHITE);
    StickCP2.Display.setCursor(20, 45);
    StickCP2.Display.print("Connecting to:");
    StickCP2.Display.setCursor(20, 62);
    StickCP2.Display.setTextColor(C_CYAN);
    String truncName = name;
    if (truncName.length() > 28) truncName = truncName.substring(0, 26) + "..";
    StickCP2.Display.print(truncName.c_str());
    StickCP2.Display.setCursor(20, 85);
    StickCP2.Display.setTextColor(C_YELLOW);
    StickCP2.Display.print("Please wait...");
}

// ==========================================
//        CONNECTED DETAILS SCREEN
// ==========================================
void drawConnectedScreen() {
    StickCP2.Display.fillScreen(C_BLACK);
    drawHeader("CONNECTED");

    StickCP2.Display.setTextSize(1);
    int y = LIST_TOP_Y;

    // Device name
    StickCP2.Display.setTextColor(C_GREEN);
    StickCP2.Display.setCursor(6, y);
    String dispName = connDeviceName;
    if (dispName.length() > 28) dispName = dispName.substring(0, 26) + "..";
    StickCP2.Display.print(dispName.c_str());
    y += 13;

    // Address
    StickCP2.Display.setTextColor(C_GRAY);
    StickCP2.Display.setCursor(6, y);
    StickCP2.Display.print(connDeviceAddr.c_str());
    y += 13;

    // Device class
    StickCP2.Display.setTextColor(C_ORANGE);
    StickCP2.Display.setCursor(6, y);
    StickCP2.Display.printf("Classic BT | %s", connDeviceClassStr.c_str());
    y += 13;

    // RSSI
    StickCP2.Display.setTextColor(C_WHITE);
    StickCP2.Display.setCursor(6, y);
    StickCP2.Display.printf("RSSI: %d dBm", connDeviceRSSI);
    drawRSSIBars(140, y, connDeviceRSSI);

    drawFooter("Disconnect", "---");
}

// ==========================================
//        ADDRESS STRING TO BYTE ARRAY
// ==========================================
void addrStringToBytes(const String& str, uint8_t* addr) {
    sscanf(str.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
           &addr[0], &addr[1], &addr[2], &addr[3], &addr[4], &addr[5]);
}

// ==========================================
//        START SCAN (CLASSIC BT)
// ==========================================
void startScan() {
    currentState = STATE_SCANNING;
    deviceCount = 0;
    selectedIndex = 0;
    scrollOffset = 0;

    showScanningScreen("Classic BT (5s)...");

    BTScanResults* btResults = SerialBT.discover(5000);
    if (btResults) {
        int count = btResults->getCount();
        for (int i = 0; i < count && deviceCount < MAX_DEVICES; i++) {
            BTAdvertisedDevice* dev = btResults->getDevice(i);
            if (!dev) continue;

            devices[deviceCount].address = dev->getAddress().toString().c_str();
            devices[deviceCount].rssi = dev->haveRSSI() ? dev->getRSSI() : -100;
            devices[deviceCount].cod = dev->getCOD();

            if (dev->haveName()) {
                String name = dev->getName().c_str();
                if (name.length() > 0) {
                    devices[deviceCount].name = name;
                    devices[deviceCount].hasName = true;
                } else {
                    devices[deviceCount].name = devices[deviceCount].address;
                    devices[deviceCount].hasName = false;
                }
            } else {
                devices[deviceCount].name = devices[deviceCount].address;
                devices[deviceCount].hasName = false;
            }
            deviceCount++;
        }
    }

    // Sort: named devices first, then by RSSI descending
    for (int i = 0; i < deviceCount - 1; i++) {
        for (int j = 0; j < deviceCount - i - 1; j++) {
            bool swap = false;
            if (devices[j + 1].hasName && !devices[j].hasName) swap = true;
            else if (devices[j + 1].hasName == devices[j].hasName &&
                     devices[j + 1].rssi > devices[j].rssi) swap = true;
            if (swap) {
                ScannedDevice tmp = devices[j];
                devices[j] = devices[j + 1];
                devices[j + 1] = tmp;
            }
        }
    }

    currentState = STATE_SCAN_RESULTS;
    drawScanResults();
}

// ==========================================
//        CONNECT TO DEVICE
// ==========================================
void connectToDevice(int index) {
    if (index < 0 || index >= deviceCount) return;

    currentState = STATE_CONNECTING;
    showConnectingScreen(devices[index].name.c_str());

    connDeviceName = devices[index].name;
    connDeviceAddr = devices[index].address;
    connDeviceRSSI = devices[index].rssi;
    connDeviceClassStr = getDeviceClassName(devices[index].cod);

    bool connected = false;
    uint8_t addr[6];
    addrStringToBytes(devices[index].address, addr);
    connected = SerialBT.connect(addr);

    if (connected) {
        isConnected = true;
        currentState = STATE_CONNECTED;

        StickCP2.Speaker.tone(1500, 80);
        delay(80);
        StickCP2.Speaker.tone(2000, 80);

        drawConnectedScreen();
    } else {
        isConnected = false;

        StickCP2.Display.fillScreen(C_BLACK);
        drawHeader("FAILED");
        StickCP2.Display.setTextSize(1);
        StickCP2.Display.setTextColor(C_RED);
        StickCP2.Display.setCursor(30, 45);
        StickCP2.Display.print("Connection failed!");
        StickCP2.Display.setTextColor(C_GRAY);
        StickCP2.Display.setCursor(30, 65);
        StickCP2.Display.print("Device may be out");
        StickCP2.Display.setCursor(30, 77);
        StickCP2.Display.print("of range or busy.");

        StickCP2.Speaker.tone(300, 200);
        drawFooter("Back", "---");

        while (true) {
            StickCP2.update();
            if (StickCP2.BtnA.wasPressed()) break;
            delay(20);
        }

        currentState = STATE_SCAN_RESULTS;
        drawScanResults();
    }
}

// ==========================================
//        DISCONNECT
// ==========================================
void disconnectDevice() {
    SerialBT.disconnect();
    isConnected = false;

    StickCP2.Speaker.tone(800, 100);
    delay(50);
    StickCP2.Speaker.tone(400, 100);

    StickCP2.Display.fillScreen(C_BLACK);
    drawHeader("DISCONNECTED");
    StickCP2.Display.setTextSize(1);
    StickCP2.Display.setTextColor(C_YELLOW);
    StickCP2.Display.setCursor(40, 55);
    StickCP2.Display.print("Disconnected.");
    StickCP2.Display.setTextColor(C_WHITE);
    StickCP2.Display.setCursor(30, 75);
    StickCP2.Display.print("Starting new scan...");
    delay(1200);

    startScan();
}

// ==========================================
//              SETUP
// ==========================================
void setup() {
    StickCP2.begin();
    StickCP2.Speaker.begin();
    StickCP2.Speaker.setVolume(120);
    StickCP2.Display.setRotation(1);
    StickCP2.Display.setTextSize(1);

    initColors();

    // Splash screen
    StickCP2.Display.fillScreen(C_BLACK);
    StickCP2.Display.setTextSize(2);
    StickCP2.Display.setTextColor(C_CYAN);
    StickCP2.Display.setCursor(55, 30);
    StickCP2.Display.print("AiPin");
    StickCP2.Display.setTextSize(1);
    StickCP2.Display.setTextColor(C_GRAY);
    StickCP2.Display.setCursor(40, 65);
    StickCP2.Display.print("BT Device Manager");
    StickCP2.Display.setCursor(35, 90);
    StickCP2.Display.setTextColor(C_WHITE);
    StickCP2.Display.print("Initializing BT...");
    delay(1000);

    // Initialize Classic BT in master mode (allows scanning + connecting)
    SerialBT.begin("AiPin", true);

    startScan();
}

// ==========================================
//              MAIN LOOP
// ==========================================
void loop() {
    StickCP2.update();

    switch (currentState) {

        case STATE_SCAN_RESULTS: {
            if (deviceCount == 0) {
                if (StickCP2.BtnA.wasPressed()) {
                    StickCP2.Speaker.tone(1000, 50);
                    startScan();
                }
            } else {
                // BtnB = next device
                if (StickCP2.BtnB.wasPressed()) {
                    selectedIndex++;
                    if (selectedIndex >= deviceCount) selectedIndex = 0;
                    StickCP2.Speaker.tone(3000, 30);
                    drawScanResults();
                }

                // BtnA = connect to selected
                if (StickCP2.BtnA.wasPressed()) {
                    StickCP2.Speaker.tone(1500, 50);
                    connectToDevice(selectedIndex);
                }
            }
            break;
        }

        case STATE_CONNECTED: {
            if (!SerialBT.connected()) {
                isConnected = false;
                StickCP2.Display.fillScreen(C_BLACK);
                drawHeader("LOST CONNECTION");
                StickCP2.Display.setTextSize(1);
                StickCP2.Display.setTextColor(C_RED);
                StickCP2.Display.setCursor(30, 55);
                StickCP2.Display.print("Device disconnected.");
                StickCP2.Display.setTextColor(C_WHITE);
                StickCP2.Display.setCursor(30, 80);
                StickCP2.Display.print("Press A to rescan");
                StickCP2.Speaker.tone(300, 200);

                while (true) {
                    StickCP2.update();
                    if (StickCP2.BtnA.wasPressed()) break;
                    delay(20);
                }
                startScan();
                break;
            }

            // BtnA = disconnect
            if (StickCP2.BtnA.wasPressed()) {
                disconnectDevice();
            }

            break;
        }

        default:
            break;
    }

    delay(20);
}
