#pragma once

#include <Arduino.h>

enum StatutSysteme
{
    TOUT_OK,
    ERREUR_MATERIELLE,
    PAS_DE_FIX
};

StatutSysteme verifierSysteme();
