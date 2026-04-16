#pragma once

namespace stick_os {

// Blocking UI: scans networks, shows a picker, returns when user selects one.
// Returns true if a network was selected and connected.
// Returns false if user pressed PWR to cancel.
// Saves as last-connected on success.
bool showWiFiPicker();

}
