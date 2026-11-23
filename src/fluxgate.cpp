#include "fluxgate.h"
#include "config.h"

static float s_excitation_freq_hz = FLUX_FREQ_DEFAULT_HZ;
static float s_feedback_duty_pct   = FLUX_PWM_DEFAULT_DUTY;
static bool  s_fluxgate_active     = false;

// 4-step state machine per fundamental excitation cycle (F):
//   Step 0: IN1=HIGH, IN2=LOW,  2F=HIGH
//   Step 1: IN1=HIGH, IN2=LOW,  2F=LOW
//   Step 2: IN1=LOW,  IN2=HIGH, 2F=HIGH
//   Step 3: IN1=LOW,  IN2=HIGH, 2F=LOW
static volatile uint8_t s_flux_step = 0;

// Timer3 10.00 kHz Bipolar Feedback PWM state (DRV8833 Channel B: IN3=PD7, IN4=PD0)
static constexpr uint16_t FLUX_FB_TIMER3_PERIOD  = (uint16_t)(F_CPU / FLUX_FB_PWM_FREQ_HZ); // 1600 @ 10 kHz
static constexpr uint16_t FLUX_FB_TIMER3_TOP     = FLUX_FB_TIMER3_PERIOD - 1;               // 1599
static constexpr uint16_t FLUX_FB_MIN_EDGE_TICKS = 24; // 1.5 us min margin for ISR & DRV8833 switching
static volatile uint8_t s_fb_pwm_mask = 0;             // _BV(PD0) for CW, _BV(PD7) for CCW, 0 when static

ISR(TIMER3_COMPA_vect) {
    // Period start (TCNT3 wraps to 0): PWM input HIGH -> slow-decay brake (both outputs GND)
    PORTD |= s_fb_pwm_mask;
}

ISR(TIMER3_COMPB_vect) {
    // Duty match (after inv_ticks): PWM input LOW -> active drive (active output VM, other GND)
    PORTD &= ~s_fb_pwm_mask;
}

static void stopFeedbackTimer3() {
    uint8_t oldSREG = SREG;
    cli();
    TIMSK3 = 0;
    TCCR3A = 0;
    TCCR3B = 0;
    s_fb_pwm_mask = 0;
    SREG = oldSREG;
}

static void startOrUpdateFeedbackTimer3(uint8_t pwm_mask, uint8_t high_mask, uint16_t inv_ticks) {
    uint8_t oldSREG = SREG;
    cli();
    if (s_fb_pwm_mask == pwm_mask && (TIMSK3 & _BV(OCIE3A))) {
        // Already running in this polarity: smooth atomic duty update
        OCR3B = inv_ticks;
    } else {
        // Starting from idle/float or switching polarity
        TIMSK3 = 0;
        TCCR3A = 0;
        TCCR3B = 0;
        TCNT3  = 0;

        // Hold static input HIGH, start PWM input HIGH (brake) until COMPB
        PORTD |= (high_mask | pwm_mask);

        s_fb_pwm_mask = pwm_mask;
        OCR3A  = FLUX_FB_TIMER3_TOP;
        OCR3B  = inv_ticks;
        TIFR3  = _BV(OCF3A) | _BV(OCF3B);
        TCCR3B = _BV(WGM32) | _BV(CS30); // CTC Mode 4, prescaler = 1 (16 MHz)
        TIMSK3 = _BV(OCIE3A) | _BV(OCIE3B);
    }
    SREG = oldSREG;
}

ISR(TIMER1_COMPA_vect) {
    switch (s_flux_step) {
        case 0:
            // Phase A start: IN2=LOW, IN1=HIGH, 2F Sync=HIGH
            PORTB &= ~_BV(PB6); // IN2 (Pin 10) LOW
            PORTB |= _BV(PB5);  // IN1 (Pin 9) HIGH
            PORTD |= _BV(PD1);  // 2F Sync (Pin 2) HIGH
            s_flux_step = 1;
            break;

        case 1:
            // Phase A mid: 2F Sync falling edge (50% duty on 2F)
            PORTD &= ~_BV(PD1); // 2F Sync (Pin 2) LOW
            s_flux_step = 2;
            break;

        case 2:
            // Phase B start: IN1=LOW, IN2=HIGH, 2F Sync=HIGH
            PORTB &= ~_BV(PB5); // IN1 (Pin 9) LOW
            PORTB |= _BV(PB6);  // IN2 (Pin 10) HIGH
            PORTD |= _BV(PD1);  // 2F Sync (Pin 2) HIGH
            s_flux_step = 3;
            break;

        case 3:
        default:
            // Phase B mid: 2F Sync falling edge (50% duty on 2F)
            PORTD &= ~_BV(PD1); // 2F Sync (Pin 2) LOW
            s_flux_step = 0;
            break;
    }
}

