/*
 * MagNiche - Pro Micro (ATmega32U4 @ 16MHz) Firmware
 *
 * Platform: PlatformIO (atmelavr / Arduino framework)
 * Target: SparkFun Pro Micro 5V / 16MHz
 */

#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    // On ATmega32U4, Serial is native USB CDC; wait up to 3 seconds for host connection
    uint32_t start = millis();
    while (!Serial && (millis() - start < 3000)) {
        delay(10);
    }

    Serial.println(F("========================================"));
    Serial.println(F("         MagNiche Firmware v0.1         "));
    Serial.println(F("   ATmega32U4 Pro Micro 5V / 16MHz     "));
    Serial.println(F("========================================"));
    Serial.println(F("Ready."));
}

void loop() {
    // Main loop logic
    delay(100);
}
