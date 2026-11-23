# MagNiche - Dual Niche Magnetic Signal Generator

Firmware for the **SparkFun Pro Micro 5V / 16MHz (ATmega32U4)** built using **PlatformIO** and Arduino framework.

This device acts as a precision signal generator to explore two niche magnetic phenomena:
1. **Magnetostriction in Nickel Wire**: High-current pulsed excitation of nickel wire via a Power MOSFET to generate acoustic strain / acoustic delay-line pulses.
2. **Billingsley-Type FluxGate Magnetometer**: Symmetrical push-pull AC excitation via a **DRV8833** H-bridge driver and variable DC nulling bias via high-frequency PWM to the feedback winding.

The 32U4 acts purely as a signal generator; an oscilloscope is used to observe, trigger, and interpret all waveforms.

---

## Hardware Pinout & Wiring

| Pro Micro Pin | Label / Port | Target Hardware | Function |
|---|---|---|---|
| **A0** (Pin 18) | `PF7` | **Analog Potentiometer** | Wiper input (outer terminals to +5V and GND) |
| **Pin 7** | `PE6` | **SPST Pushbutton** | Momentary button to GND (`INPUT_PULLUP`) |
| **Pin 8** | `PB4` | **NeoPixel (WS2812B)** | Data In for single mode-indicating RGB LED |
| **Pin 9** | `PB5` | **DRV8833 IN1** | Timer1 state-machine push-pull excitation Phase A ($F$) |
| **Pin 10** | `PB6` | **DRV8833 IN2** | Timer1 state-machine push-pull excitation Phase B ($F$, 180°) |
| **Pin 2** | `PD1` | **Demod Sync 2F** | In-sync $2F$ reference clock for sense coil demodulator |
| **Pin 4** | `PD4` | **DRV8833 nSLEEP** | H-Bridge sleep/enable control (HIGH=run, LOW=sleep) |
| **Pin 6** | `PD7` | **DRV8833 IN3** | Bipolar feedback nulling PWM (10.0 kHz Timer3, Channel B) |
| **Pin 3** | `PD0` | **DRV8833 IN4** | Bipolar feedback nulling PWM (10.0 kHz Timer3, Channel B) |
| **Pin 5** | `PC6` | **Inverting Gate Driver** | Active-LOW pulse with internal pull-up (Idle HIGH, pulses LOW to turn MOSFET ON) |
| **VCC** / **GND** | 5V / GND | Power Rail | 5V supply and common ground reference |

---

## Mode Selection & NeoPixel Color Rotation

Pressing the SPST momentary button cycles through the five modes:

```
[RED] -> [GREEN] -> [BLUE] -> [YELLOW] -> [PURPLE] -> [RED] ...
```

### 1. Magnetostrictive Mode:
* **RED**: **Adjust Pulse Width** ($1\text{ }\mu\text{s}$ to $2000\text{ }\mu\text{s}$)
  * Rotate the pot to adjust the MOSFET gate pulse width.
  * Uses a squared taper curve to give fine sub-microsecond control at the short acoustic range ($1\text{ }\mu\text{s} - 50\text{ }\mu\text{s}$) while allowing wide pulses up to $2\text{ ms}$.
  * The MOSFET pulses continuously at the set frequency.
* **GREEN**: **Adjust Repetition Frequency** ($1.0\text{ Hz}$ to $1000.0\text{ Hz}$)
  * Rotate the pot to adjust the pulse repetition rate.
  * Low frequencies ($1 - 20\text{ Hz}$) allow observing isolated, non-overlapping acoustic echoes along the wire.
  * Higher frequencies ($100 - 1000\text{ Hz}$) allow exploring acoustic resonance.

> **Safety Interlock**: Pulse width is automatically clamped to $\le 50\%$ duty cycle to protect the nickel wire and MOSFET from thermal runaway.

---

### 2. FluxGate Magnetometer Mode *(Current Boot Default @ 160 Hz + Pin 5 @ 3 us / 100 Hz)*:
* **BLUE**: **Adjust DRV8833 Excitation Frequency** ($10.0\text{ Hz}$ to $20.0\text{ kHz}$)
  * Symmetrical 50% push-pull square wave at $F$ (Pins 9 and 10) plus a phase-locked $2F$ synchronous demodulator reference clock on **Pin 2**, driven by a 4-step **Timer1** interrupt state machine.
  * Boots directly into **BLUE** mode at **$160.0\text{ Hz}$** ($320.0\text{ Hz}$ on Pin 2) for bench debugging; adjustable via serial CLI (`flux_f <val>`).
  * In addition, Pin 5 simultaneously outputs active-LOW $3.0\text{ }\mu\text{s}$ pulses at $100.0\text{ Hz}$ for bench testing both subsystems simultaneously upon boot.
  * Drives excitation coil symmetrically into deep magnetic saturation with zero DC offset.