void initFluxGate() {
    pinMode(PIN_DRV8833_IN1, OUTPUT);
    pinMode(PIN_DRV8833_IN2, OUTPUT);
    pinMode(PIN_DRV8833_NSLEEP, OUTPUT);
    pinMode(PIN_FLUX_2F_SYNC, OUTPUT);
    pinMode(PIN_DRV8833_IN3, OUTPUT);
    pinMode(PIN_DRV8833_IN4, OUTPUT);

    // Initial safe state: drivers idle (all outputs floating) and in low-power sleep
    stopFeedbackTimer3();
    digitalWrite(PIN_DRV8833_IN1, LOW);
    digitalWrite(PIN_DRV8833_IN2, LOW);
    digitalWrite(PIN_FLUX_2F_SYNC, LOW);
    digitalWrite(PIN_DRV8833_NSLEEP, LOW);
    digitalWrite(PIN_DRV8833_IN3, LOW);
    digitalWrite(PIN_DRV8833_IN4, LOW);
}

void computeFluxTimer1Params(float freq_hz, uint8_t* cs_bits_out, uint16_t* prescaler_out, uint16_t* top_out) {
    if (freq_hz < FLUX_FREQ_MIN_HZ) freq_hz = FLUX_FREQ_MIN_HZ;
    if (freq_hz > FLUX_FREQ_MAX_HZ) freq_hz = FLUX_FREQ_MAX_HZ;

    // State machine has 4 steps per fundamental cycle F (interrupt rate = 4 * F).
    // Prescaler = 1 reaches down to ~61.04 Hz; Prescaler = 8 reaches down to ~7.63 Hz.
    uint8_t  cs_bits   = _BV(CS10);
    uint16_t prescaler = 1;
    if (freq_hz < 65.0f) {
        cs_bits   = _BV(CS11);
        prescaler = 8;
    }

    uint32_t ticks = (uint32_t)round(16000000.0f / ((float)prescaler * 4.0f * freq_hz));
    uint32_t top_calc = (ticks > 0) ? (ticks - 1) : 0;
    if (top_calc > 65535UL) top_calc = 65535UL;
    if (top_calc < 50) top_calc = 50;

    if (cs_bits_out)   *cs_bits_out   = cs_bits;
    if (prescaler_out) *prescaler_out = prescaler;
    if (top_out)       *top_out       = (uint16_t)top_calc;
}

static void applyTimer1Excitation(float freq_hz) {
    uint8_t  cs_bits = 0;
    uint16_t top     = 0;
    computeFluxTimer1Params(freq_hz, &cs_bits, nullptr, &top);

    s_flux_step = 0;

    // Stop timer while reconfiguring
    TIMSK1 = 0;
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0;

    OCR1A  = top;

    if (s_fluxgate_active) {
        // CTC mode (Mode 4, TOP = OCR1A) with selected prescaler
        TCCR1B = _BV(WGM12) | cs_bits;
        TIFR1  = _BV(OCF1A);   // Clear pending interrupt flag
        TIMSK1 = _BV(OCIE1A);  // Enable Compare Match A interrupt
    }
}

void startFluxGate() {
    s_fluxgate_active = true;

    // Enable DRV8833 H-bridge from sleep
    digitalWrite(PIN_DRV8833_NSLEEP, HIGH);

    // Start Timer1 interrupt state-machine
    applyTimer1Excitation(s_excitation_freq_hz);

    // Apply active Feedback PWM
    setFluxFeedbackPWM(s_feedback_duty_pct);
}

void stopFluxGate() {
    s_fluxgate_active = false;

    // Disconnect and disable Timer1 interrupt
    TIMSK1 = 0;
    TCCR1A = 0;
    TCCR1B = 0;

    // Force pins LOW
    PORTB &= ~(_BV(PB5) | _BV(PB6));
    PORTD &= ~_BV(PD1);
    digitalWrite(PIN_DRV8833_IN1, LOW);
    digitalWrite(PIN_DRV8833_IN2, LOW);
    digitalWrite(PIN_FLUX_2F_SYNC, LOW);

    // Put DRV8833 into low-power sleep
    digitalWrite(PIN_DRV8833_NSLEEP, LOW);

    // Disable feedback Channel B H-bridge (both outputs floating: IN3=0, IN4=0)
    stopFeedbackTimer3();
    digitalWrite(PIN_DRV8833_IN3, LOW);
    digitalWrite(PIN_DRV8833_IN4, LOW);
}

bool isFluxGateActive() {
    return s_fluxgate_active;
}

void setFluxExcitationFreq(float freq_hz) {
    if (freq_hz < FLUX_FREQ_MIN_HZ) freq_hz = FLUX_FREQ_MIN_HZ;
    if (freq_hz > FLUX_FREQ_MAX_HZ) freq_hz = FLUX_FREQ_MAX_HZ;
    s_excitation_freq_hz = freq_hz;

    if (s_fluxgate_active) {
        applyTimer1Excitation(s_excitation_freq_hz);
    }
}

