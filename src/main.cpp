#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BNO055.h>
#include <EEPROM.h>
#include <TinyGPS++.h>
#include "rgb_lcd.h"

// ==================== CONFIGURATION GÉNÉRALE ====================
#define DWIN_SERIAL Serial2
const uint8_t DWIN_TAIL[] = {0xCC, 0x33, 0xC3, 0x3C};

unsigned long dernierClignotement = 0;
bool alerteVisible = false;

// Encodeur + bouton + buzzer
#define PIN_ENC_A 2 // INT4 (Pin interruptible sur Mega)
#define PIN_ENC_B 3 // INT5 (Pin interruptible sur Mega)
#define PIN_BTN 50
#define PIN_BEEP 51

volatile int8_t encoderDelta = 0;

// Pins moteurs (RAMPS 1.4)
const int Y_STEP_PIN = 60;
const int Y_DIR_PIN = 61;
const int Y_ENABLE_PIN = 56;
const int Z_STEP_PIN = 36;
const int Z_DIR_PIN = 34;
const int Z_ENABLE_PIN = 30;
const int Y_MIN_PIN = 14;

// Mécanique
const float PAS_PAR_MM_Y = 400.0;
const float PAS_PAR_DEG_Z = 222.2222;
const float L_HOME = 75.6306;
const float THETA_MAX = 110.0;
const int V_Y = 200; // délai en µs entre pas (vitesse)
const int V_Z = 400;

// GPS sur Serial3
TinyGPSPlus gps;
#define GPS_SERIAL Serial1

// BNO055
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x29);

// LCD Grove RGB
rgb_lcd lcd;

// DWIN
bool alerteActive = false;

// ==================== ÉTATS DE L'INTERFACE ====================
enum Etat
{
  MENU,
  POINTING,
  CONSTELLATION
};
Etat etatActuel = MENU;

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

// ==================== BASE DE DONNÉES DES CONSTELLATIONS ====================
const InfoConstellation toutesConstellations[20] = {
    {6, true, 165.75, 61.75, "GrOurse"},    // 1. Grande Ourse (Dubhé)
    {6, false, 37.75, 89.26, "PtOurse"},    // 2. Petite Ourse (Étoile Polaire)
    {7, true, 10.00, 56.50, "Cassiop"},     // 3. Cassiopée (Schedar)
    {7, false, 269.00, 51.50, "Dragon"},    // 4. Dragon (Eltanin)
    {8, true, 319.50, 62.50, "Cephee"},     // 5. Céphée (Alderamin)
    {8, false, 83.80, -5.40, "Orion"},      // 6. Orion (Centre)
    {9, true, 68.75, 16.50, "Taureau"},     // 7. Taureau (Aldébaran)
    {9, false, 101.25, -16.70, "GdChien"},  // 8. Grand Chien (Sirius)
    {10, true, 116.25, 28.00, "Gemaux"},    // 9. Gémeaux (Pollux)
    {10, false, 79.00, 46.00, "Cocher"},    // 10. Cocher (Capella)
    {11, true, 152.00, 12.00, "Lion"},      // 11. Lion (Régulus)
    {11, false, 201.25, -11.10, "Vierge"},  // 12. Vierge (Spica)
    {12, true, 213.75, 19.10, "Bouvier"},   // 13. Bouvier (Arcturus)
    {12, false, 250.25, 39.30, "Hercule"},  // 14. Hercule (Centre)
    {13, true, 310.25, 45.20, "Cygne"},     // 15. Cygne (Deneb)
    {13, false, 279.00, 38.70, "Lyre"},     // 16. Lyre (Véga)
    {14, true, 297.50, 8.80, "Aigle"},      // 17. Aigle (Altaïr)
    {14, false, 247.25, -26.40, "Scorpio"}, // 18. Scorpion (Antarès)
    {15, true, 283.75, -26.30, "Sagitt"},   // 19. Sagittaire (Nunki)
    {15, false, 2.00, 29.00, "Androme"}     // 20. Andromède (Alphératz)
};

const PageInfo pages[5] = {
    {0, 5}, {5, 5}, {10, 4}, {14, 5}, {19, 1}};

const uint8_t fondsMenuParPage[5] = {1, 2, 3, 4, 5};

