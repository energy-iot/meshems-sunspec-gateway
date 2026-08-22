# Changelog

## [Unreleased] - 2026-08-21

### Added

#### Feature-flag build system (`platformio.ini`, `docs/FEATURE_FLAGS.md`)
- Every optional subsystem is now guarded by an `#ifdef ENABLE_*` flag set in `platformio.ini`: `ENABLE_OLED_DISPLAY`, `ENABLE_WIFI`, `ENABLE_ETHERNET`, `ENABLE_MQTT`, `ENABLE_MODBUS_MASTER`, `ENABLE_MODBUS_TCP_SERVER`, `ENABLE_RELAYS`, `ENABLE_SD_CARD`, `ENABLE_SD_SOLARK_LOG`, `ENABLE_SD_CARD_DEBUG`, `ENABLE_DEBUG`
- Invalid flag combinations now fail at compile time with a descriptive `#error` instead of misbehaving at runtime — see the enforced-dependency table in `docs/FEATURE_FLAGS.md`
- Value flags `SUNSPEC_TCP_PORT` (default 8502) and `SD_SOLARK_LOG_INTERVAL_MS` (default 60 s)
- `docs/FEATURE_FLAGS.md` — full flag reference, example configurations, and troubleshooting

#### WiFi driver and credential handling (`include/comms/wifi.h`, `src/comms/wifi.cpp`, `include/secrets_example.h`)
- WiFi station bring-up extracted from `modbus_server.cpp` into its own `WiFiMulti`-based module with a bounded retry count, so the gateway still boots and serves RS-485 diagnostics when the AP is unreachable
- `wifi_client_connected()` and `get_wifi_ip()` status helpers available to other modules
- Credentials moved out of tracked source into a gitignored `include/secrets.h`; `include/secrets_example.h` is the committed template