float getFluxExcitationFreq() {
    return s_excitation_freq_hz;
}

void setFluxFeedbackPWM(float duty_pct) {
    if (duty_pct < FLUX_PWM_MIN_DUTY) duty_pct = FLUX_PWM_MIN_DUTY;
    if (duty_pct > FLUX_PWM_MAX_DUTY) duty_pct = FLUX_PWM_MAX_DUTY;
    s_feedback_duty_pct = duty_pct;

    // DRV8833 Channel B (IN3 = Pin 6 / PD7, IN4 = Pin 3 / PD0) Truth Table:
    //   IN3=0, IN4=0 -> OUT3=Float, OUT4=Float  (Center / Idle)
    //   IN3=1, IN4=0 -> OUT3=VM,    OUT4=GND    (CW Drive)
    //   IN3=0, IN4=1 -> OUT3=GND,   OUT4=VM     (CCW Drive)
    //   IN3=1, IN4=1 -> OUT3=GND,   OUT4=GND    (Brake / Slow Decay)
    //
    // Driven by independent Timer3 at 10.00 kHz (1600 ticks/period) so ripple is far above
    // the fluxgate excitation/2F sense band and easily filtered with a small RC capacitor.
    if (!s_fluxgate_active || fabs(s_feedback_duty_pct) < 0.05f) {
        // Center / inactive: both DRV8833 Channel B outputs floating
        stopFeedbackTimer3();
        digitalWrite(PIN_DRV8833_IN3, LOW);
        digitalWrite(PIN_DRV8833_IN4, LOW);
        return;
    }

    if (s_feedback_duty_pct > 0.0f) {
        // CW from center: OUT4 = GND, OUT3 = 10 kHz PWM (0% -> 100% VM)
        // Drive IN3 (PD7) = HIGH, IN4 (PD0) = inverted PWM
        uint16_t on_ticks = (uint16_t)round((s_feedback_duty_pct / 100.0f) * (float)FLUX_FB_TIMER3_PERIOD);
        if (on_ticks == 0) {
            stopFeedbackTimer3();
            digitalWrite(PIN_DRV8833_IN3, LOW);
            digitalWrite(PIN_DRV8833_IN4, LOW);
        } else if (on_ticks >= (FLUX_FB_TIMER3_PERIOD - FLUX_FB_MIN_EDGE_TICKS)) {
            // 100% CW: static OUT3=VM, OUT4=GND
            stopFeedbackTimer3();
            digitalWrite(PIN_DRV8833_IN3, HIGH);
            digitalWrite(PIN_DRV8833_IN4, LOW);
        } else {
            uint16_t inv_ticks = FLUX_FB_TIMER3_PERIOD - on_ticks;
            if (inv_ticks > (FLUX_FB_TIMER3_PERIOD - FLUX_FB_MIN_EDGE_TICKS)) {
                inv_ticks = FLUX_FB_TIMER3_PERIOD - FLUX_FB_MIN_EDGE_TICKS;
            }
            startOrUpdateFeedbackTimer3(_BV(PD0), _BV(PD7), inv_ticks);
        }
    } else {
        // CCW from center: OUT3 = GND, OUT4 = 10 kHz PWM (0% -> 100% VM)
        // Drive IN4 (PD0) = HIGH, IN3 (PD7) = inverted PWM
        uint16_t on_ticks = (uint16_t)round((-s_feedback_duty_pct / 100.0f) * (float)FLUX_FB_TIMER3_PERIOD);
        if (on_ticks == 0) {
            stopFeedbackTimer3();
            digitalWrite(PIN_DRV8833_IN3, LOW);
            digitalWrite(PIN_DRV8833_IN4, LOW);
        } else if (on_ticks >= (FLUX_FB_TIMER3_PERIOD - FLUX_FB_MIN_EDGE_TICKS)) {
            // 100% CCW: static OUT3=GND, OUT4=VM
            stopFeedbackTimer3();
            digitalWrite(PIN_DRV8833_IN4, HIGH);
            digitalWrite(PIN_DRV8833_IN3, LOW);
        } else {
            uint16_t inv_ticks = FLUX_FB_TIMER3_PERIOD - on_ticks;
            if (inv_ticks > (FLUX_FB_TIMER3_PERIOD - FLUX_FB_MIN_EDGE_TICKS)) {
                inv_ticks = FLUX_FB_TIMER3_PERIOD - FLUX_FB_MIN_EDGE_TICKS;
            }
            startOrUpdateFeedbackTimer3(_BV(PD7), _BV(PD0), inv_ticks);
        }
    }
}

float getFluxFeedbackPWM() {
    return s_feedback_duty_pct;
}

