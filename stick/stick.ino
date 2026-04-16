// Stick — unified launcher that bundles Arcade and AiPin-WiFi into
// one firmware image. Pick one at boot; short-click the power button
// inside either app to return to the home screen without rebooting,
// so WiFi + NTP state survive the switch.
//
// Architecture: each app lives in its own library namespace
// (libraries/arcade_app/, libraries/aipin_wifi_app/). This sketch
// owns StickCP2.begin(), kicks off WiFi + NTP bring-up on a background
// FreeRTOS task (libraries/stick_net/), renders the home screen with
// app icons and live WiFi status, and dispatches to the chosen app's
// init() + tick(). A short power-button click flips the state machine
// back to ST_LAUNCHER in-place — no ESP.restart().

#include <M5StickCPlus2.h>
#include <Preferences.h>
#include <arcade_app.h>
#include <aipin_wifi_app.h>
#include <stick_net.h>
#include <stick_os.h>

namespace {

enum AppId : uint8_t {
  APP_ARCADE = 0,
  APP_AIPIN  = 1,
  APP_COUNT  = 2,
};

const char* const kAppNames[APP_COUNT] = {
  "Arcade",
  "AiPin",
};

enum State : uint8_t {
  ST_LAUNCHER = 0,
  ST_APP      = 1,
};

State g_state      = ST_LAUNCHER;
AppId g_currentApp = APP_ARCADE;
AppId g_cursor     = APP_ARCADE;
uint32_t g_lastStatusDraw = 0;
StickNet::Stage g_lastStage = StickNet::STAGE_IDLE;
uint8_t g_btnDrainFrames = 0;

Preferences g_prefs;
constexpr const char* kPrefsNamespace = "stick";
constexpr const char* kPrefsKeyLast   = "last";

AppId loadLastApp() {
  g_prefs.begin(kPrefsNamespace, /*readOnly=*/true);
  const uint8_t v = g_prefs.getUChar(kPrefsKeyLast, APP_ARCADE);
  g_prefs.end();
  return (v < APP_COUNT) ? static_cast<AppId>(v) : APP_ARCADE;
}

void saveLastApp(AppId id) {
  g_prefs.begin(kPrefsNamespace, /*readOnly=*/false);
  g_prefs.putUChar(kPrefsKeyLast, static_cast<uint8_t>(id));
  g_prefs.end();
}

// ---------- Icons ----------
// Drawn with display primitives so we don't need bitmap assets.

void drawArcadeIcon(int x, int y, uint16_t color) {
  // Gamepad silhouette: rounded body + two round buttons + D-pad.
  auto& d = StickCP2.Display;
  d.fillRoundRect(x, y + 6, 36, 18, 6, color);
  d.fillRoundRect(x + 3, y, 10, 6, 2, color);   // left shoulder
  d.fillRoundRect(x + 23, y, 10, 6, 2, color);  // right shoulder
  d.fillCircle(x + 27, y + 14, 2, BLACK);       // A button
  d.fillCircle(x + 31, y + 18, 2, BLACK);       // B button
  // D-pad cross
  d.fillRect(x + 5, y + 13, 9, 3, BLACK);
  d.fillRect(x + 8, y + 10, 3, 9, BLACK);
}

void drawAipinIcon(int x, int y, uint16_t color) {
  // Mic icon: capsule + stand + base.
  auto& d = StickCP2.Display;
  d.fillRoundRect(x + 12, y, 12, 18, 6, color);
  d.drawLine(x + 8,  y + 16, x + 8,  y + 22, color);
  d.drawArc(x + 18, y + 16, 12, 11, 180, 360, color);
  d.drawLine(x + 28, y + 16, x + 28, y + 22, color);
  d.drawLine(x + 18, y + 22, x + 18, y + 28, color);
  d.fillRect(x + 10, y + 28, 17, 2, color);
}

// Draw a 4-bar WiFi signal meter at (x, y). RSSI < -90 shows a gray "!".
void drawWiFiStatusBars(int x, int y, int rssi, bool connected) {
  auto& d = StickCP2.Display;
  int bars = 0;
  if (connected) {
    if      (rssi >= -50) bars = 4;
    else if (rssi >= -65) bars = 3;
    else if (rssi >= -80) bars = 2;
    else                  bars = 1;
  }
  const int heights[] = {3, 5, 7, 9};
  const uint16_t dim  = d.color565(50, 50, 50);
  for (int i = 0; i < 4; i++) {
    int bx = x + i * 3;
    int by = y + 9 - heights[i];
    uint16_t col = (i < bars) ? GREEN : dim;
    d.fillRect(bx, by, 2, heights[i], col);
  }
}

void drawNetStatus() {
  auto& d = StickCP2.Display;
  const int w = d.width();
  // Clear status strip in top-right.
  d.fillRect(w - 90, 4, 86, 14, BLACK);

  const StickNet::Stage s = StickNet::status();
  const bool connected    = StickNet::isWiFiReady();
  drawWiFiStatusBars(w - 16, 4, StickNet::rssi(), connected);

  d.setTextSize(1);
  d.setCursor(w - 88, 6);
  // Once we're READY, the status strip becomes a live clock. Other
  // stages still show a short word so the user knows what's happening.
  switch (s) {
    case StickNet::STAGE_WIFI:
      d.setTextColor(YELLOW, BLACK); d.print("WiFi.. ");
      break;
    case StickNet::STAGE_NTP:
      d.setTextColor(YELLOW, BLACK); d.print("NTP..  ");
      break;
    case StickNet::STAGE_FAILED:
      d.setTextColor(RED, BLACK);    d.print("no net ");
      break;
    case StickNet::STAGE_READY: {
      auto dt = StickCP2.Rtc.getDateTime();
      d.setTextColor(GREEN, BLACK);
      d.printf("%02d:%02d", dt.time.hours, dt.time.minutes);
      break;
    }
    default:
      d.setTextColor(DARKGREY, BLACK); d.print("       ");
      break;
  }
}

void drawHome(AppId cursor) {
  auto& d = StickCP2.Display;
  d.setRotation(1);  // Landscape, matches arcade's convention
  d.fillScreen(BLACK);

  // Title
  d.setTextSize(2);
  d.setTextColor(GREEN, BLACK);
  d.setCursor(8, 6);
  d.print("STICK");
  d.drawFastHLine(0, 26, d.width(), DARKGREEN);

  // App cards
  const int cardY[APP_COUNT]   = {34, 80};
  const int cardH              = 42;
  const int iconX              = 14;
  const int labelX             = 66;

  for (uint8_t i = 0; i < APP_COUNT; i++) {
    const bool sel = (i == cursor);
    const int y = cardY[i];
    const uint16_t bg = sel ? d.color565(0, 60, 0) : BLACK;
    const uint16_t fg = sel ? GREEN : d.color565(80, 80, 80);

    d.drawRoundRect(4, y, d.width() - 8, cardH, 4, fg);
    if (sel) d.fillRoundRect(5, y + 1, d.width() - 10, cardH - 2, 4, bg);

    const int iconY = y + 6;
    if (i == APP_ARCADE) drawArcadeIcon(iconX, iconY, fg);
    else                 drawAipinIcon (iconX, iconY, fg);

    d.setTextSize(2);
    d.setTextColor(sel ? WHITE : fg, bg);
    d.setCursor(labelX, y + 14);
    d.print(kAppNames[i]);
  }

  // Footer
  d.setTextSize(1);
  d.setTextColor(DARKGREY, BLACK);
  d.setCursor(4, d.height() - 10);
  d.print("A:select  B:next  PWR:exit");

  drawNetStatus();
}

void enterLauncher() {
  g_state  = ST_LAUNCHER;
  g_cursor = loadLastApp();
  drawHome(g_cursor);
  g_lastStage       = StickNet::status();
  g_lastStatusDraw  = 0;
  // Drain any spurious button events from the power-click that just
  // brought us here (and from boot/begin() on first entry). Without
  // this, BtnA / BtnPWR immediately re-fire on the next loop().
  g_btnDrainFrames = 8;
}

void launcherTick() {
  if (g_btnDrainFrames > 0) {
    g_btnDrainFrames--;
    return;
  }

  if (StickCP2.BtnB.wasPressed()) {
    g_cursor = static_cast<AppId>((g_cursor + 1) % APP_COUNT);
    drawHome(g_cursor);
    g_lastStage = StickNet::status();
  }

  if (StickCP2.BtnA.wasPressed()) {
    saveLastApp(g_cursor);
    g_currentApp = g_cursor;
    Serial.printf("launching app %u (%s)\n",
                  static_cast<unsigned>(g_currentApp),
                  kAppNames[g_currentApp]);
    switch (g_currentApp) {
      case APP_ARCADE: ArcadeApp::init();    break;
      case APP_AIPIN:  AiPinWifiApp::init(); break;
      default:         ArcadeApp::init();    break;
    }
    g_state = ST_APP;
    return;
  }

  // Keep the net-status strip live while the user browses.
  const uint32_t now = millis();
  if (now - g_lastStatusDraw > 500) {
    drawNetStatus();
    g_lastStage      = StickNet::status();
    g_lastStatusDraw = now;
  }
}

}  // namespace

void setup() {
  StickCP2.begin();
  Serial.begin(115200);
  delay(50);
  Serial.println("stick booted");
  Serial.printf("[stick_os] %u apps registered\n", (unsigned)stick_os::appCount());
  for (size_t i = 0; i < stick_os::appCount(); i++) {
    auto* d = stick_os::appAt(i);
    Serial.printf("  [%u] %s (cat=%u)\n", (unsigned)i, d->name, d->category);
  }

  // Kick off WiFi + NTP in the background so the user can browse the
  // home screen while it runs. By the time they pick an app, chances
  // are the network is already up.
  StickNet::startAsync();

  enterLauncher();
}

void loop() {
  StickCP2.update();

  if (g_state == ST_APP && M5.BtnPWR.wasClicked()) {
    Serial.println("pwr clicked - returning to home");
    enterLauncher();
    return;
  }

  if (g_state == ST_LAUNCHER) {
    launcherTick();
    delay(20);
    return;
  }

  switch (g_currentApp) {
    case APP_ARCADE: ArcadeApp::tick();    break;
    case APP_AIPIN:  AiPinWifiApp::tick(); break;
    default: break;
  }
}