#### Static Ethernet addressing (`src/comms/ethernet.cpp`, `include/core/config.h`)
- New `ENABLE_ETH_STATIC_IP` flag configures a fixed address via `Ethernet.begin(mac, ip, dns, gateway, subnet)` instead of DHCP; addresses come from `ETH_STATIC_IP` / `ETH_STATIC_GATEWAY` / `ETH_STATIC_SUBNET` / `ETH_STATIC_DNS`, all `#ifndef`-guarded in `core/config.h` and overridable from `platformio.ini` as quoted comma-separated octet lists
- The static `Ethernet.begin()` overload returns `void`, so bring-up is validated with `hardwareStatus()` and `linkStatus()`; an unplugged cable is reported as a warning since the address is configured regardless of carrier
- `loop_ethernet()` compiles to an empty function in static builds — there is no DHCP lease to renew
- DHCP path now reports whether failure was a missing W5500 or a dead link, and its discovery timeout is capped by the new `ETH_DHCP_TIMEOUT_MS` (default 15 s, down from the library's 60 s)
- MAC address moved to an overridable `ETH_MAC` macro so units sharing a segment can be given unique addresses

#### W5500 SPI Ethernet driver (`include/comms/ethernet.h`, `src/comms/ethernet.cpp`)
- New `ENABLE_ETHERNET` feature flag for W5500 SPI Ethernet via HR961160C (BOARD_VER_V3 only; mutually exclusive with `ENABLE_WIFI`)
- `setup_ethernet()` hardware-resets the W5500, initialises the shared SPI bus with MISO, and acquires an IP via DHCP; `loop_ethernet()` calls `Ethernet.maintain()` to renew leases
- `ethernet_connected()` and `get_eth_ip()` status helpers
- `platformio.ini` — added `arduino-libraries/Ethernet @ ^2.0.0` to `lib_deps`

#### MQTT telemetry publisher (`include/comms/mqtt_client.h`, `src/comms/mqtt_client.cpp`)
- New `ENABLE_MQTT` feature flag publishing Sol-Ark inverter data under the OpenAMI topic convention `<MQTT_TOPIC>/<device_id>/<subtopic>`
- Seven telemetry subtopics, each a self-contained JSON document: `mfr`, `inverter`, `battery`, `grid`, `pv`, `load`, `energy`
- Bandwidth accounting published periodically as `stats/BWPubOut` and `stats/BWCmdIn` so upstream link budgets can be verified in the field
- Southbound command topic `<device_id>/cmd` with a dispatch table; `{"cmd":"report"}` forces an immediate full publish
- Transport is `EthernetClient` when `ENABLE_ETHERNET` is set and `WiFiClient` otherwise
- `platformio.ini` — added `ArduinoJson` and `PubSubClient` to `lib_deps`

#### SD card driver and Sol-Ark CSV logger (`include/hw/sd_card.h`, `src/hw/sd_card.cpp`, `include/hw/sd_logger.h`, `src/hw/sd_logger.cpp`)
- New `ENABLE_SD_CARD` feature flag for the SPI-shared SD card reader (BOARD_VER_V3 only); `SD_CS` wired to GPIO 10
- Optional `ENABLE_SD_CARD_DEBUG` sub-flag prints card type, size, and a full directory listing on boot via `sd_list_dir()`
- New `ENABLE_SD_SOLARK_LOG` flag logs SOC, battery power, grid power, PV total, and load total to `/solark_log.csv`; rows are only appended after the first successful Sol-Ark poll to prevent spurious all-zero entries

#### Device identity and runtime configuration (`include/core/config.h`, `src/core/config.cpp`)
- `generateDeviceID()` derives a stable `SunSpecGW_<xxxxxx>` identifier from the last three octets of the WiFi station MAC, deliberately avoiding the full EFUSE MAC so the vendor OUI is not published
- Poll and publish rates (`SolArkPoll_rate`, `MQTTPublish_rootrate`, `MQTTPoll_rate`) are mutable globals so a future config server can retune them; `MQTTPublish_rootrate` defaults to 30 s in debug builds and 5 min otherwise
- Sol-Ark node address, baud rate, MQTT broker settings, and `SUNSPEC_TCP_PORT` centralised here instead of scattered `#define`s

#### Debug logging macros (`include/core/debug.h`)
- `DBUGF` / `DBUGLN` / `DBUG` compile to nothing when `ENABLE_DEBUG` is off, so debug format strings are dropped from the binary rather than merely skipped

#### Build tooling (`.clangd`, `scripts/pick_serial_port.py`, `utilities/requirements.txt`)
- `scripts/pick_serial_port.py` pre-script selects a real USB UART for upload, skipping macOS Bluetooth and debug-console `/dev/cu.*` entries
- `.clangd` points clangd at the PlatformIO compilation database for working IntelliSense (`pio run -t compiledb`)
- `utilities/requirements.txt` pins the host-side Python dependencies for the SunSpec test scripts

#### Agent instructions (`docs/AGENTS.md`)
- Code style, commenting, README, and changelog conventions shared with the OpenAMI metering firmware

### Changed

#### Source tree reorganised into subsystem folders (`src/`, `include/`)
- `include/` split into `core/` (config, console, data_model, debug, pins), `hw/` (buttons, display, relay, sd_card, sd_logger), `comms/` (wifi, ethernet, mqtt_client), and `metering/` (modbus, modbus_master, modbus_server, modbus_solark, sunspec_models)
- `src/` split along the same lines; all `#include` directives updated to the new subsystem-qualified paths
- `examples/*.py` moved to `utilities/` to match the metering firmware layout

#### Modbus module naming aligned with the metering firmware (`metering/modbus_master.*`, `metering/modbus_server.*`)
- `modbus_client.{h,cpp}` renamed to `modbus_master.{h,cpp}` — this side drives the RS-485 bus, so "master" is accurate and no longer collides with the metering firmware's opposite use of "client"
- `setup_modbus_client_interface()` / `loop_modbus_client()` renamed to `setup_modbus_master()` / `loop_modbus_master()`; `setup_modbus_clients()` and `setup_solark()` are now internal statics
- The `solark` instance is declared once in `modbus_master.h` instead of being re-`extern`ed ad hoc in `sunspec_mapper.cpp`
- Added `get_solark_success_count()` so consumers can tell a populated register cache from a cold one

#### Board revision support (`include/core/pins.h`)
- Added the `BOARD_VER_V3` (NESL EMS Controller PCBA 865B) pin map: SD card, W5500 Ethernet, explicit OLED SPI pins, and the 865B RS-485 routing; `platformio.ini` now defaults to `-DBOARD_VER_V3`
- All three board blocks define the same canonical RS-485 macro names (`RS485_1_RX`/`RS485_1_TX`/`RS485_2_RX`/`RS485_2_TX`), replacing the previous `RS485_RX_1` spelling, so firmware sources compile unchanged across revisions
- An `#else` branch raises `#error` when no board revision is defined, instead of failing later with undefined pin macros
- Removed the unused MCP2515 CAN pin block — the gateway has no CAN subsystem

#### Modbus TCP server (`src/metering/modbus_server.cpp`, `include/metering/modbus_server.h`)
- No longer performs WiFi bring-up; the network interface is brought up by `main.cpp` before `setup_modbus_server()` runs. This removes an unbounded `while (WiFi.status() != WL_CONNECTED)` spin that could hang the boot indefinitely
- Listen port is now `SUNSPEC_TCP_PORT` from `core/config.h` rather than a literal 8502
- `ModbusIP` instance and the initialisation flag are now file-static

#### Sol-Ark poll loop (`src/metering/modbus_master.cpp`)
- Poll interval now reads the runtime-tunable `SolArkPoll_rate` instead of a hard-coded `SOLARK_POLL_INTERVAL`
- The six `print*Status()` dump helpers are `static` and compiled only under `ENABLE_DEBUG`; the per-poll status dump and the "Poll SolArk inverter" line no longer spam the serial log in release builds

#### Boot and main loop (`src/core/main.cpp`)
- Rewritten around the feature-flag table documented in the file header; each subsystem is set up and serviced only when its flag is defined
- Boot order is now Serial → device ID → display splash → network → MQTT → Modbus master → SunSpec TCP server → buttons → relays → SD card → SD logger
- MQTT poll and publish run on independent rate timers with connection maintenance in front of both
- The OLED "IP:<addr>:<port>" line now sources the address from whichever network interface is compiled in

#### Build configuration (`platformio.ini`)
- Removed `-D__AVR__`, which falsely claimed an AVR target on an ESP32-S3 and could select wrong-architecture code paths in libraries
- Replaced `-DARDUINO_USB_CDC_ON_BOOT=1` with `-UARDUINO_USB_CDC_ON_BOOT` so the firmware does not block on terminal access at startup when running from battery
- Added `monitor_speed = 115200`, `upload_speed = 921600`, `lib_ldf_mode = deep`, and the `pick_serial_port.py` pre-script
- Every flag and dependency now carries an inline comment explaining what enables it

#### Ignore rules (`.gitignore`)
- Expanded to the metering firmware's ruleset (build artifacts, IDE files, Python caches, toolchain output), plus an explicit `include/secrets.h` entry and a `!.clangd` negation so the clangd config stays tracked

### Fixed

#### Ethernet reported "Not Connected" while the IP was working (`src/comms/ethernet.cpp`, `src/core/main.cpp`)
- `get_eth_ip()` gated on `ethernet_connected()`, which requires `linkStatus() == LinkON`. Static addressing returns from `Ethernet.begin()` in ~110 ms with no DHCP exchange, so the OLED line rendered before PHY auto-negotiation (1–3 s) completed and showed a false "Not Connected" for a link that was in fact fine. DHCP had masked this by taking seconds
- `setup_ethernet()` now waits for carrier via `wait_for_link()` (`ETH_LINK_TIMEOUT_MS`, default 4 s) before any link-dependent reporting; on the DHCP path this also avoids spending the whole discovery timeout on a dead link
- `get_eth_ip()` now reports the configured address independently of carrier state — with a static address the IP is valid whether or not the cable is currently in — and returns `"No IP"` only when genuinely unconfigured. `ethernet_connected()` keeps the link check for callers that need liveness
- `src/core/main.cpp` — SPI is now initialised with MISO when `ENABLE_ETHERNET` or `ENABLE_SD_CARD` is set. `SPIClass::begin()` returns early once the bus is up, so the later `SPI.begin()` inside `setup_ethernet()` could never add MISO; this previously worked only because `spiAttachMISO(spi, -1)` happens to fall back to GPIO 13 on the ESP32-S3, which coincidentally equals `ETH_MISO`. Moving MISO to any other pin would have silently broken W5500 reads
- Corrected the stale comment in `setup_ethernet()` that claimed its `SPI.begin()` call adds MISO to an already-initialised bus

#### Garbled OLED console text (`src/core/console.cpp`)
- `Console::redrawConsole()` computed its line offset as `i+1 * 10`, which C evaluates as `i + 10` — all five console lines were drawn at y = 10..14 and overlapped into unreadable text with the 10 px font
- Now `(i + 1) * FIRST_LINE_Y_OFFSET`, giving y = 10, 20, 30, 40, 50 and matching what `redrawConsoleFrame()` (the UI-loop render path) already produced, so the boot-time and steady-state renders agree
- The unused `FIRST_LINE_Y_OFFSET` constant in `include/core/console.h` is now actually the source of the spacing
- Pre-existing defect, also present in the OpenAMI metering firmware's `src/core/console.cpp` — worth porting the same fix there

### Security

#### WiFi credentials removed from tracked source (`include/metering/modbus_server.h`)
- `WIFI_SSID` and `WIFI_PASSWORD` were committed in plaintext in the header; they now live in the gitignored `include/secrets.h`
- **Note:** the previously committed credentials remain in git history. Rotate the network password if that history is or becomes public.
