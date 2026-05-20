#include "tipe_globals.h"

const uint8_t DWIN_TAIL[4] = {0xCC, 0x33, 0xC3, 0x3C};

const uint8_t PIN_ENC_A = 2; // INT4 (Pin interruptible sur Mega)
const uint8_t PIN_ENC_B = 3; // INT5 (Pin interruptible sur Mega)
const uint8_t PIN_BTN = 50;
const uint8_t PIN_BEEP = 51;

volatile int8_t encoderDelta = 0;

const int Y_STEP_PIN = 60;
const int Y_DIR_PIN = 61;
const int Y_ENABLE_PIN = 56;
const int Z_STEP_PIN = 36;
const int Z_DIR_PIN = 34;
const int Z_ENABLE_PIN = 30;
const int Y_MIN_PIN = 14;

const float PAS_PAR_MM_Y = 400.0;
const float PAS_PAR_DEG_Z = 222.2222;
const float L_HOME = 75.6306;
const float THETA_MAX = 110.0;
const int V_Y = 200;
const int V_Z = 400;

TinyGPSPlus gps;
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28);
rgb_lcd lcd;

bool alerteActive = false;
unsigned long dernierClignotement = 0;
bool alerteVisible = false;

Etat etatActuel = MENU;

const InfoConstellation toutesConstellations[20] = {
    {6, true, 165.75, 61.75, "GrOurse"},
    {6, false, 37.75, 89.26, "PtOurse"},
    {7, true, 10.00, 56.50, "Cassiop"},
    {7, false, 269.00, 51.50, "Dragon"},
    {8, true, 319.50, 62.50, "Cephee"},
    {8, false, 83.80, -5.40, "Orion"},
    {9, true, 68.75, 16.50, "Taureau"},
    {9, false, 101.25, -16.70, "GdChien"},
    {10, true, 116.25, 28.00, "Gemaux"},
    {10, false, 79.00, 46.00, "Cocher"},
    {11, true, 152.00, 12.00, "Lion"},
    {11, false, 201.25, -11.10, "Vierge"},
    {12, true, 213.75, 19.10, "Bouvier"},
    {12, false, 250.25, 39.30, "Hercule"},
    {13, true, 310.25, 45.20, "Cygne"},
    {13, false, 279.00, 38.70, "Lyre"},
    {14, true, 297.50, 8.80, "Aigle"},
    {14, false, 247.25, -26.40, "Scorpio"},
    {15, true, 283.75, -26.30, "Sagitt"},
    {15, false, 2.00, 29.00, "Androme"}};

const PageInfo pages[5] = {{0, 5}, {5, 5}, {10, 4}, {14, 5}, {19, 1}};
const uint8_t fondsMenuParPage[5] = {1, 2, 3, 4, 5};

uint8_t pageActuelle = 0;
uint8_t indexSurPage = 0;
uint8_t ancienIndexSurPage = 0;
uint8_t anciennePage = 0;

const uint16_t positionX = 203;
const uint16_t positionsY[5] = {338, 366, 394, 422, 450};

bool isHomed = false;
long posStepsY = 0;
long posStepsZ = 0;
char movingCmd = 'S';
unsigned long lastSend = 0;
unsigned long lastLcdUpdate = 0;

uint8_t calSys = 0, calGyr = 0, calAcc = 0, calMag = 0;
uint8_t lastColorState = 255;

float targetAlt = 0, targetAz = 0;
char targetNom[8] = "";

unsigned long lastBNORead = 0;
float realAlt = 0, realAz = 0;

void smartDelay(unsigned long ms)
{
    unsigned long start = millis();
    do
    {
        while (GPS_SERIAL.available())
            gps.encode(GPS_SERIAL.read());
    } while (millis() - start < ms);
}
