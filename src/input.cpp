#include "input.h"

#include "tipe_globals.h"

void ISR_encodeur()
{
    static unsigned long lastInterruptTime = 0;
    unsigned long interruptTime = millis();

    // 15ms est parfait pour filtrer les micro-rebonds sans rater les rotations rapides
    if (interruptTime - lastInterruptTime > 15) 
    {
        // On vérifie que A est bien remonté à l'état HAUT (RISING)
        if (digitalRead(PIN_ENC_A) == HIGH) 
        {
            bool B = digitalRead(PIN_ENC_B);
            
            // La logique s'inverse par rapport au FALLING :
            // Si B est LOW à la remontée de A, c'est qu'on va à droite !
            if (B == LOW) {
                encoderDelta += 1;
            } else {
                encoderDelta -= 1;
            }
            lastInterruptTime = interruptTime;
        }
    }
}

void faireBip(int duree)
{
    digitalWrite(PIN_BEEP, HIGH);
    delay(duree);
    digitalWrite(PIN_BEEP, LOW);
}
