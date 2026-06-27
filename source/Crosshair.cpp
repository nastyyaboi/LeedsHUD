#include "plugin.h"
#include "RenderWare.h"
#include "CTxdStore.h"
#include "Patch.h"
#include "CCamera.h"
#include "CHud.h"
#include "CSprite2d.h"
#include "CSprite.h"
#include "CWeaponInfo.h"
#include "CTheScripts.h"
#include "CCutsceneMgr.h"
#include "CMenuManager.h"
#include "CWeapon.h"
#include "CPools.h"
#include <windows.h>
#include <string>

using namespace plugin;

static RwTexture* g_pCustomTex = nullptr;
static bool       g_bSwapDone = false;

static bool       g_bEnableRedLockOn = true;

static std::string GetAsiDir() {
    char buf[MAX_PATH] = {};
    HMODULE hMod = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&GetAsiDir), &hMod);
    GetModuleFileNameA(hMod, buf, MAX_PATH);
    std::string s(buf);
    return s.substr(0, s.find_last_of("\\/") + 1);
}

static void LoadConfig() {
    std::string iniPath = GetAsiDir() + "LeedsHUD.ini";

    g_bEnableRedLockOn = (GetPrivateProfileIntA("Settings", "CrosshairRedLockOn", 1, iniPath.c_str()) != 0);
}

static void LoadCustomTexture() {
    if (g_pCustomTex) return;
    std::string path = GetAsiDir() + "LeedsHUD\\LeedsHUD.txd";
    RwStream* stream = RwStreamOpen(rwSTREAMFILENAME, rwSTREAMREAD, path.c_str());
    if (!stream) return;
    if (!RwStreamFindChunk(stream, rwID_TEXDICTIONARY, nullptr, nullptr)) {
        RwStreamClose(stream, nullptr);
        return;
    }
}

static bool IsValidPedEntity(CEntity* ent) {
    if (!ent) return false;
    uintptr_t addr = (uintptr_t)ent;
    if (addr < 0x10000 || addr >= 0x7FFFFFFF) return false;
    if (!CPools::ms_pPedPool->IsObjectValid(reinterpret_cast<CPed*>(ent))) {
        return false;
    }
    if (ent->m_nType != 1) return false;
    return true;
}

#define CROSSHAIRS_TOTALSPRITES 4
enum eCrosshairSprites { CROSSHAIR_M16 = 0, CROSSHAIR_ROCKET = 1, CROSSHAIR_SNIPER = 2, CROSSHAIR_VIEWFINDER = 3 };
CSprite2d CrosshairSprites[CROSSHAIRS_TOTALSPRITES];
bool ms_bSpritesLoaded = false;
char* CrosshairsNames[CROSSHAIRS_TOTALSPRITES] = { (char*)"sitem16", (char*)"siterocket", (char*)"scope", (char*)"viewfinder" };

class CHudNew {
public:
    static void Initialise() {
        if (!ms_bSpritesLoaded) {
            int CrosshairSlot = CTxdStore::AddTxdSlot("crosshair");
            CTxdStore::LoadTxd(CrosshairSlot, PLUGIN_PATH((char*)"LeedsHUD\\LeedsHUD.txd"));
            CTxdStore::AddRef(CrosshairSlot);
            CTxdStore::PushCurrentTxd();
            CTxdStore::SetCurrentTxd(CrosshairSlot);
            for (int i = 0; i < CROSSHAIRS_TOTALSPRITES; i++)
                CrosshairSprites[i].SetTexture(CrosshairsNames[i]);
            CTxdStore::PopCurrentTxd();
            ms_bSpritesLoaded = true;
        }
    }
    static void Shutdown() {
        if (ms_bSpritesLoaded) {
            for (int i = 0; i < CROSSHAIRS_TOTALSPRITES; ++i)
                CrosshairSprites[i].Delete();
            int CrosshairSlot = CTxdStore::FindTxdSlot("crosshair");
            CTxdStore::RemoveTxdSlot(CrosshairSlot);
            ms_bSpritesLoaded = false;
        }
    }
};

