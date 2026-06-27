#define _CRT_SECURE_NO_WARNINGS
#include "plugin.h"
#include "common.h"
#include "CSprite2d.h"
#include "CTxdStore.h"
#include "CStats.h"
#include "CWorld.h"
#include "CFont.h"
#include "CCamera.h"
#include "CTheScripts.h"
#include "CText.h"
#include "CMenuManager.h"
#include "CHud.h"
#include "CTimer.h"
#include "CClock.h"
#include "CCutsceneMgr.h"
#include "RenderWare.h"
#include "CPlayerData.h"
#include "CPlayerInfo.h" 
#include "CPlayerPed.h"
#include <windows.h>
#include <string>
#include <algorithm>
#include <cmath>

using namespace plugin;

bool bHudEnabledInIni = true;
bool g_bFinalHudStatus = true;
bool bMenuSettingMemory = true;
bool moneyDiffVisibleShared = false;
bool bMoneyPadZeros = true;
std::string sHudMode = "lcs";
int HudAlpha = 225;

int outlineR = 255, outlineG = 255, outlineB = 255;
int healthR = 255, healthG = 255, healthB = 255;
int armorR = 255, armorG = 255, armorB = 255;
int breathR = 0, breathG = 100, breathB = 255;
int breathBgR = 0, breathBgG = 20, breathBgB = 60;
int plusHealthR = 255, plusHealthG = 255, plusHealthB = 255;
int plusArmorR = 255, plusArmorG = 255, plusArmorB = 255;

static float lastBreath = -1.0f;
static float breathVisibilityTimer = 0.0f;
static float breathGracePeriod = 200.0f;
static float colorFadeFactor = 0.0f;

const float valWepX = 69.00f, valWepY = 59.80f, valWepW = 177.00f, valWepH = 173.00f;
const float valHFX = 423.50f, valHFY = 157.50f, valHFW = 173.80f, valHFH = 39.60f;
const float valHBX = 401.50f, valHBY = 157.50f, valHBW = 150.40f, valHBH = 38.60f;
const float valAFX = 423.50f, valAFY = 108.50f, valAFW = 173.80f, valAFH = 39.60f;
const float valABX = 401.50f, valABY = 108.50f, valABW = 150.40f, valABH = 38.60f;
const float valAmX = 125.50f, valAmY = 167.00f, valAmW = 0.71f, valAmH = 0.84f;
const float valClX = 249.00f, valClY = 62.50f, valClW = 1.822f, valClH = 2.05f;
const float valMnX = 118.00f, valMnY = 206.00f, valMnW = 1.826f, valMnH = 2.05f;
const float valStX = 110.70f, valStY = 247.00f, valStW = 37.50f, valStH = 7.40f;

static const char* weaponTextureNames[] = {
    "fist", "brassknuckle", "golfclub", "nitestick", "knifecur", "batcur", "shovel", "poolcue", "katana", "chnsaw",
    "gun_dildo1", "gun_dildo2", "gun_vibe1", "gun_vibe2", "flower", "cane",
    "grenade", "teargas", "molotov", "null", "null", "null",
    "pistol", "silenced", "desert_eagle", "chromegun", "sawnoff", "shotgspa", "micro_uzi", "mp5lng",
    "ak47", "m4", "tec9", "cuntgun", "sniper", "rocketla", "hsrocket", "flamethrower", "minigun", "satchel",
    "detonator", "spraycan", "fire_ex", "camera", "nvgoog", "irgoog", "parachute"
};

static RwTexture* (__cdecl* _RwTextureRead)(const char*, const char*) = (RwTexture * (__cdecl*)(const char*, const char*))0x7F3AC0;

