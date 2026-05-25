#pragma once

#include <Arduino.h>

enum StatutSysteme
{
    TOUT_OK,
    ERREUR_BNO,      // Panne I2C ou composant grillé
    ERREUR_GPS,      // Panne série (fil débranché)
    PAS_DE_FIX       // Le GPS marche mais n'a pas de signal satellite
};

StatutSysteme verifierSysteme();
