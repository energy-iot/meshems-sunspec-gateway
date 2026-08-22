#pragma once

// Onboard solid-state relay on RELAY_1_PIN. Guarded by ENABLE_RELAYS.

void setup_relays();
void toggle_relay_1();
