#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>

#include "tipe_globals.h"

#include "astro.h"
#include "dwin_ui.h"
#include "imu.h"
#include "input.h"
#include "motors.h"
#include "pointing.h"
#include "status_lcd.h"
#include "system_check.h"

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
  // Écouter la fin du cran (RISING) stabilise parfaitement les deux directions
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), ISR_encodeur, RISING);

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
    // loadOffsets();
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
    smartDelay(1000);
    alerteActive = false;
  }

  afficherAlerte("HOMING", 0xFFE0, 0x0000);
  effectuerHoming();
  afficherAlerte("HOMING OK", 0x07E0, 0x0000);
  alerteActive = false;
  smartDelay(1000);

  afficherFond(fondsMenuParPage[0]);
  chargerVRAM1(0);
  afficherCurseurMenu(0);
}

// ==================== LOOP ====================
void loop()
{
  // --- SAUVEGARDE MANUELLE DE LA CALIBRATION ---
  // Si tu envoies un 'S' majuscule ou minuscule depuis le moniteur série du PC
  if (Serial.available() > 0)
  {
    char cmd = Serial.read();
    if (cmd == 'S' || cmd == 's')
    {
      saveOffsets();
      faireBip(500); // Un long bip pour confirmer que c'est bien écrit dans l'EEPROM !
    }
  }
  // ---------------------------------------------
  while (GPS_SERIAL.available())
  {
    gps.encode(GPS_SERIAL.read());
  }

  if (gps.time.isValid())
  {
    actualiserHeure();
  }

  StatutSysteme statut = verifierSysteme();
  if (statut == ERREUR_BNO)
  {
    effacerEcran(0x0000);
    afficherAlerte("PANNE BNO055", 0xF800, 0xFFFF);
    while (1)
      ; // On bloque le système
  }
  else if (statut == ERREUR_GPS)
  {
    effacerEcran(0x0000);
    afficherAlerte("PANNE CABLAGE GPS", 0xF800, 0xFFFF);
    while (1)
      ; // On bloque le système
  }
  // Fin du nouveau bloc

  else if (statut == PAS_DE_FIX)
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
      // faireBip(15);
    }

    if (digitalRead(PIN_BTN) == LOW)
    {
      smartDelay(20); // Réduit légèrement
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

        // faireBip(150);
        etatActuel = POINTING;

        while (digitalRead(PIN_BTN) == LOW)
        {
          while (GPS_SERIAL.available())
            gps.encode(GPS_SERIAL.read());
        }
      }
    }
  }
  break;

  case POINTING:
    if (digitalRead(PIN_BTN) == LOW)
    {
      smartDelay(20);
      if (digitalRead(PIN_BTN) == LOW)
      {
        faireBip(100);

        retourMenu();
        actualiserHeure();

        while (digitalRead(PIN_BTN) == LOW)
        {
          while (GPS_SERIAL.available())
            gps.encode(GPS_SERIAL.read());
        }

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
  {
    // 1. Toujours écouter le bouton d'arrêt pour revenir au menu
    if (digitalRead(PIN_BTN) == LOW)
    {
      smartDelay(20);
      if (digitalRead(PIN_BTN) == LOW)
      {
        faireBip(100);
        retourMenu();
        actualiserHeure();
        while (digitalRead(PIN_BTN) == LOW)
        {
          while (GPS_SERIAL.available())
            gps.encode(GPS_SERIAL.read());
        }
        break; // Sort du case
      }
    }

    // 2. Le mode Suivi Sidéral (Tracking)
    static unsigned long lastTrackingCheck = 0;

    // On vérifie l'alignement toutes les 2 secondes (2000 ms)
    if (millis() - lastTrackingCheck > 2000)
    {
      // On récupère la cible actuellement sélectionnée
      uint8_t idxGlobal = pages[pageActuelle].debut + indexSurPage;
      InfoConstellation cible = toutesConstellations[idxGlobal];

      // On récupère le temps GPS ACTUEL (la Terre a tourné !)
      double lat = gps.location.lat();
      double lon = gps.location.lng();
      int year = gps.date.year();
      int month = gps.date.month();
      int day = gps.date.day();
      double utc = gps.time.hour() + gps.time.minute() / 60.0 + gps.time.second() / 3600.0;

      // On recalcule les nouvelles coordonnées théoriques
      double tAlt, tAz;
      equatorialToHorizon(cible.ra_deg, cible.dec_deg, lat, lon,
                          year, month, day, utc, tAlt, tAz);

      targetAlt = tAlt;
      targetAz = tAz;

      // On lance le micro-ajustement avec des tolérances ultra-fines !
      // 0.1° en Altitude et 0.2° en Azimut
      ajusterPasBoucleFermee(targetAlt, targetAz, 0.1, 0.2);

      // On réarme le chrono pour la prochaine vérification
      lastTrackingCheck = millis();
    }
    break;
  }
  }
}
