#include "plugin.h"
#include "CHud.h"
#include "CFont.h"
#include "CCamera.h"
#include "Events.h"
#include "CTimer.h"
#include "CSprite2d.h"
#include "CCutsceneMgr.h" 

using namespace plugin;

#define SCREEN_WIDTH ((float)RsGlobal.maximumWidth)
#define SCREEN_HEIGHT ((float)RsGlobal.maximumHeight)
#define SCREEN_BOTTOM(y) (SCREEN_HEIGHT - (y))

class LeedsHUDBigMessage {
public:
    static inline bool bIsFailedShowing;
};

class LeedsHUDSubtitles {
public:
    static inline unsigned int redTextCooldownTimer = 0;

    static void DrawSubtitles() {
        if (!CHud::m_Message || CHud::m_Message[0] == '\0') {
            return;
        }

        unsigned int currentTime = CTimer::m_snTimeInMilliseconds;

        if (LeedsHUDBigMessage::bIsFailedShowing) {
            redTextCooldownTimer = currentTime + 10000;
        }

        if (currentTime < redTextCooldownTimer) {
            if (strstr(CHud::m_Message, "~r~") || strstr(CHud::m_Message, "~R~")) {
                return;
            }
        }

        CFont::SetProportional(true);
        CFont::SetOrientation(ALIGN_CENTER);
        CFont::SetFontStyle(FONT_SUBTITLES);

        float resScale = SCREEN_HEIGHT / 1080.0f;
        float scaleMultiplier = 2.2f * resScale;
        float scaleW = 0.5f * scaleMultiplier;
        float scaleH = 1.0f * scaleMultiplier;
        CFont::SetScale(scaleW, scaleH);

        float wrapFactor = CCutsceneMgr::ms_running ? 0.90f : 0.63f;
        float wrapWidth = SCREEN_WIDTH * wrapFactor;

        CFont::SetWrapx(wrapWidth);
        CFont::SetCentreSize(wrapWidth);
        CFont::SetBackground(false, false);
        CFont::SetDropShadowPosition(0);

        float fPosX = SCREEN_WIDTH * 0.50f;
        float basePosY = SCREEN_HEIGHT * 0.210f; 
        float fPosY = basePosY;

        short totalLines = CFont::GetNumberLines(fPosX, SCREEN_BOTTOM(basePosY), CHud::m_Message);

        if (totalLines > 3) {
            float singleLineHeightHeight = 32.0f * resScale;
            int extraLines = totalLines - 3;
            fPosY += (extraLines * singleLineHeightHeight);
        }

        CFont::SetEdge(1);
        CFont::SetDropColor(CRGBA(0, 0, 0, 255));
        CFont::SetColor(CRGBA(0, 0, 0, 0));
        CFont::SetAlphaFade(0.0f);
        float shadowOffset = 2.5f * resScale;
        CFont::PrintString(fPosX + shadowOffset, SCREEN_BOTTOM(fPosY) + shadowOffset, CHud::m_Message);
        CFont::SetAlphaFade(255.0f);
        CFont::SetEdge(1);
        CFont::SetDropColor(CRGBA(0, 0, 0, 255));
        CFont::SetColor(CRGBA(255, 255, 255, 255));
        CFont::PrintString(fPosX, SCREEN_BOTTOM(fPosY), CHud::m_Message);
    }

    LeedsHUDSubtitles() {
        patch::SetUChar(0x58C250, 0xC3);
        Events::drawHudEvent += [] { DrawSubtitles(); };
    }
} LeedsSubtitles;