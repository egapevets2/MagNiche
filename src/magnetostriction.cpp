#include "magnetostriction.h"
#include "config.h"

static float    s_pulse_width_us = MAG_PW_DEFAULT_US;
static float    s_frequency_hz   = MAG_FREQ_DEFAULT_HZ;
static bool     s_mag_active     = false;
static uint32_t s_last_pulse_us  = 0;

void initMagnetostriction() {
    // For an inverting gate-driver:
    // Quiescent/idle state is HIGH (inverting driver outputs LOW to MOSFET gate).
    // Enable pullup before configuring output to avoid any low glitch during boot:
    pinMode(PIN_MOSFET_GATE, INPUT_PULLUP);
    digitalWrite(PIN_MOSFET_GATE, HIGH);
    pinMode(PIN_MOSFET_GATE, OUTPUT);
    PORTC |= _BV(PC6);
}

void startMagnetostriction() {
    s_mag_active = true;
    PORTC |= _BV(PC6); // Ensure idle HIGH
    s_last_pulse_us = micros();
}

void stopMagnetostriction() {
    s_mag_active = false;
    // For an inverting gate-driver, idle state is HIGH (so driver holds MOSFET gate LOW/OFF)
    PORTC |= _BV(PC6);
    digitalWrite(PIN_MOSFET_GATE, HIGH);
}

bool isMagnetostrictionActive() {
    return s_mag_active;
}

void setMagPulseWidth(float us) {
    if (us < MAG_PW_MIN_US) us = MAG_PW_MIN_US;
    if (us > MAG_PW_MAX_US) us = MAG_PW_MAX_US;
    s_pulse_width_us = us;
}

float getMagPulseWidth() {
    return s_pulse_width_us;
}

void setMagFrequency(float hz) {
    if (hz < MAG_FREQ_MIN_HZ) hz = MAG_FREQ_MIN_HZ;
    if (hz > MAG_FREQ_MAX_HZ) hz = MAG_FREQ_MAX_HZ;
    s_frequency_hz = hz;
}

float getMagFrequency() {
    return s_frequency_hz;
}

void updateMagnetostriction() {
    if (!s_mag_active) return;

    // Calculate repetition period in microseconds
    uint32_t period_us = (uint32_t)round(1000000.0f / s_frequency_hz);
    if (period_us < 1000) period_us = 1000; // clamp to 1kHz (1000 us)

    uint32_t now = micros();
    if ((uint32_t)(now - s_last_pulse_us) >= period_us) {
        s_last_pulse_us = now;

        // Safety clamp: Never exceed configured duty cycle limit to protect wire & MOSFET
        float max_safe_pw = (float)period_us * MAG_MAX_DUTY_CYCLE;
        float actual_pw = s_pulse_width_us;
        if (actual_pw > max_safe_pw) actual_pw = max_safe_pw;
        unsigned int pw_int = (unsigned int)round(actual_pw);
        if (pw_int < 1) pw_int = 1;

        // Direct port bit manipulation on PC6 (Pin 5) for clean nanosecond edges.
        // Inverting gate driver: Active pulse is LOW, Quiescent/Idle state is HIGH.
        // For pulses <= 500us, temporarily disable interrupts for jitter-free oscilloscope triggering.
        if (pw_int <= 500) {
            uint8_t oldSREG = SREG;
            cli();
            PORTC &= ~_BV(PC6);            // Active-LOW pulse to inverting driver (MOSFET ON)
            delayMicroseconds(pw_int);
            PORTC |= _BV(PC6);             // Return to Idle-HIGH (MOSFET OFF)
            SREG = oldSREG;
        } else {
            PORTC &= ~_BV(PC6);            // Active-LOW pulse to inverting driver (MOSFET ON)
            delayMicroseconds(pw_int);
            PORTC |= _BV(PC6);             // Return to Idle-HIGH (MOSFET OFF)
        }
    }
}

