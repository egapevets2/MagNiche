#pragma once
#include <Arduino.h>

void initFluxGate();
void startFluxGate();
void stopFluxGate();
bool isFluxGateActive();

void setFluxExcitationFreq(float freq_hz);
float getFluxExcitationFreq();
void computeFluxTimer1Params(float freq_hz, uint8_t* cs_bits_out, uint16_t* prescaler_out, uint16_t* top_out);

void setFluxFeedbackPWM(float duty_pct);
float getFluxFeedbackPWM();

