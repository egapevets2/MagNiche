#pragma once
#include <Arduino.h>

void initMagnetostriction();
void startMagnetostriction();
void stopMagnetostriction();
bool isMagnetostrictionActive();

void setMagPulseWidth(float us);
float getMagPulseWidth();

void setMagFrequency(float hz);
float getMagFrequency();

void updateMagnetostriction();

