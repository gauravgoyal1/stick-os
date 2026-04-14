#include <M5Unified.h>
#include <WiFi.h>
#include <wifi_config.h>

static constexpr int ROWS_PER_PAGE = 5;
static int networkCount = 0;
static int page = 0;
static bool scanning = false;

static const char* authLabel(wifi_auth_mode_t a) {
  switch (a) {
    case WIFI_AUTH_OPEN:            return "OPEN";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-E";
    case WIFI_AUTH_WPA3_PSK:        return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2/3";
    default:                        return "?";
  }
}

static void drawHeader(const char* status) {
  M5.Display.fillRect(0, 0, M5.Display.width(), 16, BLACK);
  M5.Display.setCursor(2, 2);
  M5.Display.setTextColor(GREEN, BLACK);
  M5.Display.setTextSize(1);
  M5.Display.printf("WiFi: %s", status);
}

static void drawPage() {
  M5.Display.fillRect(0, 16, M5.Display.width(), M5.Display.height() - 16, BLACK);
  M5.Display.setTextSize(1);

  if (networkCount <= 0) {
    M5.Display.setCursor(2, 24);
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.print("no networks found");
    return;
  }

  const int totalPages = (networkCount + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE;
  if (page >= totalPages) page = 0;
  const int start = page * ROWS_PER_PAGE;
  const int end = min(start + ROWS_PER_PAGE, networkCount);

  int y = 20;
  for (int i = start; i < end; i++) {
    const int rssi = WiFi.RSSI(i);
    const uint16_t color = rssi > -60 ? GREEN : rssi > -75 ? YELLOW : RED;
    M5.Display.setCursor(2, y);
    M5.Display.setTextColor(color, BLACK);

    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) ssid = "<hidden>";
    if (ssid.length() > 16) ssid = ssid.substring(0, 16);

    M5.Display.printf("%-16s %4d c%02d %s",
                      ssid.c_str(),
                      rssi,
                      WiFi.channel(i),
                      authLabel(WiFi.encryptionType(i)));
    y += 10;
  }

  M5.Display.setCursor(2, M5.Display.height() - 10);
  M5.Display.setTextColor(DARKGREY, BLACK);
  M5.Display.printf("p%d/%d  A:rescan B:page", page + 1, totalPages);
}

static void dumpSsidHex(const char* label, const String& ssid) {
  Serial.printf("  %s \"%s\" hex:", label, ssid.c_str());
  for (size_t i = 0; i < ssid.length(); i++) {
    Serial.printf(" %02x", (unsigned char)ssid[i]);
  }
  Serial.println();
}

static int findKnownMatch() {
  for (int i = 0; i < networkCount; i++) {
    const String ssid = WiFi.SSID(i);
    for (size_t k = 0; k < kWiFiNetworkCount; k++) {
      if (ssid == kWiFiNetworks[k].ssid) {
        return (int)((k << 16) | (uint16_t)i);
      }
    }
  }
  return -1;
}

static void tryConnectKnown() {
  const int packed = findKnownMatch();
  if (packed < 0) {
    Serial.println("no known network visible — dumping SSID hex for debug:");
    for (int i = 0; i < networkCount && i < 10; i++) {
      dumpSsidHex("scan", WiFi.SSID(i));
    }
    for (size_t k = 0; k < kWiFiNetworkCount; k++) {
      String s(kWiFiNetworks[k].ssid);
      dumpSsidHex("cfg ", s);
    }
    return;
  }
  const size_t k = (size_t)(packed >> 16);
  const int    i = packed & 0xffff;
  Serial.printf("match: kWiFiNetworks[%u] == scan[%d] \"%s\"\n",
                (unsigned)k, i, kWiFiNetworks[k].ssid);
  dumpSsidHex("scan", WiFi.SSID(i));
  dumpSsidHex("cfg ", String(kWiFiNetworks[k].ssid));

  Serial.printf("connecting to %s ...\n", kWiFiNetworks[k].ssid);
  WiFi.begin(kWiFiNetworks[k].ssid, kWiFiNetworks[k].password);
  const uint32_t deadline = millis() + 15000;
  while (WiFi.status() != WL_CONNECTED && millis() < deadline) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("CONNECTED  ip=%s  rssi=%d\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.RSSI());
  } else {
    Serial.printf("FAILED  status=%d\n", WiFi.status());
  }
  WiFi.disconnect(true, true);
}

static void runScan() {
  scanning = true;
  drawHeader("scanning...");
  Serial.println("scan start");

  WiFi.scanDelete();
  networkCount = WiFi.scanNetworks(false, true);
  page = 0;

  Serial.printf("scan done: %d networks\n", networkCount);
  for (int i = 0; i < networkCount; i++) {
    Serial.printf("  %2d  %-32s  %4d dBm  ch%02d  %s\n",
                  i,
                  WiFi.SSID(i).c_str(),
                  WiFi.RSSI(i),
                  WiFi.channel(i),
                  authLabel(WiFi.encryptionType(i)));
  }

  tryConnectKnown();

  char status[24];
  snprintf(status, sizeof(status), "%d found", networkCount);
  drawHeader(status);
  drawPage();
  scanning = false;
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(3);
  M5.Display.fillScreen(BLACK);

  Serial.begin(115200);
  delay(100);
  Serial.println("wifi_scan booted");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(100);

  runScan();
}

void loop() {
  M5.update();

  if (scanning) {
    delay(20);
    return;
  }

  if (M5.BtnA.wasPressed()) {
    runScan();
  } else if (M5.BtnB.wasPressed() && networkCount > ROWS_PER_PAGE) {
    const int totalPages = (networkCount + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE;
    page = (page + 1) % totalPages;
    drawPage();
  }

  delay(30);
}
