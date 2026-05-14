# CYD Claude Monitor — Firmware

ESP32 firmware for the Cheap Yellow Display variant of the Claude Usage Monitor.

## Build

```
pio run -e esp32dev
```

## Upload to attached device

```
pio run -e esp32dev -t upload && pio device monitor
```

## Unit tests (host)

```
pio test -e native
```

## Hardware targets

- ESP32-2432S028C (capacitive, FT6336 touch) — primary
- ESP32-2432S028R (resistive, XPT2046 touch) — supported

## End-to-end smoke

Setup happens over USB: the desktop app (or, manually, a serial client at
115200 baud) sends one JSON line with WiFi credentials, the daemon's host
and port, and a bearer token. The firmware writes those to NVS and reboots
into normal polling. mDNS stays in the firmware as a fallback for when the
daemon's LAN IP changes.
