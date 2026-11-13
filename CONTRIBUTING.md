# Contributing

This project is early-stage; see the milestone sequence in the architecture plan for
where things stand. A few conventions worth knowing before sending a change:

- Follow the module boundaries described in `README.md`'s project structure section —
  in particular, `src/config/` is the single place settings are read/written from, and
  `src/state/app_state.*` is the only channel between the network task and the UI task.
- New openWB write actions go in `src/openwb/openwb_commands.*`, one function per
  simpleAPI parameter, following the existing pattern.
- Run `pio test -e native_test` before sending a change that touches
  `src/config/config_store.*`, `src/openwb/openwb_response_parser.h`, or
  `src/ota/ota_manifest.h` — these are the parts covered by host-side unit tests.
- Hardware-touching changes (`src/board/`, `src/ui/`) need to actually be flashed and
  exercised on a real Guition ESP32-4848S040C before being considered done; this isn't
  optional given how easy it is to introduce RGB-panel timing regressions that only show
  up on real hardware.