uint8_t pageActuelle = 0;
uint8_t indexSurPage = 0;
uint8_t ancienIndexSurPage = 0;
uint8_t anciennePage = 0;

const uint16_t positionX = 203;
const uint16_t positionsY[5] = {338, 366, 394, 422, 450};

// ==================== VARIABLES DE TRACKING ====================
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

// ==================== INTERRUPTIONS ENCODEUR ====================
void ISR_encodeur()
{
  bool A = digitalRead(PIN_ENC_A);
  bool B = digitalRead(PIN_ENC_B);
  encoderDelta += (A != B) ? +1 : -1;
}

// ==================== FONCTIONS DWIN ====================
void dwinSend(const uint8_t *data, uint8_t len)
{
  DWIN_SERIAL.write(0xAA);
  DWIN_SERIAL.write(data, len);
  DWIN_SERIAL.write(DWIN_TAIL, 4);
}

void actualiserHeure(bool force = false)
{
  static int derniereMinute = -1;

  if (gps.time.minute() == derniereMinute && !force)
    return;

  derniereMinute = gps.time.minute();

  char bufferHeure[10];
  int heureLocale = (gps.time.hour() + 2) % 24;
  sprintf(bufferHeure, "%02d:%02d", heureLocale, gps.time.minute());

  uint8_t len = strlen(bufferHeure);
  uint8_t cmdText[10 + len];

  cmdText[0] = 0x11;
  cmdText[1] = 0x02;
  cmdText[2] = 0xFF;
  cmdText[3] = (uint8_t)0xFF;
  cmdText[4] = 0x00;
  cmdText[5] = 0x00;
  cmdText[6] = 0x00;
  cmdText[7] = 0x05;
  cmdText[8] = 0x00;
  cmdText[9] = 0x05;

  for (uint8_t i = 0; i < len; i++)
  {
    cmdText[10 + i] = bufferHeure[i];
  }

  dwinSend(cmdText, sizeof(cmdText));
}

void actualiserCibleBarre(bool force = false)
{
  static float lastAlt = -999;
  static float lastAz = -999;

  if (!force && lastAlt == targetAlt && lastAz == targetAz)
    return;
  lastAlt = targetAlt;
  lastAz = targetAz;

  char buffer[30];
  char altStr[8];
  char azStr[8];

  dtostrf(targetAlt, 4, 1, altStr);
  dtostrf(targetAz, 3, 0, azStr);

  sprintf(buffer, "T:%s/%s", altStr, azStr);

  uint8_t len = strlen(buffer);
  uint8_t cmdText[10 + len];

  cmdText[0] = 0x11;
  cmdText[1] = 0x02;
  cmdText[2] = 0xFF;
  cmdText[3] = 0xFF;
  cmdText[4] = 0x00;
  cmdText[5] = 0x00;
  cmdText[6] = 0x00;
  cmdText[7] = 0x9D;
  cmdText[8] = 0x00;
  cmdText[9] = 0x05;

  for (uint8_t i = 0; i < len; i++)
  {
    cmdText[10 + i] = buffer[i];
  }

  dwinSend(cmdText, sizeof(cmdText));
}

void dwinForceRefresh()
{
  uint8_t r[] = {0x11, 0x01, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20};
  dwinSend(r, sizeof(r));
}

void afficherFond(uint8_t id)
{
  uint8_t cmd[] = {0x22, 0x00, id};
  dwinSend(cmd, 3);
  delay(100);
  dwinForceRefresh();
}

void chargerVRAM1(uint8_t id)
{
  uint8_t cmd[] = {0x25, 0x01, id};
  dwinSend(cmd, 3);
  delay(100);
}

void afficherCurseurMenu(uint8_t indexLigne)
{
  uint16_t ox = positionX - 20;
  uint16_t oy = positionsY[indexLigne] - 20;
  uint8_t cmd[] = {
      0x27, 0x41,
      0x00, 0x00, 0x00, 0x00,
      0x00, 0x27, 0x00, 0x27,
      (uint8_t)(ox >> 8), (uint8_t)(ox & 0xFF),
      (uint8_t)(oy >> 8), (uint8_t)(oy & 0xFF)};
  dwinSend(cmd, sizeof(cmd));
  delay(20); // Réduit pour plus de fluidité
  dwinForceRefresh();
}

