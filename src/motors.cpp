#include "motors.h"

#include "tipe_globals.h"

void fairePas(int s, int d, bool dir, int v)
{
    digitalWrite(d, dir);
    digitalWrite(s, HIGH);
    delayMicroseconds(10);
    digitalWrite(s, LOW);
    delayMicroseconds(v);
}

void effectuerHoming()
{
    movingCmd = 'S';
    digitalWrite(Y_DIR_PIN, HIGH);
    lcd.setCursor(0, 0);
    lcd.print(" HOMING EN COURS");
    while (digitalRead(Y_MIN_PIN) == LOW)
    {
        fairePas(Y_STEP_PIN, Y_DIR_PIN, HIGH, 400);
    }
    delay(100);
    digitalWrite(Y_DIR_PIN, LOW);
    for (int i = 0; i < 1000; i++)
        fairePas(Y_STEP_PIN, Y_DIR_PIN, LOW, 400);
    while (digitalRead(Y_MIN_PIN) == LOW)
    {
        fairePas(Y_STEP_PIN, Y_DIR_PIN, HIGH, 1000);
    }
    isHomed = true;
    posStepsY = 0;
    posStepsZ = 0;
    Serial.println("HOMED");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Decollage capt.");
    delay(500);
    digitalWrite(Y_DIR_PIN, LOW);
    for (int i = 0; i < (20 * PAS_PAR_MM_Y); i++)
    {
        fairePas(Y_STEP_PIN, Y_DIR_PIN, LOW, 100);
        posStepsY++;
    }
    lcd.clear();
}
