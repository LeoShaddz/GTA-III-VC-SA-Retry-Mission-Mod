// RetryMissionPauseVC.cpp - GTA Vice City 1.0 (PC, gta-vc.exe)
//
// When the player steps on the marker and the mission starts, it stores the position, weapons (with the exact ammo),
// health, armour, money, wanted level and in-game time of that moment.
// If the mission fails (by dying, being arrested or by its objectives), after respawning the game PAUSES (the game's own
// pause screen) and shows in the middle of the screen:
//
//        Retry?
//        Yes
//        No
//
//   keyboard: Up/Down arrows + Enter      mouse: hover the cursor and click
//   gamepad (XInput or GInput): dpad or left stick Up/Down + Cross (A)
//   Yes = unpauses, teleports to the marker and restores everything (including time and weather, instantly); No / Esc / gamepad
//   Y = unpauses and cancels.
// It also stores Tommy's outfit (the current model/outfit) and gives it back when teleporting, if he changed clothes during the mission.
// The screen is the game's OWN pause screen (background, logo, title and whatever CLEO writes): the mod only hides the menu
// options and writes "Retry? / Yes / No" in their place.
// The mouse cursor is the game's original icon and disappears when you use the gamepad or the keyboard arrows.
// The sounds are those of the game's menu. What you press in the menu is only released to gameplay after you let go.
//
// Configuration in RetryMissionPauseVC.ini (reloaded when saved).
// Compile as Win32 (x86). The output must be named RetryMissionPauseVC.asi
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cctype>

// ======================= Addresses (GTA VC 1.0) =======================
static const uintptr_t VERSION_ADDR = 0x667BF0;      // start of the 1.0 exe entry point
static const uint32_t  VERSION_OK   = 0x53E58955;

// Player
static const uintptr_t PLAYERS_BASE   = 0x94AD28;    // CWorld::Players[], stride 0x170 (ped at offset 0)
static const uintptr_t PLAYER_FOCUS   = 0xA10AFB;    // CWorld::PlayerInFocus (byte)
static const uintptr_t PLAYER_STRIDE  = 0x170;
static const uintptr_t PLAYER_MONEY_OFF   = 0xA0;    // CPlayerInfo::m_nMoney (0x94ADC8)
static const uintptr_t PLAYER_DISPLAY_OFF = 0xA4;    // CPlayerInfo::m_nDisplayMoney
static const uintptr_t PED_POS_OFF     = 0x34;       // position CVector
static const uintptr_t PED_SPEED_OFF   = 0x70;       // velocity CVector
static const uintptr_t PED_HEALTH_OFF  = 0x354;      // float
static const uintptr_t PED_ARMOUR_OFF  = 0x358;      // float
static const uintptr_t PED_WEAPONS_OFF = 0x408;      // CWeapon[10], 0x18 each: type +0, state +4, clip +8, total +0xC
static const uintptr_t PED_ACTIVE_SLOT = 0x504;      // slot of the weapon in hand (byte)
static const uintptr_t PED_WANTED_OFF  = 0x5F4;      // CWanted*
static const int       WEAPON_SLOTS    = 10;

// Wanted (CWanted)
static const uintptr_t WANTED_CHAOS_OFF = 0x00;
static const uintptr_t WANTED_LEVEL_OFF = 0x20;
static const uintptr_t FN_WANTED_SET    = 0x4D1FA0;  // CWanted::SetWantedLevel(int) (thiscall)

// Ped functions
static const uintptr_t FN_PED_GIVE_WEAPON   = 0x4FFA30;  // CPed::GiveWeapon(type, ammo, 1) (thiscall, 3 arguments)
static const uintptr_t FN_PED_SET_CUR_WEAPON = 0x4FF900; // CPed::SetCurrentWeapon(slot) (thiscall)
static const int       PED_VTBL_TELEPORT     = 0x2C;     // virtual method: Teleport(CVector) (thiscall, callee cleans up)
// Loads the scenery around the new position: the same pair of calls the game uses when respawning the player
static const uintptr_t FN_LOAD_SCENE_COLLISION = 0x40AF30; // (CVector*) cdecl
static const uintptr_t FN_LOAD_SCENE           = 0x40AF60; // (CVector*) cdecl
// Tommy's outfit = the player's model name (player, player2...). The game reapplies the model with this function
// (for example, goes back to "player" when respawning after a failure).
static const uintptr_t PED_MODEL_INDEX_OFF   = 0x5C;       // int16
static const uintptr_t MODELINFO_PTRS        = 0x92D4C8;   // CModelInfo::ms_modelInfoPtrs[]
static const uintptr_t MODELINFO_NAME_OFF    = 0x04;       // model name (char[24])
static const uintptr_t FN_LOAD_ALL_MODELS    = 0x40B5F0;   // CStreaming::LoadAllRequestedModels(bool) cdecl
static const uintptr_t FN_PED_REAPPLY_MODEL  = 0x4EEFD0;   // thiscall, no arguments: recreates the clump with the requested model (the game calls it right after FN_PED_SET_MODEL_NAME)
static const uintptr_t FN_PED_SET_MODEL_NAME = 0x4EF030;   // CPlayerPed: applies a model by name (thiscall, 1 argument)

// Game state
static const uintptr_t ADDR_TIMER_MS      = 0x974B2C;    // CTimer::m_snTimeInMilliseconds
static const uintptr_t ADDR_MISSION_OFF   = 0x978748;    // offset of the mission flag in ScriptSpace
static const uintptr_t ADDR_SCRIPT_SPACE  = 0x821280;
static const uintptr_t ADDR_CLOCK_HOURS   = 0xA10B6B;    // byte
static const uintptr_t ADDR_CLOCK_MINUTES = 0xA10B92;    // byte
static const uintptr_t FN_SET_GAME_CLOCK  = 0x487160;    // CClock::SetGameClock(hours, minutes) (cdecl)
// Weather: the same variables that the game's own "save/restore state" routine copies
static const uintptr_t ADDR_WEATHER_A      = 0xA10AAA;   // weather type (WORD)
static const uintptr_t ADDR_WEATHER_B      = 0xA10A2E;   // weather type (WORD)
static const uintptr_t ADDR_WEATHER_INTERP = 0x9787D8;   // interpolation between the two weathers (float)

// Messages
static const uintptr_t FN_ADD_BIG_MESSAGE = 0x584050;    // CMessages::AddBigMessage(wchar*, time, style) (cdecl)
static const uintptr_t FN_TEXT_GET        = 0x584F30;    // CText::Get(const char*) (thiscall)
static const uintptr_t ADDR_THETEXT       = 0x94B220;

// Hook points (calls)
static const uintptr_t CALL_GAMELOGIC_UPDATE = 0x4A461B; static const uintptr_t FN_GAMELOGIC_UPDATE = 0x42BB70;
// The pause menu is drawn/processed through TWO paths in VC: the frontend loop (main menu) and the game frame
// (pause during gameplay). Both are hooked.
static const uintptr_t FN_DRAW_FRONTEND   = 0x4A37A4;
static const uintptr_t CALLS_DRAW_FRONTEND[2] = { 0x4A5D33, 0x4A612E };
static const uintptr_t FN_MENU_PROCESS    = 0x49A01C;
static const uintptr_t CALLS_MENU_PROCESS[2]  = { 0x4A5C88, 0x4A4433 };

// Pause menu (FrontendMenuManager)
static const uintptr_t MENU_OBJ          = 0x869630;
static const uintptr_t MENU_START_REQ    = MENU_OBJ + 0x12;   // request to open the menu
static const uintptr_t MENU_SHUT_REQ     = MENU_OBJ + 0x11;   // request to close the menu
static const uintptr_t ADDR_MENU_ACTIVE  = MENU_OBJ + 0x38;   // m_bMenuActive
static const uintptr_t ADDR_MENU_MOUSE   = MENU_OBJ + 0x140;  // 1 = the menu draws the cursor
static const uintptr_t MENU_MOUSE_X_OFF  = 0x12C;
static const uintptr_t MENU_MOUSE_Y_OFF  = 0x130;
static const uintptr_t MENU_SPRITE_MOUSE = MENU_OBJ + 0x8C;   // cursor CSprite2d ("mouse"/"mousea")
static const uintptr_t FN_MENU_BUSY      = 0x46F737;          // thiscall at 0x7E4688
static const uintptr_t OBJ_MENU_BUSY     = 0x7E4688;
static const uintptr_t FN_MENU_UPDATE    = 0x4A3882;          // thiscall(menu)
static const uintptr_t FN_MENU_CHECK     = 0x498E5F;          // thiscall(menu): opens/closes according to the requests
static const uintptr_t FN_AUDIO_SERVICE  = 0x5F9E50;          // thiscall on DMAudio
static const uintptr_t FN_AUDIO_A        = 0x6272E0;          // cdecl(1)
static const uintptr_t FN_AUDIO_B        = 0x627590;          // cdecl(1)
static const uintptr_t ADDR_DMAUDIO      = 0xA10B8A;
static const uintptr_t FN_PLAY_FRONTEND  = 0x5F9960;          // DMAudio.PlayFrontEndSound(sound, volume) (thiscall)

