#include "astro.h"

#include <math.h>

double calcJulianDate(int y, int m, int d, double utc_hours)
{
    if (m <= 2)
    {
        y--;
        m += 12;
    }
    int A = y / 100;
    int B = 2 - A + A / 4;
    double jd = floor(365.25 * (y + 4716)) + floor(30.6001 * (m + 1)) + d + B - 1524.5;
    jd += utc_hours / 24.0;
    return jd;
}

double calcGMST_deg(int y, int m, int d, double utc_hours)
{
    if (m <= 2)
    {
        y--;
        m += 12;
    }
    long A = y / 100;
    long B = 2 - A + A / 4;
    double jd0 = floor(365.25 * (y + 4716)) + floor(30.6001 * (m + 1)) + d + B - 1524.5;

    double T = (jd0 - 2451545.0) / 36525.0;

    double gmst0 = 100.46061837 + 36000.770053608 * T + 0.000387933 * T * T - T * T * T / 38710000.0;

    double gmst = gmst0 + (utc_hours * 15.041067);

    gmst = fmod(gmst, 360.0);
    if (gmst < 0)
        gmst += 360.0;
    return gmst;
}

void equatorialToHorizon(double ra_deg, double dec_deg, double lat, double lon,
                         int year, int month, int day, double utc_hours,
                         double &alt, double &az)
{
    double lst_deg = fmod(calcGMST_deg(year, month, day, utc_hours) + lon, 360.0);
    double ha_deg = lst_deg - ra_deg;

    double ha_rad = radians(ha_deg);
    double dec_rad = radians(dec_deg);
    double lat_rad = radians(lat);

    double sinAlt = sin(dec_rad) * sin(lat_rad) + cos(dec_rad) * cos(lat_rad) * cos(ha_rad);
    alt = degrees(asin(sinAlt));

    double y = -sin(ha_rad);
    double x = tan(dec_rad) * cos(lat_rad) - sin(lat_rad) * cos(ha_rad);

    az = degrees(atan2(y, x));

    if (az < 0)
        az += 360.0;
}
