#include "plugin.h"
#include "CHud.h"
#include "CFont.h"
#include "CTimer.h"
#include "CText.h"
#include "CSprite2d.h"
#include "CCutsceneMgr.h"
#include "CCamera.h"
#include <windows.h>
#include <algorithm>
#include <string>

using namespace plugin;
bool g_LeedsCutsceneNameVisible = false;

#define SCREEN_WIDTH   ((float)RsGlobal.maximumWidth)
#define SCREEN_HEIGHT ((float)RsGlobal.maximumHeight)
#define SCALE_Y(val)  (val * (SCREEN_HEIGHT / 1080.0f))
#define SCALE_X(val)  (val * (SCREEN_WIDTH  / 1920.0f))

#define REWARD_GAP_PERCENT 0.02f 

typedef void(__cdecl* AddBigMessage_t)(const char*, unsigned int, unsigned short);
void __cdecl MyAddBigMessage(const char* text, unsigned int time, unsigned short style) {
    if (text && (style == 1 || style == 2)) return;
    ((AddBigMessage_t)0x58C6A5)(text, time, style);
}

static std::string GetAsiDir() {
    char buf[MAX_PATH] = {};
    HMODULE hMod = nullptr;
    GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCSTR>(&GetAsiDir), &hMod);
    GetModuleFileNameA(hMod, buf, MAX_PATH);
    std::string s(buf);
    return s.substr(0, s.find_last_of("\\/") + 1);
}

static int GetHudMode() {
    static bool bChecked = false;
    static int  cachedMode = 0;
    if (!bChecked) {
        std::string iniPath = GetAsiDir() + "LeedsHUD.ini";
        char result[32] = {};
        GetPrivateProfileStringA("Settings", "Mode", "default", result, sizeof(result), iniPath.c_str());
        std::string m(result);
        if (m == "vcs" || m == "VCS") cachedMode = 2;
        else if (m == "lcs" || m == "LCS") cachedMode = 1;
        bChecked = true;
    }
    return cachedMode;
}

static bool IsVcsMode() { return GetHudMode() == 2; }

static std::string StripColorTokens(const std::string& input) {
    std::string o = input;
    size_t pos = 0;
    while ((pos = o.find('~')) != std::string::npos) {
        size_t e = o.find('~', pos + 1);
        if (e != std::string::npos) o.erase(pos, (e - pos) + 1);
        else break;
    }
    return o;
}

class LeedsHUDBigMessage {
public:

    static inline bool          bDeathArrestShowing = false;
    static inline bool          bDeathArrestFadingOut = false;
    static inline float          alphaDeathArrest = 0.0f;
    static inline char          capturedDeathArrestTitle[128] = "";

    static inline bool          bMissionShowing = false;
    static inline bool          bMissionFadingOut = false;
    static inline bool          bIsTemporary = false;
    static inline float          alphaMission = 0.0f;
    static inline unsigned int  missionDisplayTimer = 0;
    static inline char          capturedMissionTitle[128] = "";
    static inline char          capturedFailReason[256] = "";
    static inline char          capturedReward[256] = "";

    static inline bool          bCutsceneNameShowing = false;
    static inline bool          bCutsceneNameFadingOut = false;
    static inline float          alphaCutsceneName = 0.0f;
    static inline unsigned int  cutsceneNameTimer = 0;
    static inline char          capturedCutsceneName[128] = "";

    static inline bool          bProcessingDeathArrest = false;
    static inline bool          bProcessingMission = false;
    static inline char          lastProcessedMissionString[128] = "";
    static inline unsigned int  missionStringResetTimer = 0;
    static inline unsigned int  deathArrestResetTimer = 0;
    static inline char          lastProcessedDeathArrestString[128] = "";
    static inline unsigned int  lastClosedTime = 0;

    static inline bool          bIsFailedShowing = false;

