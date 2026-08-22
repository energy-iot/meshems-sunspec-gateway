/**
 * @file ethernet.cpp
 * @brief W5500 SPI Ethernet uplink (HR961160C magjack) — BOARD_VER_V3 only.
 *
 * Shares the SPI bus with the OLED and SD card; only the chip-select pins differ.
 */

#ifdef ENABLE_ETHERNET
#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <core/config.h>
#include <core/pins.h>
#include <comms/ethernet.h>

#ifndef BOARD_VER_V3
  #error "ENABLE_ETHERNET requires BOARD_VER_V3 — ETH_ pins are only defined for that board revision."
#endif

// Locally-administered MAC — override ETH_MAC in core/config.h (or from
// platformio.ini) per device if multiple boards share the same network segment.
static byte eth_mac[] = { ETH_MAC };

bool ethernet_connected() {
    return Ethernet.linkStatus() == LinkON && Ethernet.localIP() != IPAddress(0, 0, 0, 0);
}

// Reports the configured address, deliberately independent of carrier state.
// With a static address the IP is valid whether or not the cable happens to be
// in right now, and callers that need liveness should ask ethernet_connected().
String get_eth_ip() {
    IPAddress ip = Ethernet.localIP();
    if (ip == IPAddress(0, 0, 0, 0)) {
        return "No IP";
    }
    return ip.toString();
}

// Blocks until the PHY reports carrier or the timeout expires.
static bool wait_for_link(unsigned long timeout_ms) {
    unsigned long start = millis();
    while (millis() - start < timeout_ms) {
        if (Ethernet.linkStatus() == LinkON) {
            return true;
        }
        delay(50);
    }
    return false;
}

bool setup_ethernet() {
    // Hardware reset the W5500
    pinMode(ETH_RST, OUTPUT);
    digitalWrite(ETH_RST, LOW);
    delay(10);
    digitalWrite(ETH_RST, HIGH);
    delay(100);

    // NOTE: SPIClass::begin() returns immediately if the bus is already up, so
    // this call is a no-op whenever the display initialised SPI first. It cannot
    // add MISO after the fact — main.cpp must supply MISO on the first call (it
    // does when ENABLE_ETHERNET is set). This is kept only for the display-less
    // build, where this really is the first call.
    SPI.begin(ETH_CLK, ETH_MISO, ETH_MOSI, -1);

    Ethernet.init(ETH_CS);

    // Wait for auto-negotiation before doing anything that depends on carrier.
    // For DHCP this also avoids burning the whole discovery timeout on a dead
    // link; for static it stops the first status query reading a false "down".
    bool link_up = wait_for_link(ETH_LINK_TIMEOUT_MS);
    if (!link_up) {
        Serial.printf("ethernet: no link after %lu ms\n", (unsigned long)ETH_LINK_TIMEOUT_MS);
    }

#ifdef ENABLE_ETH_STATIC_IP
    // Static addressing: no DHCP round-trip, so the gateway is reachable at a
    // known address the moment the link comes up.
    IPAddress ip(ETH_STATIC_IP);
    IPAddress dns(ETH_STATIC_DNS);
    IPAddress gateway(ETH_STATIC_GATEWAY);
    IPAddress subnet(ETH_STATIC_SUBNET);

    // This overload returns void, unlike the DHCP one — success has to be
    // judged from the controller and link state instead of a return code.
    Ethernet.begin(eth_mac, ip, dns, gateway, subnet);

    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
        Serial.println("ethernet: W5500 not found — check SPI wiring and ETH_CS");
        return false;
    }

    Serial.printf("ethernet: static %s\n", Ethernet.localIP().toString().c_str());

    // The address is configured regardless of carrier, so an absent link is a
    // warning rather than a hard failure — it may come up moments later.
    if (!link_up) {
        Serial.println("ethernet: WARN link down (cable unplugged?)");
    }
    return true;
#else
    Serial.println("ethernet: DHCP starting...");
    if (Ethernet.begin(eth_mac, ETH_DHCP_TIMEOUT_MS) == 0) {
        Serial.println("ethernet: DHCP failed");
        if (Ethernet.hardwareStatus() == EthernetNoHardware) {
            Serial.println("ethernet: W5500 not found — check SPI wiring and ETH_CS");
        } else if (!link_up) {
            Serial.println("ethernet: link down (cable unplugged?)");
        }
        return false;
    }
    Serial.printf("ethernet: DHCP %s\n", Ethernet.localIP().toString().c_str());
    return true;
#endif
}

// Renews the DHCP lease; must be called from the main loop. A static address
// has no lease to renew, so this compiles to an empty function in that build.
void loop_ethernet() {
#ifndef ENABLE_ETH_STATIC_IP
    Ethernet.maintain();
#endif
}

#endif // ENABLE_ETHERNET