// Drawing
static const uintptr_t FN_SPRITE_DRAW    = 0x578710;          // CSprite2d::Draw(CRect*, CRGBA*) (thiscall; null texture = flat color)
static const uintptr_t FN_RENDER_FONT_BUFFER = 0x551A30;
// Green highlight of the selected menu option (CSprite2d::Draw2DPolygon, 8 floats + CRGBA*, cdecl). The game draws it at
// its own option's position; during the prompt the mod replaces it with a rectangle behind the selected Yes/No.
static const uintptr_t FN_DRAW_POLYGON   = 0x578520;
static const uintptr_t MENU_HIGHLIGHT_SITES[] = { 0x49F4D6, 0x49FB7E };
// Calls (CSprite2d::Draw) the game uses to draw the menu cursor: [0] = shadow, [1] = cursor
static const uintptr_t CURSOR_SITES[2] = { 0x4A36B6, 0x4A3793 };
static const uintptr_t FN_FONT_PRINT     = 0x551040;          // CFont::PrintString(float x, float y, wchar_t* text)
// Points in the menu code that write the options' text. The two that write the screen TITLE are left out
// (0x49E30E shadow, 0x49E3D9 text), which stay as in the game.
static const uintptr_t MENU_PRINT_SITES[] = { 0x498933, 0x49A59F, 0x49B66C, 0x49C0F3, 0x49C135, 0x49C28D,
                                              0x49C40B, 0x49C488, 0x49E656, 0x49FCAD, 0x49FE4E };
static const uintptr_t FN_FONT_SET_SCALE = 0x550230;          // (float x, float y)
static const uintptr_t FN_FONT_SET_COLOR = 0x550170;          // (CRGBA*)
static const uintptr_t FN_FONT_SET_STYLE = 0x54FF20;          // (int16)
static const uintptr_t FN_FONT_SET_DROPCOLOR = 0x54FF30;      // (CRGBA*)
static const uintptr_t FN_FONT_SET_SHADOW = 0x54FFE0;         // (int16) shadow offset
static const uintptr_t FN_FONT_SET_WRAPX = 0x550100;          // (float)
static const uintptr_t FN_FONT_SET_CENTRESIZE = 0x5500F0;     // (float)
static const uintptr_t FN_FONT_SET_RJWRAP = 0x54FFD0;         // (float)
static const uintptr_t FN_FONT_SET_PROP_ON = 0x550020;
static const uintptr_t FN_FONT_SET_CENTRE_ON = 0x550120;      // centered (and turns off justified/right)
static const uintptr_t FN_FONT_SET_BG_OFF = 0x5500D0;
static const uintptr_t FN_FONT_SET_BGTEXT_OFF = 0x550080;

static const int* const pScreenW = (const int*)0x9B48E4;
static const int* const pScreenH = (const int*)0x9B48E8;

// Gamepad (CPad 0)
static const uintptr_t PAD0_ADDR = 0x7DBCB0;

// ======================= Types =======================
typedef void (__cdecl *VoidFn_t)();
struct Rgba8 { uint8_t r, g, b, a; };
struct RectF { float left, bottom, right, top; };

// ======================= Configuration =======================
struct PadButton { const char* name; int offset; uint16_t xmask; int xtrig; };
static const PadButton PAD_BUTTONS[] = {
    { "DPadUp", 0x10, 0x0001, 0 }, { "DPadDown", 0x12, 0x0002, 0 }, { "DPadLeft", 0x14, 0x0004, 0 }, { "DPadRight", 0x16, 0x0008, 0 },
    { "L1", 0x08, 0x0100, 0 }, { "L2", 0x0A, 0, 1 }, { "R1", 0x0C, 0x0200, 0 }, { "R2", 0x0E, 0, 2 },
    { "Start", 0x18, 0x0010, 0 }, { "Select", 0x1A, 0x0020, 0 },
    { "Square", 0x1C, 0x4000, 0 }, { "Triangle", 0x1E, 0x8000, 0 }, { "Cross", 0x20, 0x1000, 0 }, { "Circle", 0x22, 0x2000, 0 },
    { "L3", 0x24, 0x0040, 0 }, { "R3", 0x26, 0x0080, 0 },
};

static bool  g_enabled = true;
static char  g_message[256] = "Retry?";
static char  g_yesText[64] = "Yes";
static char  g_noText[64]  = "No";
static uint32_t g_titleColor = 0xFFE196FFu;   // ABGR: r=FF g=96 b=E1 (VC menu pink)
static uint32_t g_selColor   = 0xFFF0C8FFu;
static uint32_t g_normColor  = 0xFFA064B4u;
static int   g_fontStyle  = 2;
static float g_msgY = 47.0f, g_msgOffX = 0.0f, g_msgOffY = 0.0f, g_scaleMul = 1.0f, g_lineSpacing = 1.0f;
static int   g_sCodeSmall = 0x9A, g_sCodeCap = 0x8A;       // game codes for s and S with caron
static bool  g_ownCursor = true, g_hideCursorWithPad = true, g_cancelOnEsc = true;
static bool  g_showHighlight = true;
static uint32_t g_highlightColor = 0xFF468219u;      // ABGR: r=0x19 g=0x82 b=0x46 (the VC menu green)
static float g_highlightWidth = 0.34f, g_highlightHeight = 1.0f, g_highlightOffY = 0.0f;
static float g_cursorSize = 1.0f;
static int   g_sndNavigate = 0xC3, g_sndConfirm = 0xC4, g_sndBack = 0xC5;
static int   g_backOffset = 0x1E; static uint16_t g_backXMask = 0x8000; static int g_backXTrig = 0;
static int   g_padSource = 0;                 // 0 = Auto, 1 = XInput, 2 = Game (CPad)
static bool  g_restoreWeapons = true, g_restoreHealth = true, g_restoreArmour = true, g_restoreMoney = true,
             g_restoreWanted = true, g_restoreTime = true, g_restoreWeather = true, g_restoreOutfit = true;
static float g_promptDelaySec = 3.0f;   // seconds between the mission failed message and the Retry? menu

static char     g_iniPath[MAX_PATH];
static FILETIME g_lastWrite = {0, 0};
static DWORD    g_lastCheck = 0;

static bool g_logEnabled = false;      // EnableLog=1 in the .ini turns on the log file

static void Log(const char* fmt, ...)
{
    if (!g_logEnabled) return;
    FILE* f = fopen("RetryMissionPauseVC.log", "a");
    if (!f) return;
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f); fclose(f);
}

static float ReadFloat(const char* key, float def)
{
    char buf[64];
    GetPrivateProfileStringA("Retry", key, "", buf, sizeof(buf), g_iniPath);
    if (!buf[0]) return def;
    for (char* p = buf; *p; ++p) if (*p == ',') *p = '.';
    return (float)atof(buf);
}

static uint32_t ReadColor(const char* key, const char* def)
{
    char buf[32];
    GetPrivateProfileStringA("Retry", key, def, buf, sizeof(buf), g_iniPath);
    unsigned long rgb = strtoul(buf, nullptr, 16);
    return 0xFF000000u | ((rgb & 0xFF) << 16) | (rgb & 0xFF00) | ((rgb >> 16) & 0xFF);   // RRGGBB -> ABGR
}

static void LoadConfig()
{
    char buf[256];
    g_enabled = GetPrivateProfileIntA("Retry", "Enabled", 1, g_iniPath) != 0;
    g_logEnabled = GetPrivateProfileIntA("Retry", "EnableLog", 0, g_iniPath) != 0;
    GetPrivateProfileStringA("Retry", "Message", "Retry?", g_message, sizeof(g_message), g_iniPath);
    GetPrivateProfileStringA("Retry", "YesText", "Yes", g_yesText, sizeof(g_yesText), g_iniPath);
    GetPrivateProfileStringA("Retry", "NoText", "No", g_noText, sizeof(g_noText), g_iniPath);
    g_titleColor = ReadColor("TitleColor", "FF96E1");
    g_selColor   = ReadColor("SelectedColor", "FFC8F0");
    g_normColor  = ReadColor("NormalColor", "B464A0");
    g_fontStyle  = GetPrivateProfileIntA("Retry", "FontStyle", 2, g_iniPath);
    g_msgY       = ReadFloat("MessageY", 47.0f);
    g_msgOffX    = ReadFloat("MessageOffsetX", 0.0f);
    g_msgOffY    = ReadFloat("MessageOffsetY", 0.0f);
    g_scaleMul   = ReadFloat("TextScale", 1.0f);  if (g_scaleMul <= 0.0f) g_scaleMul = 1.0f;
    g_lineSpacing = ReadFloat("LineSpacing", 1.0f); if (g_lineSpacing <= 0.0f) g_lineSpacing = 1.0f;
    { char b2[32];
      GetPrivateProfileStringA("Retry", "SmallSCode", "0x9A", b2, sizeof(b2), g_iniPath);  g_sCodeSmall = (int)strtol(b2, nullptr, 0);
      GetPrivateProfileStringA("Retry", "CapitalSCode", "0x8A", b2, sizeof(b2), g_iniPath); g_sCodeCap = (int)strtol(b2, nullptr, 0); }
    g_ownCursor = GetPrivateProfileIntA("Retry", "ShowMouseCursor", 1, g_iniPath) != 0;
    g_hideCursorWithPad = GetPrivateProfileIntA("Retry", "HideCursorWithController", 1, g_iniPath) != 0;
    g_cursorSize = ReadFloat("CursorSize", 1.0f); if (g_cursorSize <= 0.0f) g_cursorSize = 1.0f;
    g_cancelOnEsc = GetPrivateProfileIntA("Retry", "CancelOnEscape", 1, g_iniPath) != 0;
    g_showHighlight   = GetPrivateProfileIntA("Retry", "ShowHighlight", 1, g_iniPath) != 0;
    g_highlightColor  = ReadColor("HighlightColor", "198246");
    g_highlightWidth  = ReadFloat("HighlightWidth", 0.34f);   if (g_highlightWidth <= 0.0f) g_highlightWidth = 0.34f;
    g_highlightHeight = ReadFloat("HighlightHeight", 1.0f);   if (g_highlightHeight <= 0.0f) g_highlightHeight = 1.0f;
    g_highlightOffY   = ReadFloat("HighlightOffsetY", 0.0f);
    g_sndNavigate = GetPrivateProfileIntA("Retry", "NavigateSound", 0xC3, g_iniPath);
    g_sndConfirm  = GetPrivateProfileIntA("Retry", "ConfirmSound", 0xC4, g_iniPath);
    g_sndBack     = GetPrivateProfileIntA("Retry", "BackSound", 0xC5, g_iniPath);

    GetPrivateProfileStringA("Retry", "BackPadButton", "Triangle", buf, sizeof(buf), g_iniPath);
    g_backOffset = -1; g_backXMask = 0; g_backXTrig = 0;
    if (buf[0] && _stricmp(buf, "none") && _stricmp(buf, "off"))
        for (const PadButton& b : PAD_BUTTONS)
            if (!_stricmp(buf, b.name)) { g_backOffset = b.offset; g_backXMask = b.xmask; g_backXTrig = b.xtrig; }

    GetPrivateProfileStringA("Retry", "PadSource", "Auto", buf, sizeof(buf), g_iniPath);
    g_padSource = !_stricmp(buf, "XInput") ? 1 : (!_stricmp(buf, "Game") ? 2 : 0);

    g_restoreWeapons = GetPrivateProfileIntA("Retry", "RestoreWeapons", 1, g_iniPath) != 0;
    g_restoreHealth  = GetPrivateProfileIntA("Retry", "RestoreHealth", 1, g_iniPath) != 0;
    g_restoreArmour  = GetPrivateProfileIntA("Retry", "RestoreArmour", 1, g_iniPath) != 0;
    g_restoreMoney   = GetPrivateProfileIntA("Retry", "RestoreMoney", 1, g_iniPath) != 0;
    g_restoreWanted  = GetPrivateProfileIntA("Retry", "RestoreWanted", 1, g_iniPath) != 0;
    g_restoreTime    = GetPrivateProfileIntA("Retry", "RestoreTime", 1, g_iniPath) != 0;
    g_restoreWeather = GetPrivateProfileIntA("Retry", "RestoreWeather", 1, g_iniPath) != 0;
    g_restoreOutfit  = GetPrivateProfileIntA("Retry", "RestoreOutfit", 1, g_iniPath) != 0;
    g_promptDelaySec = ReadFloat("PromptDelaySeconds", 3.0f); if (g_promptDelaySec < 0.0f) g_promptDelaySec = 0.0f;
}

