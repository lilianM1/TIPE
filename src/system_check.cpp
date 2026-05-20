#include "system_check.h"

#include "tipe_globals.h"

StatutSysteme verifierSysteme()
{
    uint8_t system_status, self_test_results, system_error;
    bno.getSystemStatus(&system_status, &self_test_results, &system_error);

    if (system_status == 0x01 || system_error != 0x00)
    {
        return ERREUR_MATERIELLE;
    }

    if (gps.charsProcessed() < 10 || (gps.location.isValid() && gps.location.age() > 2000))
    {
        // Sécurité supplémentaire possible
    }

    if (!gps.location.isValid() || !gps.time.isValid() || !gps.date.isValid())
    {
        return PAS_DE_FIX;
    }

    return TOUT_OK;
}
