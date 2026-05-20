#pragma once

#include <Arduino.h>

double calcJulianDate(int y, int m, int d, double utc_hours);
double calcGMST_deg(int y, int m, int d, double utc_hours);
void equatorialToHorizon(double ra_deg, double dec_deg, double lat, double lon,
                         int year, int month, int day, double utc_hours,
                         double &alt, double &az);
