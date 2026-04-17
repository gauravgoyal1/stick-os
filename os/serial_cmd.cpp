// Serial command handler for USB provisioning (WiFi creds, API key,
// and LittleFS file seeding for scripted-app dev).

#include "launcher_state.h"
#include <stick_net.h>
#include <LittleFS.h>

namespace launcher {

void processSerialCommand() {
    if (!Serial.available()) return;
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) return;

    if (line.startsWith("WIFI_SET ")) {
        // Format: WIFI_SET <ssid>\t<password>
        // Tab delimiter lets SSIDs and passwords contain spaces.
        String rest = line.substring(9);
        int tab = rest.indexOf('\t');
        if (tab < 0) {
            Serial.println("ERR: usage WIFI_SET ssid<TAB>password");
            return;
        }
        String ssid = rest.substring(0, tab);
        String pass = rest.substring(tab + 1);
        if (stick_os::saveWiFiCred(ssid.c_str(), pass.c_str())) {
            Serial.printf("OK: saved \"%s\"\n", ssid.c_str());
        } else {
            Serial.println("ERR: max networks reached");
        }
    } else if (line == "WIFI_LIST") {
        stick_os::WiFiCred creds[stick_os::kMaxWiFiNetworks];
        size_t n = stick_os::loadWiFiCreds(creds,
                                            stick_os::kMaxWiFiNetworks);
        for (size_t i = 0; i < n; i++) {
            Serial.printf("%u: %s\n", (unsigned)i, creds[i].ssid);
        }
        if (n == 0) Serial.println("(no networks stored)");
    } else if (line.startsWith("WIFI_DEL ")) {
        String ssid = line.substring(9);
        if (stick_os::deleteWiFiCred(ssid.c_str())) {
            Serial.printf("OK: deleted \"%s\"\n", ssid.c_str());
        } else {
            Serial.printf("ERR: \"%s\" not found\n", ssid.c_str());
        }
    } else if (line.startsWith("APIKEY_SET ")) {
        String key = line.substring(11);
        if (key.length() == 0) {
            Serial.println("ERR: usage APIKEY_SET <key>");
            return;
        }
        stick_os::saveApiKey(key.c_str());
        Serial.println("OK: API key saved");
    } else if (line == "APIKEY_GET") {
        char key[65];
        if (stick_os::getApiKey(key, sizeof(key))) {
            Serial.printf("OK: %s\n", key);
        } else {
            Serial.println("(no API key stored)");
        }
    } else if (line.startsWith("FILE_PUT ")) {
        // Format: FILE_PUT <path> <size>\n<size bytes>
        // Simple binary seed — meant for dev tools pushing small .py files
        // before the .stickapp installer (2e) lands.
        String rest = line.substring(9);
        int space = rest.indexOf(' ');
        if (space < 0) {
            Serial.println("ERR: usage FILE_PUT <path> <size>");
            return;
        }
        String path = rest.substring(0, space);
        size_t size = (size_t)rest.substring(space + 1).toInt();
        if (size == 0 || size > 64 * 1024) {
            Serial.println("ERR: bad size");
            return;
        }
        // Ensure parent dirs exist (LittleFS needs explicit mkdir).
        String dir = path;
        int slash = dir.lastIndexOf('/');
        if (slash > 0) {
            String parent = dir.substring(0, slash);
            if (!LittleFS.exists(parent)) LittleFS.mkdir(parent);
        }
        File f = LittleFS.open(path, "w");
        if (!f) {
            Serial.println("ERR: open failed");
            return;
        }
        size_t remaining = size;
        const uint32_t deadline = millis() + 10000;
        while (remaining > 0 && millis() < deadline) {
            if (Serial.available()) {
                int c = Serial.read();
                if (c >= 0) {
                    f.write((uint8_t)c);
                    remaining--;
                }
            }
        }
        f.close();
        if (remaining == 0) {
            Serial.printf("OK: wrote %u bytes to %s\n", (unsigned)size,
                          path.c_str());
        } else {
            Serial.printf("ERR: timeout, %u bytes missing\n",
                          (unsigned)remaining);
        }
    } else if (line.startsWith("FILE_LS ")) {
        String dir = line.substring(8);
        File root = LittleFS.open(dir, "r");
        if (!root || !root.isDirectory()) {
            Serial.println("ERR: not a directory");
            return;
        }
        File entry;
        while ((entry = root.openNextFile())) {
            Serial.printf("%s %s %u\n",
                          entry.isDirectory() ? "d" : "f",
                          entry.name(), (unsigned)entry.size());
            entry.close();
        }
        root.close();
    } else if (line.startsWith("FILE_RM ")) {
        String path = line.substring(8);
        if (LittleFS.remove(path)) {
            Serial.printf("OK: removed %s\n", path.c_str());
        } else {
            Serial.printf("ERR: remove failed %s\n", path.c_str());
        }
    } else if (line.startsWith("MPY_RUN ")) {
        // Dev helper: run a LittleFS script without going through the
        // launcher UI. Dumps any exception traceback to serial.
        String path = line.substring(8);
        Serial.printf("MPY_RUN %s\n", path.c_str());
        bool ok = stick_os::scriptRunFile(path.c_str());
        Serial.printf("MPY_RUN done: %s\n", ok ? "ok" : "fail");
    }
}

}  // namespace launcher
