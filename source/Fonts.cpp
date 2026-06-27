#include "plugin.h"
#include "CFont.h"
#include "CTxdStore.h"
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>

using namespace plugin;

static HMODULE g_hThisModule = NULL;
static bool    g_bLCSMode = false; 

class LeedsHUDFonts {
public:
    static bool ms_bLoaded;

    static void InitialiseTextures();
    static void LoadAndApplySizing();
    static void EnforceSizing();
    static void Shutdown();
    static void __cdecl SetFontStyle(int font);
};

bool LeedsHUDFonts::ms_bLoaded = false;
static int  s_FontSizes[2][208] = {};
static bool s_bHasSizeData = false;
static const char* s_szIniPath = "LeedsHUD.ini";

namespace LeedsLocal {
    enum eFontStyle {
        FONT_GOTHIC = 0,
        FONT_SUBTITLES = 1,
        FONT_MENU = 2,
        FONT_PRICEDOWN = 3,
    };
}

static void AbsPath(char* out, size_t outSize, const char* rel) {
    char base[MAX_PATH];
    GetModuleFileNameA(g_hThisModule, base, MAX_PATH);
    char* sep = strrchr(base, '\\');
    if (sep) *(sep + 1) = '\0';
    _snprintf_s(out, outSize, _TRUNCATE, "%s%s", base, rel);
}

static void LoadINI() {
    char iniPath[MAX_PATH];
    AbsPath(iniPath, MAX_PATH, s_szIniPath);

    std::ifstream file(iniPath);
    if (!file.is_open()) return;

    bool inSettings = false;
    std::string line;

    while (std::getline(file, line)) {
        size_t comment = line.find(';');
        if (comment != std::string::npos) line = line.substr(0, comment);
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r'))
            line.pop_back();

        if (line.empty()) continue;

        if (line.front() == '[') {
            inSettings = (line == "[Settings]");
            continue;
        }

        if (!inSettings) continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        auto trim = [](std::string& s) {
            size_t start = s.find_first_not_of(" \t");
            size_t end = s.find_last_not_of(" \t");
            s = (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
            };
        trim(key);
        trim(value);

        std::string valueLower = value;
        for (char& c : valueLower) c = (char)tolower((unsigned char)c);

        if (key == "Mode") {
            g_bLCSMode = (valueLower == "lcs");
        }
    }
    file.close();
}

void LeedsHUDFonts::EnforceSizing() {
    if (!s_bHasSizeData) return;

    for (int i = 0; i < 208; ++i) {
        if (s_FontSizes[0][i] != 0)
            gFontData[0].m_propValues[i] = s_FontSizes[0][i];
        if (s_FontSizes[1][i] != 0)
            gFontData[1].m_propValues[i] = s_FontSizes[1][i];
    }
}

void LeedsHUDFonts::InitialiseTextures() {
    char txdName[64];
    _snprintf_s(txdName, sizeof(txdName), _TRUNCATE, "LeedsHUD\\Fonts\\%sLeedsFonts.txd", g_bLCSMode ? "LCS" : "VCS");

    char txdPath[MAX_PATH];
    AbsPath(txdPath, MAX_PATH, txdName);

    int slot = CTxdStore::FindTxdSlot("leedsfonts");
    if (slot == -1) slot = CTxdStore::AddTxdSlot("leedsfonts");

    CTxdStore::LoadTxd(slot, txdPath);
    CTxdStore::AddRef(slot);

    CTxdStore::PushCurrentTxd();
    CTxdStore::SetCurrentTxd(slot);

    CFont::Sprite[0].SetTexture(const_cast<char*>("font2"));
    CFont::Sprite[1].SetTexture(const_cast<char*>("font1"));

    CTxdStore::PopCurrentTxd();
    ms_bLoaded = true;
}

void LeedsHUDFonts::LoadAndApplySizing() {
    char datName[64];
    _snprintf_s(datName, sizeof(datName), _TRUNCATE, "LeedsHUD\\Fonts\\%sLeedsFonts.dat", g_bLCSMode ? "LCS" : "VCS");

    char datPath[MAX_PATH];
    AbsPath(datPath, MAX_PATH, datName);

    std::ifstream file(datPath);
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream ss(line);
        std::string token;
        ss >> token;

        if (token == "FONT0_SIZES" || token == "FONT1_SIZES") {
            int fontIdx = (token == "FONT0_SIZES") ? 0 : 1;
            for (int i = 0; i < 208; ++i) {
                int w = 0;
                if (!(ss >> w)) break;
                s_FontSizes[fontIdx][i] = w;
            }
            s_bHasSizeData = true;
        }
        else {
            try {
                int fontIdx = std::stoi(token);
                int charIdx = 0, width = 0;
                if (ss >> charIdx >> width) {
                    if (fontIdx >= 0 && fontIdx < 2 && charIdx >= 0 && charIdx < 208) {
                        s_FontSizes[fontIdx][charIdx] = width;
                        s_bHasSizeData = true;
                    }
                }
            }
            catch (...) {}
        }
    }
    file.close();
    EnforceSizing();
}

void LeedsHUDFonts::Shutdown() {
    if (!ms_bLoaded) return;
    for (int i = 0; i < 2; ++i) CFont::Sprite[i].Delete();
    int slot = CTxdStore::FindTxdSlot("leedsfonts");
    if (slot != -1) CTxdStore::RemoveTxdSlot(slot);
    ms_bLoaded = false;
}

void __cdecl LeedsHUDFonts::SetFontStyle(int font) {

    if (g_bLCSMode && font == LeedsLocal::FONT_GOTHIC)
        font = LeedsLocal::FONT_MENU;

    else if (!g_bLCSMode && font == LeedsLocal::FONT_MENU)
        font = LeedsLocal::FONT_PRICEDOWN;

    switch (font) {
    case LeedsLocal::FONT_MENU:
        CFont::m_FontTextureId = 0;
        CFont::m_FontStyle = 2;
        break;
    case LeedsLocal::FONT_PRICEDOWN:
        CFont::m_FontTextureId = 1;
        CFont::m_FontStyle = 1;
        break;
    default:
        CFont::m_FontTextureId = font;
        CFont::m_FontStyle = 0;
        break;
    }
}

struct LeedsHUDFontsPlugin {
    LeedsHUDFontsPlugin() {
        patch::RedirectJump(0x719490, LeedsHUDFonts::SetFontStyle);

        Events::initRwEvent += [] {
            LoadINI();
            LeedsHUDFonts::InitialiseTextures();
            LeedsHUDFonts::LoadAndApplySizing();
            };

        Events::drawingEvent += [] {
            if (LeedsHUDFonts::ms_bLoaded) LeedsHUDFonts::EnforceSizing();
            };

        Events::shutdownRwEvent += [] {
            LeedsHUDFonts::Shutdown();
            };
    }
} g_LeedsHUDPlugin;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) g_hThisModule = hModule;
    return TRUE;
}