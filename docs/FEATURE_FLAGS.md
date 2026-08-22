# Feature Flags / Build Configuration Guide

This document describes how to enable or disable specific features in the SunSpec
gateway firmware using PlatformIO build flags.

## Overview

The project uses PlatformIO's `build_flags` configuration in `platformio.ini` to
conditionally compile features. This allows you to:

- Reduce firmware binary size by excluding unused features
- Save RAM by not loading unused code
- Enable only the peripherals you need for your specific deployment
- Avoid compilation errors when hardware is not present

## Configuration

Edit `platformio.ini` and add `-D<FEATURE_NAME>` to the `build_flags` section to
enable a feature, or comment it out (with `;`) to disable:

```ini
build_flags =
    ; ... other flags ...
    -DENABLE_OLED_DISPLAY    ; Uncomment to enable
    ;-DENABLE_WIFI           ; Comment to disable (note the ; prefix)
```

## Available Feature Flags

| Flag | Description | Dependencies | Default |
|------|-------------|--------------|---------|
| `ENABLE_OLED_DISPLAY` | SH1106 OLED display (SPI) + console UI | `SPI` | Disabled |
| `ENABLE_WIFI` | WiFi connectivity | `include/secrets.h` | **Enabled** |
| `ENABLE_ETHERNET` | W5500 SPI Ethernet via HR961160C | `BOARD_VER_V3` | Disabled |
| `ENABLE_ETH_STATIC_IP` | Static Ethernet address instead of DHCP | `ENABLE_ETHERNET` | Disabled (DHCP) |
| `ENABLE_MQTT` | MQTT telemetry of Sol-Ark data | `ENABLE_WIFI` or `ENABLE_ETHERNET` | Disabled |
| `ENABLE_MODBUS_MASTER` | Modbus RTU master on RS485_1 — Sol-Ark polling | None | **Enabled** |
| `ENABLE_MODBUS_TCP_SERVER` | SunSpec Modbus TCP server (slave) | `ENABLE_MODBUS_MASTER` + a network interface | **Enabled** |
| `ENABLE_RELAYS` | Onboard SSR control on `RELAY_1_PIN` | None | Disabled |
| `ENABLE_SD_CARD` | SD card reader (shared SPI bus) | `BOARD_VER_V3` | Disabled |
| `ENABLE_SD_SOLARK_LOG` | Log Sol-Ark telemetry to `/solark_log.csv` | `ENABLE_SD_CARD` + `ENABLE_MODBUS_MASTER` | Disabled |
| `ENABLE_SD_CARD_DEBUG` | Print card type, size, and directory listing on boot | `ENABLE_SD_CARD` | Disabled |
| `ENABLE_DEBUG` | Debug logging to Serial (`DBUGF` / `DBUGLN` macros) | None | **Enabled** |

### Value flags

| Flag | Description | Default |
|------|-------------|---------|
| `SUNSPEC_TCP_PORT=<n>` | Modbus TCP listen port | `8502` |
| `SD_SOLARK_LOG_INTERVAL_MS=<n>` | CSV log interval in milliseconds | `60000` |
| `ETH_STATIC_IP="a,b,c,d"` | Static Ethernet address | `192,168,1,50` |
| `ETH_STATIC_GATEWAY="a,b,c,d"` | Default gateway | `192,168,1,1` |
| `ETH_STATIC_SUBNET="a,b,c,d"` | Subnet mask | `255,255,255,0` |
| `ETH_STATIC_DNS="a,b,c,d"` | DNS server | same as `ETH_STATIC_GATEWAY` |
| `ETH_MAC="0x..,..."` | Six-byte MAC, unique per device | `0xDE,0xAD,0xBE,0xEF,0xFE,0xED` |
| `ETH_DHCP_TIMEOUT_MS=<n>` | DHCP discovery timeout (DHCP builds only) | `15000` |
| `ETH_LINK_TIMEOUT_MS=<n>` | How long to wait for PHY auto-negotiation at boot | `4000` |

## Ethernet Addressing

`ENABLE_ETHERNET` uses DHCP by default. Add `ENABLE_ETH_STATIC_IP` for a fixed
address — useful when the SunSpec TCP client needs a stable endpoint to poll, or
when the site has no DHCP server.

```ini
build_flags =
    -DBOARD_VER_V3
    -DENABLE_ETHERNET
    -DENABLE_ETH_STATIC_IP
    -DETH_STATIC_IP="192,168,1,50"
    -DETH_STATIC_GATEWAY="192,168,1,1"
    -DETH_STATIC_SUBNET="255,255,255,0"
    ;-DETH_STATIC_DNS="8,8,8,8"      ; defaults to ETH_STATIC_GATEWAY
```

