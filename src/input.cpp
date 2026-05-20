#include "input.h"

#include "tipe_globals.h"

void ISR_encodeur()
{
    bool A = digitalRead(PIN_ENC_A);
    bool B = digitalRead(PIN_ENC_B);
    encoderDelta += (A != B) ? +1 : -1;
}

void faireBip(int duree)
{
    digitalWrite(PIN_BEEP, HIGH);
    delay(duree);
    digitalWrite(PIN_BEEP, LOW);
}
