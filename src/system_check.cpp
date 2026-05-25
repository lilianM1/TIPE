#include "system_check.h"
#include "tipe_globals.h"

StatutSysteme verifierSysteme()
{
    static unsigned long lastCheck = 0;
    static StatutSysteme dernierStatut = TOUT_OK;

    // On ne vérifie qu'une fois par seconde
    if (millis() - lastCheck < 1000)
    {
        return dernierStatut;
    }
    lastCheck = millis();

    // --- 1. VERIFICATION MATERIELLE DU BNO055 ---
    uint8_t system_status = 0, self_test_results = 0, system_error = 0;
    bno.getSystemStatus(&system_status, &self_test_results, &system_error);

    if (system_status == 0x01)
    {
        Serial.print("DBG: CRASH BNO -> Status: ");
        Serial.print(system_status);
        Serial.print(" | Error: ");
        Serial.println(system_error);

        dernierStatut = ERREUR_BNO;
        return ERREUR_BNO;
    }

    // --- 2. VERIFICATION MATERIELLE DU GPS (Câblage) ---
    // Si après 10 secondes d'allumage on n'a reçu aucune trame texte du GPS, il est débranché.
    if (millis() > 10000 && gps.charsProcessed() < 10)
    {
        Serial.println("DBG: CRASH GPS -> Aucun caractere recu (Verifier cablage RX/TX)");
        dernierStatut = ERREUR_GPS;
        return ERREUR_GPS;
    }

    // --- 3. VERIFICATION LOGICIELLE DU GPS (Signal Satellite) ---
    if (!gps.location.isValid() || !gps.time.isValid() || !gps.date.isValid())
    {
        dernierStatut = PAS_DE_FIX;
        return PAS_DE_FIX;
    }

    dernierStatut = TOUT_OK;
    return TOUT_OK;
}