void DrawCustomFontString(CSprite2d& sprite, const std::string& text, float startX, float startY, float charWidth, float charHeight, bool alignRight, bool isCash, float spacing, unsigned char targetAlpha, CRGBA tintOverride = CRGBA(255, 255, 255, 255)) {
    if (!sprite.m_pTexture) return;
    const float texW = 128.0f, texH = 64.0f, charW = 16.0f, charH = 17.0f;
    auto getSpacing = [&](char c) -> float { return (c == ':') ? 0.5f : (c == '*') ? 1.2f : (c == '-') ? 1.0f : spacing; };
    float totalWidth = 0.0f;
    for (char c : text) totalWidth += charWidth * getSpacing(c);
    float currentX = alignRight ? (startX - totalWidth) : startX;
    for (char c : text) {
        float u1, v1, u2, v2, u3, v3, u4, v4;
        if (c == '-') {
            u1 = 0.0f / texW; v1 = 51.0f / texH; u2 = 13.0f / texW; v2 = 51.0f / texH;
            u3 = 0.0f / texW; v3 = 64.0f / texH; u4 = 13.0f / texW; v4 = 64.0f / texH;
        }
        else {
            int g = (c >= '0' && c <= '9') ? (isCash ? (c - '0') + 11 : (c - '0')) : (c == ':') ? 10 : (c == '$') ? 21 : (c == '*') ? 22 : -1;
            if (g == -1) { currentX += charWidth * getSpacing(c); continue; }
            int col = g % 8, row = g / 8;
            float glyphW = (c == '*') ? 16.0f : 13.0f;
            float glyphH = (c == '*') ? 16.0f : 16.0f;
            u1 = (col * charW) / texW; v1 = (row * charH) / texH;
            u2 = ((col * charW) + glyphW) / texW; v2 = v1;
            u3 = u1; v3 = ((row * charH) + glyphH) / texH; u4 = u2; v4 = v3;
        }

        CRGBA glyphColor;
        if (c == '*') {
            bool callerOverrode = !(tintOverride.r == 255 && tintOverride.g == 255 && tintOverride.b == 255);
            glyphColor = callerOverrode
                ? CRGBA(tintOverride.r, tintOverride.g, tintOverride.b, targetAlpha)
                : CRGBA(187, 149, 60, targetAlpha);
        }
        else {
            glyphColor = CRGBA(tintOverride.r, tintOverride.g, tintOverride.b, targetAlpha);
        }

        sprite.Draw(CRect(currentX, startY, currentX + charWidth, startY + charHeight), glyphColor, u1, v1, u2, v2, u3, v3, u4, v4);
        currentX += charWidth * getSpacing(c);
    }
}

class LeedsHUDMain {
public:
    static CSprite2d outlineSprite;
    static CSprite2d weaponSprites[47];
    static CSprite2d hudNumbersSprite;
    static CSprite2d healthBarSprite;
    static CSprite2d armorBarSprite;
    static CSprite2d ammoFontSprite;
    static CSprite2d plusSprite;
    static bool bLoaded;

    static float Res(float value) { return value * ((float)RsGlobal.maximumHeight / 1080.0f); }
    static float ResW(float value) { return value * ((float)RsGlobal.maximumHeight / 1080.0f); }