static void CheckReload()
{
    DWORD now = GetTickCount();
    if (now - g_lastCheck < 500) return;
    g_lastCheck = now;
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExA(g_iniPath, GetFileExInfoStandard, &fad)) return;
    if (CompareFileTime(&fad.ftLastWriteTime, &g_lastWrite) != 0) { g_lastWrite = fad.ftLastWriteTime; LoadConfig(); }
}

// ======================= Input: keyboard, mouse and gamepad =======================
static bool GameIsForeground()
{
    HWND fg = GetForegroundWindow();
    DWORD pid = 0; if (fg) GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

static bool KeyPressed(int vk)
{
    if (!vk || !GameIsForeground()) return false;
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

static bool MouseButtonsDown()
{
    return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) || (GetAsyncKeyState(VK_RBUTTON) & 0x8000) || (GetAsyncKeyState(VK_MBUTTON) & 0x8000);
}

static bool AnyKeyboardKeyDown()
{
    for (int vk = 8; vk < 255; ++vk)
    {
        if (vk >= 0xC3 && vk <= 0xDA) continue;     // VK_GAMEPAD_*
        if (GetAsyncKeyState(vk) & 0x8000) return true;
    }
    return false;
}

// ---- XInput (loaded dynamically) ----
struct XGamepad { uint16_t wButtons; uint8_t bLT, bRT; int16_t lx, ly, rx, ry; };
struct XState   { DWORD packet; XGamepad pad; };
typedef DWORD (WINAPI *XInputGetState_t)(DWORD, XState*);
static XInputGetState_t g_xGet = nullptr;
static bool  g_xTried = false;
static int   g_xIndex = -1;
static DWORD g_xNextProbe = 0;

static bool ReadXInput(XGamepad& out)
{
    if (!g_xTried)
    {
        g_xTried = true;
        const char* dlls[] = { "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll" };
        for (const char* d : dlls)
        {
            HMODULE m = LoadLibraryA(d);
            if (m) { g_xGet = (XInputGetState_t)GetProcAddress(m, "XInputGetState"); if (g_xGet) break; }
        }
    }
    if (!g_xGet) return false;
    XState st;
    if (g_xIndex < 0)
    {
        DWORD now = GetTickCount();
        if (now < g_xNextProbe) return false;
        for (int i = 0; i < 4; ++i)
            if (g_xGet((DWORD)i, &st) == 0) { g_xIndex = i; break; }
        if (g_xIndex < 0) { g_xNextProbe = now + 2000; return false; }
    }
    if (g_xGet((DWORD)g_xIndex, &st) != 0) { g_xIndex = -1; g_xNextProbe = GetTickCount() + 2000; return false; }
    out = st.pad;
    return true;
}

static DWORD g_lastKeyTick = 0;
static const DWORD KEY_GRACE_MS = 600;

// true while the button (and ONLY it) is pressed
static bool PadDown(int padOffset, uint16_t xmask, int xtrig)
{
    if (padOffset < 0) return false;
    if (g_padSource != 2)
    {
        XGamepad xg;
        if (ReadXInput(xg))
        {
            if (!GameIsForeground()) return false;
            if (xtrig == 1) return xg.bLT > 100;
            if (xtrig == 2) return xg.bRT > 100;
            return xmask && (xg.wButtons & xmask) != 0;
        }
        if (g_padSource == 1) return false;
    }
    // game pad (GInput injects the state here). The game also translates keys into the pad and the pad sees the key
    // released one frame later: so it ignores the pad for KEY_GRACE_MS after any key.
    const DWORD tnow = GetTickCount();
    if (AnyKeyboardKeyDown()) g_lastKeyTick = tnow;
    if (*(int16_t*)(PAD0_ADDR + padOffset) == 0) return false;
    if (tnow - g_lastKeyTick < KEY_GRACE_MS) return false;
    for (const PadButton& b : PAD_BUTTONS)
    {
        if (b.offset == padOffset) continue;
        if (b.offset >= 0x10 && *(int16_t*)(PAD0_ADDR + b.offset) != 0) return false;
    }
    return true;
}

// LEFT stick vertical: -1 = up, +1 = down, 0 = neutral
static int LeftStickVertical()
{
    if (g_padSource != 2)
    {
        XGamepad xg;
        if (ReadXInput(xg))
        {
            if (!GameIsForeground()) return 0;
            if (xg.ly >  16000) return -1;
            if (xg.ly < -16000) return  1;
            return 0;
        }
        if (g_padSource == 1) return 0;
    }
    const DWORD tnow = GetTickCount();
    if (AnyKeyboardKeyDown()) g_lastKeyTick = tnow;
    if (tnow - g_lastKeyTick < KEY_GRACE_MS) return 0;
    const int v = *(int16_t*)(PAD0_ADDR + 0x02);
    if (v < -64) return -1;
    if (v >  64) return  1;
    return 0;
}

// Like GInput: the cursor appears with the mouse and disappears with the gamepad (and, in the menu, with the keyboard arrows)
static bool g_usingPad = false;
static long g_trkMouseX = -100000, g_trkMouseY = -100000;

static void TrackInputDevice(bool inMenu)
{
    if (!GameIsForeground()) return;
    bool padAct = false;
    XGamepad xg;
    if (ReadXInput(xg))
        padAct = xg.wButtons != 0 || xg.bLT > 40 || xg.bRT > 40 ||
                 abs(xg.lx) > 14000 || abs(xg.ly) > 14000 || abs(xg.rx) > 14000 || abs(xg.ry) > 14000;
    else if (g_padSource != 1)
    {
        const DWORD tnow = GetTickCount();
        if (AnyKeyboardKeyDown()) g_lastKeyTick = tnow;
        if (tnow - g_lastKeyTick >= KEY_GRACE_MS)
            for (const PadButton& b : PAD_BUTTONS)
                if (b.offset >= 0x08 && *(int16_t*)(PAD0_ADDR + b.offset) != 0) { padAct = true; break; }
    }
    bool mouseAct = MouseButtonsDown();
    POINT pt;
    if (GetCursorPos(&pt))
    {
        if (g_trkMouseX != -100000 && (labs(pt.x - g_trkMouseX) + labs(pt.y - g_trkMouseY)) > 2) mouseAct = true;
        g_trkMouseX = pt.x; g_trkMouseY = pt.y;
    }
    const bool arrowAct = inMenu && ((GetAsyncKeyState(VK_UP) & 0x8000) || (GetAsyncKeyState(VK_DOWN) & 0x8000) ||
                                     (GetAsyncKeyState(VK_LEFT) & 0x8000) || (GetAsyncKeyState(VK_RIGHT) & 0x8000));
    if (mouseAct)                g_usingPad = false;
    else if (padAct || arrowAct) g_usingPad = true;
}

static bool GetMouseGame(float& mx, float& my)
{
    HWND fg = GetForegroundWindow();
    DWORD pid = 0; if (fg) GetWindowThreadProcessId(fg, &pid);
    if (!fg || pid != GetCurrentProcessId()) return false;
    POINT p; RECT r;
    if (!GetCursorPos(&p) || !ScreenToClient(fg, &p) || !GetClientRect(fg, &r) || r.right <= 0 || r.bottom <= 0) return false;
    mx = (float)p.x * (float)*pScreenW / (float)r.right;
    my = (float)p.y * (float)*pScreenH / (float)r.bottom;
    return true;
}

// ======================= Game access =======================
static inline uint8_t* PlayerInfo()
{
    return (uint8_t*)(PLAYERS_BASE + (uintptr_t)*(uint8_t*)PLAYER_FOCUS * PLAYER_STRIDE);
}
static inline uint8_t* PlayerPed() { return *(uint8_t**)PlayerInfo(); }

static bool IsOnMission()
{
    const uint32_t off = *(uint32_t*)ADDR_MISSION_OFF;
    return off != 0 && *(int32_t*)(ADDR_SCRIPT_SPACE + off) == 1;
}

static void PlayMenuSound(int id)
{
    ((void (__thiscall*)(void*, uint32_t, uint32_t))FN_PLAY_FRONTEND)((void*)ADDR_DMAUDIO, (uint32_t)id, 0);
}

// ======================= Game state snapshot =======================
struct WeaponSnap { int32_t type, ammoInClip, totalAmmo; };
struct StateSnap
{
    bool       valid;
    float      pos[3];
    float      health, armour;
    int32_t    money;
    WeaponSnap w[WEAPON_SLOTS];
    int8_t     activeSlot;
    int32_t    wantedChaos, wantedLevel;
    uint8_t    hours, minutes;
    int16_t    weatherA, weatherB;
    float      weatherInterp;
    bool       hasOutfit;
    char       outfit[32];
};
static StateSnap g_rolling = {};   // continuous snapshot while there is NO mission
static StateSnap g_snap    = {};   // snapshot from the moment the mission started

static uint8_t* PedWanted(uint8_t* ped) { return *(uint8_t**)(ped + PED_WANTED_OFF); }

static void TakeSnapshot(uint8_t* ped, StateSnap& o)
{
    const float* p = (const float*)(ped + PED_POS_OFF);
    o.pos[0] = p[0]; o.pos[1] = p[1]; o.pos[2] = p[2];
    o.health = *(float*)(ped + PED_HEALTH_OFF);
    o.armour = *(float*)(ped + PED_ARMOUR_OFF);
    o.money  = *(int32_t*)(PlayerInfo() + PLAYER_MONEY_OFF);
    for (int i = 0; i < WEAPON_SLOTS; ++i)
    {
        const int32_t* w = (const int32_t*)(ped + PED_WEAPONS_OFF + i * 0x18);
        o.w[i].type = w[0]; o.w[i].ammoInClip = w[2]; o.w[i].totalAmmo = w[3];
    }
    o.activeSlot = *(int8_t*)(ped + PED_ACTIVE_SLOT);
    uint8_t* wanted = PedWanted(ped);
    o.wantedChaos = wanted ? *(int32_t*)(wanted + WANTED_CHAOS_OFF) : 0;
    o.wantedLevel = wanted ? *(int32_t*)(wanted + WANTED_LEVEL_OFF) : 0;
    o.hours   = *(uint8_t*)ADDR_CLOCK_HOURS;
    o.minutes = *(uint8_t*)ADDR_CLOCK_MINUTES;
    o.weatherA = *(int16_t*)ADDR_WEATHER_A;
    o.weatherB = *(int16_t*)ADDR_WEATHER_B;
    o.weatherInterp = *(float*)ADDR_WEATHER_INTERP;
    o.hasOutfit = false; o.outfit[0] = 0;
    {
        const int16_t mdl = *(int16_t*)(ped + PED_MODEL_INDEX_OFF);
        if (mdl >= 0 && mdl < 6500)
        {
            const uint8_t* mi = *(const uint8_t**)(MODELINFO_PTRS + (uintptr_t)mdl * 4);
            if (mi) { strncpy(o.outfit, (const char*)(mi + MODELINFO_NAME_OFF), 23); o.outfit[23] = 0; o.hasOutfit = o.outfit[0] != 0; }
        }
    }
    o.valid = true;
}

static void LogSnapshot(const char* title, const StateSnap& o)
{
    Log("%s: vida=%.1f colete=%.1f dinheiro=%d procurado=%d (caos %d) hora=%02d:%02d clima=%d/%d(%.2f) slot=%d roupa=%s",
        title, o.health, o.armour, o.money, o.wantedLevel, o.wantedChaos, o.hours, o.minutes,
        (int)o.weatherA, (int)o.weatherB, o.weatherInterp, (int)o.activeSlot, o.hasOutfit ? o.outfit : "?");
    for (int i = 0; i < WEAPON_SLOTS; ++i)
        if (o.w[i].type > 0) Log("   slot %d: arma %d, municao %d (pente %d)", i, o.w[i].type, o.w[i].totalAmmo, o.w[i].ammoInClip);
}

static void ApplyPlayerState(uint8_t* ped, bool firstTime)
{
    const StateSnap& S = g_snap;

    if (firstTime && g_restoreOutfit && S.hasOutfit)             // outfit: before the weapons (the model is recreated)
    {
        const int16_t mdl = *(int16_t*)(ped + PED_MODEL_INDEX_OFF);
        const uint8_t* mi = (mdl >= 0 && mdl < 6500) ? *(const uint8_t**)(MODELINFO_PTRS + (uintptr_t)mdl * 4) : nullptr;
        const char* cur = mi ? (const char*)(mi + MODELINFO_NAME_OFF) : "";
        if (_stricmp(cur, S.outfit) != 0)
        {
            Log("   roupa: %s -> %s.", cur, S.outfit);
            ((void (__thiscall*)(void*, const char*))FN_PED_SET_MODEL_NAME)(ped, S.outfit);
            // Without this the new model is not loaded and Tommy ends up without a clump (crash on the next animation).
            // It is the same sequence the game uses when respawning: load the requested models and recreate the ped's model.
            ((void (__cdecl*)(int))FN_LOAD_ALL_MODELS)(0);
            ((void (__thiscall*)(void*))FN_PED_REAPPLY_MODEL)(ped);
        }
        else Log("   roupa ja era %s.", S.outfit);
    }

    if (g_restoreWeapons)
    {
        if (firstTime)
        {
            for (int i = 0; i < WEAPON_SLOTS; ++i)                          // clears the weapons
            {
                int32_t* w = (int32_t*)(ped + PED_WEAPONS_OFF + i * 0x18);
                w[0] = 0; w[1] = 0; w[2] = 0; w[3] = 0;
            }
            ((void (__thiscall*)(void*, int))FN_PED_SET_CUR_WEAPON)(ped, 0);
            for (int i = 0; i < WEAPON_SLOTS; ++i)
            {
                const WeaponSnap& ws = S.w[i];
                if (ws.type <= 0) continue;
                ((int (__thiscall*)(void*, int, uint32_t, int))FN_PED_GIVE_WEAPON)(ped, ws.type, (uint32_t)ws.totalAmmo, 1);
            }
            if (S.activeSlot >= 0 && S.activeSlot < WEAPON_SLOTS)
                ((void (__thiscall*)(void*, int))FN_PED_SET_CUR_WEAPON)(ped, S.activeSlot);
        }
        for (int i = 0; i < WEAPON_SLOTS; ++i)                              // exact ammo
        {
            const WeaponSnap& ws = S.w[i];
            if (ws.type <= 0) continue;
            int32_t* w = (int32_t*)(ped + PED_WEAPONS_OFF + i * 0x18);
            if (w[0] != ws.type)
            {
                ((int (__thiscall*)(void*, int, uint32_t, int))FN_PED_GIVE_WEAPON)(ped, ws.type, (uint32_t)ws.totalAmmo, 1);
                if (!firstTime) Log("   arma %d tinha sumido do slot %d; entregue de novo.", ws.type, i);
            }
            if (w[0] == ws.type && (w[2] != ws.ammoInClip || w[3] != ws.totalAmmo))
            {
                if (!firstTime) Log("   municao do slot %d mudou (%d/%d); corrigindo para %d/%d.", i, w[3], w[2], ws.totalAmmo, ws.ammoInClip);
                w[2] = ws.ammoInClip; w[3] = ws.totalAmmo;
            }
        }
    }
    if (g_restoreHealth)
    {
        float* h = (float*)(ped + PED_HEALTH_OFF);
        if (*h != S.health) { if (!firstTime) Log("   vida mudou (%.1f); corrigindo para %.1f.", *h, S.health); *h = S.health; }
    }
    if (g_restoreArmour)
    {
        float* a = (float*)(ped + PED_ARMOUR_OFF);
        if (*a != S.armour) { if (!firstTime) Log("   colete mudou (%.1f); corrigindo para %.1f.", *a, S.armour); *a = S.armour; }
    }
    if (g_restoreMoney)
    {
        int32_t* money   = (int32_t*)(PlayerInfo() + PLAYER_MONEY_OFF);
        int32_t* display = (int32_t*)(PlayerInfo() + PLAYER_DISPLAY_OFF);
        if (*money != S.money || *display != S.money)
        {
            if (!firstTime) Log("   dinheiro mudou (%d); corrigindo para %d.", *money, S.money);
            *money = S.money; *display = S.money;
        }
    }
    if (g_restoreWanted)
    {
        uint8_t* wanted = PedWanted(ped);
        if (wanted)
        {
            int32_t* level = (int32_t*)(wanted + WANTED_LEVEL_OFF);
            int32_t* chaos = (int32_t*)(wanted + WANTED_CHAOS_OFF);
            if (firstTime || *level != S.wantedLevel)
            {
                const int before = *level, chaosBefore = *chaos;
                ((void (__thiscall*)(void*, int))FN_WANTED_SET)(wanted, S.wantedLevel);   // official function: clears pending crimes
                if (firstTime || before != *level)
                    Log("   procurado: %d (caos %d) -> %d (caos %d); salvo era %d.", before, chaosBefore, *level, *chaos, S.wantedLevel);
            }
        }
    }
}

// Time (only the first time) and weather (immediately, and again during the check)
static void ApplyWorldState(bool firstTime)
{
    const StateSnap& S = g_snap;
    if (g_restoreTime && firstTime)
        ((void (__cdecl*)(int, int))FN_SET_GAME_CLOCK)((int)S.hours, (int)S.minutes);     // the game's official function
    if (g_restoreWeather)
    {
        // Immediate, like a weather cheat, but without registering as a cheat: the weather visible when the mission started
        // (the old or the new one, depending on the interpolation) becomes the current weather in both variables, with no gradual transition.
        const int16_t type = (S.weatherInterp >= 0.5f) ? S.weatherB : S.weatherA;
        *(int16_t*)ADDR_WEATHER_A = type;
        *(int16_t*)ADDR_WEATHER_B = type;
        *(float*)ADDR_WEATHER_INTERP = 0.0f;
    }
}

// ======================= Phases of the prompt pause =======================
enum Phase { PH_NONE = 0, PH_REQUEST, PH_ACTIVE, PH_WAITRELEASE, PH_CLOSING };
static Phase    g_phase = PH_NONE;
static DWORD    g_phaseTick = 0;           // GetTickCount (real time; the game timer stops while paused)
static bool     g_forcedActivate = false;
static bool     g_acceptAfterClose = false;
static bool     g_teleportPending = false;
static bool     g_haveSaved = false, g_prevOnMission = false;
static bool     g_failSeen = false, g_pending = false;
static DWORD    g_failTime = 0, g_missionEndTime = 0, g_lastBusy = 0;
static bool     g_reapply = false;
static DWORD    g_reapplyUntil = 0;
static uint8_t  g_savedMouseFlag = 0;

static void StartPrompt()
{
    g_pending = false;
    g_phase = PH_REQUEST; g_phaseTick = GetTickCount(); g_forcedActivate = false;
    if (!*(uint8_t*)ADDR_MENU_ACTIVE) *(uint8_t*)MENU_START_REQ = 1;    // requests the pause (like pressing Esc)
    Log("Pausa solicitada para mostrar o aviso.");
}

// Removes the player from the vehicle, copying what the game itself does when a script moves the player elsewhere
// while inside a car (without this, Teleport does not work with the player inside the vehicle).
static bool LeaveVehicle(uint8_t* ped)
{
    if (!ped[0x3AC]) return false;                                   // bInVehicle
    uint8_t* veh = *(uint8_t**)(ped + 0x3A8);                        // m_pMyVehicle
    if (veh)
    {
        if ((veh[0x1FA] >> 1) & 1) ped[0x14E] = (uint8_t)((ped[0x14E] & 0xFB) | 0x04);
        if (*(uint8_t**)(veh + 0x1A8) == ped)                        // was the driver
        {
            ((void (__thiscall*)(void*))0x5B8920)(veh);              // CVehicle::RemoveDriver
            veh[0x50] = (uint8_t)((veh[0x50] & 0x07) | 0x20);        // status: abandoned
            veh[0x1F9] &= 0xEF;
            veh[0x160] = 0;
        }
        else ((void (__thiscall*)(void*, void*))0x5B8CE0)(veh, ped); // CVehicle::RemovePassenger(ped)
    }
    ((void (__thiscall*)(void*, int))0x512440)(ped, 1);
    ped[0x3AC] = 0;
    *(uint32_t*)(ped + 0x3A8) = 0;
    if (*(int32_t*)(ped + 0x244) == 0x0B) ((void (__thiscall*)(void*))0x4F7920)(ped);
    *(int32_t*)(ped + 0x244) = 1;                                    // PED_IDLE
    ped[0x51] = (uint8_t)((ped[0x51] & 0xFE) | 0x01);                // collision is back
    ((void (__thiscall*)(void*))0x4FF5A0)(ped);
    uint8_t* assoc = *(uint8_t**)(ped + 0x1F8);                      // "sitting in the car" animation: fades out gradually
    if (assoc) *(uint32_t*)(assoc + 0x1C) = 0xC47A0000;
    *(uint32_t*)(ped + 0x1F8) = 0;
    ((void (__thiscall*)(void*, int))0x50D110)(ped, 0);
    ((void (__cdecl*)(void*, void*, int, uint32_t))0x405640)(*(void**)(ped + 0x4C), *(void**)(ped + 0x1F4), 3, *(uint32_t*)0x688650); // idle animation
    ((void (__thiscall*)(void*))0x50CCF0)(ped);
    return true;
}

static void DoTeleport(uint8_t* ped)
{
    const StateSnap& S = g_snap;
    if (LeaveVehicle(ped)) Log("Jogador estava em um veiculo: removido antes de teleportar.");
    void** vtbl = *(void***)ped;
    ((void (__thiscall*)(void*, float, float, float))vtbl[PED_VTBL_TELEPORT / 4])(ped, S.pos[0], S.pos[1], S.pos[2]);
    float* sp = (float*)(ped + PED_SPEED_OFF); sp[0] = sp[1] = sp[2] = 0.0f;
    float pos[3] = { S.pos[0], S.pos[1], S.pos[2] };
    ((void (__cdecl*)(float*))FN_LOAD_SCENE_COLLISION)(pos);             // loads collision and scenery around the marker
    ((void (__cdecl*)(float*))FN_LOAD_SCENE)(pos);

    StateSnap cur = {}; TakeSnapshot(ped, cur);
    LogSnapshot("Antes de restaurar (estado atual)", cur);
    ApplyPlayerState(ped, true);
    ApplyWorldState(true);
    g_reapply = true; g_reapplyUntil = GetTickCount() + 2500;
    Log("Teleportado para %.1f %.1f %.1f e estado restaurado.", S.pos[0], S.pos[1], S.pos[2]);
    LogSnapshot("Estado salvo no marcador", S);
}

// ---- Tick: once per game frame (right after CGameLogic::Update) ----
static void Tick()
{
    CheckReload();
    if (!g_enabled) return;
    TrackInputDevice(false);

    uint8_t* ped = PlayerPed();
    if (!ped) return;
    const DWORD now = *(uint32_t*)ADDR_TIMER_MS;
    const bool onMission = IsOnMission();
    const bool alive = *(float*)(ped + PED_HEALTH_OFF) > 0.5f;

    if (!onMission && alive) TakeSnapshot(ped, g_rolling);      // state from before stepping on the marker

    if (onMission && !g_prevOnMission)
    {
        if (g_rolling.valid) g_snap = g_rolling; else TakeSnapshot(ped, g_snap);
        g_haveSaved = true; g_pending = false; g_failSeen = false; g_reapply = false;
        Log("Missao iniciada. Marcador salvo: %.1f %.1f %.1f.", g_snap.pos[0], g_snap.pos[1], g_snap.pos[2]);
        LogSnapshot("Estado salvo", g_snap);
    }
    if (!onMission && g_prevOnMission)
    {
        g_missionEndTime = now;
        if (g_failSeen && now - g_failTime < 15000 && g_haveSaved) { g_pending = true; Log("Missao terminou com falha."); }
        else Log("Missao terminou sem falha (ou sem marcador salvo).");
    }
    g_prevOnMission = onMission;

    if (!onMission && g_failSeen && g_haveSaved && !g_pending && g_phase == PH_NONE &&
        now - g_failTime < 15000 && now - g_missionEndTime < 15000)
        g_pending = true;                                       // the failure message arrived after the end of the mission

    if (onMission || !alive) g_lastBusy = now;

    if (g_pending && g_phase == PH_NONE && !onMission && now - g_lastBusy >= 1500 && now - g_failTime >= (DWORD)(g_promptDelaySec * 1000.0f))
    {
        g_failSeen = false;
        StartPrompt();
    }
    if (g_phase == PH_REQUEST && (onMission || !alive))
    {
        g_phase = PH_NONE; Log("Aviso cancelado antes de pausar.");
    }

    if (g_teleportPending && !*(uint8_t*)ADDR_MENU_ACTIVE)
    {
        g_teleportPending = false;
        DoTeleport(ped);
    }
    if (g_reapply)
    {
        if ((int32_t)(GetTickCount() - g_reapplyUntil) > 0) { g_reapply = false; Log("Conferencia pos-restauracao encerrada."); }
        else { ApplyPlayerState(ped, false); ApplyWorldState(false); }
    }
}

// ======================= Menu text and drawing =======================
// Converts the .ini text (UTF-8 or ANSI) to the game's character codes. "s" and "S" with caron (U+0161/U+0160)
// become SmallSCode/CapitalSCode; the other accented letters become the plain letter.
static unsigned StripAccent(unsigned cp)
{
    static const char* from = "\xC0\xC1\xC2\xC3\xC4\xC5\xC7\xC8\xC9\xCA\xCB\xCC\xCD\xCE\xCF\xD1\xD2\xD3\xD4\xD5\xD6\xD9\xDA\xDB\xDC\xDD"
                              "\xE0\xE1\xE2\xE3\xE4\xE5\xE7\xE8\xE9\xEA\xEB\xEC\xED\xEE\xEF\xF1\xF2\xF3\xF4\xF5\xF6\xF9\xFA\xFB\xFC\xFD\xFF";
    static const char* to   = "AAAAAACEEEEIIIINOOOOOUUUUY"
                              "aaaaaaceeeeiiiinooooouuuuyy";
    if (cp >= 0xC0 && cp <= 0xFF)
    {
        const char* p = (const char*)memchr(from, (int)cp, strlen(from));
        if (p) return (unsigned char)to[p - from];
    }
    return '?';
}

static size_t DecodeText(const char* s, unsigned* out, size_t n)
{
    size_t o = 0;
    while (*s && o + 1 < n)
    {
        const unsigned char c = (unsigned char)*s;
        unsigned cp = 0; int len = 0;
        if (c < 0x80) { out[o++] = c; ++s; continue; }
        if (c >= 0xC2 && c <= 0xDF && (s[1] & 0xC0) == 0x80)                                   { cp = ((c & 0x1F) << 6) | (s[1] & 0x3F); len = 2; }
        else if (c >= 0xE0 && c <= 0xEF && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80)     { cp = ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); len = 3; }
        if (len)
        {
            out[o++] = (cp == 0x161) ? (unsigned)g_sCodeSmall : (cp == 0x160) ? (unsigned)g_sCodeCap : StripAccent(cp);
            s += len;
        }
        else                                                   // stray byte (file saved as ANSI): 0x9A / 0x8A = s / S with caron
        {
            out[o++] = (c == 0x9A) ? (unsigned)g_sCodeSmall : (c == 0x8A) ? (unsigned)g_sCodeCap : StripAccent(c);
            ++s;
        }
    }
    out[o] = 0;
    return o;
}

