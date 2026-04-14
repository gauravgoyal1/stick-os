// Thin wrapper around the AiPinWifiApp library.
// The real implementation lives in libraries/aipin_wifi_app/.
// This sketch exists so `./tools/flash.sh aipin/wifi` still flashes
// AiPin standalone for solo debugging. The canonical production
// target is stick/.

#include <M5StickCPlus2.h>
#include <aipin_wifi_app.h>

void setup() {
  StickCP2.begin();
  Serial.begin(115200);
  AiPinWifiApp::init();
}

void loop() {
  StickCP2.update();
  AiPinWifiApp::tick();
}
