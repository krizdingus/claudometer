# Claudometer — Firmware

ESP32 firmware for the Cheap Yellow Display. The host-side daemon and end-user
install story live in the [top-level README](../README.md). This file is for
people building the firmware themselves.

## Build

```
pio run -e esp32dev
```

## Upload to attached device

```
pio run -e esp32dev -t upload && pio device monitor
```

To flash a release `.bin` instead of building from source, use `claudometer
add-device --port /dev/cu.usbserial-XXXX` from the daemon — it pulls the
latest firmware from GitHub and handles provisioning.

## Unit tests (host)

```
pio test -e native
```

`src/` files that touch Arduino, ESP-IDF, LVGL, WiFi, or Preferences must
wrap their bodies in `#ifndef UNIT_TEST … #endif`. Pure-C++ declarations
need no guard. See `firmware/test/`.

## Hardware targets

- ESP32-2432S028C (capacitive, FT6336 touch) — primary
- ESP32-2432S028R (resistive, XPT2046 touch) — supported

## End-to-end smoke

Setup happens over USB. `claudometer add-device` (or, manually, a serial
client at 115200 baud) sends one JSON line with WiFi credentials, the
daemon's host and port, and a bearer token. The firmware writes those to
NVS and reboots into normal polling. mDNS stays in the firmware as a
fallback for when the daemon's LAN IP changes.
