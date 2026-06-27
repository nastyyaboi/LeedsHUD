#include "plugin.h"
#include "common.h"
#include "CMenuManager.h"
#include "CTxdStore.h"
#include "CRadar.h"
#include "CSprite2d.h"
#include "CPed.h"
#include "CVehicle.h"
#include <string>
#include <windows.h>
#include <cmath>
#include <algorithm>
#include "injector.hpp"

using namespace plugin;

static RwTexture* (__cdecl* _RwTextureRead)(const char*, const char*) = (RwTexture * (__cdecl*)(const char*, const char*))0x7F3AC0;

static bool isLoaded = false;
static bool bLoaded;
static CSprite2d mLeedsHudSprite;
static CSprite2d mBarFrame;
static RwTexture* pMapDirTex = nullptr;
static RwTexture* pNorthTex = nullptr;
static RwTexture* pDestinationTex = nullptr;

const CVector2D mLeedsHudPos(169.5f, 148.0f);
const float mLeedsHudBaseSize = 85.0f;
const float mLeedsHudRingSize = 95.0f;

static float g_BlipSize = 11.0f;
static float g_SpriteBlipSize = 25.0f;
static float g_EntityBlipSize = 11.0f;

static float gHudScale = 1.0f;
static const float kScaleDefault = 1.0f;
static const float kScaleLarge = 1.55f;

static float gZoomFoot = 180.0f;
static float gZoomCar = 283.3333f;

static const float kZoomDefault = 180.0f;
static const float kZoomCarDefault = 283.3333f;
static const float kZoomLarge = 350.0f;
static const float kZoomCarLarge = 420.0f;

static bool bBigRadarActive = false;
static bool bBigFadingOut = false;
static DWORD bigFadeStartTime = 0;

static bool  bTZoomActive = false;
static bool  bTFadingOut = false;
static DWORD tZoomStartTime = 0;
static DWORD tFadeStartTime = 0;

static int gKeyZoomOut = 0x54;  // T
static int gKeyBigRadar = 0x58;  // X

#define screenHeight (static_cast<float>(RsGlobal.maximumHeight))
#define screenRes(val) (static_cast<float>(val) * (screenHeight / 900.0f))

#define LIVE_RING  (mLeedsHudRingSize * gHudScale)
#define LIVE_BASE  (mLeedsHudBaseSize * gHudScale)
#define LIVE_POS_X (mLeedsHudPos.x + (mLeedsHudRingSize * (gHudScale - 1.0f)))
#define LIVE_POS_Y (mLeedsHudPos.y + (mLeedsHudRingSize * (gHudScale - 1.0f)))

void RotateVertices(CVector2D* rect, unsigned int numVerts, float x, float y, float angle) {
    float _cos = cosf(angle);
    float _sin = sinf(angle);
    for (unsigned int i = 0; i < numVerts; i++) {
        float xold = rect[i].x;
        float yold = rect[i].y;
        rect[i].x = x + (xold - x) * _cos + (yold - y) * _sin;
        rect[i].y = y - (xold - x) * _sin + (yold - y) * _cos;
    }
}

class LeedsHUD {
public:
    static int txdIndex;