void effacerCurseurMenu(uint8_t indexLigne)
{
  uint16_t ox = positionX - 20;
  uint16_t oy = positionsY[indexLigne] - 20;
  uint16_t ox2 = ox + 39;
  uint16_t oy2 = oy + 39;
  uint8_t cmd[] = {
      0x27, 0x80,
      (uint8_t)(ox >> 8), (uint8_t)(ox & 0xFF),
      (uint8_t)(oy >> 8), (uint8_t)(oy & 0xFF),
      (uint8_t)(ox2 >> 8), (uint8_t)(ox2 & 0xFF),
      (uint8_t)(oy2 >> 8), (uint8_t)(oy2 & 0xFF),
      (uint8_t)(ox >> 8), (uint8_t)(ox & 0xFF),
      (uint8_t)(oy >> 8), (uint8_t)(oy & 0xFF)};
  dwinSend(cmd, sizeof(cmd));
  delay(10); // Réduit pour plus de fluidité
}



void afficherConstellation(uint8_t indexGlobal)
{
  InfoConstellation info = toutesConstellations[indexGlobal];
  chargerVRAM1(info.imageID);

  uint16_t xs = 0x0000;
  uint16_t xe = 0x01DF;
  uint16_t ys, ye;

  if (info.moitieHaute)
  {
    ys = 0x0000;
    ye = 0x00EF;
  }
  else
  {
    ys = 0x00F0;
    ye = 0x01DF;
  }

  uint16_t xdst = 0x0000;
  uint16_t ydst = 0x0000;

  uint8_t cmd[] = {
      0x27, 0x41,
      (uint8_t)(xs >> 8), (uint8_t)(xs & 0xFF),
      (uint8_t)(ys >> 8), (uint8_t)(ys & 0xFF),
      (uint8_t)(xe >> 8), (uint8_t)(xe & 0xFF),
      (uint8_t)(ye >> 8), (uint8_t)(ye & 0xFF),
      (uint8_t)(xdst >> 8), (uint8_t)(xdst & 0xFF),
      (uint8_t)(ydst >> 8), (uint8_t)(ydst & 0xFF)};

  dwinSend(cmd, sizeof(cmd));
  delay(50);
  dwinForceRefresh();

  actualiserHeure(true);
}

void retourMenu()
{
  afficherFond(fondsMenuParPage[pageActuelle]);
  chargerVRAM1(0);
  afficherCurseurMenu(indexSurPage);
  actualiserHeure(true);
  etatActuel = MENU;
}

// ==================== MOTEURS & BUZZER ====================
void fairePas(int s, int d, bool dir, int v)
{
  digitalWrite(d, dir);
  digitalWrite(s, HIGH);
  delayMicroseconds(10);
  digitalWrite(s, LOW);
  delayMicroseconds(v);
}

void faireBip(int duree)
{
  digitalWrite(PIN_BEEP, HIGH);
  delay(duree);
  digitalWrite(PIN_BEEP, LOW);
}

// ==================== HOMING ====================
void effectuerHoming()
{
  movingCmd = 'S';
  digitalWrite(Y_DIR_PIN, HIGH);
  lcd.setCursor(0, 0);
  lcd.print(" HOMING EN COURS");
  while (digitalRead(Y_MIN_PIN) == LOW)
  {
    fairePas(Y_STEP_PIN, Y_DIR_PIN, HIGH, 400);
  }
  delay(100);
  digitalWrite(Y_DIR_PIN, LOW);
  for (int i = 0; i < 1000; i++)
    fairePas(Y_STEP_PIN, Y_DIR_PIN, LOW, 400);
  while (digitalRead(Y_MIN_PIN) == LOW)
  {
    fairePas(Y_STEP_PIN, Y_DIR_PIN, HIGH, 1000);
  }
  isHomed = true;
  posStepsY = 0;
  posStepsZ = 0;
  Serial.println("HOMED");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Decollage capt.");
  delay(500);
  digitalWrite(Y_DIR_PIN, LOW);
  for (int i = 0; i < (20 * PAS_PAR_MM_Y); i++)
  {
    fairePas(Y_STEP_PIN, Y_DIR_PIN, LOW, 100);
    posStepsY++;
  }
  lcd.clear();
}

// ==================== EEPROM (offsets BNO055) ====================
const long MAGIC_NUMBER = 0x424E4F33;

