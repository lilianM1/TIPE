#include "pointing.h"
#include "tipe_globals.h"
#include "imu.h"
#include "motors.h"
#include <math.h>

bool ajusterPasBoucleFermee(float cibleAlt, float cibleAz)
{
    float realAltCible, realAzCible;
    readBNOAltAz(realAltCible, realAzCible);

    const float tolAlt = 0.5;
    const float tolAz = 1.0;

    float errAlt = cibleAlt - realAltCible;
    float errAz = cibleAz - realAzCible;

    while (errAz > 180) errAz -= 360;
    while (errAz < -180) errAz += 360;

    bool mouvement = false;

    // --- 1. AXE AZIMUT (Z - Mouvement linéaire) ---
    if (fabs(errAz) > tolAz)
    {
        // Calcul du nombre de pas exact à faire
        long pas_Z_a_faire = errAz * PAS_PAR_DEG_Z;

        if (pas_Z_a_faire > 0) 
        {
            for (long i = 0; i < pas_Z_a_faire; i++) {
                fairePas(Z_STEP_PIN, Z_DIR_PIN, HIGH, V_Z);
                posStepsZ++;
            }
        } 
        else 
        {
            for (long i = 0; i < -pas_Z_a_faire; i++) { // -pas_Z pour le rendre positif
                fairePas(Z_STEP_PIN, Z_DIR_PIN, LOW, V_Z);
                posStepsZ--;
            }
        }
        mouvement = true;
    }

    // --- 2. AXE ALTITUDE (Y - Cinématique Inverse) ---
    if (fabs(errAlt) > tolAlt && cibleAlt < THETA_MAX)
    {
        // Où sommes-nous VRAIMENT sur la tige filetée selon le capteur ?
        float cosT_actuel = cos(realAltCible * PI / 180.0);
        float L_actuel = sqrt(88400.0 - 88000.0 * cosT_actuel);
        
        // Quelle doit être la longueur de la tige pour l'angle cible ?
        float cosT_cible = cos(cibleAlt * PI / 180.0);
        float L_cible = sqrt(88400.0 - 88000.0 * cosT_cible);
        
        // Conversion de la différence (en mm) en nombre de pas
        float delta_L = L_cible - L_actuel;
        long pas_Y_a_faire = delta_L * PAS_PAR_MM_Y;

        if (pas_Y_a_faire > 0) // Montée
        {
            for (long i = 0; i < pas_Y_a_faire; i++) {
                fairePas(Y_STEP_PIN, Y_DIR_PIN, LOW, V_Y);
                posStepsY++;
            }
        }
        else // Descente
        {
            for (long i = 0; i < -pas_Y_a_faire; i++) {
                if (digitalRead(Y_MIN_PIN) == LOW) { // Sécurité fin de course
                    fairePas(Y_STEP_PIN, Y_DIR_PIN, HIGH, V_Y);
                    posStepsY--;
                } else {
                    break; 
                }
            }
        }
        mouvement = true;
    }

    return (!mouvement);
}

bool pointerVersCible()
{
    Serial.println("\nDBG: === Appel pointerVersCible() ===");
    static unsigned long startTime = 0;
    
    if (startTime == 0)
    {
        startTime = millis();
        Serial.print("DBG: Demarrage Chrono = "); Serial.println(startTime);
    }

    unsigned long elapsed = millis() - startTime;
    Serial.print("DBG: Temps ecoule: "); Serial.println(elapsed);

    if (elapsed > 60000)
    {
        Serial.println("DBG: ERREUR -> POINTING_TIMEOUT (60s)");
        startTime = 0;
        return true;
    }

    Serial.print("DBG: Cibles globales -> targetAlt: "); Serial.print(targetAlt);
    Serial.print(" | targetAz: "); Serial.println(targetAz);

    bool termine = ajusterPasBoucleFermee(targetAlt, targetAz);

    Serial.print("DBG: pointerVersCible retourne -> ");
    Serial.println(termine ? "TRUE (Fini)" : "FALSE (En cours)");
    Serial.println("DBG: ==================================\n");

    if (termine) startTime = 0;
    return termine;
}