#pragma once

#include <Arduino.h>
#include <Adafruit_BNO055.h>
#include <TinyGPS++.h>
#include "rgb_lcd.h"

// ==================== CONFIGURATION GÉNÉRALE ====================
#define DWIN_SERIAL Serial2
#define GPS_SERIAL Serial1

extern const uint8_t DWIN_TAIL[4];

// Encodeur + bouton + buzzer
extern const uint8_t PIN_ENC_A;
extern const uint8_t PIN_ENC_B;
extern const uint8_t PIN_BTN;
extern const uint8_t PIN_BEEP;

extern volatile int8_t encoderDelta;

// Pins moteurs (RAMPS 1.4)
extern const int Y_STEP_PIN;
extern const int Y_DIR_PIN;
extern const int Y_ENABLE_PIN;
extern const int Z_STEP_PIN;
extern const int Z_DIR_PIN;
extern const int Z_ENABLE_PIN;
extern const int Y_MIN_PIN;

// Mécanique
extern const float PAS_PAR_MM_Y;
extern const float PAS_PAR_DEG_Z;
extern const float L_HOME;
extern const float THETA_MAX;
extern const int V_Y;
extern const int V_Z;

// GPS / IMU / LCD
extern TinyGPSPlus gps;
extern Adafruit_BNO055 bno;
extern rgb_lcd lcd;

// ==================== ÉTATS DE L'INTERFACE ====================
enum Etat
{
    MENU,
    POINTING,
    CONSTELLATION
};

extern Etat etatActuel;

// ==================== STRUCTURES ====================
struct InfoConstellation
{
    uint8_t imageID;
    bool moitieHaute;
    float ra_deg;
    float dec_deg;
    const char *nom;
};

struct PageInfo
{
    uint8_t debut;
    uint8_t nb;
};

// Base de données
extern const InfoConstellation toutesConstellations[20];
extern const PageInfo pages[5];
extern const uint8_t fondsMenuParPage[5];

extern uint8_t pageActuelle;
extern uint8_t indexSurPage;
extern uint8_t ancienIndexSurPage;
extern uint8_t anciennePage;

extern const uint16_t positionX;
extern const uint16_t positionsY[5];

// ==================== VARIABLES DE TRACKING ====================
extern bool isHomed;
extern long posStepsY;
extern long posStepsZ;
extern char movingCmd;
extern unsigned long lastSend;
extern unsigned long lastLcdUpdate;

extern uint8_t calSys, calGyr, calAcc, calMag;
extern uint8_t lastColorState;

extern float targetAlt, targetAz;
extern char targetNom[8];

extern unsigned long lastBNORead;
extern float realAlt, realAz;

// alertes DWIN
extern bool alerteActive;
extern unsigned long dernierClignotement;
extern bool alerteVisible;

// Attente intelligente (ne bloque pas le GPS)
void smartDelay(unsigned long ms);
