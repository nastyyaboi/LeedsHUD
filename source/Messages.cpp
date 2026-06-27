#include "plugin.h"
#include "CFont.h"
#include "CMessages.h"
#include "CHud.h"
#include "CTimer.h"
#include "CMenuSystem.h"
#include "CStats.h"
#include "CSprite2d.h"
#include "CText.h"
#include "CHudColours.h"
#include "CCamera.h"
#include "CPad.h"
#include <windows.h>
#include <string>

using namespace plugin;

static constexpr float MessageOriginX = 48.0f;
static constexpr float MessageOriginY = 29.5f;
static constexpr float MessageScaleW = 0.36f;
static constexpr float MessageScaleH = 0.69f;
static constexpr float MessageWidthMax = 210.0f;
static constexpr uint8_t MessageAlpha = 110;

static constexpr float StatLabelScaleW = 0.35f;
static constexpr float StatLabelScaleH = 0.70f;
static constexpr float StatBarHeight = 8.0f;
static constexpr float StatSignGap = 4.0f;
static constexpr float StatBarGap = 10.0f;
static constexpr float StatBarYShift = 2.0f;

static CRGBA CustomBarColor(203, 68, 125, 255);
static CRGBA CustomMinusColor(180, 0, 0, 255); 
static CRGBA CustomTrackColor(57, 23, 37, 255); 
static CRGBA CustomDeltaColor(140, 50, 90, 0); 

static float Sc(float value) {
    float screenHeight = static_cast<float>(*(int*)0xC17048);
    return value * (screenHeight / 448.0f);
}

static float ScreenWidth() {
    return static_cast<float>(*(int*)0xC17044);
}

static std::string GetIniPath() {
    char path[MAX_PATH];
    HMODULE hm = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)&Sc, &hm);
    GetModuleFileNameA(hm, path, sizeof(path));
    std::string strPath(path);
    size_t lastSlash = strPath.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        return strPath.substr(0, lastSlash + 1) + "LeedsHUD.ini";
    }
    return "LeedsHUD.ini";
}

static void LoadConfiguration() {
    std::string iniPath = GetIniPath();

    char modeStr[32] = { 0 };
    GetPrivateProfileStringA("Settings", "Mode", "vcs", modeStr, sizeof(modeStr), iniPath.c_str());
    std::string mode = modeStr;

    if (mode == "lcs") {
        CustomBarColor = CRGBA(7, 49, 129, 255);
        CustomTrackColor = CRGBA(10, 10, 10, 55);
        CustomDeltaColor = CRGBA(0, 0, 0, 0);  
    }
    else {
        CustomBarColor = CRGBA(196, 96, 138, 255);
        CustomTrackColor = CRGBA(10, 10, 10, 55);
        CustomDeltaColor = CRGBA(140, 50, 90, 0); 
    }
    CustomMinusColor = CRGBA(180, 0, 0, 255);

    int r = GetPrivateProfileIntA("Colors", "MessagesStatsBarR", 255, iniPath.c_str());
    int g = GetPrivateProfileIntA("Colors", "MessagesStatsBarG", 255, iniPath.c_str());
    int b = GetPrivateProfileIntA("Colors", "MessagesStatsBarB", 255, iniPath.c_str());
    if (r != 255 && g != 255 && b != 255) {
        CustomBarColor = CRGBA(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), 255);
        CustomTrackColor = CRGBA(static_cast<uint8_t>(r / 4), static_cast<uint8_t>(g / 4), static_cast<uint8_t>(b / 4), 255);

        CustomDeltaColor = CRGBA(
            static_cast<uint8_t>(r / 1.6f),
            static_cast<uint8_t>(g / 1.6f),
            static_cast<uint8_t>(b / 1.6f),
            0
        );
    }

    int mr = GetPrivateProfileIntA("Colors", "MinusR", -1, iniPath.c_str()); // FUH TS
    int mg = GetPrivateProfileIntA("Colors", "MinusG", -1, iniPath.c_str());
    int mb = GetPrivateProfileIntA("Colors", "MinusB", -1, iniPath.c_str());
    if (mr != -1 && mg != -1 && mb != -1) {
        CustomMinusColor = CRGBA(static_cast<uint8_t>(mr), static_cast<uint8_t>(mg), static_cast<uint8_t>(mb), 255);
    }
}

