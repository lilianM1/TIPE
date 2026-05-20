#include "imu.h"

#include "tipe_globals.h"

#include <EEPROM.h>
#include <math.h>

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

void readBNOAltAz(float &alt, float &az)
{
    imu::Quaternion q = bno.getQuat();
    bno.getCalibration(&calSys, &calGyr, &calAcc, &calMag);
    float qw = q.w(), qx = q.x(), qy = q.y(), qz = q.z();

    // Au lieu des angles d'Euler, on projette le vecteur "Avant" de ton capteur.
    // Projection de l'axe de visée sur la verticale (Altitude)
    float vecteurZ = 2.0 * (qy * qz + qw * qx);
    alt = degrees(asin(constrain(vecteurZ, -1.0, 1.0)));

    // Projection de l'axe de visée sur le plan horizontal (Boussole/Azimut)
    float vecteurX = 2.0 * (qx * qy - qw * qz);
    float vecteurY = 1.0 - 2.0 * (qx * qx + qz * qz);
    
    float rawYaw = degrees(atan2(vecteurX, vecteurY));
    
    // (Ajuste le signe - selon le sens de montage physique de ton BNO055)
    az = fmod((-rawYaw + 360.0), 360.0);
    if (az < 0) az += 360.0;
}