void loadOffsets()
{
  int addr = 0;
  long magic;
  EEPROM.get(addr, magic);
  if (magic == MAGIC_NUMBER)
  {
    addr += sizeof(long);
    adafruit_bno055_offsets_t calib;
    EEPROM.get(addr, calib);
    bno.setSensorOffsets(calib);
    Serial.println("CONFIRM_LOAD");
  }
  else
  {
    Serial.println("ERROR_LOAD");
  }
}

void saveOffsets()
{
  if (!bno.isFullyCalibrated())
  {
    Serial.println("ERR_CALIB");
    return;
  }
  adafruit_bno055_offsets_t calib;
  bno.getSensorOffsets(calib);
  int addr = 0;
  EEPROM.put(addr, MAGIC_NUMBER);
  addr += sizeof(long);
  EEPROM.put(addr, calib);
  Serial.println("CONFIRM_SAVE");
}

// ==================== LECTURE BNO055 ====================
void readBNOAltAz(float &alt, float &az)
{
  imu::Quaternion q = bno.getQuat();
  bno.getCalibration(&calSys, &calGyr, &calAcc, &calMag);
  float qw = q.w(), qx = q.x(), qy = q.y(), qz = q.z();
  float rawYaw = -degrees(atan2(2.0 * (qw * qz + qx * qy), 1.0 - 2.0 * (qy * qy + qz * qz)));
  az = fmod((rawYaw + 360.0), 360.0);
  if (az < 0)
    az += 360.0;
  alt = degrees(atan2(2.0 * (qw * qx + qy * qz), 1.0 - 2.0 * (qx * qx + qy * qy)));
}

// ==================== GESTION LCD ====================
void actualiserLCD(float alt, float az, bool showTarget = false, float tAlt = 0, float tAz = 0)
{
  if (millis() - lastLcdUpdate < 250)
    return;

  lcd.setCursor(0, 0);
  lcd.print("A:");
  lcd.print(alt, 1);
  lcd.print(" Z:");
  lcd.print(az, 1);
  if (showTarget)
  {
    lcd.print(" T:");
    lcd.print(tAlt, 1);
    lcd.print("/");
    lcd.print((int)tAz);
    lcd.print(" ");
  }
  else
  {
    lcd.print("       ");
  }

  lcd.setCursor(0, 1);
  char buf[17];
  sprintf(buf, "Cal:S%d G%d A%d M%d", calSys, calGyr, calAcc, calMag);
  lcd.print(buf);

  if (calSys != lastColorState)
  {
    if (calSys == 3)
      lcd.setRGB(0, 255, 0);
    else if (calSys > 0)
      lcd.setRGB(255, 100, 0);
    else
      lcd.setRGB(255, 0, 0);
    lastColorState = calSys;
  }
  lastLcdUpdate = millis();
}

// ==================== GESTION DWIN ====================
void afficherAlerte(String message, uint16_t rectColor, uint16_t textColor)
{
  uint8_t cmdRect[] = {
      0x05, 0x01,
      (uint8_t)(rectColor >> 8), (uint8_t)(rectColor & 0xFF),
      0x00, 0x10, 0x00, 0xD2,
      0x01, 0x00, 0x01, 0x0E};
  dwinSend(cmdRect, sizeof(cmdRect));
  delay(50);

  if (message.length() > 30)
    message = message.substring(0, 30);
  uint8_t len = message.length();
  uint8_t cmdText[10 + len];
  cmdText[0] = 0x11;
  cmdText[1] = 0x05;
  cmdText[2] = 0x00;
  cmdText[3] = 0x00;
  cmdText[4] = (uint8_t)(textColor >> 8);
  cmdText[5] = (uint8_t)(textColor & 0xFF);
  cmdText[6] = 0x00;
  cmdText[7] = 0x18;
  cmdText[8] = 0x00;
  cmdText[9] = 0xE0;
  for (uint8_t i = 0; i < len; i++)
    cmdText[10 + i] = message[i];
  dwinSend(cmdText, sizeof(cmdText));
  delay(50);
  dwinForceRefresh();
  alerteActive = true;
}

void effacerEcran(uint16_t color)
{
  uint8_t cmd[] = {
      0x01,
      (uint8_t)(color >> 8), (uint8_t)(color & 0xFF)};
  dwinSend(cmd, sizeof(cmd));
  delay(10);
}

