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