    static std::string GetAsiFolder() {
        char buffer[MAX_PATH];
        HMODULE hm = NULL;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&bLoaded, &hm);
        GetModuleFileNameA(hm, buffer, MAX_PATH);
        std::string path(buffer);
        size_t lastSlash = path.find_last_of("\\/");
        return path.substr(0, lastSlash) + "\\";
    }

    static std::string fetchPath() {
        char pathBuf[MAX_PATH];
        HMODULE module = NULL;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&isLoaded, &module);
        GetModuleFileNameA(module, pathBuf, MAX_PATH);
        std::string fullPath(pathBuf);
        size_t pos = fullPath.find_last_of("\\/");
        return fullPath.substr(0, pos) + "\\LeedsHUD\\";
    }

    static bool IsPlayerInVehicle() {
        CVehicle* veh = FindPlayerVehicle(-1, false);
        return veh != nullptr;
    }

    static void ApplyBlipSizePatches() {

        injector::WriteMemory<float*>(0x586047, &g_BlipSize, true);
        injector::WriteMemory<float*>(0x586060, &g_BlipSize, true);

        injector::WriteMemory<float*>(0x584A02, &g_SpriteBlipSize, true);
        injector::WriteMemory<float*>(0x584A1A, &g_SpriteBlipSize, true);
        injector::WriteMemory<float*>(0x584A32, &g_SpriteBlipSize, true);
        injector::WriteMemory<float*>(0x584A42, &g_SpriteBlipSize, true);
        injector::WriteMemory<float*>(0x5886DC, &g_EntityBlipSize, true);
    }

    static void UpdateRadarZoom() {
        static int s_ver = -1;
        if (s_ver == -1) {
            float probe = 0.0f;
            memcpy(&probe, (void*)0x586C9B, sizeof(float));
            s_ver = (probe == 180.0f) ? 10 : 11;
        }

        bool bigPressed = (GetAsyncKeyState(gKeyBigRadar) & 0x8000) != 0;
        bool tHeld = (GetAsyncKeyState(gKeyZoomOut) & 0x8000) != 0;
        bool inCar = IsPlayerInVehicle();

        static bool s_bigWasDown = false;

        if (bigPressed && !s_bigWasDown) {
            bBigRadarActive = !bBigRadarActive;
            if (bBigRadarActive) {
                bBigFadingOut = false;
                gHudScale = kScaleLarge;
            }
            else {
                bBigFadingOut = true;
                bigFadeStartTime = GetTickCount();
            }
        }
        s_bigWasDown = bigPressed;

        if (bBigFadingOut) {
            DWORD elapsed = GetTickCount() - bigFadeStartTime;
            float progress = std::min(1.0f, elapsed / 280.0f);
            float t = 1.0f - progress;
            gHudScale = kScaleDefault + t * (kScaleLarge - kScaleDefault);
            if (progress >= 1.0f) {
                gHudScale = kScaleDefault;
                bBigFadingOut = false;
            }
        }

        if (bBigRadarActive) {
            if (inCar) gZoomCar = kZoomCarLarge;
            else       gZoomFoot = kZoomLarge;
        }
        else if (bBigFadingOut) {
            float t = (gHudScale - kScaleDefault) / (kScaleLarge - kScaleDefault);
            if (inCar) gZoomCar = kZoomCarDefault + t * (kZoomCarLarge - kZoomCarDefault);
            else       gZoomFoot = kZoomDefault + t * (kZoomLarge - kZoomDefault);
        }

        if (tHeld && !bBigRadarActive) {
            bTZoomActive = true;
            bTFadingOut = false;
            tZoomStartTime = GetTickCount();
            if (inCar) gZoomCar = kZoomCarLarge;
            else       gZoomFoot = kZoomLarge;
        }

        if (!tHeld && bTZoomActive && !bTFadingOut && !bBigRadarActive) {
            if (GetTickCount() - tZoomStartTime > 5000) {
                bTFadingOut = true;
                tFadeStartTime = GetTickCount();
            }
        }

        if (bTFadingOut && !bBigRadarActive) {
            DWORD elapsed = GetTickCount() - tFadeStartTime;
            float progress = std::min(1.0f, elapsed / 350.0f);
            float t = 1.0f - progress;
            if (inCar) gZoomCar = kZoomCarDefault + t * (kZoomCarLarge - kZoomCarDefault);
            else       gZoomFoot = kZoomDefault + t * (kZoomLarge - kZoomDefault);
            if (progress >= 1.0f) {
                if (inCar) gZoomCar = kZoomCarDefault;
                else       gZoomFoot = kZoomDefault;
                bTFadingOut = false;
                bTZoomActive = false;
            }
        }

        float currentZoom = inCar ? gZoomCar : gZoomFoot;
        if (s_ver == 10) {
            patch::SetFloat(0x586C9B, currentZoom);
            patch::SetFloat(0x586C7F, currentZoom);
        }
        else {
            patch::SetFloat(0x58746B, currentZoom);
            patch::SetFloat(0x58744F, currentZoom);
        }
    }

    static void initiate() {
        if (isLoaded) return;

        txdIndex = CTxdStore::AddTxdSlot("leedshud_internal");
        std::string dir = fetchPath();
        std::string asiPath = GetAsiFolder();
        std::string txdPath = dir + "LeedsHUD.TXD";
        std::string iniPath = asiPath + "LeedsHUD.ini";

        int radarMapAlpha = GetPrivateProfileIntA("Settings", "RadarMapAlpha", 150, iniPath.c_str());
        int blipsAlpha = GetPrivateProfileIntA("Settings", "BlipsAlpha", 210, iniPath.c_str());

        gKeyZoomOut = GetPrivateProfileIntA("Settings", "ZoomOut", 0x54, iniPath.c_str());
        gKeyBigRadar = GetPrivateProfileIntA("Settings", "ExpandedRadar", 0x58, iniPath.c_str());

        if (CTxdStore::LoadTxd(txdIndex, txdPath.c_str())) {
            CTxdStore::AddRef(txdIndex);
            CTxdStore::PushCurrentTxd();
            CTxdStore::SetCurrentTxd(txdIndex);
            mLeedsHudSprite.SetTexture((char*)"radardisc");
            mBarFrame.SetTexture((char*)"heightframe");
            pMapDirTex = _RwTextureRead("mapdir", NULL);
            pNorthTex = _RwTextureRead("radar_north", NULL);
            pDestinationTex = _RwTextureRead("destination", NULL);
            CTxdStore::PopCurrentTxd();
            isLoaded = true;
        }

        patch::RedirectCall(0x58AA25, renderHudElement);
        patch::RedirectJump(0x583480, applyHudTranslation);
        patch::RedirectCall(0x58A551, DrawRadarPlane);
        patch::RedirectCall(0x58A649, DrawPlaneHeight);
        patch::RedirectCall(0x58A77A, DrawPlaneHeightBorder);

        patch::Nop(0x58A818, 16);
        patch::Nop(0x58A8C2, 16);
        patch::Nop(0x58A96C, 16);

        patch::SetUChar(0x58694E + 1, TRUE);

        patch::SetUInt(0x586432 + 1, radarMapAlpha);
        patch::SetUInt(0x58647B + 1, radarMapAlpha);
        patch::SetUInt(0x5864BC + 1, radarMapAlpha);

        patch::SetUInt(0x586F34 + 1, blipsAlpha);
        patch::SetUInt(0x587BD1 + 1, blipsAlpha);

        ApplyBlipSizePatches();

        {
            float probe = 0.0f;
            memcpy(&probe, (void*)0x586C9B, sizeof(float));
            bool is10 = (probe == 180.0f);
            DWORD zoomCarAddr = reinterpret_cast<DWORD>(&gZoomCar);
            DWORD zoomAddr = reinterpret_cast<DWORD>(&gZoomFoot);

            if (is10) {
                patch::SetUInt(0x586C66, zoomCarAddr);
                patch::SetUInt(0x586C60, zoomAddr);
            }
            else {
                patch::SetUInt(0x587436, zoomCarAddr);
                patch::SetUInt(0x587430, zoomAddr);
            }
        }

        Events::gameProcessEvent += [] { UpdateRadarZoom(); };
    }

    static void RadarTextures() {
        if (!isLoaded) return;
        if (pMapDirTex && CRadar::RadarBlipSprites[2].m_pTexture != pMapDirTex)
            CRadar::RadarBlipSprites[2].m_pTexture = pMapDirTex;
        if (pNorthTex && CRadar::RadarBlipSprites[4].m_pTexture != pNorthTex)
            CRadar::RadarBlipSprites[4].m_pTexture = pNorthTex;

        if (pDestinationTex) {
            for (int i = 0; i < 64; i++) {
                RwTexture* tex = CRadar::RadarBlipSprites[i].m_pTexture;
                if (tex && strncmp(tex->name, "radar_waypoint", 14) == 0) {
                    CRadar::RadarBlipSprites[i].m_pTexture = pDestinationTex;
                    break;
                }
            }
        }
    }

    static void __fastcall renderHudElement(CSprite2d* self, int, CRect const& rect, CRGBA const& color) {
        if (!isLoaded || !mLeedsHudSprite.m_pTexture) return;
        RadarTextures();

        float posX = screenRes(LIVE_POS_X);
        float posY = screenHeight - screenRes(LIVE_POS_Y);
        float ring = screenRes(LIVE_RING);

        CRect drawArea(posX - ring, posY - ring, posX + ring, posY + ring);
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)TRUE);
        mLeedsHudSprite.Draw(drawArea, CRGBA(255, 255, 255, 255));
    }

    static void applyHudTranslation(CVector2D* screenOut, CVector2D* worldIn) {
        if (FrontEndMenuManager.m_bDrawRadarOrMap) {
            screenOut->x = FrontEndMenuManager.m_fMapZoom * worldIn->x + FrontEndMenuManager.m_fMapBaseX;
            screenOut->y = FrontEndMenuManager.m_fMapBaseY - FrontEndMenuManager.m_fMapZoom * worldIn->y;
        }
        else {
            screenOut->x = screenRes(LIVE_POS_X) + (worldIn->x * screenRes(LIVE_BASE));
            screenOut->y = screenHeight - screenRes(LIVE_POS_Y) - (worldIn->y * screenRes(LIVE_BASE));
        }
    }

    static void __fastcall DrawRadarPlane(CSprite2d* sprite, int, float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, CRGBA const& color) {
        if (!sprite || !sprite->m_pTexture) return;
        float rollAngle = 0.0f;
        CVehicle* veh = FindPlayerVehicle(-1, false);
        if (veh && veh->m_matrix) {
            float rightZ = std::max(-1.0f, std::min(1.0f, veh->m_matrix->right.z));
            rollAngle = asinf(rightZ);
        }
        float arrowHalf = screenRes(LIVE_RING * 0.99f);
        float cx = screenRes(LIVE_POS_X), cy = screenHeight - screenRes(LIVE_POS_Y);
        CVector2D posn[4] = { {cx - arrowHalf, cy - arrowHalf},{cx + arrowHalf, cy - arrowHalf},
                             {cx - arrowHalf, cy + arrowHalf},{cx + arrowHalf, cy + arrowHalf} };
        RotateVertices(posn, 4, cx, cy, rollAngle);
        RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)TRUE);
        sprite->Draw(posn[2].x, posn[2].y, posn[3].x, posn[3].y, posn[0].x, posn[0].y, posn[1].x, posn[1].y, CRGBA(255, 255, 255, 255));
    }

    static float GetNormalizedPlayerHeight() {
        CPed* playa = FindPlayerPed();
        CVehicle* playaVeh = FindPlayerVehicle(-1, false);
        float pZ = (playaVeh) ? playaVeh->GetPosition().z : (playa ? playa->GetPosition().z : 0.0f);
        return std::max(0.0f, std::min(1.0f, pZ / 900.0f));
    }

    static void DrawPlaneHeightBorder(CRect const& rect, CRGBA const& color) {
        if (!mBarFrame.m_pTexture) return;
        float x = screenRes(LIVE_POS_X + LIVE_RING + 15.0f), y = screenHeight - screenRes(LIVE_POS_Y);
        float wHalf = screenRes(17.0f * 0.5f * 0.6f), hHalf = screenRes(265.0f * 0.5f * 0.6f);
        mBarFrame.Draw(CRect(x - wHalf, y - hHalf, x + wHalf, y + hHalf), CRGBA(255, 255, 255, 255));
        float currentY = (y + screenRes(258.0f * 0.5f * 0.6f)) - ((screenRes(258.0f * 0.6f)) * GetNormalizedPlayerHeight());
        float barW = screenRes(11.0f * 0.5f * 0.6f);
        CSprite2d::DrawRect(CRect(x - barW - screenRes(1.5f), currentY - screenRes(0.5f), x + barW + screenRes(1.5f), currentY + screenRes(0.5f)), CRGBA(255, 255, 255, 255));
    }

    static void DrawPlaneHeight(CRect const& rect, CRGBA const& color) {
        float x = screenRes(LIVE_POS_X + LIVE_RING + 15.0f), yCenter = screenHeight - screenRes(LIVE_POS_Y);
        float barW = screenRes(11.0f * 0.5f * 0.6f), barHMax = screenRes(258.0f * 0.5f * 0.6f);
        float barBottom = yCenter + barHMax;
        float currentY = barBottom - ((barHMax * 2.0f) * GetNormalizedPlayerHeight());
        CSprite2d::DrawRect(CRect(x - barW, currentY, x + barW, barBottom), CRGBA(140, 211, 239, 255));
    }

    static void release() {
        if (txdIndex != -1) {
            mLeedsHudSprite.Delete();
            mBarFrame.Delete();
            CTxdStore::RemoveTxdSlot(txdIndex);
            txdIndex = -1;
            pMapDirTex = pNorthTex = pDestinationTex = nullptr;
        }
    }
};

int LeedsHUD::txdIndex = -1;

class LeedsHUDLoad {
public:
    LeedsHUDLoad() {
        Events::initRwEvent += [] { LeedsHUD::initiate(); };
        Events::shutdownRwEvent += [] { LeedsHUD::release(); };
    }
} leedsradar;