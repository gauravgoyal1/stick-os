#pragma once
#include <stdint.h>

// Candidate TCL NEC power-toggle codes, collected from public IR databases
// (LIRC, Flipper IR Assets, IRDB). Listed in rough popularity order.
// The ir_probe sketch cycles through these; whichever toggles the TV
// gets copied into TCL_POWER_CODE below.
struct IrCandidate {
  const char* label;
  uint32_t raw;      // 32-bit NEC code, MSB-first
};

static constexpr IrCandidate kTclCandidates[] = {
  { "TCL-A 0x57E3E817", 0x57E3E817 },  // most commonly reported
  { "TCL-B 0x20DF10EF", 0x20DF10EF },
  { "TCL-C 0x02FD48B7", 0x02FD48B7 },
  { "TCL-D 0x20DF23DC", 0x20DF23DC },
  { "TCL-E 0x807F02FD", 0x807F02FD },
};
static constexpr size_t kTclCandidateCount =
    sizeof(kTclCandidates) / sizeof(kTclCandidates[0]);

// Filled in after ir_probe identifies the working code.
// Defaults to the first candidate so clap_remote still compiles until
// Task 2 step 7 updates this line.
static constexpr uint32_t TCL_POWER_CODE = 0x57E3E817;