// ==================== CALCULS ASTRONOMIQUES ====================
double calcJulianDate(int y, int m, int d, double utc_hours)
{
  if (m <= 2)
  {
    y--;
    m += 12;
  }
  int A = y / 100;
  int B = 2 - A + A / 4;
  double jd = floor(365.25 * (y + 4716)) + floor(30.6001 * (m + 1)) + d + B - 1524.5;
  jd += utc_hours / 24.0;
  return jd;
}

double calcGMST_deg(int y, int m, int d, double utc_hours)
{
  // Calcul du jour Julien à 0h UTC
  if (m <= 2)
  {
    y--;
    m += 12;
  }
  long A = y / 100;
  long B = 2 - A + A / 4;
  double jd0 = floor(365.25 * (y + 4716)) + floor(30.6001 * (m + 1)) + d + B - 1524.5;

  double T = (jd0 - 2451545.0) / 36525.0;

  // Temps sidéral moyen à Greenwich à 0h UTC (en degrés)
  double gmst0 = 100.46061837 + 36000.770053608 * T + 0.000387933 * T * T - T * T * T / 38710000.0;

  // Ajouter la rotation de la Terre depuis 0h UTC (15.041067 degrés par heure solaire)
  double gmst = gmst0 + (utc_hours * 15.041067);

  gmst = fmod(gmst, 360.0);
  if (gmst < 0)
    gmst += 360.0;
  return gmst;
}

void equatorialToHorizon(double ra_deg, double dec_deg, double lat, double lon,
                         int year, int month, int day, double utc_hours,
                         double &alt, double &az)
{
  double lst_deg = fmod(calcGMST_deg(year, month, day, utc_hours) + lon, 360.0);
  double ha_deg = lst_deg - ra_deg;

  double ha_rad = radians(ha_deg);
  double dec_rad = radians(dec_deg);
  double lat_rad = radians(lat);

  // Altitude
  double sinAlt = sin(dec_rad) * sin(lat_rad) + cos(dec_rad) * cos(lat_rad) * cos(ha_rad);
  alt = degrees(asin(sinAlt));

  // Azimut (Correction mathématique)
  double y = -sin(ha_rad);
  double x = tan(dec_rad) * cos(lat_rad) - sin(lat_rad) * cos(ha_rad);

  az = degrees(atan2(y, x));

  if (az < 0)
    az += 360.0;
}
// ==================== POINTAGE EN BOUCLE FERMÉE ====================
bool ajusterPasBoucleFermee(float cibleAlt, float cibleAz)
{
  float realAltCible, realAzCible;
  readBNOAltAz(realAltCible, realAzCible); // Lecture temps réel nécessaire ici

  const float tolAlt = 0.5;
  const float tolAz = 1.0;

  float errAlt = cibleAlt - realAltCible;
  float errAz = cibleAz - realAzCible;

  while (errAz > 180)
    errAz -= 360;
  while (errAz < -180)
    errAz += 360;

  bool mouvement = false;
  // Altitude
  if (fabs(errAlt) > tolAlt)
  {
    if (errAlt > 0)
    {
      float L = L_HOME + (posStepsY / PAS_PAR_MM_Y);
      float cosT = (88400.0 - (L * L)) / 88000.0;
      float angY = acos(constrain(cosT, -1, 1)) * 180.0 / PI;
      if (angY < THETA_MAX)
      {
        fairePas(Y_STEP_PIN, Y_DIR_PIN, LOW, V_Y);
        posStepsY++;
      }
    }
    else
    {
      if (digitalRead(Y_MIN_PIN) == LOW)
      {
        fairePas(Y_STEP_PIN, Y_DIR_PIN, HIGH, V_Y);
        posStepsY--;
      }
    }
    mouvement = true;
  }

  // Azimut
  if (fabs(errAz) > tolAz)
  {
    if (errAz > 0)
    {
      fairePas(Z_STEP_PIN, Z_DIR_PIN, HIGH, V_Z);
      posStepsZ++;
    }
    else
    {
      fairePas(Z_STEP_PIN, Z_DIR_PIN, LOW, V_Z);
      posStepsZ--;
    }
    mouvement = true;
  }

  return (!mouvement);
}

