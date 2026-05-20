#pragma once

#include <Arduino.h>

void loadOffsets();
void saveOffsets();
void readBNOAltAz(float &alt, float &az);
