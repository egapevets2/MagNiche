#include "serial_cli.h"
#include "config.h"
#include "fluxgate.h"
#include "magnetostriction.h"
#include "ui_controller.h"
#include <ctype.h>

#define CLI_BUFFER_SIZE 64

static char s_rx_buffer[CLI_BUFFER_SIZE];
static uint8_t s_rx_index = 0;

static float parseFreqWithUnit(const char* str) {
    if (!str || *str == '\0') return -1.0f;

    char* endptr = nullptr;
    float val = strtod(str, &endptr);
    if (endptr == str) return -1.0f; // No number parsed

    // Check for 'k' or 'K' suffix (e.g. 10k -> 10000)
    while (*endptr == ' ') endptr++;
    if (*endptr == 'k' || *endptr == 'K') {
        val *= 1000.0f;
    }
    return val;
}

static void printHelp() {
    Serial.println(F("\n=== MagNiche Serial Command Interface ==="));
    Serial.println(F("FluxGate Commands:"));
    Serial.println(F("  flux            - Set up and activate FluxGate driving mode (LED: BLUE)"));
    Serial.println(F("  flux_f <val>    - Set excitation frequency in Hz (10 to 20000, e.g. '10', '160' or '12.5k')"));
    Serial.println(F("  flux_fb <val>   - Set bipolar feedback PWM (-100.0 CCW .. 0 Float .. +100.0 CW %)"));
    Serial.println(F("\nMagnetostriction Commands:"));
    Serial.println(F("  mag             - Set up and activate Magnetostriction mode (LED: RED)"));
    Serial.println(F("  mag_pw <val>    - Set pulse width in us (1.0 to 2000.0)"));
    Serial.println(F("  mag_f <val>     - Set repetition frequency in Hz (1.0 to 1000.0)"));
    Serial.println(F("\nGeneral / Bench Commands:"));
    Serial.println(F("  standby         - Disable all generator outputs (LED: PURPLE)"));
    Serial.println(F("  status          - Display full instrument status"));
    Serial.println(F("  help / ?        - Show this command reference"));
    Serial.println(F("==========================================\n"));
}