bool pointerVersCible()
{
  static unsigned long startTime = 0;
  if (startTime == 0)
    startTime = millis();

  if (millis() - startTime > 60000)
  {
    Serial.println("POINTING_TIMEOUT");
    startTime = 0;
    return true;
  }

  bool termine = ajusterPasBoucleFermee(targetAlt, targetAz);
  if (termine)
    startTime = 0;
  return termine;
}

// =================== Test Materiel ====================
enum StatutSysteme
{
  TOUT_OK,
  ERREUR_MATERIELLE,
  PAS_DE_FIX
};

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

// ==================== SETUP ====================
void setup()
{
  Serial.begin(115200);
  DWIN_SERIAL.begin(115200);
  GPS_SERIAL.begin(9600);

  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_BTN, INPUT_PULLUP);
  pinMode(PIN_BEEP, OUTPUT);
  digitalWrite(PIN_BEEP, LOW);

  // Attachement de l'interruption pour l'encodeur
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), ISR_encodeur, CHANGE);

  pinMode(Y_STEP_PIN, OUTPUT);
  pinMode(Y_DIR_PIN, OUTPUT);
  pinMode(Y_ENABLE_PIN, OUTPUT);
  pinMode(Z_STEP_PIN, OUTPUT);
  pinMode(Z_DIR_PIN, OUTPUT);
  pinMode(Z_ENABLE_PIN, OUTPUT);
  pinMode(Y_MIN_PIN, INPUT_PULLUP);
  digitalWrite(Y_ENABLE_PIN, LOW);
  digitalWrite(Z_ENABLE_PIN, LOW);

  effacerEcran(0x0000);

  lcd.begin(16, 2);
  lcd.setRGB(255, 255, 255);
  lcd.print("Init BNO...");
  afficherAlerte("Init BNO055", 0xFFE0, 0x0000);

  if (!bno.begin())
  {
    Serial.println("ERREUR_BNO");
    lcd.setRGB(255, 0, 0);
    lcd.setCursor(0, 1);
    lcd.print("ERR: BNO absent");
    afficherAlerte("ERR BNO055", 0xF800, 0xFFFF);
    while (1)
      ;
  }
  else
  {
    loadOffsets();
    bno.setExtCrystalUse(true);
    Wire.setClock(400000);
    afficherAlerte("BNO055 ready", 0x07E0, 0x0000);
    alerteActive = false;
  }

  delay(1000);

  unsigned long debutTestGps = millis();
  bool gpsPresent = false;
  afficherAlerte("Init GPS", 0xFFE0, 0x0000);
  while (millis() - debutTestGps < 5000)
  {
    if (GPS_SERIAL.available() > 0)
    {
      gpsPresent = true;
      break;
    }
  }

  if (!gpsPresent)
  {
    afficherAlerte("ERR GPS", 0xF800, 0xFFFF);
    while (1)
      ;
  }
  else if (gpsPresent)
  {
    afficherAlerte("GPS ready", 0x07E0, 0x0000);
    delay(1000);
    alerteActive = false;
  }

  afficherAlerte("HOMING", 0xFFE0, 0x0000);
  effectuerHoming();
  afficherAlerte("HOMING OK", 0x07E0, 0x0000);
  alerteActive = false;
  delay(1000);

  afficherFond(fondsMenuParPage[0]);
  chargerVRAM1(0);
  afficherCurseurMenu(0);
}