**Quote the octet lists.** Each value is a comma-separated list expanded straight
into an `IPAddress(a,b,c,d)` constructor. The quotes are required so the comma
survives `platformio.ini` flag parsing; PlatformIO strips them before the
compiler sees the flag, leaving `-DETH_STATIC_IP=192,168,1,50`.

Defaults for all of these live in `include/core/config.h` under `#ifndef` guards,
so you can either override them from `platformio.ini` or edit them there directly.

### Behavioural differences

| | DHCP (default) | `ENABLE_ETH_STATIC_IP` |
|---|---|---|
| Boot time | Blocks until lease or `ETH_DHCP_TIMEOUT_MS` (default 15 s) | Returns immediately |
| Failure detection | `Ethernet.begin()` returns 0 | Return type is `void`; checked via `hardwareStatus()` / `linkStatus()` |
| Unplugged cable | Setup fails | Warns, but the address is still configured |
| Link wait | `ETH_LINK_TIMEOUT_MS` before discovery | `ETH_LINK_TIMEOUT_MS` before status is reported |
| `loop_ethernet()` | Renews the lease via `Ethernet.maintain()` | Compiles to an empty function |

### Per-device MAC

The firmware ships a single locally-administered MAC
(`0xDE,0xAD,0xBE,0xEF,0xFE,0xED`). Two boards with the same MAC on one segment
will conflict — with static addressing you are likely deploying more than one, so
give each unit its own:

```ini
-DETH_MAC="0xDE,0xAD,0xBE,0xEF,0xFE,0x01"
```

## Board Configuration

Exactly one board revision macro must be set — `include/core/pins.h` raises an
`#error` if none is defined:

```ini
; Uncomment exactly ONE of these:
-DBOARD_VER_V3    ; NESL EMS Controller PCBA 865B (default)
;-DBOARD_VER_V1   ; Legacy hand-soldered 2025 prototype
;-DBOARD_VER_V2   ; Legacy 2025 board with swapped RS-485 module pins
```

Every board block defines the same canonical macro names (`RS485_1_RX`,
`RS485_1_TX`, `RS485_2_RX`, `RS485_2_TX`, `DISPLAY_*`, `RELAY_1_PIN`), so
firmware sources compile unchanged across revisions. `SD_*` and `ETH_*` pins
exist only on `BOARD_VER_V3`.

## Enforced Dependencies

These combinations fail at compile time rather than misbehaving at runtime:

| Invalid combination | Error raised in |
|---|---|
| `ENABLE_WIFI` **and** `ENABLE_ETHERNET` together | `src/core/main.cpp` |
| `ENABLE_MQTT` without a network interface | `src/core/main.cpp` |
| `ENABLE_MODBUS_TCP_SERVER` without a network interface | `src/core/main.cpp` |
| `ENABLE_MODBUS_TCP_SERVER` without `ENABLE_MODBUS_MASTER` | `include/metering/modbus_server.h` |
| `ENABLE_SD_CARD` without `BOARD_VER_V3` | `include/hw/sd_card.h` |
| `ENABLE_ETHERNET` without `BOARD_VER_V3` | `src/comms/ethernet.cpp` |
| `ENABLE_SD_SOLARK_LOG` without `ENABLE_SD_CARD` / `ENABLE_MODBUS_MASTER` | `include/hw/sd_logger.h` |
| No `BOARD_VER_*` set | `include/core/pins.h` |

## WiFi Credentials

WiFi credentials are **not** stored in tracked source. Before building with
`-DENABLE_WIFI`:

```bash
cp include/secrets_example.h include/secrets.h
# edit include/secrets.h and set WIFI_SSID / WIFI_PW
```

`include/secrets.h` is gitignored and must never be committed.

## Examples

### Default gateway (WiFi + Sol-Ark RTU + SunSpec TCP)
```ini
build_flags =
    -DUSE_SOFTWARE_SERIAL
    -DBOARD_VER_V3
    -DENABLE_WIFI
    -DENABLE_MODBUS_MASTER
    -DENABLE_MODBUS_TCP_SERVER
    -DENABLE_DEBUG
    -UARDUINO_USB_CDC_ON_BOOT
```

### Wired gateway (Ethernet uplink, no WiFi)
```ini
build_flags =
    -DUSE_SOFTWARE_SERIAL
    -DBOARD_VER_V3
    -DENABLE_ETHERNET
    -DENABLE_MODBUS_MASTER
    -DENABLE_MODBUS_TCP_SERVER
    -DENABLE_DEBUG
    -UARDUINO_USB_CDC_ON_BOOT
```