static void ToWide(const char* s, wchar_t* out, size_t n)
{
    unsigned codes[600];
    DecodeText(s, codes, 600);
    size_t o = 0;
    for (; codes[o] && o + 1 < n; ++o) out[o] = (wchar_t)codes[o];
    out[o] = 0;
}

static void FillRectPx(float l, float t, float r, float b, Rgba8 col)
{
    static uint32_t nullSprite = 0;              // CSprite2d without a texture = flat color rectangle
    RectF rc = { l, b, r, t };
    ((void (__thiscall*)(void*, RectF*, Rgba8*))FN_SPRITE_DRAW)(&nullSprite, &rc, &col);
}

// Cursor and shadow EXACTLY as the game draws them (including the Widescreen Fix adjustment): the mod captures the rectangles
// and colors the game uses (Hook_CursorDraw), does not draw them right away, and later draws them shifted to the current
// mouse position and on top of the prompt text.
struct CursorCap { bool have; RectF r; Rgba8 c; };
static CursorCap g_curCap[2];
static bool g_curHooked = false;
static uintptr_t g_origCursor[2] = { 0, 0 };

template<int N> static void __fastcall Hook_CursorDrawN(void* sprite, void*, RectF* r, Rgba8* c)
{
    if (g_phase == PH_ACTIVE || g_phase == PH_WAITRELEASE)
    {
        if (r && c) { g_curCap[N].r = *r; g_curCap[N].c = *c; g_curCap[N].have = true; }
        return;
    }
    ((void (__thiscall*)(void*, RectF*, Rgba8*))g_origCursor[N])(sprite, r, c);
}