// ==================== LOOP ====================
void loop()
{
  while (GPS_SERIAL.available())
  {
    gps.encode(GPS_SERIAL.read());
  }

  if (gps.time.isValid())
  {
    actualiserHeure();
  }

  StatutSysteme statut = verifierSysteme();
  if (statut == ERREUR_MATERIELLE)
  {
    effacerEcran(0x0000);
    afficherAlerte("ERREUR CAPTEUR !", 0xF800, 0xFFFF);
    while (1)
      ;
  }
  if (statut == PAS_DE_FIX)
  {
    if (millis() - dernierClignotement > 500)
    {
      dernierClignotement = millis();
      alerteVisible = !alerteVisible;

      if (alerteVisible)
      {
        afficherAlerte("PAS DE FIX GPS", 0xFFE0, 0x0000);
      }
      else
      {
        afficherFond(fondsMenuParPage[pageActuelle]);
        if (etatActuel == MENU)
          afficherCurseurMenu(indexSurPage);
      }
    }
    return;
  }
  else if (statut == TOUT_OK && alerteActive)
  {
    alerteActive = false;
    afficherFond(fondsMenuParPage[pageActuelle]);
    if (etatActuel == MENU)
      afficherCurseurMenu(indexSurPage);
    faireBip(200);
  }

  // Throttle la lecture BNO055 pour la loop
  if (millis() - lastBNORead > 100)
  {
    readBNOAltAz(realAlt, realAz);
    lastBNORead = millis();
  }

  if (etatActuel == POINTING || etatActuel == CONSTELLATION)
  {
    actualiserLCD(realAlt, realAz, true, targetAlt, targetAz);
  }
  else
  {
    actualiserLCD(realAlt, realAz);
  }

  switch (etatActuel)
  {
  case MENU:
  {
    int8_t delta = 0;
    noInterrupts();
    delta = encoderDelta;
    encoderDelta = 0;
    interrupts();

    if (delta != 0)
    {
      ancienIndexSurPage = indexSurPage;
      anciennePage = pageActuelle;

      if (delta < 0)
      {
        if (indexSurPage == 0)
        {
          if (pageActuelle > 0)
          {
            pageActuelle--;
            indexSurPage = pages[pageActuelle].nb - 1;
          }
        }
        else
        {
          indexSurPage--;
        }
      }
      else
      {
        uint8_t maxIndex = pages[pageActuelle].nb - 1;
        if (indexSurPage == maxIndex)
        {
          if (pageActuelle < 4)
          {
            pageActuelle++;
            indexSurPage = 0;
          }
        }
        else
        {
          indexSurPage++;
        }
      }

      if (anciennePage != pageActuelle)
      {
        afficherFond(fondsMenuParPage[pageActuelle]);
        chargerVRAM1(0);
        afficherCurseurMenu(indexSurPage);
      }
      else
      {
        effacerCurseurMenu(ancienIndexSurPage);
        afficherCurseurMenu(indexSurPage);
      }
      faireBip(15);
    }

    if (digitalRead(PIN_BTN) == LOW)
    {
      delay(20); // Réduit légèrement
      if (digitalRead(PIN_BTN) == LOW)
      {
        uint8_t idxGlobal = pages[pageActuelle].debut + indexSurPage;
        InfoConstellation cible = toutesConstellations[idxGlobal];

        double lat = gps.location.lat();
        double lon = gps.location.lng();
        int year = gps.date.year();
        int month = gps.date.month();
        int day = gps.date.day();
        double utc = gps.time.hour() + gps.time.minute() / 60.0 + gps.time.second() / 3600.0;

        double tAlt, tAz;
        equatorialToHorizon(cible.ra_deg, cible.dec_deg, lat, lon,
                            year, month, day, utc, tAlt, tAz);

        targetAlt = tAlt;
        targetAz = tAz;
        strncpy(targetNom, cible.nom, 7);
        targetNom[7] = '\0';

        afficherConstellation(idxGlobal);
        actualiserHeure();
        Serial.print("targetAlt=");
        Serial.print(targetAlt);
        Serial.print(" targetAz=");
        Serial.println(targetAz);
        actualiserCibleBarre(true);
        actualiserCibleBarre(true);

        faireBip(150);
        etatActuel = POINTING;

        while (digitalRead(PIN_BTN) == LOW)
          ;
      }
    }
  }
  break;

  case POINTING:
    if (digitalRead(PIN_BTN) == LOW)
    {
      delay(20);
      if (digitalRead(PIN_BTN) == LOW)
      {
        faireBip(100);

        retourMenu();
        actualiserHeure();

        while (digitalRead(PIN_BTN) == LOW)
          ;

        break;
      }
    }

    if (pointerVersCible())
    {
      faireBip(200);
      etatActuel = CONSTELLATION;
    }
    break;

  case CONSTELLATION:
    if (digitalRead(PIN_BTN) == LOW)
    {
      delay(20);
      if (digitalRead(PIN_BTN) == LOW)
      {
        faireBip(100);
        retourMenu();
        actualiserHeure();
        while (digitalRead(PIN_BTN) == LOW)
          ;
      }
    }
    break;
  }
}