static void handleCommand(char* cmd_line) {
    // 1. Trim leading whitespace
    while (*cmd_line && isspace((unsigned char)*cmd_line)) cmd_line++;
    if (*cmd_line == '\0') return; // Empty line

    // 2. Separate command token and argument token
    char* arg = nullptr;
    char* p = cmd_line;
    while (*p) {
        if (isspace((unsigned char)*p) || *p == '=' || *p == ':') {
            *p = '\0';
            arg = p + 1;
            while (*arg && (isspace((unsigned char)*arg) || *arg == '=' || *arg == ':')) {
                arg++;
            }
            break;
        }
        p++;
    }

    // Convert command token to lowercase
    for (char* c = cmd_line; *c; c++) {
        *c = (char)tolower((unsigned char)*c);
    }

    // --- COMMAND DISPATCH ---

    // 1. 'flux' -> Sets up in flux-gate driving mode
    if (strcmp(cmd_line, "flux") == 0) {
        setMode(MODE_FLUX_EXCITATION);
        Serial.print(F("[OK] FluxGate driving mode ACTIVE (DRV8833 Pins D9/D10, nSLEEP Pin D4=HIGH)\n"));
        Serial.print(F("     Excitation Freq: "));
        Serial.print(getFluxExcitationFreq(), 1);
        Serial.print(F(" Hz | Feedback PWM: "));
        Serial.print(getFluxFeedbackPWM(), 1);
        Serial.println(F(" %"));
        return;
    }

    // 2. 'flux_f' -> Sets flux gate driving frequency
    if (strcmp(cmd_line, "flux_f") == 0) {
        if (!arg || *arg == '\0') {
            Serial.print(F("FluxGate Excitation Frequency: "));
            Serial.print(getFluxExcitationFreq(), 2);
            Serial.print(F(" Hz (Range: "));
            Serial.print((int)FLUX_FREQ_MIN_HZ);
            Serial.print(F(" to "));
            Serial.print((int)FLUX_FREQ_MAX_HZ);
            Serial.println(F(" Hz)"));
            Serial.println(F("Usage: flux_f <frequency_in_hz> (e.g. flux_f 12500 or flux_f 12.5k)"));
            return;
        }

        float freq = parseFreqWithUnit(arg);
        if (freq < FLUX_FREQ_MIN_HZ || freq > FLUX_FREQ_MAX_HZ) {
            Serial.print(F("[ERROR] Frequency out of bounds: "));
            Serial.print(freq);
            Serial.print(F(" Hz. Valid range is "));
            Serial.print((int)FLUX_FREQ_MIN_HZ);
            Serial.print(F(" Hz to "));
            Serial.print((int)FLUX_FREQ_MAX_HZ);
            Serial.println(F(" Hz."));
            return;
        }

        setFluxExcitationFreq(freq);
        resetPotPickup();

        uint16_t prescaler = 1;
        uint16_t top = 0;
        computeFluxTimer1Params(freq, nullptr, &prescaler, &top);
        float actual_freq = 16000000.0f / ((float)prescaler * 4.0f * (float)(top + 1));
        float period_us = 1000000.0f / actual_freq;

        Serial.print(F("[OK] FluxGate excitation frequency set to: "));
        Serial.print(actual_freq, 2);
        Serial.print(F(" Hz (Period: "));
        Serial.print(period_us, 2);
        Serial.println(F(" us)"));
        Serial.print(F("     Synchronous 2F Demod (Pin D2): "));
        Serial.print(actual_freq * 2.0f, 2);
        Serial.print(F(" Hz (Period: "));
        Serial.print(period_us / 2.0f, 2);
        Serial.println(F(" us)"));

        if (!isFluxGateActive()) {
            Serial.println(F("     [Note: FluxGate is currently in standby/mag mode. Type 'flux' to enable output.]"));
        }
        return;
    }

    // 3. 'flux_fb' or 'flux_pwm' -> Set bipolar feedback PWM (-100% CCW .. 0% Float .. +100% CW)
    if (strcmp(cmd_line, "flux_fb") == 0 || strcmp(cmd_line, "flux_pwm") == 0) {
        if (!arg || *arg == '\0') {
            Serial.print(F("FluxGate Bipolar Feedback PWM: "));
            Serial.print(getFluxFeedbackPWM(), 2);
            Serial.println(F(" % (Range: -100.0 CCW to +100.0 CW; 0.0 = Floating)"));
            return;
        }
        float duty = atof(arg);
        if (duty < FLUX_PWM_MIN_DUTY || duty > FLUX_PWM_MAX_DUTY) {
            Serial.println(F("[ERROR] Duty cycle out of bounds (-100.0 to +100.0%)"));
            return;
        }
        setFluxFeedbackPWM(duty);
        resetPotPickup();
        Serial.print(F("[OK] FluxGate bipolar feedback set to: "));
        if (fabs(duty) < 0.05f) {
            Serial.println(F("0.00 % (D6=LOW, D3=LOW -> OUT3/OUT4 FLOAT)"));
        } else if (duty > 0.0f) {
            Serial.print(F("+")); Serial.print(duty, 2);
            Serial.println(F(" % CW (D6=HIGH, D3=PWM -> OUT3=PWM, OUT4=GND)"));
        } else {
            Serial.print(duty, 2);
            Serial.println(F(" % CCW (D6=PWM, D3=HIGH -> OUT3=GND, OUT4=PWM)"));
        }
        return;
    }

    // 4. 'mag' -> Set up in magnetostrictive mode
    if (strcmp(cmd_line, "mag") == 0) {
        setMode(MODE_MAG_PULSEWIDTH);
        Serial.println(F("[OK] Magnetostrictive mode ACTIVE (MOSFET Gate Pin D5)"));
        return;
    }

    // 5. 'mag_pw' -> Set pulse width
    if (strcmp(cmd_line, "mag_pw") == 0) {
        if (!arg || *arg == '\0') {
            Serial.print(F("Magnetostrictive Pulse Width: "));
            Serial.print(getMagPulseWidth(), 2);
            Serial.println(F(" us"));
            return;
        }
        float pw = atof(arg);
        if (pw < MAG_PW_MIN_US || pw > MAG_PW_MAX_US) {
            Serial.println(F("[ERROR] Pulse width out of bounds (1.0 to 2000.0 us)"));
            return;
        }
        setMagPulseWidth(pw);
        resetPotPickup();
        Serial.print(F("[OK] Magnetostrictive pulse width set to: "));
        Serial.print(pw, 2);
        Serial.println(F(" us"));
        return;
    }

    // 6. 'mag_f' -> Set repetition frequency
    if (strcmp(cmd_line, "mag_f") == 0) {
        if (!arg || *arg == '\0') {
            Serial.print(F("Magnetostrictive Repetition Frequency: "));
            Serial.print(getMagFrequency(), 2);
            Serial.println(F(" Hz"));
            return;
        }
        float fq = parseFreqWithUnit(arg);
        if (fq < MAG_FREQ_MIN_HZ || fq > MAG_FREQ_MAX_HZ) {
            Serial.println(F("[ERROR] Repetition frequency out of bounds (1.0 to 1000.0 Hz)"));
            return;
        }
        setMagFrequency(fq);
        resetPotPickup();
        Serial.print(F("[OK] Magnetostrictive repetition frequency set to: "));
        Serial.print(fq, 2);
        Serial.println(F(" Hz"));
        return;
    }

    // 7. 'standby' or 'stop'
    if (strcmp(cmd_line, "standby") == 0 || strcmp(cmd_line, "stop") == 0) {
        setMode(MODE_STANDBY);
        Serial.println(F("[OK] STANDBY: All generator outputs disabled."));
        return;
    }

    // 8. 'status'
    if (strcmp(cmd_line, "status") == 0) {
        printSystemStatus();
        return;
    }

    // 9. 'help' or '?'
    if (strcmp(cmd_line, "help") == 0 || strcmp(cmd_line, "?") == 0) {
        printHelp();
        return;
    }

    // Unknown command
    Serial.print(F("[ERROR] Unknown command: '"));
    Serial.print(cmd_line);
    Serial.println(F("'. Type 'help' for command list."));
}