### Full build (display + MQTT + SD logging + relays)
```ini
build_flags =
    -DUSE_SOFTWARE_SERIAL
    -DBOARD_VER_V3
    -DENABLE_OLED_DISPLAY
    -DENABLE_WIFI
    -DENABLE_MQTT
    -DENABLE_MODBUS_MASTER
    -DENABLE_MODBUS_TCP_SERVER
    -DENABLE_RELAYS
    -DENABLE_SD_CARD
    -DENABLE_SD_SOLARK_LOG
    -DENABLE_DEBUG
    -UARDUINO_USB_CDC_ON_BOOT
```

### Serial-only diagnostics (poll the inverter, no network)
```ini
build_flags =
    -DUSE_SOFTWARE_SERIAL
    -DBOARD_VER_V3
    -DENABLE_MODBUS_MASTER
    -DENABLE_DEBUG
    -UARDUINO_USB_CDC_ON_BOOT
```

## Verifying Your Configuration

After building, check the serial output at boot:

```
INFO - Booting
wifi: <ssid>: 192.168.1.42
SETUP: MODBUS: SolArk #1: address:1
INFO - Modbus Server: SunSpec models initialized
INFO - Modbus Server: SunSpec-compliant TCP server on port 8502
INFO - Debug logging enabled
```

Features that are disabled produce no output and consume no resources.

## Troubleshooting

### Feature not working after enabling
1. Rebuild the project: `pio run`
2. Check for compilation errors in the build output
3. Verify the feature's hardware dependencies are connected
4. Confirm the board revision matches your hardware

### `secrets.h: No such file or directory`
You enabled `ENABLE_WIFI` without creating `include/secrets.h`. See
[WiFi Credentials](#wifi-credentials).

### Binary too large
1. Disable features you don't need
2. Disable `ENABLE_DEBUG` — this also removes the debug format strings and the
   Sol-Ark status dump helpers from the binary entirely
3. Disable `ENABLE_OLED_DISPLAY` if not using a display

### SPI bus conflicts
On `BOARD_VER_V3` the OLED, SD card, and W5500 Ethernet share CLK/MOSI/MISO and
differ only by chip select. Each driver re-calls `SPI.begin()` with its own MISO
requirement; this is safe because write-only peripherals on the bus are
unaffected by adding a MISO line.

## Runtime Behavior

### Disabled features
- No memory is allocated
- No CPU cycles are consumed
- Setup and loop functions are not called
- Source files compile to nothing (each `.cpp` is wrapped in its own `#ifdef`)

### Enabled features
- Initialisation runs in the order listed in `src/core/main.cpp`
- Resources (SPI, GPIO, UART) are claimed at setup
- Sub-loops run at their own rate timers (`core/config.h`)

## Advanced: Conditional Code in Your Projects

You can use the same feature flags in your own code:

```cpp
#ifdef ENABLE_WIFI
  setup_wifi();
#endif

// Prefer the debug macros over raw Serial.print — they compile away entirely
// when ENABLE_DEBUG is off.
#include <core/debug.h>
DBUGF("SOC=%.0f%%", solark.getBatterySOC());
```

## Feature Interdependencies

```
Basic System
    ├── Buttons (always enabled)
    ├── Config / Device ID (always enabled)
    └── Data Model (always enabled)
        ├── OLED Display
        │   └── Console UI
        ├── Network (exactly one)
        │   ├── WiFi
        │   └── Ethernet (W5500, V3 only)
        │       ├── MQTT
        │       └── Modbus TCP Server (SunSpec)
        ├── Modbus RTU Master (RS485_1, Sol-Ark)
        │   └── SunSpec mapper
        ├── Relays (onboard SSR)
        └── SD Card (V3 only)
            └── Sol-Ark CSV logger
```

## Troubleshooting Checklist

- [ ] `platformio.ini` has correct `build_flags`
- [ ] Board revision (`BOARD_VER_*`) is set
- [ ] `include/secrets.h` exists if `ENABLE_WIFI` is set
- [ ] Project has been rebuilt (`pio run`)
- [ ] Serial output shows expected boot messages
- [ ] Hardware is properly connected
- [ ] Feature dependencies are met (see the enforced-dependency table above)
- [ ] SPI pins are not conflicting (check `include/core/pins.h`)

## Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-08-21 | Initial feature flag system for the SunSpec gateway |