static void DrawSafeProgressBarWithDelta(float x, float y, float width, float height, float progressPercent, float progressAddPercent, CRGBA mainBarColor, CRGBA addColor) {

    CRect borderRect(x - Sc(2.0f), y - Sc(2.0f), x + width + Sc(2.0f), y + height + Sc(2.1f));
    CSprite2d::DrawRect(borderRect, CRGBA(0, 0, 0, 155));

    CRect trackRect(x, y, x + width, y + height);
    CSprite2d::DrawRect(trackRect, CustomTrackColor);

    if (progressPercent < 0.0f) progressPercent = 0.0f;
    if (progressPercent > 100.0f) progressPercent = 100.0f;

    float fillWidth = (progressPercent / 100.0f) * width;
    if (fillWidth > 0.0f) {
        CRect fillRect(x, y, x + fillWidth, y + height);
        CSprite2d::DrawRect(fillRect, mainBarColor);
    }
    if (progressAddPercent != 0.0f) {
        float addWidth = (progressAddPercent / 100.0f) * width;
        float startX = x + fillWidth;

        if (addWidth < 0.0f) {
            startX += addWidth;
            addWidth = -addWidth;
        }

        if (startX < x) startX = x;
        if (startX + addWidth > x + width) addWidth = (x + width) - startX;

        if (addWidth > 0.0f) {
            CRect addRect(startX, y, startX + addWidth, y + height);
            CSprite2d::DrawRect(addRect, addColor);
        }
    }
}

