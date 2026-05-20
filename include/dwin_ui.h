#pragma once

#include <Arduino.h>

void dwinSend(const uint8_t *data, uint8_t len);
void dwinForceRefresh();

void actualiserHeure(bool force = false);
void actualiserCibleBarre(bool force = false);

void afficherFond(uint8_t id);
void chargerVRAM1(uint8_t id);
void afficherCurseurMenu(uint8_t indexLigne);
void effacerCurseurMenu(uint8_t indexLigne);
void afficherConstellation(uint8_t indexGlobal);
void retourMenu();

void afficherAlerte(String message, uint16_t rectColor, uint16_t textColor);
void effacerEcran(uint16_t color);