    static std::string GetAsiFolder() {
        char buffer[MAX_PATH];
        HMODULE hm = NULL;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&bLoaded, &hm);
        GetModuleFileNameA(hm, buffer, MAX_PATH);
        std::string path(buffer);
        size_t lastSlash = path.find_last_of("\\/");
        return path.substr(0, lastSlash) + "\\";
    }

    LeedsHUDMain() {
        try {
            unsigned char* checkAddr = (unsigned char*)0x58F441;
            if (checkAddr && *checkAddr != 0x90) patch::Nop(0x58F441, 5);
        }
        catch (...) {}

        Events::drawHudEvent += [] {
            LeedsHUDMain::DrawBars();
            };

        Events::initRwEvent += [] {
            std::string asiPath = GetAsiFolder();
            std::string iniPath = asiPath + "LeedsHUD.ini";
            std::string modPath = asiPath + "LeedsHUD\\";

            bMoneyPadZeros = GetPrivateProfileIntA("Settings", "MoneyPadZeros", 1, iniPath.c_str()) != 0;
            HudAlpha = GetPrivateProfileIntA("Settings", "HUDAlpha", 225, iniPath.c_str());
            HudAlpha = std::clamp(HudAlpha, 0, 255);

            char modeBuffer[32];
            GetPrivateProfileStringA("Settings", "Mode", "lcs", modeBuffer, sizeof(modeBuffer), iniPath.c_str());
            sHudMode = modeBuffer;
            std::transform(sHudMode.begin(), sHudMode.end(), sHudMode.begin(), ::tolower);

            outlineR = GetPrivateProfileIntA("Colors", "OutlineR", 255, iniPath.c_str());
            outlineG = GetPrivateProfileIntA("Colors", "OutlineG", 255, iniPath.c_str());
            outlineB = GetPrivateProfileIntA("Colors", "OutlineB", 255, iniPath.c_str());

            healthR = GetPrivateProfileIntA("Colors", "HealthR", 255, iniPath.c_str());
            healthG = GetPrivateProfileIntA("Colors", "HealthG", 255, iniPath.c_str());
            healthB = GetPrivateProfileIntA("Colors", "HealthB", 255, iniPath.c_str());

            armorR = GetPrivateProfileIntA("Colors", "ArmorR", 255, iniPath.c_str());
            armorG = GetPrivateProfileIntA("Colors", "ArmorG", 255, iniPath.c_str());
            armorB = GetPrivateProfileIntA("Colors", "ArmorB", 255, iniPath.c_str());

            breathR = GetPrivateProfileIntA("Colors", "BreathR", 0, iniPath.c_str());
            breathG = GetPrivateProfileIntA("Colors", "BreathG", 100, iniPath.c_str());
            breathB = GetPrivateProfileIntA("Colors", "BreathB", 255, iniPath.c_str());

            breathBgR = GetPrivateProfileIntA("Colors", "BreathBgR", 0, iniPath.c_str());
            breathBgG = GetPrivateProfileIntA("Colors", "BreathBgG", 20, iniPath.c_str());
            breathBgB = GetPrivateProfileIntA("Colors", "BreathBgB", 60, iniPath.c_str());

            plusHealthR = GetPrivateProfileIntA("Colors", "PlusHealthR", 255, iniPath.c_str());
            plusHealthG = GetPrivateProfileIntA("Colors", "PlusHealthG", 255, iniPath.c_str());
            plusHealthB = GetPrivateProfileIntA("Colors", "PlusHealthB", 255, iniPath.c_str());

            plusArmorR = GetPrivateProfileIntA("Colors", "PlusArmorR", 255, iniPath.c_str());
            plusArmorG = GetPrivateProfileIntA("Colors", "PlusArmorG", 255, iniPath.c_str());
            plusArmorB = GetPrivateProfileIntA("Colors", "PlusArmorB", 255, iniPath.c_str());

            int txdSlot = CTxdStore::AddTxdSlot("leedshud");
            if (CTxdStore::LoadTxd(txdSlot, (modPath + "LeedsHUD.TXD").c_str())) {
                CTxdStore::AddRef(txdSlot);
                CTxdStore::PushCurrentTxd();
                CTxdStore::SetCurrentTxd(txdSlot);

                if (sHudMode == "lcs") {
                    outlineSprite.SetTexture((char*)"lcs_bar_outline");
                    armorBarSprite.SetTexture((char*)"lcs_bar_inside1");
                    healthBarSprite.SetTexture((char*)"lcs_bar_inside2");
                    ammoFontSprite.SetTexture((char*)"lcs_ammotext");
                }
                else {
                    outlineSprite.SetTexture((char*)"outline_frame");
                    armorBarSprite.SetTexture((char*)"bar_inside1");
                    healthBarSprite.SetTexture((char*)"bar_inside2");
                    ammoFontSprite.SetTexture((char*)"ammotext");
                }

                hudNumbersSprite.SetTexture((char*)"hudnumbers");
                plusSprite.SetTexture((char*)"plus");
                CTxdStore::PopCurrentTxd();
            }

            int wepSlot = CTxdStore::AddTxdSlot("leedshud_wep");
            if (CTxdStore::LoadTxd(wepSlot, (modPath + "LeedsHUDweapons.txd").c_str())) {
                CTxdStore::AddRef(wepSlot);
                CTxdStore::PushCurrentTxd();
                CTxdStore::SetCurrentTxd(wepSlot);
                for (int i = 0; i < 47; i++) {
                    if (strcmp(weaponTextureNames[i], "null") != 0)
                        weaponSprites[i].SetTexture((char*)weaponTextureNames[i]);
                }
                CTxdStore::PopCurrentTxd();
            }
            bLoaded = true;
            };

        Events::gameProcessEvent += [] {
            static bool bLastMenuActive = false;
            bool bMenuActive = FrontEndMenuManager.m_bMenuActive;
            if (bMenuActive && !bLastMenuActive) *(unsigned char*)0xBA6769 = bMenuSettingMemory ? 1 : 0;
            if (bMenuActive) bMenuSettingMemory = (*(unsigned char*)0xBA6769 != 0);
            else if (bMenuSettingMemory) *(unsigned char*)0xBA6769 = 0;
            bLastMenuActive = bMenuActive;

            g_bFinalHudStatus = bHudEnabledInIni && bMenuSettingMemory;
            };
    }

    static void DrawAmmoFontString(const std::string& text, float anchorX, float startY, float charWidth, float charHeight, float spacing = 0.85f) {
    if (!ammoFontSprite.m_pTexture) return;

    CPlayerPed* pPlayer = FindPlayerPed(-1);
    if (pPlayer && pPlayer->m_pPlayerData) {
        int activeSlot = pPlayer->m_nSelectedWepSlot;
        int activeWeapon = pPlayer->m_aWeapons[activeSlot].m_eWeaponType;

        if (activeWeapon == 40) {
            return;
        }
    }

    const float texW = 256.0f, texH = 64.0f, charW = 16.0f, charH = 21.0f;
    float totalWidth = 0.0f;
    for (char c : text) totalWidth += charWidth * spacing;

    float resScale = (float)RsGlobal.maximumHeight / 1080.0f;

    float startX = 0.0f;
    float shiftOffset = 0.0f;

    if (sHudMode == "lcs") {
        startX = anchorX - (totalWidth / 2.0f);
        shiftOffset = -54.0f * resScale;
    }
    else {
        startX = anchorX - totalWidth;
        shiftOffset = 0.0f;
    }

    const float outlineRadius = 2.0f * resScale;
    CRGBA outlineColor(0, 0, 0, 255);
    CRGBA textColor(255, 255, 255, 255);

    const struct { float x, y; } radialOffsets[8] = {
        {  0.0f, -1.0f }, {  0.707f, -0.707f }, {  1.0f,  0.0f }, {  0.707f,  0.707f },
        {  0.0f,  1.0f }, { -0.707f,  0.707f }, { -1.0f,  0.0f }, { -0.707f, -0.707f }
    };

    float currentX = startX + shiftOffset;
    for (char c : text) {
        int index = -1;
        if (c >= '0' && c <= '9') index = c - '0';
        else if (c == '-') index = 10;

        if (index != -1) {
            const float bleedPadding = 0.5f;
            float u1 = ((index * charW) + bleedPadding) / texW;
            float v1 = 0.0f;
            float u2 = (((index + 1) * charW) - bleedPadding) / texW;
            float v2 = charH / texH;

            for (int i = 0; i < 8; ++i) {
                float offX = radialOffsets[i].x * outlineRadius;
                float offY = radialOffsets[i].y * outlineRadius;

                ammoFontSprite.Draw(
                    CRect(currentX + offX, startY + offY, currentX + offX + charWidth, startY + offY + charHeight),
                    outlineColor,
                    u1, v1, u2, v1, u1, v2, u2, v2
                );
            }
        }
        currentX += charWidth * spacing;
    }

    currentX = startX + shiftOffset;
    for (char c : text) {
        int index = -1;
        if (c >= '0' && c <= '9') index = c - '0';
        else if (c == '-') index = 10;

        if (index != -1) {
            const float bleedPadding = 0.5f;
            float u1 = ((index * charW) + bleedPadding) / texW;
            float v1 = 0.0f;
            float u2 = (((index + 1) * charW) - bleedPadding) / texW;
            float v2 = charH / texH;

            ammoFontSprite.Draw(
                CRect(currentX, startY, currentX + charWidth, startY + charHeight),
                textColor,
                u1, v1, u2, v1, u1, v2, u2, v2
            );
        }
        currentX += charWidth * spacing;
    }
}
    static void DrawBars() {
        if (!g_bFinalHudStatus || FrontEndMenuManager.m_bMenuActive) return;
        if (TheCamera.m_bWideScreenOn) return;

        if (!CTheScripts::bDisplayHud) return;
        if (CCutsceneMgr::ms_cutsceneProcessing) return;

        CPlayerPed* player = FindPlayerPed();
        if (!player || !player->m_pPlayerData) return;

        float screenW = (float)RsGlobal.maximumWidth;

        float currentBreath = player->m_pPlayerData->m_fBreath;
        float maxBreath = 1000.0f + (CStats::GetStatValue(22) * 1.5f);
        float breathPerc = std::clamp(currentBreath / maxBreath, 0.0f, 1.0f);

        if (lastBreath < 0.0f) lastBreath = currentBreath;

        if (breathGracePeriod > 0.0f) {
            breathGracePeriod -= CTimer::ms_fTimeStep;
            lastBreath = currentBreath;
        }
        else if (currentBreath < (lastBreath - 0.1f) && breathPerc < 0.98f) {
            breathVisibilityTimer = 150.0f;
        }

        if (breathVisibilityTimer > 0.0f) {
            breathVisibilityTimer -= CTimer::ms_fTimeStep;
        }
        lastBreath = currentBreath;

        float fadeSpeed = 0.04f * CTimer::ms_fTimeStep;
        if (breathVisibilityTimer > 0.0f && breathPerc < 0.99f)
            colorFadeFactor = std::min(1.0f, colorFadeFactor + fadeSpeed);
        else
            colorFadeFactor = std::max(0.0f, colorFadeFactor - fadeSpeed);

        float wepX = screenW - ResW(valWepX) - ResW(valWepW);
        float wepY = Res(valWepY);
        float wepSizeW = ResW(valWepW);
        float wepSizeH = ResW(valWepH);

        int weaponId = player->m_aWeapons[player->m_nSelectedWepSlot].m_eWeaponType;
        if (weaponId >= 0 && weaponId < 47 && weaponSprites[weaponId].m_pTexture) {
            unsigned char weaponAlpha = static_cast<unsigned char>((HudAlpha / 255.0f) * 245);
            weaponSprites[weaponId].Draw(wepX, wepY + wepSizeH, wepX + wepSizeW, wepY + wepSizeH, wepX, wepY, wepX + wepSizeW, wepY, CRGBA(255, 255, 255, weaponAlpha));
        }

        short playerFocus = CWorld::PlayerInFocus;
        float maxHealth = static_cast<float>(CWorld::Players[playerFocus].m_nMaxHealth);

        float armorStatValue = CStats::GetStatValue(260);
        float maxArmor = (armorStatValue >= 1000.0f) ? 150.0f : 100.0f;

        if (player->m_fArmour > maxArmor) {
            maxArmor = player->m_fArmour;
        }

        float armorPerc = std::clamp(player->m_fArmour / maxArmor, 0.0f, 1.0f);

        unsigned char outlineAlpha = HudAlpha;
        float hBarX = screenW - ResW(valHBX), hBarY = Res(valHBY), fullW = ResW(valHBW), hH = Res(valHBH);

        if (healthBarSprite.m_pTexture) {
            float healthPerc = std::clamp(player->m_fHealth / maxHealth, 0.0f, 1.0f);
            const float uStart = 16.0f / 128.0f;
            unsigned char healthAlpha = static_cast<unsigned char>((HudAlpha / 255.0f) * 200);

            if (healthPerc <= 0.25f && healthPerc > 0.0f) {
                if ((CTimer::m_snTimeInMilliseconds / 200) % 2 == 1) {
                    healthAlpha = 0;
                    outlineAlpha = 0;
                }
            }

            healthBarSprite.Draw(CRect(hBarX, hBarY, hBarX + fullW, hBarY + hH), CRGBA(20, 20, 20, healthAlpha), uStart, 0.0f, 1.0f, 0.0f, uStart, 1.0f, 1.0f, 1.0f);

            if (healthPerc > 0.01f) {
                float uEnd = uStart + (1.0f - uStart) * healthPerc;
                healthBarSprite.Draw(CRect(hBarX, hBarY, hBarX + (fullW * healthPerc), hBarY + hH), CRGBA(healthR, healthG, healthB, healthAlpha), uStart, 0.0f, uEnd, 0.0f, uStart, 1.0f, uEnd, 1.0f);
            }

            float hFX = screenW - ResW(valHFX), hFY = Res(valHFY);
            outlineSprite.Draw(CRect(hFX, hFY, hFX + ResW(valHFW), hFY + Res(valHFH)), CRGBA(outlineR, outlineG, outlineB, outlineAlpha));
        }

        bool bDrawBreath = (colorFadeFactor > 0.01f);
        float aBarX = screenW - ResW(valABX), aBarY = Res(valABY), fullW_A = ResW(valABW), aH = Res(valABH);

        int dynamicBreathR = breathR;
        int dynamicBreathG = breathG;
        if (bDrawBreath) {
            dynamicBreathR = static_cast<int>(dynamicBreathR + ((255 - dynamicBreathR) * (1.0f - colorFadeFactor)));
            dynamicBreathG = static_cast<int>(dynamicBreathG + ((255 - dynamicBreathG) * (1.0f - colorFadeFactor)));
        }

        if (player->m_fArmour > 1.0f || bDrawBreath) {
            float aFX = screenW - ResW(valAFX), aFY = Res(valAFY);
            const float uStart = 16.0f / 128.0f;

            unsigned char finalFillAlpha = bDrawBreath ? 255 : static_cast<unsigned char>((HudAlpha / 255.0f) * 200);
            unsigned char finalBgAlpha = bDrawBreath ? 180 : static_cast<unsigned char>((HudAlpha / 255.0f) * 200);

            CRGBA backColor = CRGBA(breathBgR, breathBgG, breathBgB, finalBgAlpha);
            CRGBA fillColor;

            if (bDrawBreath) {
                fillColor = CRGBA(
                    static_cast<unsigned char>(std::clamp(dynamicBreathR, 0, 255)),
                    static_cast<unsigned char>(std::clamp(dynamicBreathG, 0, 255)),
                    static_cast<unsigned char>(breathB),
                    finalFillAlpha
                );
            }
            else {
                fillColor = CRGBA(armorR, armorG, armorB, finalFillAlpha);
            }

            float displayPerc = bDrawBreath ? breathPerc : armorPerc;

            armorBarSprite.Draw(CRect(aBarX, aBarY, aBarX + fullW_A, aBarY + aH), backColor, uStart, 0.0f, 1.0f, 0.0f, uStart, 1.0f, 1.0f, 1.0f);

            if (displayPerc > 0.01f) {
                float uEnd = uStart + (1.0f - uStart) * displayPerc;
                armorBarSprite.Draw(CRect(aBarX, aBarY, aBarX + (fullW_A * displayPerc), aBarY + aH), fillColor, uStart, 0.0f, uEnd, 0.0f, uStart, 1.0f, uEnd, 1.0f);
            }

            outlineSprite.Draw(CRect(aFX, aFY, aFX + ResW(valAFW), aFY + Res(valAFH)), bDrawBreath ? fillColor : CRGBA(outlineR, outlineG, outlineB, HudAlpha));
        }

        int activeSlot = player->m_nSelectedWepSlot;
        CWeapon& activeWep = player->m_aWeapons[activeSlot];

        if (activeSlot > 1 && activeSlot != 10 &&
            activeWep.m_eWeaponType > 1 && activeWep.m_eWeaponType < 44 &&
            activeWep.m_eWeaponType != 21 && activeWep.m_eWeaponType != 14) {

            CWeaponInfo* info = CWeaponInfo::GetWeaponInfo(activeWep.m_eWeaponType, player->GetWeaponSkill());
            if (activeWep.m_nAmmoTotal < 20000) {
                char ammoStr[64];

                if (activeWep.m_eWeaponType == 43) { // camera
                    snprintf(ammoStr, sizeof(ammoStr), "%d", activeWep.m_nAmmoTotal);
                }
                else if (info && info->m_nAmmoClip > 1) {
                    snprintf(ammoStr, sizeof(ammoStr), "%d-%d", activeWep.m_nAmmoTotal - activeWep.m_nAmmoInClip, activeWep.m_nAmmoInClip);
                }
                else {
                    snprintf(ammoStr, sizeof(ammoStr), "%d", activeWep.m_nAmmoTotal);
                }
                LeedsHUDMain::DrawAmmoFontString(ammoStr, screenW - ResW(valAmX), Res(valAmY), ResW(16.0f * valAmW), Res(21.0f * valAmH), 1.05f);
            }
        }

        if (plusSprite.m_pTexture) {
            float healthPlusOffsetX = ResW(-45.0f);
            float healthPlusOffsetY = Res(41.5f);
            float armorPlusOffsetX = ResW(-45.0f);
            float armorPlusOffsetY = Res(41.5f);

            if (maxHealth >= 101.0f && outlineAlpha > 0 && healthBarSprite.m_pTexture) {
                float midX = hBarX + (fullW / 2.0f);
                float pX1 = midX - ResW(19.0f);
                float pX2 = midX + ResW(19.0f);
                float pY1 = hBarY - Res(40.0f);
                float pY2 = hBarY - Res(4.0f);

                CRect finalHealthPos(pX1 + healthPlusOffsetX, pY1 + healthPlusOffsetY, pX2 + healthPlusOffsetX, pY2 + healthPlusOffsetY);
                plusSprite.Draw(finalHealthPos, CRGBA(plusHealthR, plusHealthG, plusHealthB, HudAlpha));
            }

            if (!bDrawBreath && player->m_fArmour > 1.0f) {
                short playerFocus = CWorld::PlayerInFocus;
                unsigned int maxArmorLimit = CWorld::Players[playerFocus].m_nMaxArmour;

                if (maxArmorLimit > 100) {
                    float midX = aBarX + (fullW_A / 2.0f);
                    float pX1 = midX - ResW(19.0f);
                    float pX2 = midX + ResW(19.0f);
                    float pY1 = aBarY - Res(40.0f);
                    float pY2 = aBarY - Res(4.0f);

                    CRect finalArmorPos(pX1 + armorPlusOffsetX, pY1 + armorPlusOffsetY, pX2 + armorPlusOffsetX, pY2 + armorPlusOffsetY);
                    plusSprite.Draw(finalArmorPos, CRGBA(plusArmorR, plusArmorG, plusArmorB, HudAlpha));
                }
            }
        }
    }
};