static void ProcessMessageRendering() {
    if (!CHud::m_pHelpMessage[0]) {
        CHud::m_nHelpMessageState = 0;
        return;
    }

    if (!CMessages::StringCompare(CHud::m_pHelpMessage, CHud::m_pLastHelpMessage, 400)) {
        if (CHud::m_nHelpMessageState == 0) {
            reinterpret_cast<void(__thiscall*)(void*, int, float, float)>(0x506EA0)((void*)0xB6BC90, 32, 0.0f, 1.0f);
        }
        CHud::m_nHelpMessageState = 1;
        CHud::m_nHelpMessageTimer = 0;
        CMessages::StringCopy(CHud::m_pHelpMessageToPrint, CHud::m_pHelpMessage, 400);
        CMessages::StringCopy(CHud::m_pLastHelpMessage, CHud::m_pHelpMessage, 400);
        CFont::SetScale(Sc(MessageScaleW), Sc(MessageScaleH));
        CFont::SetWrapx(Sc(MessageOriginX + MessageWidthMax));
        CHud::m_fHelpMessageTime = static_cast<float>(CFont::GetNumberLines(Sc(MessageOriginX), Sc(MessageOriginY), CHud::m_pHelpMessageToPrint)) + 1.0f;
    }

    if (CHud::m_nHelpMessageState == 0) return;
    CHud::m_nHelpMessageTimer += static_cast<int>(CTimer::ms_fTimeStep * 20.0f);

    if (!CHud::m_bHelpMessagePermanent && CHud::m_nHelpMessageTimer > CHud::m_fHelpMessageTime * 1500.0f) {
        CHud::m_nHelpMessageState = 0;
        CHud::m_pHelpMessage[0] = 0;
        return;
    }

    float originX = Sc(MessageOriginX);
    float originY = Sc(MessageOriginY);
    float wrapX = Sc(MessageOriginX + MessageWidthMax);

    CFont::SetAlphaFade(255.0f);
    CFont::SetProportional(true);
    CFont::SetOrientation(ALIGN_LEFT);
    CFont::SetFontStyle(FONT_SUBTITLES);
    CFont::SetBackground(false, false);
    CFont::SetEdge(1);
    CFont::SetDropColor(CRGBA(0, 0, 0, 255));

    if (CHud::m_nHelpMessageStatId) {
        CFont::SetScale(Sc(StatLabelScaleW), Sc(StatLabelScaleH));
        CFont::SetWrapx(wrapX + 0.5f);

        int numLines = CFont::GetNumberLines(originX, originY, CHud::m_pHelpMessageToPrint);
        float lineH = Sc(StatLabelScaleH * 19.0f);
        float boxHeight = (numLines * lineH) + Sc(7.0f);

        CRect statRect(originX - Sc(6.0f), originY - Sc(4.0f), wrapX + Sc(6.0f), originY - Sc(4.0f) + boxHeight);
        CSprite2d::DrawRect(statRect, CRGBA(0, 0, 0, MessageAlpha));

        CFont::SetColor(CRGBA(255, 255, 255, 255));
        CFont::PrintString(originX, originY, CHud::m_pHelpMessageToPrint);

        char gxtKey[16];
        int sid = CHud::m_nHelpMessageStatId;
        sprintf(gxtKey, (sid < 10) ? "STAT00%d" : (sid < 100) ? "STAT0%d" : "STAT%d", sid);
        const char* statName = TheText.Get(gxtKey);

        float signW = CFont::GetStringWidth(CHud::m_pHelpMessageToPrint, true, false);
        float nameW = CFont::GetStringWidth(const_cast<char*>(statName), true, false);
        float barW = fmaxf(Sc(MessageWidthMax) - (signW + Sc(StatSignGap) + nameW + Sc(StatBarGap)), Sc(30.0f));

        float nameX = originX + signW + Sc(StatSignGap);
        CFont::PrintString(nameX, originY, const_cast<char*>(statName));

        float barX = nameX + nameW + Sc(StatBarGap);
        float barY = originY + Sc(StatBarYShift) + (Sc(StatLabelScaleH) * 2.2f) - Sc(0.9f);

        float progress = (sid == 336) ? static_cast<float>(CallMethodAndReturn<unsigned int, 0x5F6AA0>((void*)(0xC09928 + FindPlayerPed(-1)->m_pPlayerData->m_nPlayerGroup * 0x2D4))) : CStats::GetStatValue(sid);
        float maxVal = (CHud::m_nHelpMessageMaxStatValue <= 0) ? 1000.0f : static_cast<float>(CHud::m_nHelpMessageMaxStatValue);

        CRGBA activeDeltaColor = (CHud::m_fHelpMessageStatUpdateValue >= 0.0f) ? CustomDeltaColor : CustomMinusColor;

        float currentProgressPercent = (1.0f / maxVal) * progress * 100.0f;
        float addedProgressPercent = (1.0f / maxVal) * CHud::m_fHelpMessageStatUpdateValue * 100.0f;

        DrawSafeProgressBarWithDelta(barX, barY, barW, Sc(StatBarHeight), currentProgressPercent, addedProgressPercent, CustomBarColor, activeDeltaColor);
    }
    else {
        CFont::SetScale(Sc(MessageScaleW), Sc(MessageScaleH));
        CFont::SetWrapx(wrapX + 0.5f);

        int numLines = CFont::GetNumberLines(originX, originY, CHud::m_pHelpMessageToPrint);

        float lineMultiplier = 18.6f;
        float padding = Sc(9.5f);

        float lineH = Sc(MessageScaleH * lineMultiplier);
        float boxHeight = (numLines * lineH) + padding;

        CRect helpRect(originX - Sc(6.0f), originY - Sc(4.0f), wrapX + Sc(6.0f), originY - Sc(4.0f) + boxHeight);

        CSprite2d::DrawRect(helpRect, CRGBA(0, 0, 0, MessageAlpha));

        CFont::SetColor(CRGBA(255, 255, 255, 255));
        CFont::PrintString(originX, originY + Sc(1.5f), CHud::m_pHelpMessageToPrint);
    }
    CFont::SetEdge(0);
    CFont::SetDropShadowPosition(0);
    CFont::SetWrapx(ScreenWidth());
}

class LeedsHUD {
public:
    LeedsHUD() {
        LoadConfiguration();
        patch::RedirectCall(0x58FCFA, ProcessMessageRendering);
    }
} LeedsMessages;