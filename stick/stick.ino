// Stick — unified launcher that bundles Arcade and AiPin-WiFi into
// one firmware image. Pick one at boot; short-click the power button
// inside either app to restart back to this launcher.
//
// Architecture: each app lives in its own library namespace
// (libraries/arcade_app/, libraries/aipin_wifi_app/). This sketch
// owns StickCP2.begin(), renders the menu, dispatches to the chosen
// app's init(), and forwards loop() to the chosen app's tick() until
// the power button is clicked, at which point it calls ESP.restart().

#include <M5StickCPlus2.h>
#include <Preferences.h>
#include <arcade_app.h>
#include <aipin_wifi_app.h>

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

AppId g_currentApp = APP_ARCADE;

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

void drawLauncher(AppId cursor) {
  auto& d = StickCP2.Display;
  d.setRotation(1);  // Landscape, matches arcade's convention
  d.fillScreen(BLACK);

  d.setTextSize(2);
  d.setTextColor(GREEN, BLACK);
  d.setCursor(8, 6);
  d.print("STICK");
  d.drawFastHLine(0, 26, d.width(), DARKGREEN);

  d.setTextSize(2);
  for (uint8_t i = 0; i < APP_COUNT; i++) {
    const int y = 36 + i * 22;
    d.setCursor(8, y);
    if (i == cursor) {
      d.setTextColor(BLACK, GREEN);
      d.printf(" %-10s ", kAppNames[i]);
    } else {
      d.setTextColor(GREEN, BLACK);
      d.printf("  %-10s ", kAppNames[i]);
    }
  }

  d.setTextSize(1);
  d.setTextColor(DARKGREY, BLACK);
  d.setCursor(4, d.height() - 10);
  d.print("A:select  B:next  PWR:exit");
}

AppId launcherShow() {
  AppId cursor = loadLastApp();
  drawLauncher(cursor);

  // Drain any spurious button events left over from boot/begin().
  // Without this, the very first wasPressed() on BtnA fires on a fresh
  // boot and the launcher immediately exits to the saved app.
  for (int i = 0; i < 5; i++) {
    StickCP2.update();
    delay(10);
  }

  while (true) {
    StickCP2.update();

    if (StickCP2.BtnB.wasPressed()) {
      cursor = static_cast<AppId>((cursor + 1) % APP_COUNT);
      drawLauncher(cursor);
    }

    if (StickCP2.BtnA.wasPressed()) {
      saveLastApp(cursor);
      return cursor;
    }

    delay(20);
  }
}

}  // namespace

void setup() {
  StickCP2.begin();
  Serial.begin(115200);
  delay(50);
  Serial.println("stick booted");

  g_currentApp = launcherShow();
  Serial.printf("launching app %u (%s)\n",
                static_cast<unsigned>(g_currentApp),
                kAppNames[g_currentApp]);

  switch (g_currentApp) {
    case APP_ARCADE: ArcadeApp::init();    break;
    case APP_AIPIN:  AiPinWifiApp::init(); break;
    default:         ArcadeApp::init();    break;
  }
}

void loop() {
  StickCP2.update();

  if (M5.BtnPWR.wasClicked()) {
    Serial.println("pwr clicked - restarting to launcher");
    delay(30);
    ESP.restart();
  }

  switch (g_currentApp) {
    case APP_ARCADE: ArcadeApp::tick();    break;
    case APP_AIPIN:  AiPinWifiApp::tick(); break;
    default: break;
  }
}
