#include "pointing.h"
#include "tipe_globals.h"
#include "imu.h"
#include "motors.h"
#include "status_lcd.h"
#include <math.h>

bool ajusterPasBoucleFermee(float cibleAlt, float cibleAz, float tolAlt = 0.5, float tolAz = 1.0)
{
    float realAltCible, realAzCible;
    readBNOAltAz(realAltCible, realAzCible);

    float errAlt = cibleAlt - realAltCible;
    float errAz = cibleAz - realAzCible;

    // Normalisation de l'erreur d'azimut (chemin le plus court)
    while (errAz > 180) errAz -= 360;
    while (errAz < -180) errAz += 360;

    bool mouvement = false;

    // --- 1. CALCUL DES PAS A FAIRE POUR Z (AZIMUT) ---
    long pas_Z_total = 0;
    if (fabs(errAz) > tolAz) {
        pas_Z_total = errAz * PAS_PAR_DEG_Z;
    }
    long abs_Z = abs(pas_Z_total);
    // Inversion de direction car le moteur est branché sur E1
    bool dir_Z = (pas_Z_total > 0) ? LOW : HIGH; 
    int inc_Z = (pas_Z_total > 0) ? 1 : -1;

    // --- 2. CALCUL DES PAS A FAIRE POUR Y (ALTITUDE) ---
    long pas_Y_total = 0;
    if (fabs(errAlt) > tolAlt && cibleAlt < THETA_MAX) {
        // Cinématique inverse : on calcule la longueur exacte de la tige filetée
        float cosT_actuel = cos(realAltCible * PI / 180.0);
        float L_actuel = sqrt(88400.0 - 88000.0 * cosT_actuel);
        
        float cosT_cible = cos(cibleAlt * PI / 180.0);
        float L_cible = sqrt(88400.0 - 88000.0 * cosT_cible);
        
        pas_Y_total = (L_cible - L_actuel) * PAS_PAR_MM_Y;
    }
    long abs_Y = abs(pas_Y_total);
    bool dir_Y = (pas_Y_total > 0) ? LOW : HIGH; // LOW = Montée, HIGH = Descente
    int inc_Y = (pas_Y_total > 0) ? 1 : -1;

    // --- 3. ALGORITHME DE BRESENHAM (MOUVEMENT SIMULTANÉ ET FLUIDE) ---
    if (abs_Y > 0 || abs_Z > 0) {
        mouvement = true;
        
        long master_pas = max(abs_Y, abs_Z);
        long slave_pas = min(abs_Y, abs_Z);
        long err = master_pas / 2;
        
        bool y_is_master = (abs_Y >= abs_Z);

        for (long i = 0; i < master_pas; i++) {
            // Arrêt d'urgence (retour au menu immédiat si bouton pressé)
            if (digitalRead(PIN_BTN) == LOW) return true;

            // --- ACTUALISATION LCD (Toutes les 250ms max) ---
            if (millis() - lastLcdUpdate >= 250) {
                float tempAlt, tempAz;
                readBNOAltAz(tempAlt, tempAz); // Lecture de l'angle en direct
                actualiserLCD(tempAlt, tempAz, true, cibleAlt, cibleAz);
            }
            // ------------------------------------------------

            bool step_Y = false;
            bool step_Z = false;

            // Logique de distribution des pas
            if (y_is_master) {
                step_Y = true;
                err -= slave_pas;
                if (err < 0) {
                    step_Z = true;
                    err += master_pas;
                }
            } else {
                step_Z = true;
                err -= slave_pas;
                if (err < 0) {
                    step_Y = true;
                    err += master_pas;
                }
            }

            // Exécution du pas Y (Altitude) avec sécurité matérielle
            if (step_Y) {
                // Si on descend (HIGH) et que le fin de course est déclenché
                if (dir_Y == HIGH && digitalRead(Y_MIN_PIN) != LOW) {
                    break; // Arrêt de l'axe pour protéger la mécanique
                }
                fairePas(Y_STEP_PIN, Y_DIR_PIN, dir_Y, V_Y);
                posStepsY += inc_Y;
            }

            // Exécution du pas Z (Azimut)
            if (step_Z) {
                fairePas(Z_STEP_PIN, Z_DIR_PIN, dir_Z, V_Z);
                posStepsZ += inc_Z;
            }
        }
    }

    return (!mouvement); // Retourne TRUE si la cible est atteinte
}

bool pointerVersCible()
{
    Serial.println("\nDBG: === Appel pointerVersCible() ===");
    static unsigned long startTime = 0;

    if (startTime == 0)
    {
        startTime = millis();
        Serial.print("DBG: Demarrage Chrono = ");
        Serial.println(startTime);
    }

    unsigned long elapsed = millis() - startTime;
    Serial.print("DBG: Temps ecoule: ");
    Serial.println(elapsed);

    if (elapsed > 60000)
    {
        Serial.println("DBG: ERREUR -> POINTING_TIMEOUT (60s)");
        startTime = 0;
        return true;
    }

    Serial.print("DBG: Cibles globales -> targetAlt: ");
    Serial.print(targetAlt);
    Serial.print(" | targetAz: ");
    Serial.println(targetAz);

    bool termine = ajusterPasBoucleFermee(targetAlt, targetAz, 0.5, 1.0);

    Serial.print("DBG: pointerVersCible retourne -> ");
    Serial.println(termine ? "TRUE (Fini)" : "FALSE (En cours)");
    Serial.println("DBG: ==================================\n");

    if (termine)
        startTime = 0;
    return termine;
}