CSprite2d LeedsHUDMain::outlineSprite;
CSprite2d LeedsHUDMain::ammoFontSprite;
CSprite2d LeedsHUDMain::weaponSprites[47];
CSprite2d LeedsHUDMain::hudNumbersSprite;
CSprite2d LeedsHUDMain::healthBarSprite;
CSprite2d LeedsHUDMain::armorBarSprite;
CSprite2d LeedsHUDMain::plusSprite;
bool LeedsHUDMain::bLoaded = false;
LeedsHUDMain leedsHudMainInstance;

class LeedsHUDClock {
public:
    static float Res(float value) { return value * ((float)RsGlobal.maximumHeight / 1080.0f); }
    static float ResW(float value) { return value * ((float)RsGlobal.maximumHeight / 1080.0f); }
    static void Draw() {
        if (CCutsceneMgr::ms_cutsceneProcessing || TheCamera.m_bWideScreenOn || !g_bFinalHudStatus) return;
        if (!CTheScripts::bDisplayHud) return;
        char timeStr[64]; snprintf(timeStr, sizeof(timeStr), "%02d:%02d", CClock::ms_nGameClockHours, CClock::ms_nGameClockMinutes);
        DrawCustomFontString(LeedsHUDMain::hudNumbersSprite, timeStr, (float)RsGlobal.maximumWidth - ResW(valClX), Res(valClY), ResW(16.0f * valClW), Res(17.0f * valClH), true, false, 1.0f, HudAlpha);
    }
};