* **YELLOW**: **Adjust Bipolar Feedback DC Current PWM** ($-100.0\%$ CCW to $0\%$ Float to $+100.0\%$ CW)
  * Drives the second half of the DRV8833 (**Pin 6 = IN3**, **Pin 3 = IN4**) at **10.0 kHz** (independent Timer3) into an RC filter to generate a bipolar DC nulling voltage.
  * Pot centered: both `OUT3` & `OUT4` float (`IN3=0, IN4=0`). CW: `OUT4=GND`, `OUT3=PWM (0..100%)`. CCW: `OUT3=GND`, `OUT4=PWM (0..100%)`.
  * Also controllable via serial CLI (`flux_fb <val>`, range `-100.0` to `+100.0`).

---

### 3. Standby / Safe Bench Mode:
* **PURPLE**: **Safe Standby (All Outputs Disabled)**
  * Pin 5 held HIGH (+5V) so inverting gate driver holds MOSFET gate LOW (0V / OFF).
  * DRV8833 IN1/IN2 and Pin 2 ($2F$ Sync) held LOW, and `nSLEEP` pulled LOW (H-bridge in sleep mode).
  * Feedback PWM forced to 0V.
  * Allows safe probe positioning, coil adjustments, or observing baseline noise on the scope.

---

## Oscilloscope Setup Guide

### Magnetostriction Experiment:
```
[Pro Micro Pin 5] ---> [Inverting Gate Driver] ---> Gate of Power MOSFET
                                                         |
                                                    [Nickel Wire] <--- Current Pulse
                                                         |
[Pickup Coil / Piezo] ------------------------------ Scope Channel 2 (Signal)
[Scope Channel 1 / Trig] --------------------------- MOSFET Gate (Rising) or Pin 5 (Falling)
```
1. Connect **Scope Channel 1** to **MOSFET Gate** (trigger on **Rising Edge**) or **Pin 5** (trigger on **Falling Edge**).
2. Connect **Scope Channel 2** to the acoustic pickup coil or piezo sensor on the nickel wire.
3. In **RED** mode, dial the pulse width to optimize acoustic impulse generation.
4. In **GREEN** mode, set the repetition rate low (e.g. $10 - 50\text{ Hz}$) to view time-of-flight acoustic echoes without pileup.

### FluxGate Magnetometer Experiment:
```
[Pro Micro Pin 9 (IN1)]  ---> DRV8833 IN1 ---> [OUT1] -----\
[Pro Micro Pin 10 (IN2)] ---> DRV8833 IN2 ---> [OUT2] ----- [Excitation Coil]

[Pro Micro Pin 2 (2F)]   ---> [Sense Demodulator Switch]
[Pro Micro Pin 6]        ---> [RC Filter: 10k + 1uF] ---> [Feedback / Nulling Coil]

[Secondary / Sense Coil] ---> Scope Channel 2 (2nd Harmonic @ 2 * f_exc)
[Scope Channel 1 / Trig] ---> Pro Micro Pin 9 (Excitation Reference)
```
1. Connect **Scope Channel 1** to **Pin 9** and use as trigger source.
2. Probe differential voltage across the excitation coil with channels 1 & 2 (or math `CH1 - CH2`).
3. Connect **Scope Channel 2** to the sense/secondary coil. In an ambient field, look for the $2f$ (second harmonic) waveform synchronized with **Pin 2**.
4. Switch to **YELLOW** mode and adjust the feedback PWM until the second harmonic on the scope is nulled to zero!

---

## Features & Implementation Details

- **Timer1 State-Machine Excitation & 2F Sync**: 4-step CTC interrupt state machine drives DRV8833 complementary square waves at $F$ ($10\text{ Hz} - 20\text{ kHz}$) on Pins 9 & 10 and a phase-locked $2F$ demodulator clock on Pin 2.
- **Microsecond Jitter-Free Pulses**: Magnetostrictive pulses use direct port register manipulation (`PORTC`) with nanosecond edges.
- **Potentiometer Soft Takeover (Pickup)**: Switching modes does not jerk the newly selected parameter until the user physically rotates the knob past a threshold.
- **Interactive Serial CLI & Telemetry (115200 baud)**: Supports `flux`, `flux_f <val>`, `flux_fb <pct>`, `mag`, `mag_pw <us>`, `mag_f <hz>`, `standby`, `status`, and `help` (see [docs/OPERATION.md](docs/OPERATION.md)).

---

## Build & Upload Commands

```bash
# Build firmware
pio run

# Upload to SparkFun Pro Micro
pio run --target upload

# Open Serial Monitor at 115200 baud
pio device monitor
```
