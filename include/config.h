#pragma once
#include <Arduino.h>

// ============================================================================
// MagNiche - Hardware Configuration & Parameter Definitions
// Target: SparkFun Pro Micro 5V / 16MHz (ATmega32U4)
// ============================================================================

// ----------------------------------------------------------------------------
// PIN ASSIGNMENTS
// ----------------------------------------------------------------------------
// User Controls
#define PIN_POT               A0   // PF7 (Pin 18): Analog Potentiometer wiper (0-5V)
#define PIN_BUTTON            7    // PE6 (Pin 7):  SPST Momentary Pushbutton to GND
#define PIN_NEOPIXEL          8    // PB4 (Pin 8):  Single WS2812B NeoPixel Data In

// FluxGate Magnetometer DRV8833 Excitation & Demodulator Sync
#define PIN_DRV8833_IN1       9    // PB5 (Pin 9):  Phase A excitation square wave (F)
#define PIN_DRV8833_IN2       10   // PB6 (Pin 10): Phase B excitation square wave (F, 180 deg)
#define PIN_DRV8833_NSLEEP    4    // PD4 (Pin 4):  nSLEEP (HIGH=enabled, LOW=sleep)
#define PIN_FLUX_2F_SYNC      2    // PD1 (Pin 2):  In-sync 2F reference clock for sense coil demodulator

// FluxGate Magnetometer Feedback DC Current (DRV8833 Channel B Bipolar Nulling Bias)
#define PIN_DRV8833_IN3       6    // PD7 (Pin 6):  Timer3 10 kHz PWM - DRV8833 Channel B IN3
#define PIN_DRV8833_IN4       3    // PD0 (Pin 3):  Timer3 10 kHz PWM - DRV8833 Channel B IN4

// Magnetostrictive Nickel Wire Pulse Driver (Inverting Gate-Driver: Active-LOW, Pull-up enabled)
#define PIN_MOSFET_GATE       5    // PC6 (Pin 5):  Active-LOW pulsed drive to inverting gate driver

// ----------------------------------------------------------------------------
// OPERATING MODES & NEOPIXEL COLORS
// ----------------------------------------------------------------------------
enum OperatingMode {
    MODE_MAG_PULSEWIDTH = 0, // RED:    Magnetostrictive - Adjust Pulse Width (1 - 2000 us)
    MODE_MAG_FREQUENCY  = 1, // GREEN:  Magnetostrictive - Adjust Repetition Freq (1 - 1000 Hz)
    MODE_FLUX_EXCITATION = 2,// BLUE:   FluxGate - Adjust DRV8833 Frequency (10 Hz - 20 kHz)
    MODE_FLUX_FEEDBACK   = 3,// YELLOW: FluxGate - Adjust Feedback PWM Duty (0 - 100%)
    MODE_STANDBY         = 4,// PURPLE: Standby / Safe Idle - All outputs disabled
    MODE_COUNT           = 5
};

// ----------------------------------------------------------------------------
// PARAMETER LIMITS & DEFAULTS
// ----------------------------------------------------------------------------
// Magnetostriction Limits
#define MAG_PW_MIN_US         1.0f     // Minimum pulse width in microseconds
#define MAG_PW_MAX_US         2000.0f  // Maximum pulse width in microseconds
#define MAG_PW_DEFAULT_US     3.0f     // Default pulse width (power-up bench debug: 3.0 us)

#define MAG_FREQ_MIN_HZ       1.0f     // Minimum repetition frequency in Hz
#define MAG_FREQ_MAX_HZ       1000.0f  // Maximum repetition frequency in Hz
#define MAG_FREQ_DEFAULT_HZ   100.0f   // Default repetition frequency (power-up bench debug: 100.0 Hz)

#define MAG_MAX_DUTY_CYCLE    0.50f    // Safety clamp: maximum pulse width <= 50% period

// FluxGate DRV8833 Limits
#define FLUX_FREQ_MIN_HZ      10.0f    // 10 Hz expanded lower bound
#define FLUX_FREQ_MAX_HZ      20000.0f // 20 kHz per specification
#define FLUX_FREQ_DEFAULT_HZ  160.0f   // Hardware debug default (160 Hz)
#define DRV8833_DEADTIME_TICKS 8       // ~500ns non-overlapping dead-time at 16MHz

// FluxGate Feedback Limits (Bipolar DRV8833 Channel B: -100% CCW .. 0% Float .. +100% CW)
#define FLUX_FB_PWM_FREQ_HZ      10000UL // 10.00 kHz independent Timer3 PWM carrier for nulling coil
#define FLUX_PWM_MIN_DUTY       -100.0f  // Full CCW (OUT3=GND, OUT4=100% VM)
#define FLUX_PWM_MAX_DUTY        100.0f  // Full CW  (OUT4=GND, OUT3=100% VM)
#define FLUX_PWM_DEFAULT_DUTY    0.0f    // Default 0% (Center: OUT3/OUT4 floating)
#define FLUX_FB_CENTER_ADC       512     // Midpoint of 10-bit ADC (0-1023)
#define FLUX_FB_CENTER_DEADBAND  25      // +/-25 ADC counts around center -> both outputs floating

// ----------------------------------------------------------------------------
// UI & CONTROL TUNING
// ----------------------------------------------------------------------------
#define BUTTON_DEBOUNCE_MS    40       // Milliseconds for debounce lockout
#define NEOPIXEL_BRIGHTNESS   45       // Comfortable lab bench LED brightness (0-255)
#define POT_DEADBAND          10       // ADC counts to ignore wiper noise
#define POT_PICKUP_THRESHOLD  50       // ADC counts (~5% rotation) of deliberate movement before pot takeover
#define SERIAL_BAUD_RATE      115200   // USB CDC baud rate

