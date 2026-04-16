// Serial command handler for USB provisioning (WiFi creds, API key).

#include "launcher_state.h"
#include <stick_net.h>

namespace launcher {

void processSerialCommand() {
    if (!Serial.available()) return;
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) return;

    if (line.startsWith("WIFI_SET ")) {
        String rest = line.substring(9);
        int space = rest.indexOf(' ');
        if (space < 0) {
            Serial.println("ERR: usage WIFI_SET ssid password");
            return;
        }
        String ssid = rest.substring(0, space);
        String pass = rest.substring(space + 1);
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
    }
}

}  // namespace launcher
