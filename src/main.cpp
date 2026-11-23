/*
 * MagNiche - Dual Niche Magnetic Signal Generator Firmware
 * Board: SparkFun Pro Micro 5V / 16MHz (ATmega32U4)
 *
 * Applications:
 *   1. Magnetostrictive Nickel Wire Pulse Excitation (Power MOSFET Gate Driver)
 *      - RED   : Adjust Pulse Width (1 us - 2000 us)
 *      - GREEN : Adjust Repetition Frequency (1 Hz - 1000 Hz)
 *
 *   2. Billingsley FluxGate Magnetometer Driver
 *      - BLUE   : Drive DRV8833 H-Bridge Excitation Coil (5 kHz - 20 kHz push-pull)
 *      - YELLOW : Adjust DC Feedback Winding PWM (0% - 100% duty cycle)
 *
 *   3. Standby / Safe Bench Mode
 *      - PURPLE : All outputs muted/grounded for safe probing and resting coils
 */

#include <Arduino.h>
#include "config.h"
#include "magnetostriction.h"
#include "fluxgate.h"
#include "ui_controller.h"
#include "serial_cli.h"

void setup() {
    // 1. Initialize hardware outputs immediately upon power-up / reset
    initMagnetostriction();
    initFluxGate();
    initUI(); // Starts immediately in FluxGate Excitation mode @ 160 Hz (LED: BLUE)

    // 2. Initialize Serial USB CDC non-blockingly for development CLI
    Serial.begin(SERIAL_BAUD_RATE);
    initSerialCLI();

    if (Serial) {
        printWelcomeBanner();
    }
}

void loop() {
    // 1. Service Magnetostrictive pulse generator (high-precision timing)
    updateMagnetostriction();

    // 2. Service User Interface (Button debouncing, Potentiometer, NeoPixel, Serial telemetry)
    updateUI();

    // 3. Service Serial Development CLI (Interactive commands: flux, flux_f, etc.)
    updateSerialCLI();
}

