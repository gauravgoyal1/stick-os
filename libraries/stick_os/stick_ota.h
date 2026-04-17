#pragma once

// Firmware OTA — Task 2h of the Phase 2 plan.
//
// Flow:
//   1. otaCheckForUpdate() fetches GET /api/firmware, parses version +
//      path + size + sha256, fills the caller's OtaInfo.
//   2. Caller shows UI, compares versions, asks user to confirm.
//   3. otaDownloadAndApply() streams the binary into the inactive OTA
//      slot, streams a SHA256 hash in parallel, and aborts on mismatch.
//      On success it marks the new slot as boot target and returns.
//   4. Caller reboots (ESP.restart()).
//
// LittleFS is untouched, so installed .stickapp bundles survive an OTA.

#include <stddef.h>
#include <stdint.h>

namespace stick_os {

struct OtaInfo {
    char version[16];       // "1.2.3"
    char path[96];          // "firmware/v1.2.3.bin"
    uint32_t size;          // bytes
    char sha256[65];        // 64 hex chars + NUL
};

// Fetch GET /api/firmware from the configured Stick server host, parse
// the JSON, and fill `out`. Returns false on network failure, HTTP error,
// or malformed JSON. `out->version == ""` after a successful call means
// no update is published (server returned the default placeholder).
bool otaCheckForUpdate(OtaInfo* out);

// Progress callback: bytes_written / total_bytes (total may be zero at
// the very first callback).
using OtaProgressFn = void (*)(uint32_t done, uint32_t total);

// Download the firmware at `info->path` (relative to the Stick server)
// into the inactive OTA slot, verifying SHA256. On success, marks the
// slot as boot target — caller must reboot. On failure, aborts cleanly.
bool otaDownloadAndApply(const OtaInfo* info, OtaProgressFn progress);

}  // namespace stick_os