static bool s_was_serial_connected = false;
static uint32_t s_last_enter_ms = 0;

void printWelcomeBanner() {
    Serial.println(F("\n============================================================"));
    Serial.println(F("       MagNiche - Dual Magnetic Signal Generator v1.0       "));
    Serial.println(F("       ATmega32U4 Pro Micro 5V / 16MHz (PlatformIO)        "));
    Serial.println(F("============================================================"));
    Serial.println(F("HARDWARE PIN ASSIGNMENTS:"));
    Serial.println(F("  - Potentiometer Wiper : Pin A0 (Analog 0-5V)"));
    Serial.println(F("  - Mode Select Button  : Pin 7  (SPST to GND, internal pullup)"));
    Serial.println(F("  - NeoPixel Mode LED   : Pin 8  (WS2812B single LED)"));
    Serial.println(F("  - DRV8833 IN1         : Pin 9  (Phase A excitation square wave F)"));
    Serial.println(F("  - DRV8833 IN2         : Pin 10 (Phase B excitation square wave F, 180 deg)"));
    Serial.println(F("  - DRV8833 nSLEEP      : Pin 4  (H-Bridge sleep enable)"));
    Serial.println(F("  - Sense Demod Sync 2F : Pin 2  (Synchronous 2F clock for demodulator)"));
    Serial.println(F("  - DRV8833 IN3/IN4     : Pin 6 / Pin 3 (Bipolar feedback nulling PWM, 10.0 kHz)"));
    Serial.println(F("  - Magneto Gate Driver : Pin 5  (Active-LOW pulse, pull-up, for inverting driver)"));
    Serial.println(F("------------------------------------------------------------"));
    Serial.println(F("STARTUP MODE: FluxGate (BLUE) @ 160 Hz (Sync 2F @ 320 Hz) + Pin 5 Pulse (3 us @ 100 Hz)"));
    Serial.println(F("Type 'help' for commands, or hit Enter for prompt."));
    Serial.println(F("============================================================\n"));
    Serial.print(F("MagNiche> "));
}

void initSerialCLI() {
    s_rx_index = 0;
    s_was_serial_connected = (bool)Serial;
}

void updateSerialCLI() {
    bool is_connected = (bool)Serial;
    if (is_connected && !s_was_serial_connected) {
        // Serial monitor just connected (or reconnected)
        delay(20);
        printWelcomeBanner();
    }
    s_was_serial_connected = is_connected;

    if (!is_connected) {
        return;
    }

    while (Serial.available() > 0) {
        char c = (char)Serial.read();

        // Process Enter (CR or LF)
        if (c == '\r' || c == '\n') {
            uint32_t now = millis();
            if (s_rx_index > 0) {
                s_rx_buffer[s_rx_index] = '\0';
                Serial.println();
                handleCommand(s_rx_buffer);
                s_rx_index = 0;
                Serial.print(F("MagNiche> "));
                s_last_enter_ms = now;
            } else if ((uint32_t)(now - s_last_enter_ms) > 100) { // debounce \r\n pair
                Serial.println();
                Serial.print(F("MagNiche> "));
                s_last_enter_ms = now;
            }
        } else if (c == '\b' || c == 127) { // backspace
            if (s_rx_index > 0) {
                s_rx_index--;
                Serial.print(F("\b \b"));
            }
        } else if (s_rx_index < (CLI_BUFFER_SIZE - 1)) {
            s_rx_buffer[s_rx_index++] = c;
            Serial.write(c); // local echo
        }
    }
}