void leedsCrosshair() {
    CPlayerPed* player = FindPlayerPed(0);
    if (!player || !player->m_pPlayerData || FrontEndMenuManager.m_bMenuActive || !CHud::m_Wants_To_Draw_Hud) return;
    if (FrontEndMenuManager.m_bMenuActive) return;
    if (TheCamera.m_bWideScreenOn) return;
    if (CCutsceneMgr::ms_cutsceneProcessing) return;
    if (!CTheScripts::bDisplayHud) return;                    // driving school
    if (!CHud::m_Wants_To_Draw_Hud) return;

    eCamMode Mode = TheCamera.m_aCams[TheCamera.m_nActiveCam].m_nMode;

    if (Mode == MODE_CAMERA) {
        float size = SCREEN_COORD(512.0f);
        CRect rect(
            (SCREEN_WIDTH / 2.0f) - size,
            (SCREEN_HEIGHT / 2.0f) - size,
            (SCREEN_WIDTH / 2.0f) + size,
            (SCREEN_HEIGHT / 2.0f) + size
        );
        CrosshairSprites[CROSSHAIR_VIEWFINDER].Draw(rect, CRGBA(255, 255, 255, 255));
        return;
    }

    if ((int)Mode == 45) {
        float x = SCREEN_WIDTH * 0.5f;
        float y = SCREEN_HEIGHT * 0.5f;
        float fixedSize = SCREEN_COORD(40.0f);
        CRect rect(x - fixedSize, y - fixedSize, x + fixedSize, y + fixedSize);
        CrosshairSprites[CROSSHAIR_M16].Draw(rect, CRGBA(255, 255, 255, 255));
        return;
    }

    if (Mode == MODE_1STPERSON) {
        CVehicle* playerVehicle = FindPlayerVehicle(0, false);
        if (playerVehicle) {
            unsigned int model = playerVehicle->m_nModelIndex;
            if (model == 476 || model == 520 || model == 425) {
                float x = SCREEN_WIDTH * 0.5f;
                float y = SCREEN_HEIGHT * 0.5f;
                float rocketSize = SCREEN_COORD(80.0f);
                CRect rect(x - rocketSize, y - rocketSize, x + rocketSize, y + rocketSize);
                CrosshairSprites[CROSSHAIR_ROCKET].Draw(rect, CRGBA(255, 255, 255, 255));
                return;
            }
        }
    }

    int slot = player->m_nSelectedWepSlot;
    CWeaponInfo* info = CWeaponInfo::GetWeaponInfo(player->m_aWeapons[slot].m_eWeaponType, player->GetWeaponSkill());
    if (!info) return;

    if (Mode == MODE_AIMWEAPON || Mode == MODE_AIMWEAPON_FROMCAR ||
        Mode == MODE_ROCKETLAUNCHER || Mode == MODE_ROCKETLAUNCHER_HS ||
        Mode == MODE_SNIPER || Mode == MODE_AIMWEAPON_ATTACHED) {

        CRect rect;

        if (info->m_nModelId == 358) {
            float sniperSize = 700.0f;
            rect.left = (SCREEN_WIDTH / 2.0f) - SCREEN_COORD(sniperSize);
            rect.top = (SCREEN_HEIGHT / 2.0f) - SCREEN_COORD(sniperSize);
            rect.right = (SCREEN_WIDTH / 2.0f) + SCREEN_COORD(sniperSize);
            rect.bottom = (SCREEN_HEIGHT / 2.0f) + SCREEN_COORD(sniperSize);
            CrosshairSprites[CROSSHAIR_SNIPER].Draw(rect, CRGBA(255, 255, 255, 255));
        }

        else if (info->m_nModelId == 359 || info->m_nModelId == 360) {
            float rocketSize = 80.0f;
            rect.left = (SCREEN_WIDTH / 2.0f) - SCREEN_COORD(rocketSize);
            rect.top = (SCREEN_HEIGHT / 2.0f) - SCREEN_COORD(rocketSize);
            rect.right = (SCREEN_WIDTH / 2.0f) + SCREEN_COORD(rocketSize);
            rect.bottom = (SCREEN_HEIGHT / 2.0f) + SCREEN_COORD(rocketSize);
            CrosshairSprites[CROSSHAIR_ROCKET].Draw(rect, CRGBA(255, 255, 255, 255));
        }

        else {
            float x = (float)SCREEN_WIDTH * CCamera::m_f3rdPersonCHairMultX;
            float y = (float)SCREEN_HEIGHT * CCamera::m_f3rdPersonCHairMultY;

            bool bIsAimingAtPed = false;

            if (g_bEnableRedLockOn) {
                if (player->m_pPlayerTargettedPed) {
                    bIsAimingAtPed = true;
                }
                else if (player->m_pPlayerData && player->m_pPlayerData->m_bHaveTargetSelected) {
                    bIsAimingAtPed = true;
                }
                else {
                    CEntity* crosshairEnt = *(CEntity**)0xB79358;
                    if (IsValidPedEntity(crosshairEnt)) {
                        bIsAimingAtPed = true;
                    }
                }
            }

            float fixedSize = SCREEN_COORD(40.0f);
            rect.left = x - fixedSize;
            rect.top = y - fixedSize;
            rect.right = x + fixedSize;
            rect.bottom = y + fixedSize;

            if (bIsAimingAtPed) {
                CrosshairSprites[CROSSHAIR_M16].Draw(rect, CRGBA(191, 0, 0, 255));
            }
            else {
                CrosshairSprites[CROSSHAIR_M16].Draw(rect, CRGBA(255, 255, 255, 255));
            }
        }
    }
}

class leedsCrosshairEmpty {
public:
    leedsCrosshairEmpty() {
        Events::initRwEvent += []() { LoadCustomTexture(); };
        Events::initGameEvent += []() {
            patch::PutRetn(0x58E020);
            g_bSwapDone = false;
            LoadConfig();

            CHudNew::Initialise();
            };
        Events::reInitGameEvent += []() { g_bSwapDone = false;  };
        Events::drawingEvent += []() { if (!g_bSwapDone) leedsCrosshair(); };
        Events::shutdownRwEvent += []() { CHudNew::Shutdown(); };
    }
} LeedsCrosshair;