#pragma once

#include <Arduino.h>

bool ajusterPasBoucleFermee(float cibleAlt, float cibleAz, float tolAlt = 0.5, float tolAz = 1.0);
bool pointerVersCible();
