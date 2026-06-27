#include "plugin.h"
#include "CCamera.h"
#include "CFont.h"
#include "CHud.h"
#include "CReplay.h"
#include "CTimer.h"
#include "CAERadioTrackManager.h"

using namespace plugin;

static float Res(float value) { return value * ((float)RsGlobal.maximumHeight / 980.0f); }
static float ResW(float value) { return value * ((float)RsGlobal.maximumHeight / 980.0f); }

enum eSAFont {
    SA_FONT_GOTHIC = 0,
    SA_FONT_SUBTITLES = 1,
    SA_FONT_MENU = 2,
};

struct RadioCfg {
    int   fontStyle{ SA_FONT_MENU };
    CRGBA colorNormal{ 30,  60,  140, 255 };
    CRGBA colorActive{ 100, 180, 255, 255 };
    float posX{ 0.0f };
    float posY{ 46.0f };
    float scaleX{ 1.30f };
    float scaleY{ 2.30f };
};

static RadioCfg cfg;

static void LoadConfig() {
    char iniPath[MAX_PATH];
    sprintf_s(iniPath, "%s\\LeedsHUD.ini", PLUGIN_PATH(""));

    char mode[16];
    GetPrivateProfileStringA("Settings", "Mode", "vcs", mode, sizeof(mode), iniPath);
    cfg.fontStyle = (_stricmp(mode, "lcs") == 0) ? SA_FONT_MENU : SA_FONT_GOTHIC;

    auto readIntSafe = [&](const char* key, int def) -> int {
        char buf[32];
        GetPrivateProfileStringA("Colors", key, "", buf, sizeof(buf), iniPath);
        return buf[0] ? atoi(buf) : def;
        };

    auto readFloat = [&](const char* key, float def) -> float {
        char buf[32];
        GetPrivateProfileStringA("Colors", key, "", buf, sizeof(buf), iniPath);
        return buf[0] ? (float)atof(buf) : def;
        };

    cfg.colorNormal.r = (uint8_t)readIntSafe("ColorNormal_R", cfg.colorNormal.r);
    cfg.colorNormal.g = (uint8_t)readIntSafe("ColorNormal_G", cfg.colorNormal.g);
    cfg.colorNormal.b = (uint8_t)readIntSafe("ColorNormal_B", cfg.colorNormal.b);
    cfg.colorNormal.a = (uint8_t)readIntSafe("ColorNormal_A", cfg.colorNormal.a);

    cfg.colorActive.r = (uint8_t)readIntSafe("ColorActive_R", cfg.colorActive.r);
    cfg.colorActive.g = (uint8_t)readIntSafe("ColorActive_G", cfg.colorActive.g);
    cfg.colorActive.b = (uint8_t)readIntSafe("ColorActive_B", cfg.colorActive.b);
    cfg.colorActive.a = (uint8_t)readIntSafe("ColorActive_A", cfg.colorActive.a);

    cfg.posX = readFloat("PosX", cfg.posX);
    cfg.posY = readFloat("PosY", cfg.posY);
    cfg.scaleX = readFloat("ScaleX", cfg.scaleX);
    cfg.scaleY = readFloat("ScaleY", cfg.scaleY);
}

static void DrawRadioStation() {
    if (CTimer::m_UserPause
        || CTimer::m_CodePause
        || TheCamera.m_bWideScreenOn
        || !FindPlayerVehicle(-1, 0)
        || CReplay::Mode == 1)
    {
        return;
    }

    bool isScrollingNow = (AERadioTrackManager.m_nStationsListed != 0 || AERadioTrackManager.m_nStationsListDown != 0);
    if (isScrollingNow) {
        AERadioTrackManager.m_nTimeToDisplayRadioName = CTimer::m_snTimeInMilliseconds + 2500;
    }

    if (AERadioTrackManager.field_1 && AERadioTrackManager.IsVehicleRadioActive()) {
        AERadioTrackManager.m_nTimeToDisplayRadioName = CTimer::m_snTimeInMilliseconds + 2500;
        AERadioTrackManager.field_1 = 0;
    }

    if (CTimer::m_snTimeInMilliseconds >= AERadioTrackManager.m_nTimeToDisplayRadioName)
        return;

    int currentStationIdx = (int)AERadioTrackManager.m_Settings.m_nCurrentRadioStation;
    int stationIdx = AERadioTrackManager.m_nStationsListed + currentStationIdx;

    if (stationIdx == 0 && isScrollingNow) {
        if (AERadioTrackManager.m_nStationsListed > 0)
            stationIdx = AERadioTrackManager.m_nStationsListed;
        else if (AERadioTrackManager.m_nStationsListDown > 0)
            stationIdx = 13 - AERadioTrackManager.m_nStationsListDown;
    }

    if (stationIdx == 0)
        return;

    if (stationIdx > 0) {
        if (stationIdx >= 14)
            stationIdx = (stationIdx & 0xFF) - 13;
    }
    else {
        stationIdx = (stationIdx & 0xFF) + 13;
    }

    char* pName = AERadioTrackManager.GetRadioStationName((signed char)stationIdx);
    if (!pName || pName[0] == '\0')
        return;

    CFont::SetProportional(true);
    CFont::SetBackground(false, false);
    CFont::SetOrientation(ALIGN_CENTER);
    CFont::SetCentreSize(ResW(2240.0f));
    CFont::SetFontStyle(cfg.fontStyle);
    CFont::SetScale(ResW(cfg.scaleX), Res(cfg.scaleY));
    CFont::SetDropShadowPosition(1);
    CFont::SetDropColor(CRGBA(0, 0, 0, 255));
    CFont::SetWrapx(10000.0f);

    if (isScrollingNow)
        CFont::SetColor(cfg.colorNormal);
    else
        CFont::SetColor(cfg.colorActive);

    CFont::PrintString(
        ((float)RsGlobal.maximumWidth / 2.0f) + ResW(cfg.posX),
        Res(cfg.posY),
        pName
    );

    CFont::DrawFonts();
}

class RadioTextOverride {
public:
    RadioTextOverride() {
        LoadConfig();
        patch::Set(0x53E4FA, 5);
        Events::drawHudEvent += [] {
            DrawRadioStation();
            };
    }
} radioTextOverride;