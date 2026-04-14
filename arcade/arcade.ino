// Thin wrapper around the ArcadeApp library.
// The real implementation lives in libraries/arcade_app/.
// This sketch exists so `./tools/flash.sh arcade` still flashes Arcade
// standalone for solo debugging. The canonical production target is stick/.

#include <M5StickCPlus2.h>
#include <arcade_app.h>

void setup() {
  StickCP2.begin();
  Serial.begin(115200);
  ArcadeApp::init();
}

void loop() {
  StickCP2.update();
  ArcadeApp::tick();
}
