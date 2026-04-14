# clap_remote

A clap-activated IR remote for a TCL Mini LED TV, built on the **M5StickC Plus 2**.

Two claps toggle TV power. The detector uses a two-stage filter (envelope onset + 2–5 kHz spectral gate) to reject TV audio, speech, and door slams.

Phase 2 (planned): replace the clap detector with a voice-command pipeline (Wi-Fi → Whisper → local command dispatch).

## Layout

```
clap_remote/
├── firmware/
│   ├── hello_world/       # boot sanity check
│   ├── ir_probe/          # cycle TCL candidate power codes
│   ├── ir_sweep/          # GPIO sweep diagnostic (find the real IR pin)
│   ├── common/
│   │   └── ir_codes.h     # TCL candidate codes + locked power code
│   └── clap_remote/       # main firmware (modular: mic, detector, gesture, ir, ui)
├── docs/
│   └── superpowers/
│       ├── specs/2026-04-14-clap-ir-remote-design.md
│       └── plans/2026-04-14-clap-ir-remote.md
└── CLAUDE.md              # working notes, locked decisions, open questions
```

## Status

🚧 Phase 1 in progress. Currently stuck at IR pin discovery — the official M5 example uses GPIO 19 but that pin doesn't produce a visible IR emission, so we're sweeping pins with `ir_sweep/` to find the real one.

## Build

From the repo root:

```bash
./tools/flash.sh clap_remote/firmware/hello_world
./tools/flash.sh clap_remote/firmware/ir_probe
./tools/flash.sh clap_remote/firmware/ir_sweep
./tools/flash.sh clap_remote/firmware/clap_remote
```

## References

- Full design spec: [`docs/superpowers/specs/2026-04-14-clap-ir-remote-design.md`](docs/superpowers/specs/2026-04-14-clap-ir-remote-design.md)
- Implementation plan: [`docs/superpowers/plans/2026-04-14-clap-ir-remote.md`](docs/superpowers/plans/2026-04-14-clap-ir-remote.md)
- Working notes: [`CLAUDE.md`](CLAUDE.md)
