# Blinds Controller: Agent Notes

## Project shape

- `src/` is the Arduino sketch. Arduino requires the sketch file to match its
  directory, so the entry point is `src/src.ino`.
- Build for **ESP32S2 Dev Module** with USB CDC enabled and **Upload Mode:
  Internal USB** (`esp32:esp32:esp32s2:CDCOnBoot=cdc,UploadMode=cdc`). These
  options are defined by `FQBN` in the `Makefile`.
- `src/config.local.h` contains Wi-Fi, MQTT, controller MAC, zone, RF remote,
  and travel-time configuration. It is ignored by Git; never print, commit, or
  replace its credentials.
- `src/config.local.example.h` is the safe template for new installations.
- `tests/` contains small host-side C++ tests. `tests/fakes/Arduino.h` lets
  tests include firmware types without an Arduino SDK.

## Everyday commands

- `make test` runs host-side tests only.
- `make build` compiles normal, RF-enabled firmware without changing a device.
- `make flash` builds and installs normal firmware on `PORT` (default:
  `/dev/cu.usbmodem01`). This enables live RF control.
- `make build-dry-run` / `make flash-dry-run` compile or install a build with
  radio writes disabled. Use these for safe device-path checks.
- `make monitor` opens the serial console; close it before flashing.
- `make boards` lists serial devices.
- `make capture-build` / `make capture-flash` build or install the passive RF
  capture sketch. It has no RF write calls and auto-acknowledgements are
  disabled. Capture firmware replaces normal control temporarily; restore it
  afterwards with `make flash`.
- `CAPTURE_CHANNEL=52 make channel-capture-flash` installs a passive receiver
  locked to one RF channel and reports packet-train timing. Repeat it for 52,
  71, and 33 to measure the physical remote's per-channel dwell. Restore normal
  firmware afterwards with `make flash`.

Always run `make test`, `make build`, and `make build-dry-run` after firmware
changes. Do not flash a normal build unless the user has authorized live RF.

## RF and motion behaviour

- Every direction gesture mirrors the physical remote: an active UP/DOWN phase
  followed by its matching release phase, with one packet rapidly sent on each
  channel per hop cycle. Starts repeat the complete same-direction gesture for
  reception redundancy; STOP uses one gesture only. Queue coalescing and
  post-stop suppression guard against a delayed follow-on reversal.
- When the radio is not transmitting, it scans the three channels in receive
  mode. Active remote UP/DOWN frames for owned zones update the estimated motion
  state without RF transmission; release and duplicate frames are ignored.
- The command queue coalesces pending work per blind. Motion also suppresses a
  duplicate opposite-direction command for a short post-stop window.
- The runtime position is estimated from configured travel times; there is no
  physical position feedback.
- Command paths must reject invalid input and RF-unavailable states rather than
  updating optimistic motion state without a possible transmission.

## Device and serial notes

- The connected board is an ESP32-S2 at `/dev/cu.usbmodem01` when attached.
- A `screen` or monitor session owns the serial port and will make flashing
  fail with “Resource busy”. Close it (`Ctrl-A`, `K`, `Y` in `screen`) before
  `make flash`.
- Dry-run firmware waits briefly after boot, then exercises a start/start/stop
  profile sequence. Its log must show `dryRun=true` and `not transmitted`.
- After any dry-run flash, install normal firmware with `make flash` before
  leaving the controller in service.

## Change discipline

- Keep changes scoped and preserve unrelated user edits.
- For reliability or control-path work, add a focused host test where practical
  and verify both normal and dry-run builds.
- Use separate, descriptive commits for independent fixes.