    static bool IsMajorEvent(const char* t) {
        if (!t) return false;
        return strstr(t, "PASSED") || strstr(t, "FAILED") || strstr(t, "WASTED") || strstr(t, "BUSTED") || strstr(t, "WINNER");
    }
    static bool IsAnyResult(const char* t) {
        if (!t || !t[0]) return false;
        return IsMajorEvent(t) || strstr(t, "Round") || strstr(t, "LEVEL") || strstr(t, "Passed") || strstr(t, "Winner");
    }
    static bool IsBadReadPtr(void* p) {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(p, &mbi, sizeof(mbi))) {
            DWORD mask = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
            return !(mbi.Protect & mask) || (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS));
        }
        return true;
    }

    static void Update() {
        unsigned int now = CTimer::m_snTimeInMilliseconds;

        if (now > missionStringResetTimer)  lastProcessedMissionString[0] = '\0';
        if (now > deathArrestResetTimer)    lastProcessedDeathArrestString[0] = '\0';

        if (bCutsceneNameShowing) {
            if (!bCutsceneNameFadingOut) {
                alphaCutsceneName += 0.05f;
                if (alphaCutsceneName > 1.0f) alphaCutsceneName = 1.0f;
                if (now > cutsceneNameTimer) bCutsceneNameFadingOut = true;
            }
            else {
                alphaCutsceneName -= 0.03f;
                if (alphaCutsceneName <= 0.0f) {
                    alphaCutsceneName = 0.0f;
                    bCutsceneNameShowing = bCutsceneNameFadingOut = false;
                    memset(capturedCutsceneName, 0, sizeof(capturedCutsceneName));
                }
            }
        }

        if (CHud::m_BigMessage[1][0] != '\0') {
            if (strcmp(capturedCutsceneName, CHud::m_BigMessage[1]) != 0) {
                std::string raw = StripColorTokens(std::string(CHud::m_BigMessage[1]));
                if (!raw.empty()) {
                    strncpy(capturedCutsceneName, raw.c_str(), 127);
                    alphaCutsceneName = 0.0f;
                    bCutsceneNameShowing = true;
                    bCutsceneNameFadingOut = false;
                    cutsceneNameTimer = now + 4000;
                }
            }
        }

        if (CCutsceneMgr::ms_running) {
            CHud::m_BigMessage[1][0] = '\0';
            return;
        }

        if (!bMissionShowing && !bDeathArrestShowing) {
            if (CHud::m_Message[0] && (strstr(CHud::m_Message, "~r~") || strstr(CHud::m_Message, "~R~")))
                strncpy(capturedFailReason, CHud::m_Message, 255);
        }

        CPed* player = FindPlayerPed();
        if (player && (uintptr_t)player > 0x1000 && !IsBadReadPtr(player) && !bProcessingDeathArrest) {
            float fHealth = patch::Get<float>(reinterpret_cast<uintptr_t>(player) + 0x540);
            bool  isDead = fHealth < 1.0f;
            unsigned int ps = 0;
            if (!isDead) ps = patch::Get<unsigned int>(reinterpret_cast<uintptr_t>(player) + 0x530);
            bool isBusted = (ps == 63);
            if (isDead || isBusted) {
                const char* ev = isDead ? "WASTED!" : "BUSTED!";
                if (strcmp(lastProcessedDeathArrestString, ev) != 0) {
                    bProcessingDeathArrest = true;
                    strncpy(capturedDeathArrestTitle, ev, 127);
                    strncpy(lastProcessedDeathArrestString, ev, 127);
                    deathArrestResetTimer = now + 5000;
                    bDeathArrestShowing = true;
                    bDeathArrestFadingOut = false;
                    memset(capturedFailReason, 0, sizeof(capturedFailReason));
                }
            }
        }

        if (!bProcessingMission && CHud::m_BigMessage[0][0]) {
            const char* hudMsg = CHud::m_BigMessage[0];
            if (IsAnyResult(hudMsg) && strcmp(lastProcessedMissionString, hudMsg) != 0) {
                bool isResult = strstr(hudMsg, "PASSED") || strstr(hudMsg, "FAILED") ||
                    strstr(hudMsg, "Passed") || strstr(hudMsg, "WINNER") || strstr(hudMsg, "Winner");
                if (isResult && (now - lastClosedTime < 4000)) {
                    CHud::m_BigMessage[0][0] = '\0';
                    return;
                }
                bProcessingMission = true;
                strncpy(lastProcessedMissionString, hudMsg, 127);
                missionStringResetTimer = now + 10000;

                char* nl = (char*)strstr(hudMsg, "~n~");
                if (nl) {
                    int len = nl - hudMsg;
                    strncpy(capturedMissionTitle, hudMsg, len > 127 ? 127 : len);
                    capturedMissionTitle[len] = '\0';
                    strncpy(capturedReward, nl + 3, 255);
                }
                else {
                    strncpy(capturedMissionTitle, hudMsg, 127);
                    capturedReward[0] = '\0';
                }
                bMissionShowing = true;
                bMissionFadingOut = false;
                bIsTemporary = !IsMajorEvent(hudMsg);
                missionDisplayTimer = now + (bIsTemporary ? 2500 : 6000);
            }
        }

        CHud::m_BigMessage[0][0] = '\0';
        CHud::m_BigMessage[1][0] = '\0';

        bIsFailedShowing = bMissionShowing && strstr(capturedMissionTitle, "FAILED") && !bIsTemporary;

        if (bDeathArrestShowing) {
            if (!bDeathArrestFadingOut) {
                alphaDeathArrest += 0.05f;
                if (alphaDeathArrest > 1.0f) alphaDeathArrest = 1.0f;
                if (TheCamera.m_fFadeAlpha > 240.0f) bDeathArrestFadingOut = true;
            }
            else {
                alphaDeathArrest -= 0.05f;
                if (alphaDeathArrest <= 0.0f) {
                    alphaDeathArrest = 0.0f;
                    bDeathArrestShowing = bDeathArrestFadingOut = bProcessingDeathArrest = false;
                    memset(capturedDeathArrestTitle, 0, sizeof(capturedDeathArrestTitle));
                }
            }
        }

        if (bMissionShowing) {
            if (!bMissionFadingOut) {
                alphaMission += 0.05f;
                if (alphaMission > 1.0f) alphaMission = 1.0f;
                if ((bDeathArrestShowing && TheCamera.m_fFadeAlpha > 240.0f) || now > missionDisplayTimer)
                    bMissionFadingOut = true;
            }
            else {
                alphaMission -= 0.05f;
                if (alphaMission <= 0.0f) {
                    alphaMission = 0.0f;
                    lastClosedTime = now;
                    bMissionShowing = bMissionFadingOut = bIsTemporary = bProcessingMission = false;
                    memset(capturedMissionTitle, 0, sizeof(capturedMissionTitle));
                    memset(capturedReward, 0, sizeof(capturedReward));
                    memset(capturedFailReason, 0, sizeof(capturedFailReason));
                }
            }
        }
        g_LeedsCutsceneNameVisible = (bCutsceneNameShowing && alphaCutsceneName > 0.0f);
    }

    static void ApplySafeNoWrap() {
        CFont::SetWrapx(10000.0f);
        CFont::SetCentreSize(10000.0f);
        CFont::SetRightJustifyWrap(10000.0f);
    }

    static void Draw() {
        float resScale = SCREEN_HEIGHT / 1080.0f;
        float centerX = SCREEN_WIDTH / 2.0f;
        float centerY = SCREEN_HEIGHT / 2.0f;
        int   hudMode = GetHudMode();
        bool  vcs = (hudMode == 2);

        if (!CCutsceneMgr::ms_running) {

            if (alphaDeathArrest > 0.0f) {
                unsigned char a = (unsigned char)(210.0f * alphaDeathArrest);
                CRGBA col = vcs ? CRGBA(9, 211, 114, a)
                    : (strstr(capturedDeathArrestTitle, "BUSTED") ? CRGBA(100, 180, 255, a) : CRGBA(255, 60, 60, a));
                CFont::SetProportional(true);
                CFont::SetOrientation(ALIGN_CENTER);
                CFont::SetDropColor(CRGBA(0, 0, 0, a));
                CFont::SetFontStyle(FONT_PRICEDOWN);
                CFont::SetScale(3.2f * resScale, 4.8f * resScale);
                CFont::SetColor(col);
                CFont::SetEdge(1);
                CFont::SetDropShadowPosition(2);

                ApplySafeNoWrap();
                CFont::PrintString(centerX, centerY - SCALE_Y(220.0f), capturedDeathArrestTitle);
            }

            if (alphaMission > 0.0f) {
                unsigned char a = (unsigned char)(210.0f * alphaMission);
                CRGBA col = vcs ? CRGBA(200, 120, 151, a)
                    : ((strstr(capturedMissionTitle, "PASSED") || strstr(capturedMissionTitle, "WINNER") || bIsTemporary)
                        ? CRGBA(255, 220, 0, a) : CRGBA(255, 60, 60, a));

                CFont::SetProportional(true);
                CFont::SetOrientation(ALIGN_CENTER);
                CFont::SetDropColor(CRGBA(0, 0, 0, a));
                CFont::SetFontStyle(FONT_PRICEDOWN);
                CFont::SetScale((bIsTemporary ? 2.8f : 3.2f) * resScale, (bIsTemporary ? 4.2f : 4.8f) * resScale);
                CFont::SetColor(col);
                CFont::SetEdge(1);
                CFont::SetDropShadowPosition(2);

                std::string title = capturedMissionTitle;
                if (vcs) title = StripColorTokens(title);

                ApplySafeNoWrap();
                CFont::PrintString(centerX, centerY - SCALE_Y(40.0f), (char*)title.c_str());

                CFont::SetEdge(1);
                CFont::SetDropShadowPosition(2);
                float y = centerY + SCALE_Y(70.0f);

                if (capturedFailReason[0] && strstr(capturedMissionTitle, "FAILED") && !bIsTemporary) {
                    std::string fr = capturedFailReason;
                    if (vcs) fr = StripColorTokens(fr);
                    CFont::SetFontStyle(FONT_SUBTITLES);
                    CFont::SetDropShadowPosition(1);
                    CFont::SetScale(0.85f * resScale, 1.70f * resScale);
                    CFont::SetColor(vcs ? col : CRGBA(255, 60, 60, a));

                    ApplySafeNoWrap();
                    CFont::PrintString(centerX, y, (char*)fr.c_str());
                    y += SCALE_Y(45.0f);
                }

                if (capturedReward[0]) {
                    std::string rw = capturedReward;

                    rw = StripColorTokens(rw);

                    size_t tokenPos = 0;
                    while ((tokenPos = rw.find("~n~")) != std::string::npos) rw.erase(tokenPos, 3);
                    while ((tokenPos = rw.find('\n')) != std::string::npos) rw.erase(tokenPos, 1);
                    while ((tokenPos = rw.find('\r')) != std::string::npos) rw.erase(tokenPos, 1);

                    while (!rw.empty() && (rw.back() == ' ' || rw.back() == '\t')) rw.pop_back();
                    while (!rw.empty() && (rw.front() == ' ' || rw.front() == '\t')) rw.erase(0, 1);

                    CFont::SetProportional(true);
                    CFont::SetFontStyle(FONT_PRICEDOWN);
                    CFont::SetScale(1.5f * resScale, 2.8f * resScale);
                    CFont::SetColor(vcs ? col : CRGBA(225, 225, 225, a));
                    CFont::SetOrientation(ALIGN_CENTER);

                    size_t respectPos = rw.find("RESPECT");
                    size_t moneyPos = rw.find("$");

                    std::string finalizedRewardString;

                    if (respectPos != std::string::npos && moneyPos != std::string::npos) {
                        std::string moneyPart = rw.substr(0, respectPos);
                        std::string respectPart = rw.substr(respectPos);

                        while (!moneyPart.empty() && moneyPart.back() == ' ') moneyPart.pop_back();
                        while (!respectPart.empty() && respectPart.front() == ' ') respectPart.erase(0, 1);

                        if (respectPart.find('+') == std::string::npos) respectPart += " +";

                        finalizedRewardString = moneyPart + " - " + respectPart;
                    }
                    else {
                        if (respectPos != std::string::npos && rw.find('+') == std::string::npos) {
                            rw += " +";
                        }
                        finalizedRewardString = rw;
                    }

                    ApplySafeNoWrap();
                    CFont::PrintString(centerX, centerY + SCALE_Y(70.0f), const_cast<char*>(finalizedRewardString.c_str()));
                }
            }
        }

        if (alphaCutsceneName > 0.0f) {
            unsigned char a = (unsigned char)(210.0f * alphaCutsceneName);

            eFontStyle font;
            if (hudMode == 1) font = FONT_MENU;
            else if (hudMode == 2) font = FONT_GOTHIC;

            std::string name = std::string(capturedCutsceneName);

            float targetX = SCREEN_WIDTH - SCALE_X(100.0f);
            float targetY = SCREEN_HEIGHT - SCALE_Y(120.0f);

            CFont::SetProportional(true);
            CFont::SetOrientation(ALIGN_RIGHT);
            CFont::SetFontStyle(font);
            CFont::SetWrapx(-1000.0f);

            CFont::SetColor(CRGBA(255, 220, 0, a));
            CFont::SetDropColor(CRGBA(0, 0, 0, a));
            CFont::SetEdge(1);
            CFont::SetDropShadowPosition(1);

            if (hudMode == 2) {
                CFont::SetScale(2.4f * resScale, 3.7f * resScale);
                CFont::PrintString(targetX, targetY, (char*)name.c_str());
            }
            else if (hudMode == 1) {
                CFont::SetScale(1.4f * resScale, 2.2f * resScale);
                CFont::PrintString(targetX, targetY, (char*)name.c_str());
            }
        }

        CFont::SetOrientation(ALIGN_LEFT);
        CFont::SetJustify(false);
        CFont::SetEdge(0);
        CFont::SetDropShadowPosition(0);
    }

    LeedsHUDBigMessage() {
        patch::RedirectJump(0x58C6A0, (void*)MyAddBigMessage);
        Events::processScriptsEvent += Update;
        Events::drawHudEvent += Draw;
    }
} leedshudbigmessage;