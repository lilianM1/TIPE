#include "status_lcd.h"

#include "tipe_globals.h"

void actualiserLCD(float alt, float az, bool showTarget, float tAlt, float tAz)
{
    if (millis() - lastLcdUpdate < 250)
        return;

    lcd.setCursor(0, 0);
    lcd.print("A:");
    lcd.print(alt, 1);
    lcd.print(" Z:");
    lcd.print(az, 1);
    if (showTarget)
    {
        lcd.print(" T:");
        lcd.print(tAlt, 1);
        lcd.print("/");
        lcd.print((int)tAz);
        lcd.print(" ");
    }
    else
    {
        lcd.print("       ");
    }

    lcd.setCursor(0, 1);
    char buf[17];
    sprintf(buf, "Cal:S%d G%d A%d M%d", calSys, calGyr, calAcc, calMag);
    lcd.print(buf);

    if (calSys != lastColorState)
    {
        if (calSys == 3)
            lcd.setRGB(0, 255, 0);
        else if (calSys > 0)
            lcd.setRGB(255, 100, 0);
        else
            lcd.setRGB(255, 0, 0);
        lastColorState = calSys;
    }
    lastLcdUpdate = millis();
}