static void DrawGameCursor(float mx, float my)
{
    uint8_t* sprite = (uint8_t*)MENU_SPRITE_MOUSE;
    if (!*(void**)sprite) return;                // cursor texture not loaded yet
    if (g_curHooked)
    {
        if (!g_curCap[1].have) return;           // the game has not drawn the cursor yet this frame
        const float ox = g_curCap[1].r.left, oy = g_curCap[1].r.top;      // cursor tip in the game's drawing
        const float dx = mx - ox, dy = my - oy, k = g_cursorSize;
        for (int i = 0; i < 2; ++i)
        {
            if (!g_curCap[i].have) continue;
            RectF r = g_curCap[i].r;
            r.left   = ox + (r.left   - ox) * k + dx;  r.right = ox + (r.right - ox) * k + dx;
            r.top    = oy + (r.top    - oy) * k + dy;  r.bottom = oy + (r.bottom - oy) * k + dy;
            ((void (__thiscall*)(void*, RectF*, Rgba8*))FN_SPRITE_DRAW)(sprite, &r, &g_curCap[i].c);
            g_curCap[i].have = false;
        }
        return;
    }
    // Fallback (if the game's points could not be hooked): same measurements as the game at 640x448.
    const float ux = (float)*pScreenW / 640.0f * g_cursorSize, uy = (float)*pScreenH / 448.0f * g_cursorSize;
    RectF shadowR = { mx + 10.0f * ux, my + 38.0f * uy, mx + 45.0f * ux, my + 3.0f * uy };
    Rgba8 shadow = { 100, 100, 100, 50 };
    ((void (__thiscall*)(void*, RectF*, Rgba8*))FN_SPRITE_DRAW)(sprite, &shadowR, &shadow);
    RectF r = { mx, my + 35.0f * uy, mx + 35.0f * ux, my };
    Rgba8 white = { 255, 255, 255, 255 };
    ((void (__thiscall*)(void*, RectF*, Rgba8*))FN_SPRITE_DRAW)(sprite, &r, &white);
}

