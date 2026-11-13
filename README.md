# openwb-simple-display

A small ESP32-S3 touchscreen display for [openWB](https://openwb.de/) EV-charger
installations: live chargepoint status at a glance, touch/swipe control of charge mode,
current and lock state, a local web UI for device settings, and browser-based (no
toolchain) firmware installation and updates.

**This is an independent, unofficial, community project.** It is not affiliated with,
endorsed by, or supported by the openWB project or openWB GmbH & Co. KG. It is also not
affiliated with, and does not share code with, the similar
[evcc-garage-display](https://github.com/tobymarks/evcc-garage-display) project for the
`evcc` charge controller, which inspired the idea of a dedicated garage/wall display for
this product category.

## Status

Early development. See [`docs/`](docs/) (once published) for the install page, and the
milestones below for current progress.

## Hardware

- **Guition ESP32-4848S040C** — ESP32-S3, 4" 480×480 IPS touch panel (ST7701S RGB/DPI,
  GT911 capacitive touch), USB-C powered. This is currently the only supported board.

## Software requirements (your openWB installation)

- openWB2 (current, MQTT/Python-based software). The display talks to openWB's built-in
  `simpleAPI` HTTP endpoint (`/openWB/simpleAPI/simpleapi.php`), which is enabled by
  default on every stock install — no configuration needed on the openWB side.

## Building

This is a [PlatformIO](https://platformio.org/) project. It uses a git submodule for
the board definition, so clone with `--recurse-submodules`, or run
`git submodule update --init` after a plain clone.

```sh
pio run -e guition_4848s040c          # build
pio run -e guition_4848s040c -t upload  # flash over USB-C
pio test -e native_test                 # run host-side unit tests
```

Most users won't need any of this — once published, flashing a pre-built release is a
one-click browser install from the project's GitHub Pages site, using
[ESP Web Tools](https://esphome.github.io/esp-web-tools/) (Web Serial, Chrome/Edge only).

### Seeing the UI without hardware

The same `src/ui/*` code the firmware runs also builds as a desktop program — useful for
checking layout/theme/text changes before touching real hardware:

```sh
pio run -e sim_screenshot && .pio/build/sim_screenshot/program   # renders every
                                                                   # screen to src/sim/out/*.bmp

pio run -e sim_interactive && .pio/build/sim_interactive/program [--multi]  # live window,
                                                                              # mouse = touch;
                                                                              # needs libsdl2-dev
```

## Project structure

```
src/board/    hardware bring-up: display driver, touch driver, pin map
src/config/   persistent settings (NVS), shared by all three UIs below
src/net/      WiFi provisioning, network task
src/openwb/   openWB simpleAPI client (status polling + control commands)
src/state/    shared app state between the network task and the UI task
src/ui/       LVGL screens, widgets, and the quartz-derived theme
src/web/      local web settings server (ESPAsyncWebServer)
src/ota/      firmware update manager
src/sim/      desktop screenshot/interactive tools (see "Seeing the UI" above) — never
              built into the real firmware
tools/        font/icon asset generation, release packaging
docs/         GitHub Pages site: landing page, web installer, release manifests
```

## License

MIT — see [`LICENSE`](LICENSE). Bundled fonts and icons are separately licensed; see
`tools/fonts/source/LICENSE` and `tools/icons/source/LICENSE`.
