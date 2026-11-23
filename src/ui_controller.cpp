#include "ui_controller.h"
#include "config.h"
#include "magnetostriction.h"
#include "fluxgate.h"
#include <Adafruit_NeoPixel.h>

static Adafruit_NeoPixel s_pixel(1, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

static OperatingMode s_current_mode = MODE_MAG_PULSEWIDTH;
static bool          s_last_btn_state = HIGH;
static uint32_t      s_last_btn_time  = 0;

static int           s_pot_entry_adc      = 0;
static bool          s_pot_pickup_active  = false;
static int           s_last_processed_adc = -1;


static int readPotAveraged() {
    uint32_t sum = 0;
    const uint8_t SAMPLES = 16;
    for (uint8_t i = 0; i < SAMPLES; i++) {
        sum += analogRead(PIN_POT);
    }
    return (int)(sum / SAMPLES);
}

static void applyLedColor(OperatingMode mode) {
    switch (mode) {
        case MODE_MAG_PULSEWIDTH:
            // RED: Magnetostrictive Pulse Width
            s_pixel.setPixelColor(0, s_pixel.Color(255, 0, 0));
            break;
        case MODE_MAG_FREQUENCY:
            // GREEN: Magnetostrictive Repetition Frequency
            s_pixel.setPixelColor(0, s_pixel.Color(0, 255, 0));
            break;
        case MODE_FLUX_EXCITATION:
            // BLUE: FluxGate DRV8833 Excitation (5 - 20 kHz)
            s_pixel.setPixelColor(0, s_pixel.Color(0, 0, 255));
            break;
        case MODE_FLUX_FEEDBACK:
            // YELLOW: FluxGate Feedback DC PWM (0 - 100%)
            s_pixel.setPixelColor(0, s_pixel.Color(255, 180, 0));
            break;
        case MODE_STANDBY:
        default:
            // PURPLE: Safe Standby / Idle (All Outputs Off)
            s_pixel.setPixelColor(0, s_pixel.Color(180, 0, 255));
            break;
    }
    s_pixel.show();
}

void printSystemStatus() {
    Serial.println(F("\n------------------------------------------------------------"));
    switch (s_current_mode) {
        case MODE_MAG_PULSEWIDTH:
            Serial.println(F("[MODE: RED] Magnetostrictive - Adjusting PULSE WIDTH"));
            break;
        case MODE_MAG_FREQUENCY:
            Serial.println(F("[MODE: GREEN] Magnetostrictive - Adjusting REPETITION FREQ"));
            break;
        case MODE_FLUX_EXCITATION:
            Serial.println(F("[MODE: BLUE] FluxGate - Adjusting EXCITATION FREQ (10 Hz - 20 kHz)"));
            break;
        case MODE_FLUX_FEEDBACK:
            Serial.println(F("[MODE: YELLOW] FluxGate - Adjusting BIPOLAR FEEDBACK PWM (-100%..0..+100%)"));
            break;
        case MODE_STANDBY:
            Serial.println(F("[MODE: PURPLE] STANDBY / SAFE IDLE - All Outputs Disabled"));
            break;
        default:
            break;
    }

    if (s_current_mode == MODE_MAG_PULSEWIDTH || s_current_mode == MODE_MAG_FREQUENCY) {
        float pw = getMagPulseWidth();
        float fq = getMagFrequency();
        float period_us = 1000000.0f / fq;
        float duty_pct = (pw / period_us) * 100.0f;
        Serial.print(F("  >> Magnetostriction (MOSFET Gate Pin D5): ACTIVE\n"));
        Serial.print(F("     Pulse Width : ")); Serial.print(pw, 2); Serial.println(F(" us"));
        Serial.print(F("     Frequency   : ")); Serial.print(fq, 2); Serial.println(F(" Hz"));
        Serial.print(F("     Period      : ")); Serial.print(period_us, 1); Serial.println(F(" us"));
        Serial.print(F("     Duty Cycle  : ")); Serial.print(duty_pct, 3); Serial.println(F(" %"));
        Serial.println(F("  >> FluxGate (DRV8833 ChA D9/D10, ChB D6/D3): OFF"));
    } else if (s_current_mode == MODE_FLUX_EXCITATION || s_current_mode == MODE_FLUX_FEEDBACK) {
        float f_exc = getFluxExcitationFreq();
        float duty  = getFluxFeedbackPWM();
        if (isMagnetostrictionActive()) {
            float pw = getMagPulseWidth();
            float fq = getMagFrequency();
            Serial.print(F("  >> Magnetostriction (MOSFET Gate Pin D5): ACTIVE @ "));
            Serial.print(pw, 2); Serial.print(F(" us, "));
            Serial.print(fq, 2); Serial.println(F(" Hz (Active-LOW)"));
        } else {
            Serial.println(F("  >> Magnetostriction (MOSFET Gate Pin D5): OFF"));
        }
        Serial.print(F("  >> FluxGate Excitation (DRV8833 Pins D9/D10): ACTIVE @ "));
        if (f_exc >= 1000.0f) {
            Serial.print(f_exc / 1000.0f, 3);
            Serial.println(F(" kHz"));
        } else {
            Serial.print(f_exc, 1);
            Serial.println(F(" Hz"));
        }
        float f_2x = f_exc * 2.0f;
        Serial.print(F("  >> Sense Demod Sync 2F (Pin D2)             : ACTIVE @ "));
        if (f_2x >= 1000.0f) {
            Serial.print(f_2x / 1000.0f, 3);
            Serial.println(F(" kHz"));
        } else {
            Serial.print(f_2x, 1);
            Serial.println(F(" Hz"));
        }
        Serial.print(F("     Feedback Bipolar (DRV8833 D6/D3)      : "));
        if (fabs(duty) < 0.05f) {
            Serial.println(F("0.00 % (D6=LOW, D3=LOW -> OUT3/OUT4 FLOAT)"));
        } else if (duty > 0.0f) {
            Serial.print(F("+")); Serial.print(duty, 2);
            Serial.println(F(" % CW (D6=HIGH, D3=PWM -> OUT3=PWM, OUT4=GND)"));
        } else {
            Serial.print(duty, 2);
            Serial.println(F(" % CCW (D6=PWM, D3=HIGH -> OUT3=GND, OUT4=PWM)"));
        }
    } else {
        Serial.println(F("  >> ALL GENERATOR OUTPUTS: OFF"));
    }
    Serial.println(F("------------------------------------------------------------"));
}

void resetPotPickup() {
    s_pot_entry_adc = readPotAveraged();
    s_pot_pickup_active = false;
    s_last_processed_adc = s_pot_entry_adc;
}

void setMode(OperatingMode new_mode) {
    if (new_mode >= MODE_COUNT) new_mode = MODE_MAG_PULSEWIDTH;
    s_current_mode = new_mode;

    applyLedColor(s_current_mode);

    // Coordinate hardware state machines
    if (s_current_mode == MODE_MAG_PULSEWIDTH || s_current_mode == MODE_MAG_FREQUENCY) {
        stopFluxGate();
        startMagnetostriction();
    } else if (s_current_mode == MODE_FLUX_EXCITATION) {
        // FluxGate excitation mode: Keep DRV8833 active AND Pin 5 pulse active for bench testing
        startFluxGate();
        startMagnetostriction();
    } else if (s_current_mode == MODE_FLUX_FEEDBACK) {
        stopMagnetostriction();
        startFluxGate();
    } else { // MODE_STANDBY
        stopMagnetostriction();
        stopFluxGate();
    }

    // Initialize pot pickup so changing modes doesn't jerk the active parameter
    resetPotPickup();

    printSystemStatus();
}

OperatingMode getCurrentMode() {
    return s_current_mode;
}

void initUI() {
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    pinMode(PIN_POT, INPUT);

    s_pixel.begin();
    s_pixel.setBrightness(NEOPIXEL_BRIGHTNESS);
    s_pixel.show();

    // Hardware debug startup:
    // DRV8833 outputs initial push-pull square wave @ 160 Hz with Pin 2 @ 320 Hz
    // In addition, Pin 5 active-LOW pulses at 3 us width @ 100 Hz rate
    setFluxExcitationFreq(FLUX_FREQ_DEFAULT_HZ);
    setMagPulseWidth(MAG_PW_DEFAULT_US);
    setMagFrequency(MAG_FREQ_DEFAULT_HZ);
    setMode(MODE_FLUX_EXCITATION);
    startMagnetostriction();
}

static float mapPotToBipolarFeedback(int raw_adc) {
    const int low_thresh  = FLUX_FB_CENTER_ADC - FLUX_FB_CENTER_DEADBAND;
    const int high_thresh = FLUX_FB_CENTER_ADC + FLUX_FB_CENTER_DEADBAND;

    if (raw_adc >= low_thresh && raw_adc <= high_thresh) {
        return 0.0f; // Center deadband: both DRV8833 outputs floating
    }
    if (raw_adc > high_thresh) {
        // CW from center: 0% to +100%
        return ((float)(raw_adc - high_thresh) / (float)(1023 - high_thresh)) * 100.0f;
    }
    // CCW from center: 0% to -100%
    return -((float)(low_thresh - raw_adc) / (float)low_thresh) * 100.0f;
}

void updateUI() {
    uint32_t now = millis();

    // --- 1. Pushbutton Debounce & Mode Cycling ---
    bool btn_state = digitalRead(PIN_BUTTON);
    if (btn_state != s_last_btn_state) {
        if ((now - s_last_btn_time) >= BUTTON_DEBOUNCE_MS) {
            s_last_btn_time = now;
            s_last_btn_state = btn_state;
            if (btn_state == LOW) { // Button pressed (active-low)
                OperatingMode next_mode = (OperatingMode)((s_current_mode + 1) % MODE_COUNT);
                setMode(next_mode);
            }
        }
    }

    // --- 2. Potentiometer Reading & Soft Pickup ---
    int raw_adc = readPotAveraged();

    if (!s_pot_pickup_active) {
        // Require deliberate pot turn before taking over new mode's parameter
        if (abs(raw_adc - s_pot_entry_adc) >= POT_PICKUP_THRESHOLD) {
            s_pot_pickup_active = true;
            s_last_processed_adc = raw_adc;
        }
    }

    if (s_pot_pickup_active) {
        if (abs(raw_adc - s_last_processed_adc) >= POT_DEADBAND) {
            s_last_processed_adc = raw_adc;
            float norm = (float)raw_adc / 1023.0f; // 0.0 to 1.0

            bool param_changed = false;

            switch (s_current_mode) {
                case MODE_MAG_PULSEWIDTH: {
                    // Power curve for fine microsecond resolution at low end, wide 2000us span
                    float pw = MAG_PW_MIN_US + (MAG_PW_MAX_US - MAG_PW_MIN_US) * (norm * norm);
                    setMagPulseWidth(pw);
                    param_changed = true;
                    break;
                }
                case MODE_MAG_FREQUENCY: {
                    // Power curve for fine resolution at low single-pulse repetition rates
                    float fq = MAG_FREQ_MIN_HZ + (MAG_FREQ_MAX_HZ - MAG_FREQ_MIN_HZ) * (norm * norm);
                    setMagFrequency(fq);
                    param_changed = true;
                    break;
                }
                case MODE_FLUX_EXCITATION: {
                    // Locked to CLI/defaults in BLUE mode so floating A0 cannot overwrite flux_f or flux_fb.
                    // Switch to YELLOW (MODE_FLUX_FEEDBACK) when using the potentiometer to adjust nulling bias.
                    break;
                }
                case MODE_FLUX_FEEDBACK: {
                    // Bipolar DRV8833 Channel B nulling coil control:
                    // Center = floating, CW = 0..+100% (OUT4=GND, OUT3=PWM), CCW = 0..-100% (OUT3=GND, OUT4=PWM)
                    float duty = mapPotToBipolarFeedback(raw_adc);
                    setFluxFeedbackPWM(duty);
                    param_changed = true;
                    break;
                }
                case MODE_STANDBY:
                default:
                    // In standby, all outputs remain safely disabled
                    break;
            }
            (void)param_changed; // Pot adjusted smoothly without flooding serial terminal
        }
    }
}