struct MenuLayout { float step, top[3], bandL, bandR; };
static MenuLayout GetLayout(float W, float H)
{
    MenuLayout L;
    L.step = H * 0.0676f * g_lineSpacing;
    const float y0 = H * g_msgY / 100.0f + g_msgOffY;
    L.top[0] = y0 - L.step; L.top[1] = y0; L.top[2] = y0 + L.step;
    L.bandL = W * 0.5f - W * 0.17f + g_msgOffX;
    L.bandR = W * 0.5f + W * 0.17f + g_msgOffX;
    return L;
}
static int HitItem(const MenuLayout& L, float mx, float my)
{
    if (mx < L.bandL || mx > L.bandR) return -1;
    for (int i = 0; i < 2; ++i)
        if (my >= L.top[i + 1] - L.step * 0.10f && my < L.top[i + 1] + L.step * 0.90f) return i;
    return -1;
}

static int   g_sel = 0;                       // 0 = Yes, 1 = No
static float g_mx = -1, g_my = -1, g_lastMouseX = -1000, g_lastMouseY = -1000;
static bool  g_mouseSeen = false, g_cursorVisible = true;

static void PrintMenuLine(const char* text, float x, float y, uint32_t abgr)
{
    wchar_t w[300]; ToWide(text, w, 300);
    Rgba8 c = { (uint8_t)(abgr & 0xFF), (uint8_t)((abgr >> 8) & 0xFF), (uint8_t)((abgr >> 16) & 0xFF), (uint8_t)(abgr >> 24) };
    ((void (__cdecl*)(Rgba8*))FN_FONT_SET_COLOR)(&c);
    ((void (__cdecl*)(float, float, wchar_t*))FN_FONT_PRINT)(x, y, w);
}

static float g_hlPts[8]; static Rgba8 g_hlCol = { 0, 0, 0, 0 }; static bool g_hlHave = false;   // highlight the game tried to draw this frame
static void DrawPauseMenu(const MenuLayout& L)
{
    const float W = (float)*pScreenW, H = (float)*pScreenH;

    // Green highlight: the SAME drawing (shape and color) the game uses on the other menu options, only shifted to the
    // selected option (Yes/No) and drawn before the text, so it stays behind it.
    if (g_showHighlight)
    {
        const float cxT = W * 0.5f + g_msgOffX;
        const float cyT = L.top[g_sel + 1] + L.step * 0.43f + g_highlightOffY;
        if (g_hlHave)
        {
            const float cxC = (g_hlPts[0] + g_hlPts[2] + g_hlPts[4] + g_hlPts[6]) * 0.25f;
            const float cyC = (g_hlPts[1] + g_hlPts[3] + g_hlPts[5] + g_hlPts[7]) * 0.25f;
            const float dx = cxT - cxC, dy = cyT - cyC;
            ((void (__cdecl*)(float, float, float, float, float, float, float, float, Rgba8*))FN_DRAW_POLYGON)(
                g_hlPts[0] + dx, g_hlPts[1] + dy, g_hlPts[2] + dx, g_hlPts[3] + dy,
                g_hlPts[4] + dx, g_hlPts[5] + dy, g_hlPts[6] + dx, g_hlPts[7] + dy, &g_hlCol);
        }
        else
        {
            // the game did not draw a highlight this frame: uses the old rectangle as a fallback
            const float halfH = L.step * 0.46f * g_highlightHeight;
            const float l = cxT - W * g_highlightWidth * 0.5f, r = cxT + W * g_highlightWidth * 0.5f;
            Rgba8 col = { (uint8_t)(g_highlightColor & 0xFF), (uint8_t)((g_highlightColor >> 8) & 0xFF),
                          (uint8_t)((g_highlightColor >> 16) & 0xFF), (uint8_t)(g_highlightColor >> 24) };
            ((void (__cdecl*)(float, float, float, float, float, float, float, float, Rgba8*))FN_DRAW_POLYGON)(l, cyT - halfH, r, cyT - halfH, l, cyT + halfH, r, cyT + halfH, &col);
        }
    }
    g_hlHave = false;

    // Background, logo, screen title and the rest are the game's own; only the options were hidden (Hook_MenuPrint).
    ((void (__cdecl*)())FN_FONT_SET_PROP_ON)();
    ((void (__cdecl*)())FN_FONT_SET_BG_OFF)();
    ((void (__cdecl*)())FN_FONT_SET_BGTEXT_OFF)();
    ((void (__cdecl*)())FN_FONT_SET_CENTRE_ON)();
    ((void (__cdecl*)(float))FN_FONT_SET_WRAPX)(W);
    ((void (__cdecl*)(float))FN_FONT_SET_CENTRESIZE)(W);
    ((void (__cdecl*)(float))FN_FONT_SET_RJWRAP)(W);
    ((void (__cdecl*)(int16_t))FN_FONT_SET_STYLE)((int16_t)g_fontStyle);
    ((void (__cdecl*)(int16_t))FN_FONT_SET_SHADOW)(1);
    Rgba8 dropc = { 0, 0, 0, 255 };
    ((void (__cdecl*)(Rgba8*))FN_FONT_SET_DROPCOLOR)(&dropc);
    ((void (__cdecl*)(float, float))FN_FONT_SET_SCALE)(W * 0.0009375f * g_scaleMul, H * 0.0026785715f * g_scaleMul);

    const float cx = W * 0.5f + g_msgOffX;
    PrintMenuLine(g_message, cx, L.top[0], g_titleColor);
    PrintMenuLine(g_yesText, cx, L.top[1], g_sel == 0 ? g_selColor : g_normColor);
    PrintMenuLine(g_noText,  cx, L.top[2], g_sel == 1 ? g_selColor : g_normColor);
    ((VoidFn_t)FN_RENDER_FONT_BUFFER)();           // draws the text now so the cursor stays on top

    if (g_mouseSeen)
    {
        *(int32_t*)(MENU_OBJ + MENU_MOUSE_X_OFF) = (int32_t)g_mx;
        *(int32_t*)(MENU_OBJ + MENU_MOUSE_Y_OFF) = (int32_t)g_my;
        if (g_ownCursor && g_cursorVisible) DrawGameCursor(g_mx, g_my);
    }
}

