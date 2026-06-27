#include "plugin.h"
#include "CSprite2d.h"
#include "CFont.h"
#include "CMenuManager.h"
#include "CUserDisplay.h"
#include "CHud.h"
#include "CGarages.h"
#include "CText.h"
#include "CHudColours.h"
#include <string>
#include <algorithm>
#include <vector>
#include <windows.h>

using namespace plugin;

enum class HudTheme {
    VCS,
    LCS
};

class LeedsHUDProgressBar {
public:
    static HudTheme currentTheme;

    static constexpr float GlobalScale = 1.25f;

    static float Res(float value) {
        return value * ((float)RsGlobal.maximumHeight / 1080.0f) * GlobalScale;
    }

    static void LoadConfiguration() {
        char asiPath[MAX_PATH] = { 0 };
        HMODULE hModule = NULL;

        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&LoadConfiguration, &hModule))
        {
            GetModuleFileNameA(hModule, asiPath, MAX_PATH);
        }

        std::string iniPath(asiPath);
        size_t lastSlash = iniPath.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            iniPath = iniPath.substr(0, lastSlash) + "\\LeedsHUD.ini";
        }
        else {
            iniPath = ".\\LeedsHUD.ini";
        }

        char modeBuffer[32] = { 0 };
        GetPrivateProfileStringA("Settings", "Mode", "vcs", modeBuffer, sizeof(modeBuffer), iniPath.c_str());

        std::string modeStr(modeBuffer);
        std::transform(modeStr.begin(), modeStr.end(), modeStr.begin(), ::tolower);

        if (modeStr == "lcs") {
            currentTheme = HudTheme::LCS;
        }
        else {
            currentTheme = HudTheme::VCS;
        }
    }

    static CRGBA GetCustomOrThemeColor(bool bIsHealthOrSweet) {
        char asiPath[MAX_PATH] = { 0 };
        HMODULE hModule = NULL;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&LoadConfiguration, &hModule)) {
            GetModuleFileNameA(hModule, asiPath, MAX_PATH);
        }
        std::string iniPath(asiPath);
        size_t lastSlash = iniPath.find_last_of("\\/");
        iniPath = (lastSlash != std::string::npos) ? iniPath.substr(0, lastSlash) + "\\LeedsHUD.ini" : ".\\LeedsHUD.ini";

        int r = GetPrivateProfileIntA("Colors", "ProgressBarR", 255, iniPath.c_str());
        int g = GetPrivateProfileIntA("Colors", "ProgressBarG", 255, iniPath.c_str());
        int b = GetPrivateProfileIntA("Colors", "ProgressBarB", 255, iniPath.c_str());

        if (r != 255 || g != 255 || b != 255) {
            return CRGBA(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), 255);
        }

        if (currentTheme == HudTheme::LCS) {
            return bIsHealthOrSweet ? CRGBA(138, 1, 1, 255) : CRGBA(0, 45, 130, 255);
        }
        else {
            return bIsHealthOrSweet ? CRGBA(204, 69, 122, 255) : CRGBA(0, 215, 209, 255);
        }
    }

    LeedsHUDProgressBar() {
        LoadConfiguration();

        patch::RedirectCall(0x58FBEE, MissionTimers);
        patch::RedirectJump(0x728640, ProgressBar);
    }

    static bool IsHealthOrSweetBar(const std::string& key, const std::string& text) {
        auto containsSubstring = [](std::string str, std::string sub) {
            std::transform(str.begin(), str.end(), str.begin(), ::tolower);
            std::transform(sub.begin(), sub.end(), sub.begin(), ::tolower);
            return str.find(sub) != std::string::npos;
            };

        return containsSubstring(key, "sweet") || containsSubstring(key, "health") ||
            containsSubstring(text, "sweet") || containsSubstring(text, "health");
    }

    static CRGBA GetThemeTextColor(bool bIsHealthOrSweet) {
        return GetCustomOrThemeColor(bIsHealthOrSweet);
    }

    static void GetLayoutPositions(float& outX, float& outValueX, float& outBaseY, float& outLineSpacing) {
        float screenW = (float)RsGlobal.maximumWidth;
        float screenH = (float)RsGlobal.maximumHeight;

        outX = screenW - Res(400.0f / GlobalScale);
        outValueX = screenW - Res(310.0f / GlobalScale) + Res(72.0f / GlobalScale);
        outBaseY = screenH - Res(730.0f / GlobalScale);
        outLineSpacing = Res(48.0f / GlobalScale);
    }

    static std::vector<std::string> SplitStringLines(const std::string& input) {
        std::vector<std::string> lines;
        std::string current = "";
        size_t i = 0;
        while (i < input.length()) {
            if (input[i] == '\n') {
                lines.push_back(current);
                current = "";
                i++;
            }
            else if (i + 3 <= input.length() && input.substr(i, 3) == "~n~") {
                lines.push_back(current);
                current = "";
                i += 3;
            }
            else {
                current += input[i];
                i++;
            }
        }
        lines.push_back(current);
        return lines;
    }

    static void __cdecl MissionTimers() {
        if ((CHud::m_BigMessage[4][0] && !CHud::bScriptForceDisplayWithCounters) || CGarages::MessageIDString[0])
            return;

        if (CUserDisplay::OnscnTimer.m_bDisplay != 1)
            return;

        float hX, valueColumnX, currentTextY, lineSpacing;
        GetLayoutPositions(hX, valueColumnX, currentTextY, lineSpacing);

        float barMaxW = Res(110.0f / GlobalScale);
        float rightAlignValueX = valueColumnX + barMaxW;
        float rightAlignLabelX = valueColumnX - Res(20.0f / GlobalScale);

        for (int i = 0; i < 4; ++i) {
            if (CUserDisplay::OnscnTimer.m_aCounters[i].m_bEnabled) {
                if (CUserDisplay::OnscnTimer.m_aCounters[i].m_nType == 1) {

                    float progress = (float)atoi(CUserDisplay::OnscnTimer.m_aCounters[i].m_szDisplayedText);

                    char* translatedText = nullptr;
                    if (CUserDisplay::OnscnTimer.m_aCounters[i].m_szDescriptionTextKey[0]) {
                        translatedText = const_cast<char*>(TheText.Get(CUserDisplay::OnscnTimer.m_aCounters[i].m_szDescriptionTextKey));
                        if (!translatedText || !translatedText[0]) {
                            translatedText = CUserDisplay::OnscnTimer.m_aCounters[i].m_szDescriptionTextKey;
                        }
                    }

                    bool bIsHealthBar = false;
                    if (CUserDisplay::OnscnTimer.m_aCounters[i].m_szDescriptionTextKey[0]) {
                        bIsHealthBar = IsHealthOrSweetBar(
                            CUserDisplay::OnscnTimer.m_aCounters[i].m_szDescriptionTextKey,
                            translatedText ? translatedText : ""
                        );
                    }

                    float startY = currentTextY;
                    float maxBlockHeight = lineSpacing;

                    if (translatedText) {
                        CFont::SetFontStyle(FONT_SUBTITLES);
                        CFont::SetScale(Res(0.7f / GlobalScale) * GlobalScale, Res(1.3f / GlobalScale) * GlobalScale);
                        CFont::SetEdge(1);
                        CFont::SetDropColor(CRGBA(0, 0, 0, 255));
                        CFont::SetOrientation(ALIGN_RIGHT);
                        CFont::SetCentreSize((float)RsGlobal.maximumWidth);
                        CFont::SetColor(CRGBA(255, 255, 255, 255));

                        std::vector<std::string> labelLines = SplitStringLines(translatedText);
                        float lineY = currentTextY + Res(3.0f / GlobalScale);
                        for (size_t l = 0; l < labelLines.size(); ++l) {
                            CFont::PrintString(rightAlignLabelX, lineY, const_cast<char*>(labelLines[l].c_str()));
                            if (l > 0) {
                                maxBlockHeight += Res(32.0f / GlobalScale);
                            }
                            lineY += Res(32.0f / GlobalScale);
                        }
                    }

                    CRGBA passingColor = HudColour.GetRGB(CUserDisplay::OnscnTimer.m_aCounters[i].m_nColourId, 255);
                    passingColor.a = bIsHealthBar ? 100 : 50;

                    ProgressBar(valueColumnX, startY, 100, 20, progress, 0, 0, 0, passingColor, CRGBA(0, 0, 0, 0));

                    currentTextY += maxBlockHeight;
                }
                else {
                    char* transText = nullptr;
                    if (CUserDisplay::OnscnTimer.m_aCounters[i].m_szDescriptionTextKey[0]) {
                        transText = const_cast<char*>(TheText.Get(CUserDisplay::OnscnTimer.m_aCounters[i].m_szDescriptionTextKey));
                    }
                    bool bIsHealth = IsHealthOrSweetBar(
                        CUserDisplay::OnscnTimer.m_aCounters[i].m_szDescriptionTextKey,
                        transText ? transText : ""
                    );

                    float advancedY = currentTextY;
                    DrawLeedsTextElement(rightAlignLabelX, rightAlignValueX, advancedY,
                        CUserDisplay::OnscnTimer.m_aCounters[i].m_szDisplayedText,
                        CUserDisplay::OnscnTimer.m_aCounters[i].m_szDescriptionTextKey,
                        GetThemeTextColor(bIsHealth));

                    currentTextY = advancedY;
                }
            }
        }

        if (CUserDisplay::OnscnTimer.m_Clock.m_bEnabled) {
            char* transText = nullptr;
            if (CUserDisplay::OnscnTimer.m_Clock.m_szDescriptionTextKey[0]) {
                transText = const_cast<char*>(TheText.Get(CUserDisplay::OnscnTimer.m_Clock.m_szDescriptionTextKey));
            }
            bool bIsHealth = IsHealthOrSweetBar(
                CUserDisplay::OnscnTimer.m_Clock.m_szDescriptionTextKey,
                transText ? transText : ""
            );

            float clockY = currentTextY;
            DrawLeedsTextElement(rightAlignLabelX, rightAlignValueX, clockY,
                CUserDisplay::OnscnTimer.m_Clock.m_szDisplayedText,
                CUserDisplay::OnscnTimer.m_Clock.m_szDescriptionTextKey,
                GetThemeTextColor(bIsHealth));
        }
    }

    static void DrawLeedsTextElement(float labelX, float valueX, float& ioY, const char* value, const char* description, CRGBA color) {
        CFont::SetFontStyle(FONT_SUBTITLES);
        CFont::SetScale(Res(0.7f / GlobalScale) * GlobalScale, Res(1.3f / GlobalScale) * GlobalScale);
        CFont::SetEdge(1);
        CFont::SetDropColor(CRGBA(0, 0, 0, 255));

        float startY = ioY;
        float totalBlockHeight = Res(48.0f / GlobalScale);

        if (description && description[0]) {
            char* textToPrint = const_cast<char*>(TheText.Get(const_cast<char*>(description)));
            if (!textToPrint || !textToPrint[0]) {
                textToPrint = const_cast<char*>(description);
            }
            CFont::SetOrientation(ALIGN_RIGHT);
            CFont::SetCentreSize((float)RsGlobal.maximumWidth);
            CFont::SetColor(CRGBA(255, 255, 255, 255));

            std::vector<std::string> lines = SplitStringLines(textToPrint);
            float lineY = startY;
            for (size_t i = 0; i < lines.size(); ++i) {
                CFont::PrintString(labelX, lineY, const_cast<char*>(lines[i].c_str()));
                if (i > 0) {
                    totalBlockHeight += Res(32.0f / GlobalScale);
                }
                lineY += Res(32.0f / GlobalScale);
            }
        }

        if (value && value[0]) {
            CFont::SetOrientation(ALIGN_RIGHT);
            CFont::SetColor(color);
            CFont::PrintString(valueX, startY, const_cast<char*>(value));
        }

        ioY += totalBlockHeight;
    }

    static void __cdecl ProgressBar(float x, float y, unsigned short width, unsigned char height, float progress,
        signed char progressAdd, unsigned char drawPercentage, unsigned char drawBlackBorder,
        CRGBA color, CRGBA addColor)
    {
        bool bFromOurPipeline = (color.a == 100 || color.a == 50);
        bool bIsHealth = (color.a == 100 || color.a == 1);

        if (!bFromOurPipeline && CUserDisplay::OnscnTimer.m_bDisplay && !FrontEndMenuManager.m_bMenuActive) {
            float dummyX, targetValueX, dummyY, dummySpacing;
            GetLayoutPositions(dummyX, targetValueX, dummyY, dummySpacing);

            x = targetValueX;

            for (int i = 0; i < 4; ++i) {
                if (CUserDisplay::OnscnTimer.m_aCounters[i].m_bEnabled && CUserDisplay::OnscnTimer.m_aCounters[i].m_nType == 1) {
                    char* translatedText = const_cast<char*>(TheText.Get(CUserDisplay::OnscnTimer.m_aCounters[i].m_szDescriptionTextKey));
                    if (IsHealthOrSweetBar(CUserDisplay::OnscnTimer.m_aCounters[i].m_szDescriptionTextKey, translatedText ? translatedText : "")) {
                        bIsHealth = true;
                    }
                }
            }
            bFromOurPipeline = true;
        }

        if ((x > (float)RsGlobal.maximumWidth * 0.3f || bFromOurPipeline) && CUserDisplay::OnscnTimer.m_bDisplay && !FrontEndMenuManager.m_bMenuActive) {

            float barMaxW = Res(110.0f / GlobalScale);
            float barH = Res(32.0f / GlobalScale);
            float thickness = Res(4.0f / GlobalScale);

            float safeProgress = std::clamp(progress, 0.0f, 100.0f);
            float progressWidth = (barMaxW * safeProgress) / 100.0f;

            CSprite2d::DrawRect(CRect(x - thickness, y - thickness, x + barMaxW + thickness, y + barH + thickness), CRGBA(0, 0, 0, 255));

            CRGBA barColor = GetCustomOrThemeColor(bIsHealth);

            CRGBA backgroundTint;
            if (barColor.r == 138 && barColor.g == 1 && barColor.b == 1) backgroundTint = CRGBA(30, 0, 0, 200);
            else if (barColor.r == 0 && barColor.g == 45 && barColor.b == 130) backgroundTint = CRGBA(0, 8, 25, 200);
            else if (barColor.r == 204 && barColor.g == 69 && barColor.b == 122) backgroundTint = CRGBA(45, 10, 22, 200);
            else if (barColor.r == 0 && barColor.g == 215 && barColor.b == 209) backgroundTint = CRGBA(0, 45, 44, 200);
            else {
                backgroundTint = CRGBA(barColor.r / 5, barColor.g / 5, barColor.b / 5, 200);
            }

            CSprite2d::DrawRect(CRect(x, y, x + barMaxW, y + barH), backgroundTint);

            if (safeProgress > 0.0f) {
                CSprite2d::DrawRect(CRect(x, y, x + progressWidth, y + barH), barColor);
            }
        }
        else {
            float fWidth = (float)width;
            float fHeight = (float)height;
            float outlineThickness = 3.0f;

            CSprite2d::DrawRect(
                CRect(x - outlineThickness, y - outlineThickness, x + fWidth + outlineThickness, y + fHeight + outlineThickness),
                CRGBA(0, 0, 0, 255)
            );

            char loadingBarAsiPath[MAX_PATH] = { 0 };
            HMODULE hLoadingBarModule = NULL;
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&LoadConfiguration, &hLoadingBarModule)) {
                GetModuleFileNameA(hLoadingBarModule, loadingBarAsiPath, MAX_PATH);
            }
            std::string loadingBarIniPath(loadingBarAsiPath);
            size_t loadingBarLastSlash = loadingBarIniPath.find_last_of("\\/");
            loadingBarIniPath = (loadingBarLastSlash != std::string::npos) ? loadingBarIniPath.substr(0, loadingBarLastSlash) + "\\LeedsHUD.ini" : ".\\LeedsHUD.ini";

            int loadingBarR = GetPrivateProfileIntA("Colors", "LoadingBarR", 255, loadingBarIniPath.c_str());
            int loadingBarG = GetPrivateProfileIntA("Colors", "LoadingBarG", 255, loadingBarIniPath.c_str());
            int loadingBarB = GetPrivateProfileIntA("Colors", "LoadingBarB", 255, loadingBarIniPath.c_str());

            CRGBA loadingBarFillColor(0, 215, 209, 255);

            if (loadingBarR != 255 || loadingBarG != 255 || loadingBarB != 255) {
                loadingBarFillColor = CRGBA(static_cast<uint8_t>(loadingBarR), static_cast<uint8_t>(loadingBarG), static_cast<uint8_t>(loadingBarB), 255);
            }
            else {
                if (currentTheme == HudTheme::LCS) {
                    loadingBarFillColor = CRGBA(100, 0, 0, 255); 
                }
                else {
                    loadingBarFillColor = CRGBA(204, 69, 122, 255);
                }
            }

            CRGBA loadingBarBackgroundTint(
                loadingBarFillColor.r / 5,
                loadingBarFillColor.g / 5,
                loadingBarFillColor.b / 5,
                255
            );

            CSprite2d::DrawRect(CRect(x, y, x + (float)width, y + (float)height), loadingBarBackgroundTint);

            float clampedProgress = std::clamp(progress, 0.0f, 100.0f);
            float fill = ((float)width * clampedProgress) / 100.0f;

            CSprite2d::DrawRect(CRect(x, y, x + fill, y + (float)height), loadingBarFillColor);
        }
    }
};

HudTheme LeedsHUDProgressBar::currentTheme = HudTheme::VCS;
LeedsHUDProgressBar LeedsStatsProgressBar;