class LeedsHUDMoney {
public:
    static int m_nPreviousMoney, m_nDisplayMoney;
    static bool bInitialized;

    static float Res(float value) { return value * ((float)RsGlobal.maximumHeight / 1080.0f); }
    static float ResW(float value) { return value * ((float)RsGlobal.maximumHeight / 1080.0f); }

    static void Draw() {
        if (!g_bFinalHudStatus) return;
        if (!CTheScripts::bDisplayHud || FrontEndMenuManager.m_bMenuActive || TheCamera.m_bWideScreenOn) return;

        int currentMoney = *(int*)0xB7CE50;
        if (!bInitialized) { m_nPreviousMoney = currentMoney; m_nDisplayMoney = currentMoney; bInitialized = true; return; }

        if (m_nDisplayMoney != currentMoney) {
            int diff = currentMoney - m_nDisplayMoney;
            int step = abs(diff) / 10 + 1;
            if (abs(diff) <= step) m_nDisplayMoney = currentMoney;
            else if (diff > 0) m_nDisplayMoney += step;
            else m_nDisplayMoney -= step;
        }

        char str[64];
        bool bNegative = (m_nDisplayMoney < 0);
        int absMoney = abs(m_nDisplayMoney);

        if (bMoneyPadZeros) snprintf(str, sizeof(str), bNegative ? "-$%08d" : "$%08d", absMoney);
        else snprintf(str, sizeof(str), bNegative ? "-$%d" : "$%d", absMoney);

        CRGBA tint = bNegative ? CRGBA(150, 0, 0, 255) : CRGBA(32, 110, 32, 255);

        DrawCustomFontString(
            LeedsHUDMain::hudNumbersSprite,
            str,
            (float)RsGlobal.maximumWidth - ResW(valMnX),
            Res(valMnY),
            ResW(16.0f * valMnW),
            Res(17.0f * valMnH),
            true,
            true,
            1.0f,
            HudAlpha,
            tint
        );
    }
};