static void BeginClose(bool accept, const char* why)
{
    PlayMenuSound(accept ? g_sndConfirm : g_sndBack);
    g_acceptAfterClose = accept;
    g_phase = PH_WAITRELEASE; g_phaseTick = GetTickCount();
    Log("%s -> %s.", why, accept ? "recomecar (teleportar)" : "cancelar");
}

static bool g_pUp = false, g_pDown = false, g_pEnter = false, g_pOk = false, g_pClick = false, g_pEsc = false, g_pBack = false;

static bool AnyMenuInputHeld()
{
    static const int vks[] = { VK_UP, VK_DOWN, VK_RETURN, VK_ESCAPE, VK_SPACE, VK_LBUTTON, VK_RBUTTON };
    if (GameIsForeground()) for (int vk : vks) if (GetAsyncKeyState(vk) & 0x8000) return true;
    XGamepad xg;
    if (ReadXInput(xg) && (xg.wButtons != 0 || xg.bLT > 40 || xg.bRT > 40)) return true;
    return false;
}

// Called every frame with the menu on screen, right after the game draws the menu
static void PauseFrame()
{
    if (g_phase == PH_NONE) return;
    CheckReload();
    const bool menuActive = *(uint8_t*)ADDR_MENU_ACTIVE != 0;
    const DWORD tnow = GetTickCount();

    if (g_phase == PH_REQUEST)
    {
        if (menuActive)
        {
            g_phase = PH_ACTIVE; g_phaseTick = tnow;
            g_sel = 0; g_mouseSeen = false; g_lastMouseX = g_lastMouseY = -1000;
            g_savedMouseFlag = *(uint8_t*)ADDR_MENU_MOUSE;
            if (!g_curHooked) *(uint8_t*)ADDR_MENU_MOUSE = 0;  // without capture, the cursor is drawn only by this mod
            g_cursorVisible = !(g_hideCursorWithPad && g_usingPad);
            const int stick0 = LeftStickVertical();
            g_pUp   = KeyPressed(VK_UP)   || PadDown(0x10, 0x0001, 0) || stick0 < 0;
            g_pDown = KeyPressed(VK_DOWN) || PadDown(0x12, 0x0002, 0) || stick0 > 0;
            g_pEnter = KeyPressed(VK_RETURN); g_pOk = PadDown(0x20, 0x1000, 0);
            g_pClick = GameIsForeground() && (GetAsyncKeyState(VK_LBUTTON) & 0x8000);
            g_pEsc = KeyPressed(VK_ESCAPE); g_pBack = PadDown(g_backOffset, g_backXMask, g_backXTrig);
            Log("Jogo pausado; menu Retry? na tela.");
        }
        else if (tnow - g_phaseTick > 1500 && !g_forcedActivate)
        {
            g_forcedActivate = true; *(uint8_t*)MENU_START_REQ = 1;
            Log("Pedido de pausa repetido.");
        }
        else if (tnow - g_phaseTick > 5000) { g_phase = PH_NONE; Log("Nao foi possivel pausar o jogo; aviso cancelado."); }
        return;
    }

    if (!menuActive && g_phase != PH_CLOSING && g_phase != PH_WAITRELEASE)
    {
        g_phase = PH_NONE; *(uint8_t*)ADDR_MENU_MOUSE = g_savedMouseFlag;
        Log("Menu fechado por outro motivo; aviso cancelado.");
        return;
    }

    const float W = (float)*pScreenW, H = (float)*pScreenH;
    const MenuLayout L = GetLayout(W, H);

    if (g_phase == PH_ACTIVE)
    {
        TrackInputDevice(true);
        g_cursorVisible = !(g_hideCursorWithPad && g_usingPad);

        const int  stick = LeftStickVertical();
        const bool up    = KeyPressed(VK_UP)   || PadDown(0x10, 0x0001, 0) || stick < 0;
        const bool down  = KeyPressed(VK_DOWN) || PadDown(0x12, 0x0002, 0) || stick > 0;
        const bool enter = KeyPressed(VK_RETURN);
        const bool ok    = PadDown(0x20, 0x1000, 0);
        const bool click = GameIsForeground() && (GetAsyncKeyState(VK_LBUTTON) & 0x8000);
        const bool esc   = g_cancelOnEsc && KeyPressed(VK_ESCAPE);
        const bool back  = PadDown(g_backOffset, g_backXMask, g_backXTrig);

        float mx, my; int hit = -1;
        if (GetMouseGame(mx, my))
        {
            g_mx = mx; g_my = my; g_mouseSeen = true;
            hit = g_cursorVisible ? HitItem(L, mx, my) : -1;
            if (fabsf(mx - g_lastMouseX) + fabsf(my - g_lastMouseY) > 3.0f)
            {
                g_lastMouseX = mx; g_lastMouseY = my;
                if (hit >= 0 && hit != g_sel) { g_sel = hit; PlayMenuSound(g_sndNavigate); }
            }
        }

        if ((tnow - g_phaseTick) > 300)
        {
            if ((up && !g_pUp) || (down && !g_pDown)) { g_sel = 1 - g_sel; PlayMenuSound(g_sndNavigate); }
            if ((esc && !g_pEsc) || (back && !g_pBack))
                BeginClose(false, (esc && !g_pEsc) ? "Esc: No" : "Voltar do controle (Y): No");
            else if (click && !g_pClick && hit >= 0)
            { g_sel = hit; BeginClose(hit == 0, hit == 0 ? "Mouse: Yes" : "Mouse: No"); }
            else if ((enter && !g_pEnter) || (ok && !g_pOk))
                BeginClose(g_sel == 0, g_sel == 0 ? "Confirmar: Yes" : "Confirmar: No");
        }
        g_pUp = up; g_pDown = down; g_pEnter = enter; g_pOk = ok; g_pClick = click; g_pEsc = esc; g_pBack = back;
    }
    else if (g_phase == PH_WAITRELEASE)
    {
        // the menu stays on screen until you release what you pressed: that way the click/key does not "leak" into gameplay
        if (!AnyMenuInputHeld() || tnow - g_phaseTick > 1500)
        {
            g_phase = PH_CLOSING; g_phaseTick = tnow;
        }
    }

    if (g_phase == PH_ACTIVE || g_phase == PH_WAITRELEASE)
    {
        DrawPauseMenu(L);
        // The game only "draws" the cursor (which the mod captures) if this flag is on: it takes effect on the next frame.
        if (g_curHooked) *(uint8_t*)ADDR_MENU_MOUSE = (g_ownCursor && g_cursorVisible && g_mouseSeen) ? 1 : 0;
    }
}

// ======================= Hooks =======================
static VoidFn_t g_origGameLogicUpdate = nullptr;
// Each call site stores the target IT had when it was hooked. If another plugin (SilentPatch, CLEO etc.)
// had already replaced the call, the mod joins that plugin's chain instead of giving up.
static uintptr_t g_origProc[2] = { 0, 0 };
static uintptr_t g_origDraw[2] = { 0, 0 };
typedef void (__cdecl *BigMsg_t)(const wchar_t*, uint32_t, uint32_t);
static BigMsg_t g_origBigMessage = nullptr;

// Replaces the drawing of the game's green highlight while the prompt is on screen (the mod draws its own, on the right option).
static void __cdecl Hook_MenuHighlight(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, Rgba8* col)
{
    if (g_phase != PH_NONE)
    {
        // stores the game's original shape and color; the mod redraws them on the selected Yes/No option
        g_hlPts[0] = x1; g_hlPts[1] = y1; g_hlPts[2] = x2; g_hlPts[3] = y2; g_hlPts[4] = x3; g_hlPts[5] = y3; g_hlPts[6] = x4; g_hlPts[7] = y4;
        if (col) g_hlCol = *col;
        g_hlHave = true;
        return;
    }
    ((void (__cdecl*)(float, float, float, float, float, float, float, float, Rgba8*))FN_DRAW_POLYGON)(x1, y1, x2, y2, x3, y3, x4, y4, col);
}

// Replaces the calls that write the menu options' text: while the prompt is on screen, it writes nothing.
// (The screen title does not go through here, so it stays as in the game.)
static void __cdecl Hook_MenuPrint(float x, float y, wchar_t* text)
{
    if (g_phase != PH_NONE) return;
    ((void (__cdecl*)(float, float, wchar_t*))FN_FONT_PRINT)(x, y, text);
}

static void __cdecl Hook_GameLogicUpdate()
{
    g_origGameLogicUpdate();
    Tick();
}

template<int N> static void __fastcall Hook_DrawFrontEndN(void* self, void*)
{
    ((void (__thiscall*)(void*))g_origDraw[N])(self);
    static int s_lastPhase = -1;
    if ((int)g_phase != s_lastPhase) { s_lastPhase = (int)g_phase; if (g_phase != PH_NONE) Log("Desenho do menu: fase %d.", (int)g_phase); }
    PauseFrame();
}

