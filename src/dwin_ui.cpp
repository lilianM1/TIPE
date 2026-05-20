#include "dwin_ui.h"

#include "tipe_globals.h"

#include <string.h>

void dwinSend(const uint8_t *data, uint8_t len)
{
    DWIN_SERIAL.write(0xAA);
    DWIN_SERIAL.write(data, len);
    DWIN_SERIAL.write(DWIN_TAIL, 4);
}

void effacerZoneTexte(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    uint16_t x2 = x + w - 1;
    uint16_t y2 = y + h - 1;
    uint8_t cmd[] = {
        0x27, 0x80,
        (uint8_t)(x >> 8), (uint8_t)(x & 0xFF),
        (uint8_t)(y >> 8), (uint8_t)(y & 0xFF),
        (uint8_t)(x2 >> 8), (uint8_t)(x2 & 0xFF),
        (uint8_t)(y2 >> 8), (uint8_t)(y2 & 0xFF),
        (uint8_t)(x >> 8), (uint8_t)(x & 0xFF),
        (uint8_t)(y >> 8), (uint8_t)(y & 0xFF)};
    dwinSend(cmd, sizeof(cmd));
    smartDelay(10);
}

void actualiserHeure(bool force)
{
    static int derniereMinute = -1;

    if (gps.time.minute() == derniereMinute && !force)
        return;

    derniereMinute = gps.time.minute();

    char bufferHeure[10];
    int heureLocale = (gps.time.hour() + 2) % 24;
    sprintf(bufferHeure, "%02d:%02d", heureLocale, gps.time.minute());

    effacerZoneTexte(5, 5, 100, 35); // Restaure le fond de l'image (x=5, y=5, largeur=100, hauteur=35)

    uint8_t len = strlen(bufferHeure);
    uint8_t cmdText[10 + len];

    cmdText[0] = 0x11; // text command
    cmdText[1] = 0x02; // font ID
    cmdText[2] = 0xFF; // color
    cmdText[3] = (uint8_t)0xFF;
    cmdText[4] = 0x00; // bg color (0x00)
    cmdText[5] = 0x00; // bg color
    cmdText[6] = 0x00; // transparent bg = 0
    cmdText[7] = 0x05; // x
    cmdText[8] = 0x00; // x
    cmdText[9] = 0x05; // y

    for (uint8_t i = 0; i < len; i++)
    {
        cmdText[10 + i] = bufferHeure[i];
    }

    dwinSend(cmdText, sizeof(cmdText));
}

void actualiserCibleBarre(bool force)
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

    effacerZoneTexte(157, 5, 200, 35); // Restaure le fond derrière les coordonnées (x=157, y=5)

    uint8_t len = strlen(buffer);
    uint8_t cmdText[10 + len];

    cmdText[0] = 0x11;
    cmdText[1] = 0x02;
    cmdText[2] = 0xFF;
    cmdText[3] = 0xFF;
    cmdText[4] = 0x00;
    cmdText[5] = 0x00;
    cmdText[6] = 0x00; // transparent bg = 0
    cmdText[7] = 0x9D; // x = 0x009D = 157
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
    smartDelay(100);
    dwinForceRefresh();
}

void chargerVRAM1(uint8_t id)
{
    uint8_t cmd[] = {0x25, 0x01, id};
    dwinSend(cmd, 3);
    smartDelay(100);
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
    smartDelay(20);
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
    smartDelay(10);
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
    smartDelay(50);
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

void afficherAlerte(String message, uint16_t rectColor, uint16_t textColor)
{
    uint8_t cmdRect[] = {
        0x05, 0x01,
        (uint8_t)(rectColor >> 8), (uint8_t)(rectColor & 0xFF),
        0x00, 0x10, 0x00, 0xD2,
        0x01, 0x00, 0x01, 0x0E};
    dwinSend(cmdRect, sizeof(cmdRect));
    smartDelay(50);

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
    smartDelay(50);
    dwinForceRefresh();
    alerteActive = true;
}

void effacerEcran(uint16_t color)
{
    uint8_t cmd[] = {
        0x01,
        (uint8_t)(color >> 8), (uint8_t)(color & 0xFF)};
    dwinSend(cmd, sizeof(cmd));
    smartDelay(10);
}