int LeedsHUDMoney::m_nPreviousMoney = 0;
int LeedsHUDMoney::m_nDisplayMoney = 0;
bool LeedsHUDMoney::bInitialized = false;

class LeedsHUDWanted {
private:
    static inline int m_nLastWantedLevel = -1;
    static inline int m_nLastMinWantedLevel = -1;
    static inline unsigned int m_nFlashEndTime = 0;

    static constexpr unsigned int FLASH_CYCLE_DURATION = 400;
    static constexpr unsigned int TOTAL_FLASH_DURATION = FLASH_CYCLE_DURATION * 6;
    static constexpr unsigned int UNSUSPEND_FLASH_DURATION = FLASH_CYCLE_DURATION * 7; 
    static constexpr unsigned int SUSPENDED_FLASH_CYCLE = 400;

public:
    static float Res(float value) { return value * ((float)RsGlobal.maximumHeight / 1080.0f); }
    static float ResW(float value) { return value * ((float)RsGlobal.maximumHeight / 1080.0f); }

    static void Draw() {
        if (!g_bFinalHudStatus) return;
        if (!CTheScripts::bDisplayHud || FrontEndMenuManager.m_bMenuActive || TheCamera.m_bWideScreenOn) return;

        CPlayerPed* player = FindPlayerPed();
        if (!player || !player->m_pPlayerData || !player->m_pPlayerData->m_pWanted) return;

        CWanted* pWanted = player->m_pPlayerData->m_pWanted;
        int wantedLevel = pWanted->m_nWantedLevel;
        int minWantedLevel = pWanted->m_nWantedLevelBeforeParole;

        bool isSuspended = minWantedLevel > wantedLevel;

        bool wasSuspendedMerge = (wantedLevel > m_nLastWantedLevel) && (wantedLevel <= m_nLastMinWantedLevel);

        if (m_nLastWantedLevel != -1 && wantedLevel != m_nLastWantedLevel && !wasSuspendedMerge)
            m_nFlashEndTime = CTimer::m_snTimeInMilliseconds + TOTAL_FLASH_DURATION;

        if (wasSuspendedMerge)
            m_nFlashEndTime = CTimer::m_snTimeInMilliseconds + UNSUSPEND_FLASH_DURATION;

        m_nLastWantedLevel = wantedLevel;
        m_nLastMinWantedLevel = minWantedLevel;

        if (wantedLevel == 0 && minWantedLevel == 0) return;

        unsigned char renderAlpha = 225;
        unsigned int currentTime = CTimer::m_snTimeInMilliseconds;

        if (currentTime < m_nFlashEndTime) {
            unsigned int timePassed = TOTAL_FLASH_DURATION - (m_nFlashEndTime - currentTime);
            if ((timePassed / (FLASH_CYCLE_DURATION / 2)) % 2 == 1)
                renderAlpha = 0;
        }

        if (renderAlpha > 0 && wantedLevel > 0) {
            std::string starString(wantedLevel, '*');
            unsigned char combinedAlpha = static_cast<unsigned char>((HudAlpha / 255.0f) * renderAlpha);
            DrawCustomFontString(
                LeedsHUDMain::hudNumbersSprite,
                starString,
                (float)RsGlobal.maximumWidth - ResW(valStX),
                Res(valStY),
                ResW(valStW),
                Res(valStH * 5.0f),
                true,
                false,
                0.8f,
                combinedAlpha
            );
        }

        if (isSuspended) {
            bool visible = (currentTime / SUSPENDED_FLASH_CYCLE) % 2 == 0;

            if (visible) {
                std::string suspendedString(minWantedLevel, '*');
                unsigned char suspendedAlpha = static_cast<unsigned char>((HudAlpha / 255.0f) * 225);

                DrawCustomFontString(
                    LeedsHUDMain::hudNumbersSprite,
                    suspendedString,
                    (float)RsGlobal.maximumWidth - ResW(valStX),
                    Res(valStY),
                    ResW(valStW),
                    Res(valStH * 5.0f),
                    true,
                    false,
                    0.8f,
                    suspendedAlpha,
                    CRGBA(100, 200, 255, 255)
                );
            }
        }
    }
};
class LeedsHUDClockPlugin { public: LeedsHUDClockPlugin() { Events::drawHudEvent += [] { LeedsHUDClock::Draw(); }; } } leedsHudClockPlugin;
class LeedsHUDWantedPlugin { public: LeedsHUDWantedPlugin() { Events::drawHudEvent += [] { if (FrontEndMenuManager.m_bMenuActive || TheCamera.m_bWideScreenOn) return; LeedsHUDWanted::Draw(); }; } } leedsHUDWantedPlugin;
class LeedsHUDMoneyPlugin { public: LeedsHUDMoneyPlugin() { Events::drawHudEvent += [] { if (FrontEndMenuManager.m_bMenuActive || TheCamera.m_bWideScreenOn) return; LeedsHUDMoney::Draw(); }; } } leedsHUDMoneyPlugin;