// Replaces CMenuManager::Process in the menu loop. While the prompt is on screen the menu does NOT receive input
// (nobody picks an invisible option); we only do the maintenance the game does every frame.
static void MenuProcessImpl(void* self, void (__thiscall *orig)(void*))
{
    if (g_phase == PH_ACTIVE || g_phase == PH_WAITRELEASE || g_phase == PH_CLOSING)
    {
        if (!((int (__thiscall*)(void*))FN_MENU_BUSY)((void*)OBJ_MENU_BUSY))
        {
            ((void (__thiscall*)(void*))FN_MENU_UPDATE)(self);
            if (*((uint8_t*)self + 0x38))
            {
                ((void (__thiscall*)(void*))FN_AUDIO_SERVICE)((void*)ADDR_DMAUDIO);
                ((void (__cdecl*)(int))FN_AUDIO_A)(1);
                ((void (__cdecl*)(int))FN_AUDIO_B)(1);
            }
            if (g_phase == PH_CLOSING) *(uint8_t*)MENU_SHUT_REQ = 1;          // asks to close the menu
            ((void (__thiscall*)(void*))FN_MENU_CHECK)(self);
        }
        if (g_phase == PH_CLOSING)
        {
            if (!*(uint8_t*)ADDR_MENU_ACTIVE)
            {
                *(uint8_t*)ADDR_MENU_MOUSE = g_savedMouseFlag;
                g_phase = PH_NONE;
                if (g_acceptAfterClose) g_teleportPending = true;
                Log("Menu fechado; jogo despausado.");
            }
            else if (GetTickCount() - g_phaseTick > 4000)
            {
                *(uint8_t*)ADDR_MENU_MOUSE = g_savedMouseFlag;
                g_phase = PH_NONE;
                if (g_acceptAfterClose) g_teleportPending = true;
                Log("Fechamento demorou; usando o processamento normal do menu.");
                orig(self);
            }
        }
        return;
    }
    orig(self);
}

template<int N> static void __fastcall Hook_MenuProcessN(void* self, void*)
{
    MenuProcessImpl(self, (void (__thiscall*)(void*))g_origProc[N]);
}

static void __cdecl Hook_BigMessage(const wchar_t* text, uint32_t time, uint32_t style)
{
    if (text && g_enabled)
    {
        const wchar_t* fail = ((const wchar_t* (__thiscall*)(void*, const char*))FN_TEXT_GET)((void*)ADDR_THETEXT, "M_FAIL");
        if (fail && wcscmp(text, fail) == 0)
        {
            g_failSeen = true;
            g_failTime = *(uint32_t*)ADDR_TIMER_MS;
            Log("Mensagem de missao falhada detectada.");
        }
    }
    g_origBigMessage(text, time, style);
}

// Swaps the target of a "call rel32" and returns the original target
static uintptr_t PatchCall(uintptr_t site, void* newTarget)
{
    uint8_t* p = (uint8_t*)site;
    if (p[0] != 0xE8) return 0;
    uintptr_t orig = site + 5 + (uintptr_t)(intptr_t)(*(int32_t*)(p + 1));
    DWORD old;
    if (!VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old)) return 0;
    *(int32_t*)(p + 1) = (int32_t)((uintptr_t)newTarget - (site + 5));
    VirtualProtect(p, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, 5);
    return orig;
}

static int PatchAllCalls(uintptr_t target, void* hook)
{
    int count = 0;
    const uintptr_t begin = 0x401000, end = 0x67E000;
    for (uintptr_t a = begin; a + 5 <= end; ++a)
    {
        const uint8_t* p = (const uint8_t*)a;
        if (p[0] != 0xE8) continue;
        uintptr_t t = a + 5 + (uintptr_t)(intptr_t)(*(const int32_t*)(p + 1));
        if (t == target && PatchCall(a, hook)) ++count;
    }
    return count;
}

BOOL APIENTRY DllMain(HMODULE hm, DWORD reason, LPVOID)
{
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(hm);

    GetModuleFileNameA(hm, g_iniPath, MAX_PATH);
    char* dot = strrchr(g_iniPath, '.');
    if (dot) strcpy(dot, ".ini");

    g_logEnabled = GetPrivateProfileIntA("Retry", "EnableLog", 0, g_iniPath) != 0;
    if (g_logEnabled) { FILE* f = fopen("RetryMissionPauseVC.log", "w"); if (f) fclose(f); }

    if (*(uint32_t*)VERSION_ADDR != VERSION_OK)
    {
        Log("Versao do exe nao reconhecida (esperado Vice City 1.0). Mod desativado.");
        return TRUE;
    }

    LoadConfig();
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (GetFileAttributesExA(g_iniPath, GetFileExInfoStandard, &fad)) g_lastWrite = fad.ftLastWriteTime;

    // The call site must be a "call rel32" made on the menu object (mov ecx, 0x869630 right before).
    // If the target is not the original game's, another plugin has already hooked the call: we join its chain.
    auto hookSite = [&](uintptr_t site, uintptr_t expected, void* hook, uintptr_t& origOut, bool needMenuObj) -> bool
    {
        const uint8_t* q = (const uint8_t*)site;
        if (q[0] != 0xE8) { Log("Ponto %X nao e uma chamada; ignorado.", (unsigned)site); return false; }
        if (needMenuObj && !(q[-5] == 0xB9 && *(const uint32_t*)(q - 4) == MENU_OBJ))
        { Log("Ponto %X nao chama sobre o objeto do menu; ignorado.", (unsigned)site); return false; }
        const uintptr_t target = site + 5 + (uintptr_t)(intptr_t)(*(const int32_t*)(q + 1));
        if (target != expected) Log("Ponto %X: destino %X (esperado %X) - outro plugin ja ganchou; entrando na cadeia.", (unsigned)site, (unsigned)target, (unsigned)expected);
        origOut = PatchCall(site, hook);
        return origOut != 0;
    };

    // 1) per-frame logic, right after CGameLogic::Update
    uintptr_t o1 = 0;
    if (!hookSite(CALL_GAMELOGIC_UPDATE, FN_GAMELOGIC_UPDATE, (void*)&Hook_GameLogicUpdate, o1, false))
    { Log("Nao foi possivel ganchar CGameLogic::Update. Mod desativado."); return TRUE; }
    g_origGameLogicUpdate = (VoidFn_t)o1;

    // 2) pause menu drawing (both paths)
    static void* const drawHooks[2] = { (void*)&Hook_DrawFrontEndN<0>, (void*)&Hook_DrawFrontEndN<1> };
    int nDraw = 0;
    for (int i = 0; i < 2; ++i)
        if (hookSite(CALLS_DRAW_FRONTEND[i], FN_DRAW_FRONTEND, drawHooks[i], g_origDraw[i], true)) ++nDraw;
    if (nDraw == 0) { Log("Nenhum ponto de desenho do menu pode ser ganchado. Mod desativado."); g_enabled = false; return TRUE; }

    // 3) menu processing (both paths)
    static void* const procHooks[2] = { (void*)&Hook_MenuProcessN<0>, (void*)&Hook_MenuProcessN<1> };
    int nProc = 0;
    for (int i = 0; i < 2; ++i)
        if (hookSite(CALLS_MENU_PROCESS[i], FN_MENU_PROCESS, procHooks[i], g_origProc[i], true)) ++nProc;
    if (nProc == 0) { Log("Nenhum ponto de CMenuManager::Process pode ser ganchado. Mod desativado."); g_enabled = false; return TRUE; }

    // 3b) hides the menu options' text while the prompt is on screen
    int nPrint = 0;
    for (uintptr_t site : MENU_PRINT_SITES)
    {
        uint8_t* q = (uint8_t*)site;
        const uintptr_t t = (q[0] == 0xE8) ? site + 5 + (uintptr_t)(intptr_t)(*(int32_t*)(q + 1)) : 0;
        if (t != FN_FONT_PRINT) { Log("Ponto de texto do menu em %X nao confere; ignorado.", (unsigned)site); continue; }
        if (PatchCall(site, (void*)&Hook_MenuPrint)) ++nPrint;
    }

    // 3c) replaces the game's green highlight with the mod's while the prompt is on screen
    int nHigh = 0;
    for (uintptr_t site : MENU_HIGHLIGHT_SITES)
    {
        uint8_t* q = (uint8_t*)site;
        const uintptr_t t = (q[0] == 0xE8) ? site + 5 + (uintptr_t)(intptr_t)(*(int32_t*)(q + 1)) : 0;
        if (t != FN_DRAW_POLYGON) { Log("Ponto do destaque do menu em %X nao confere; ignorado.", (unsigned)site); continue; }
        if (PatchCall(site, (void*)&Hook_MenuHighlight)) ++nHigh;
    }

    // 3d) menu cursor: captures the game's own drawing (identical size/shape, with the Widescreen Fix)
    {
        static void* const curHooks[2] = { (void*)&Hook_CursorDrawN<0>, (void*)&Hook_CursorDrawN<1> };
        int nCur = 0;
        for (int i = 0; i < 2; ++i)
            if (hookSite(CURSOR_SITES[i], FN_SPRITE_DRAW, curHooks[i], g_origCursor[i], false)) ++nCur;
        g_curHooked = (nCur == 2);
        Log("Cursor do menu: %d/2 pontos ganchados (%s).", nCur, g_curHooked ? "desenho do jogo" : "reserva");
    }

    // 4) "MISSION FAILED" message
    g_origBigMessage = (BigMsg_t)FN_ADD_BIG_MESSAGE;
    int nMsg = PatchAllCalls(FN_ADD_BIG_MESSAGE, (void*)&Hook_BigMessage);

    Log("OK: hooks instalados (AddBigMessage x%d, desenho do menu x%d, processamento do menu x%d, texto das opcoes x%d).", nMsg, nDraw, nProc, nPrint);
    Log("Destaque do menu x%d.", nHigh);
    Log("Build: %s %s | Vice City 1.0 | sons: navegar=%d confirmar=%d voltar=%d (ajustaveis no .ini).", __DATE__, __TIME__, g_sndNavigate, g_sndConfirm, g_sndBack);
    return TRUE;
}
