#pragma once
#include <Arduino.h>
#include "config.h"

void initUI();
void updateUI();

OperatingMode getCurrentMode();
void setMode(OperatingMode new_mode);
void resetPotPickup();
void printSystemStatus();


