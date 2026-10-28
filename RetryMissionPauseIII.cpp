// RetryMissionPauseIII.cpp - GTA III 1.0 (PC, gta3.exe)   [NOT COMPILED, NOT TESTED]
//
// GTA III version of RetryMissionPause (Vice City). When the player steps on the marker and the mission starts, it stores
// the position, weapons (with the exact ammo), health, armour, money, wanted level and time/weather.
// If the mission fails (by dying, being arrested or by its objectives), after respawning the game PAUSES (the game's own
// pause screen) and shows, in place of the pause menu options:
//
//        Retry?
//        Yes
//        No
//
//   keyboard: Up/Down arrows + Enter      mouse: hover the cursor and click
//   gamepad (XInput or GInput): dpad or left stick Up/Down + Cross (A)
//   Yes = unpauses, teleports to the marker and restores everything; No / Esc / gamepad Triangle = unpauses and cancels.
//
// The screen is the game's OWN pause screen: the mod only replaces the text of the first 3 options ("Retry?", "Yes", "No") and
// hides the rest. The highlight of the selected option (color and green rectangle) and the mouse cursor are drawn by the
// game itself, so they are identical to the original menu's.
// GTA III has no outfit changes or rain drops on the screen: those parts of the Vice City version do not exist here.
// The sounds are those of the game's menu. What you press in the menu is only released to gameplay after you let go.
//
// Configuration in RetryMissionPauseIII.ini (reloaded when saved).
// Compile as Win32 (x86). The output must be named RetryMissionPauseIII.asi
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cctype>
#include <cwchar>

// ======================= Addresses (GTA III 1.0) =======================
static const uintptr_t VERSION_ADDR = 0x5C1E70;      // start of the 1.0 exe entry point
static const uint32_t  VERSION_OK   = 0x53E58955;

// Player
static const uintptr_t PLAYERS_BASE   = 0x9412F0;    // CWorld::Players[], stride 0x13C (ped at offset 0)
static const uintptr_t PLAYER_FOCUS   = 0x95CD61;    // CWorld::PlayerInFocus (byte)
static const uintptr_t PLAYER_STRIDE  = 0x13C;
static const uintptr_t PLAYER_MONEY_OFF = 0xAC;      // CPlayerInfo::m_nMoney (0x94139C for player 0). The value shown on screen follows by itself.
static const uintptr_t PED_POS_OFF     = 0x34;       // position CVector
static const uintptr_t PED_SPEED_OFF   = 0x78;       // velocity CVector (the game itself zeroes it here when respawning the player)
static const uintptr_t PED_HEALTH_OFF  = 0x2C0;      // float
static const uintptr_t PED_ARMOUR_OFF  = 0x2C4;      // float
static const uintptr_t PED_WEAPONS_OFF = 0x35C;      // CWeapon[13], 0x18 each, indexed BY the weapon TYPE: type +0, state +4, clip +8, total +0xC
static const uintptr_t PED_CUR_WEAPON  = 0x498;      // type of the weapon in hand (int8)
static const uintptr_t PED_WANTED_OFF  = 0x53C;      // CWanted*
static const int       WEAPON_SLOTS    = 13;

// Wanted (CWanted)
static const uintptr_t WANTED_CHAOS_OFF = 0x00;
static const uintptr_t WANTED_LEVEL_OFF = 0x18;
static const uintptr_t FN_WANTED_SET    = 0x4ADA50;  // CWanted::SetWantedLevel(int) (thiscall)

// Ped functions
static const uintptr_t FN_PED_GIVE_WEAPON    = 0x4CF9B0;  // CPed::GiveWeapon(type, ammo) (thiscall, 2 arguments)
static const uintptr_t FN_PED_SET_CUR_WEAPON = 0x4F2560;  // CPed::SetCurrentWeapon(type) (thiscall)
static const int       PED_VTBL_TELEPORT     = 0x2C;      // virtual method: Teleport(x, y, z) (thiscall, the function cleans the stack)
static const uintptr_t FN_LOAD_SCENE         = 0x40A6D0;  // loads the scenery around the new position: (CVector*) cdecl (the game uses it when respawning)

// Game state
static const uintptr_t ADDR_TIMER_MS      = 0x885B48;    // CTimer::m_snTimeInMilliseconds (stops when paused)
static const uintptr_t ADDR_MISSION_OFF   = 0x8F1B64;    // offset of the mission flag in ScriptSpace
static const uintptr_t ADDR_SCRIPT_SPACE  = 0x74B248;
static const uintptr_t ADDR_CLOCK_HOURS   = 0x95CDA6;    // byte
static const uintptr_t ADDR_CLOCK_MINUTES = 0x95CDC8;    // byte
static const uintptr_t FN_SET_GAME_CLOCK  = 0x4733C0;    // CClock::SetGameClock(hours, minutes) (cdecl)
// Weather: what the game copies in its own weather-update routine (old <- new)
static const uintptr_t ADDR_WEATHER_A      = 0x95CCEC;   // old weather (WORD)
static const uintptr_t ADDR_WEATHER_B      = 0x95CC70;   // new weather (WORD)
static const uintptr_t ADDR_WEATHER_INTERP = 0x8F2520;   // interpolation between the two weathers (float)

// Messages
static const uintptr_t FN_ADD_BIG_MESSAGE = 0x529EB0;    // CMessages::AddBigMessage(wchar*, time, style) (cdecl)
static const uintptr_t CALLS_BIG_MESSAGE[3] = { 0x43D0A8, 0x4A1317, 0x4A1367 };
static const uintptr_t FN_TEXT_GET        = 0x52C5A0;    // CText::Get(const char*) (thiscall)
static const uintptr_t ADDR_THETEXT       = 0x941520;

// Hook points (calls)
static const uintptr_t CALL_GAMELOGIC_UPDATE = 0x48C9C8; static const uintptr_t FN_GAMELOGIC_UPDATE = 0x421400;
static const uintptr_t FN_DRAW_FRONTEND   = 0x47A540;    // CMenuManager::DrawFrontEnd (only called with the menu active)
static const uintptr_t CALL_DRAW_FRONTEND = 0x48E45E;
static const uintptr_t FN_MENU_PROCESS    = 0x485100;    // CMenuManager::Process
static const uintptr_t CALLS_MENU_PROCESS[2]  = { 0x48C8A4, 0x48E721 };

// Pause menu (FrontendMenuManager)
static const uintptr_t MENU_OBJ          = 0x8F59D8;
static const uintptr_t MENU_ACTIVE       = MENU_OBJ + 0x111;   // m_bMenuActive
static const uintptr_t MENU_STATE_CHG    = 0x112;              // zeroed at the start of each Process
static const uintptr_t MENU_FLAG_114     = 0x114;              // same
static const uintptr_t MENU_FADE_BYPASS  = 0x453;              // if 0 and the screen is fading, Process does nothing
static const uintptr_t MENU_SHOW_MOUSE   = 0x128;              // != 0: the menu draws the cursor
static const uintptr_t MENU_MOUSE_X      = 0x118;              // mouse position (int, game screen pixels)
static const uintptr_t MENU_MOUSE_Y      = 0x11C;
static const uintptr_t MENU_MOUSE_X2     = 0x120;              // position destination (Process copies it to 0x118/0x11C)
static const uintptr_t MENU_MOUSE_Y2     = 0x124;
static const uintptr_t MENU_PAGE         = 0x548;              // current page
static const uintptr_t MENU_CUR_OPTION   = 0x54C;              // selected option
static const uintptr_t ADDR_REQ_OPEN     = 0x95CCF4;           // request to open the menu (like pressing Esc)
static const uintptr_t ADDR_REQ_CLOSE    = 0x95CD6A;           // request to close the menu
static const uintptr_t FN_MENU_SERVICE   = 0x47A4D0;           // thiscall(menu): Process per-frame maintenance
static const uintptr_t FN_MENU_REQUESTS  = 0x488790;           // thiscall(menu): opens/closes according to the requests
static const uintptr_t FN_MENU_TEXTURES  = 0x47A230;           // thiscall(menu): makes sure the menu textures are loaded
static const uintptr_t FN_FADE_STATUS    = 0x46B9C0;           // thiscall on OBJ_CAMERA: != 0 while the screen is fading
static const uintptr_t OBJ_CAMERA        = 0x6FACF8;
static const uintptr_t MENU_PAUSE_PAGE   = 0x34;               // "FET_PAU" page (pause during the game)
static const uintptr_t MENU_SCREENS      = 0x611930;           // page table (0x184 bytes each; options from +0x1C, 0x14 bytes each)

// Audio
static const uintptr_t FN_PLAY_FRONTEND  = 0x57CC20;           // DMAudio.PlayFrontEndSound(sound, 0) (thiscall)
static const uintptr_t ADDR_DMAUDIO      = 0x95CDBE;

// Text: the two game calls that write the menu options' text (left and right column)
static const uintptr_t FN_FONT_PRINT      = 0x500F50;          // CFont::PrintString(float x, float y, wchar_t* text)
static const uintptr_t SITE_PRINT_OPTION  = 0x47C666;
static const uintptr_t SITE_PRINT_VALUE   = 0x47C74C;

static const int* const pScreenW = (const int*)0x8F436C;
static const int* const pScreenH = (const int*)0x8F4370;

// Gamepad (CPad 0)
static const uintptr_t PAD0_ADDR = 0x6F0360;

// ======================= Types =======================
typedef void (__cdecl *VoidFn_t)();

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
static bool  g_ownCursor = true, g_hideCursorWithPad = true, g_cancelOnEsc = true;
static int   g_sndNavigate = 0x97, g_sndConfirm = 0x98, g_sndBack = 0x9D;
static int   g_backOffset = 0x1E; static uint16_t g_backXMask = 0x8000; static int g_backXTrig = 0;
static int   g_padSource = 0;                 // 0 = Auto, 1 = XInput, 2 = Game (CPad)
static bool  g_restoreWeapons = true, g_restoreHealth = true, g_restoreArmour = true, g_restoreMoney = true,
             g_restoreWanted = true, g_restoreTime = true, g_restoreWeather = true;
static float g_promptDelaySec = 3.0f;   // seconds between the mission failed message and the Retry? menu
static float g_hitTop = 0.20f, g_hitBottom = 0.80f;   // clickable area of each line, as a fraction of the line height

static char     g_iniPath[MAX_PATH];
static FILETIME g_lastWrite = {0, 0};
static DWORD    g_lastCheck = 0;

static bool g_logEnabled = false;      // EnableLog=1 in the .ini turns on the log file

static void Log(const char* fmt, ...)
{
    if (!g_logEnabled) return;
    FILE* f = fopen("RetryMissionPauseIII.log", "a");
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

static void LoadConfig()
{
    char buf[256];
    g_enabled = GetPrivateProfileIntA("Retry", "Enabled", 1, g_iniPath) != 0;
    g_logEnabled = GetPrivateProfileIntA("Retry", "EnableLog", 0, g_iniPath) != 0;
    GetPrivateProfileStringA("Retry", "Message", "Retry?", g_message, sizeof(g_message), g_iniPath);
    GetPrivateProfileStringA("Retry", "YesText", "Yes", g_yesText, sizeof(g_yesText), g_iniPath);
    GetPrivateProfileStringA("Retry", "NoText", "No", g_noText, sizeof(g_noText), g_iniPath);
    g_ownCursor = GetPrivateProfileIntA("Retry", "ShowMouseCursor", 1, g_iniPath) != 0;
    g_hideCursorWithPad = GetPrivateProfileIntA("Retry", "HideCursorWithController", 1, g_iniPath) != 0;
    g_cancelOnEsc = GetPrivateProfileIntA("Retry", "CancelOnEscape", 1, g_iniPath) != 0;
    g_hitTop    = ReadFloat("ClickAreaTop", 0.20f);
    g_hitBottom = ReadFloat("ClickAreaBottom", 0.80f);
    g_sndNavigate = GetPrivateProfileIntA("Retry", "NavigateSound", 0x97, g_iniPath);
    g_sndConfirm  = GetPrivateProfileIntA("Retry", "ConfirmSound", 0x98, g_iniPath);
    g_sndBack     = GetPrivateProfileIntA("Retry", "BackSound", 0x9D, g_iniPath);

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

static inline bool MenuIsActive() { return *(uint8_t*)MENU_ACTIVE != 0; }

// ======================= Game state snapshot =======================
struct WeaponSnap { int32_t type, ammoInClip, totalAmmo; };
struct StateSnap
{
    bool       valid;
    float      pos[3];
    float      health, armour;
    int32_t    money;
    WeaponSnap w[WEAPON_SLOTS];
    int8_t     curWeapon;
    int32_t    wantedChaos, wantedLevel;
    uint8_t    hours, minutes;
    int16_t    weatherA, weatherB;
    float      weatherInterp;
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
    o.curWeapon = *(int8_t*)(ped + PED_CUR_WEAPON);
    uint8_t* wanted = PedWanted(ped);
    o.wantedChaos = wanted ? *(int32_t*)(wanted + WANTED_CHAOS_OFF) : 0;
    o.wantedLevel = wanted ? *(int32_t*)(wanted + WANTED_LEVEL_OFF) : 0;
    o.hours   = *(uint8_t*)ADDR_CLOCK_HOURS;
    o.minutes = *(uint8_t*)ADDR_CLOCK_MINUTES;
    o.weatherA = *(int16_t*)ADDR_WEATHER_A;
    o.weatherB = *(int16_t*)ADDR_WEATHER_B;
    o.weatherInterp = *(float*)ADDR_WEATHER_INTERP;
    o.valid = true;
}

static void LogSnapshot(const char* title, const StateSnap& o)
{
    Log("%s: vida=%.1f colete=%.1f dinheiro=%d procurado=%d (caos %d) hora=%02d:%02d clima=%d/%d(%.2f) arma na mao=%d",
        title, o.health, o.armour, o.money, o.wantedLevel, o.wantedChaos, o.hours, o.minutes,
        (int)o.weatherA, (int)o.weatherB, o.weatherInterp, (int)o.curWeapon);
    for (int i = 0; i < WEAPON_SLOTS; ++i)
        if (o.w[i].type > 0) Log("   arma %d: municao %d (pente %d)", o.w[i].type, o.w[i].totalAmmo, o.w[i].ammoInClip);
}

static void ApplyPlayerState(uint8_t* ped, bool firstTime)
{
    const StateSnap& S = g_snap;

    if (g_restoreWeapons)
    {
        if (firstTime)
        {
            for (int i = 0; i < WEAPON_SLOTS; ++i)                          // clears the weapons
            {
                int32_t* w = (int32_t*)(ped + PED_WEAPONS_OFF + i * 0x18);
                for (int k = 0; k < 6; ++k) w[k] = 0;
            }
            ((void (__thiscall*)(void*, int))FN_PED_SET_CUR_WEAPON)(ped, 0);
            for (int i = 0; i < WEAPON_SLOTS; ++i)
            {
                const WeaponSnap& ws = S.w[i];
                if (ws.type <= 0) continue;
                ((int (__thiscall*)(void*, int, uint32_t))FN_PED_GIVE_WEAPON)(ped, ws.type, (uint32_t)ws.totalAmmo);
            }
            if (S.curWeapon >= 0 && S.curWeapon < WEAPON_SLOTS)
                ((void (__thiscall*)(void*, int))FN_PED_SET_CUR_WEAPON)(ped, S.curWeapon);
        }
        for (int i = 0; i < WEAPON_SLOTS; ++i)                              // exact ammo
        {
            const WeaponSnap& ws = S.w[i];
            if (ws.type <= 0) continue;
            int32_t* w = (int32_t*)(ped + PED_WEAPONS_OFF + i * 0x18);
            if (w[0] != ws.type)
            {
                ((int (__thiscall*)(void*, int, uint32_t))FN_PED_GIVE_WEAPON)(ped, ws.type, (uint32_t)ws.totalAmmo);
                if (!firstTime) Log("   arma %d tinha sumido; entregue de novo.", ws.type);
            }
            if (w[0] == ws.type && (w[2] != ws.ammoInClip || w[3] != ws.totalAmmo))
            {
                if (!firstTime) Log("   municao da arma %d mudou (%d/%d); corrigindo para %d/%d.", ws.type, w[3], w[2], ws.totalAmmo, ws.ammoInClip);
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
        int32_t* money = (int32_t*)(PlayerInfo() + PLAYER_MONEY_OFF);
        if (*money != S.money)
        {
            if (!firstTime) Log("   dinheiro mudou (%d); corrigindo para %d.", *money, S.money);
            *money = S.money;
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
        // Immediate, like a weather cheat: the weather visible when the mission started (the old or the new one, depending on the
        // interpolation) becomes the current weather in both variables, with no gradual transition.
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
static uint8_t  g_savedShowMouse = 0;

static void StartPrompt()
{
    g_pending = false;
    g_phase = PH_REQUEST; g_phaseTick = GetTickCount(); g_forcedActivate = false;
    if (!MenuIsActive()) *(uint8_t*)ADDR_REQ_OPEN = 1;                  // requests the pause (like pressing Esc)
    Log("Pausa solicitada para mostrar o aviso.");
}

// Removes the player from the vehicle, copying what the game itself does when a script moves the player elsewhere
// while inside a car (without this, Teleport does not work with the player inside the vehicle).
static bool LeaveVehicle(uint8_t* ped)
{
    if (!ped[0x314]) return false;                                   // bInVehicle
    uint8_t* veh = *(uint8_t**)(ped + 0x310);                        // m_pMyVehicle
    if (veh)
    {
        if ((veh[0x1F6] >> 1) & 1) ped[0x156] = (uint8_t)((ped[0x156] & 0xFB) | 0x04);
        if (*(uint8_t**)(veh + 0x1A4) == ped)                        // was the driver
        {
            ((void (__thiscall*)(void*))0x5520A0)(veh);              // CVehicle::RemoveDriver
            veh[0x50] = (uint8_t)((veh[0x50] & 0x07) | 0x20);        // status: abandoned
            veh[0x1F5] &= 0xEF;
            veh[0x164] = 0;
        }
        else ((void (__thiscall*)(void*, void*))0x551EB0)(veh, ped); // CVehicle::RemovePassenger(ped)
    }
    ped[0x314] = 0;
    *(uint32_t*)(ped + 0x310) = 0;
    *(int32_t*)(ped + 0x224) = 1;                                    // PED_IDLE
    ped[0x51] = (uint8_t)((ped[0x51] & 0xFE) | 0x01);                // collision is back
    ((void (__thiscall*)(void*))0x4E4E20)(ped);
    uint8_t* assoc = *(uint8_t**)(ped + 0x1D8);                      // "sitting in the car" animation: fades out gradually
    if (assoc) *(uint32_t*)(assoc + 0x1C) = 0xC47A0000;
    *(uint32_t*)(ped + 0x1D8) = 0;
    ((void (__thiscall*)(void*, int))0x4C5A30)(ped, 0);
    ((void (__cdecl*)(void*, void*, int, uint32_t))0x403710)(*(void**)(ped + 0x4C), *(void**)(ped + 0x1D4), 3, *(uint32_t*)0x5EE80C); // idle animation
    ((void (__thiscall*)(void*))0x4C5D80)(ped);
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
    ((void (__cdecl*)(float*))FN_LOAD_SCENE)(pos);                       // loads scenery around the marker

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

    if (g_teleportPending && !MenuIsActive())
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

// ======================= Prompt text =======================
// Converts the .ini text (UTF-8 or ANSI) to the game's character codes (accented letters become the plain letter).
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

static void ToWide(const char* s, wchar_t* out, size_t n)
{
    size_t o = 0;
    while (*s && o + 1 < n)
    {
        const unsigned char c = (unsigned char)*s;
        if (c < 0x80) { out[o++] = c; ++s; continue; }
        unsigned cp = 0; int len = 0;
        if (c >= 0xC2 && c <= 0xDF && (s[1] & 0xC0) == 0x80)                                   { cp = ((c & 0x1F) << 6) | (s[1] & 0x3F); len = 2; }
        else if (c >= 0xE0 && c <= 0xEF && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80)     { cp = ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); len = 3; }
        if (len) { out[o++] = (wchar_t)StripAccent(cp); s += len; }
        else     { out[o++] = (wchar_t)StripAccent(c); ++s; }          // stray byte (file saved as ANSI)
    }
    out[o] = 0;
}

// ======================= Menu =======================
static int   g_sel = 0;                       // 0 = Yes, 1 = No
static float g_mx = -1, g_my = -1, g_lastMouseX = -1000, g_lastMouseY = -1000;
static bool  g_mouseSeen = false, g_cursorWanted = false, g_cursorVisible = true;
static wchar_t g_wMsg[300], g_wYes[120], g_wNo[120];
static int   g_printCalls = 0;                // how many times the game wrote an option this frame (2 per line: shadow and text)
static float g_rowY[3] = { -1, -1, -1 };      // y of each line (Retry?, Yes, No), captured from the game's own drawing
static bool  g_onPausePage = false;           // the menu is on the pause page (and only then is the text replaced)

static bool g_pUp = false, g_pDown = false, g_pEnter = false, g_pOk = false, g_pClick = false, g_pEsc = false, g_pBack = false;

static int HitItem(float mx, float my)
{
    const float W = (float)*pScreenW, H = (float)*pScreenH;
    if (mx < W * 10.0f / 640.0f || mx > W - W * 11.0f / 640.0f) return -1;     // same width as the game's selection rectangle
    float pitch = (g_rowY[1] >= 0 && g_rowY[2] >= 0) ? (g_rowY[2] - g_rowY[1]) : H * 0.07f;
    if (pitch < 4.0f) pitch = H * 0.07f;
    for (int i = 0; i < 2; ++i)
    {
        const float y = g_rowY[i + 1];
        if (y < 0) continue;
        if (my >= y - pitch * g_hitTop && my < y + pitch * g_hitBottom) return i;
    }
    return -1;
}

static void BeginClose(bool accept, const char* why)
{
    PlayMenuSound(accept ? g_sndConfirm : g_sndBack);
    g_acceptAfterClose = accept;
    g_phase = PH_WAITRELEASE; g_phaseTick = GetTickCount();
    Log("%s -> %s.", why, accept ? "recomecar (teleportar)" : "cancelar");
}

static bool AnyMenuInputHeld()
{
    static const int vks[] = { VK_UP, VK_DOWN, VK_RETURN, VK_ESCAPE, VK_SPACE, VK_LBUTTON, VK_RBUTTON };
    if (GameIsForeground()) for (int vk : vks) if (GetAsyncKeyState(vk) & 0x8000) return true;
    XGamepad xg;
    if (ReadXInput(xg) && (xg.wButtons != 0 || xg.bLT > 40 || xg.bRT > 40)) return true;
    return false;
}

// Menu maintenance while the prompt is on screen: the menu does NOT receive player input (nobody picks an invisible
// option). It does only what CMenuManager::Process does every frame before handling the buttons.
static void MenuLite(uint8_t* m)
{
    m[MENU_STATE_CHG] = 0;
    if (m[MENU_FADE_BYPASS] == 0 && ((int (__thiscall*)(void*))FN_FADE_STATUS)((void*)OBJ_CAMERA) != 0) return;
    m[MENU_FLAG_114] = 0;
    ((void (__thiscall*)(void*))FN_MENU_SERVICE)(m);
    ((void (__thiscall*)(void*))FN_MENU_REQUESTS)(m);
    if (MenuIsActive()) ((void (__thiscall*)(void*))FN_MENU_TEXTURES)(m);
}

// Prompt input (once per frame, with the menu open)
static void InputStep(uint8_t* m)
{
    CheckReload();
    const DWORD tnow = GetTickCount();
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
            hit = g_cursorVisible ? HitItem(mx, my) : -1;
            if (fabsf(mx - g_lastMouseX) + fabsf(my - g_lastMouseY) > 3.0f)
            {
                if (g_lastMouseX > -999.0f && g_cursorVisible) g_cursorWanted = true;      // the cursor appears when the mouse moves
                g_lastMouseX = mx; g_lastMouseY = my;
                if (hit >= 0 && hit != g_sel) { g_sel = hit; PlayMenuSound(g_sndNavigate); }
            }
        }

        if ((tnow - g_phaseTick) > 300)
        {
            if ((up && !g_pUp) || (down && !g_pDown)) { g_sel = 1 - g_sel; PlayMenuSound(g_sndNavigate); }
            if ((esc && !g_pEsc) || (back && !g_pBack))
                BeginClose(false, (esc && !g_pEsc) ? "Esc: No" : "Voltar do controle: No");
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
        if (!AnyMenuInputHeld() || tnow - g_phaseTick > 1500) { g_phase = PH_CLOSING; g_phaseTick = tnow; }
    }

    // The cursor and the selected option are drawn by the game itself from these menu fields
    if (g_mouseSeen)
    {
        *(int32_t*)(m + MENU_MOUSE_X) = (int32_t)g_mx;  *(int32_t*)(m + MENU_MOUSE_Y) = (int32_t)g_my;
        *(int32_t*)(m + MENU_MOUSE_X2) = (int32_t)g_mx; *(int32_t*)(m + MENU_MOUSE_Y2) = (int32_t)g_my;
    }
    m[MENU_SHOW_MOUSE] = (g_mouseSeen && g_ownCursor && g_cursorVisible && g_cursorWanted) ? 1 : 0;
}

// ======================= Hooks =======================
static VoidFn_t g_origGameLogicUpdate = nullptr;
// Each call site stores the target IT had when it was hooked. If another plugin had already replaced the call, the
// mod joins that plugin's chain instead of giving up.
static uintptr_t g_origProc[2] = { 0, 0 };
static uintptr_t g_origDraw = 0;
static uintptr_t g_origPrintOpt = 0, g_origPrintVal = 0;
typedef void (__cdecl *BigMsg_t)(const wchar_t*, uint32_t, uint32_t);
static BigMsg_t g_origBigMessage[3] = { nullptr, nullptr, nullptr };

static void __cdecl Hook_GameLogicUpdate()
{
    g_origGameLogicUpdate();
    Tick();
}

// Menu drawing (only called with the menu active): sets the selection to Yes/No and lets the game draw.
static void __fastcall Hook_DrawFrontEnd(void* self, void*)
{
    if (g_phase != PH_NONE)
    {
        uint8_t* m = (uint8_t*)self;
        g_printCalls = 0;
        const int32_t page = *(int32_t*)(m + MENU_PAGE);
        g_onPausePage = (page == 0 || page == (int32_t)MENU_PAUSE_PAGE);        // 0: the game is still going to pick the pause page this frame
        if (g_onPausePage && g_phase != PH_REQUEST)
        {
            *(int32_t*)(m + MENU_CUR_OPTION) = 1 + g_sel;   // line 0 = Retry?, 1 = Yes, 2 = No
            // The game's drawing changes the option by itself when the mouse position (0x118/0x11C) differs from the last one it stored
            // (0x53C/0x540), and since the normal Process does not run during the prompt, that position was never updated: with the mouse
            // over a line, the game overwrote the selection every frame and the arrows/dpad stopped working.
            *(int32_t*)(m + 0x53C) = *(int32_t*)(m + MENU_MOUSE_X);
            *(int32_t*)(m + 0x540) = *(int32_t*)(m + MENU_MOUSE_Y);
            *(int32_t*)0x628D3C = 1 + g_sel;                // the game's "last option": avoids a duplicated navigation sound
        }
    }
    ((void (__thiscall*)(void*))g_origDraw)(self);
}

// Replaces the call that writes the options' text: while the prompt is on screen it writes "Retry?", "Yes" and "No" on the
// first three lines (with the font, color and position the game has already prepared) and nothing on the others.
static void __cdecl Hook_PrintOption(float x, float y, wchar_t* text)
{
    if (g_phase != PH_NONE && g_onPausePage)
    {
        const int idx = g_printCalls++;
        const int row = idx / 2;                       // the game writes each line twice: shadow and text
        if (row < 3)
        {
            if (idx & 1) g_rowY[row] = y;
            ((void (__cdecl*)(float, float, wchar_t*))FN_FONT_PRINT)(x, y, row == 0 ? g_wMsg : (row == 1 ? g_wYes : g_wNo));
        }
        return;
    }
    ((void (__cdecl*)(float, float, wchar_t*))g_origPrintOpt)(x, y, text);
}
static void __cdecl Hook_PrintValue(float x, float y, wchar_t* text)       // right column (option values)
{
    if (g_phase != PH_NONE && g_onPausePage) return;
    ((void (__cdecl*)(float, float, wchar_t*))g_origPrintVal)(x, y, text);
}

static void EnterActive(uint8_t* m, DWORD tnow)
{
    g_phase = PH_ACTIVE; g_phaseTick = tnow;
    g_sel = 0; g_mouseSeen = false; g_lastMouseX = g_lastMouseY = -1000;
    g_rowY[0] = g_rowY[1] = g_rowY[2] = -1;
    g_savedShowMouse = m[MENU_SHOW_MOUSE];
    g_cursorWanted = g_savedShowMouse != 0;
    g_cursorVisible = !(g_hideCursorWithPad && g_usingPad);
    const int stick0 = LeftStickVertical();
    g_pUp   = KeyPressed(VK_UP)   || PadDown(0x10, 0x0001, 0) || stick0 < 0;
    g_pDown = KeyPressed(VK_DOWN) || PadDown(0x12, 0x0002, 0) || stick0 > 0;
    g_pEnter = KeyPressed(VK_RETURN); g_pOk = PadDown(0x20, 0x1000, 0);
    g_pClick = GameIsForeground() && (GetAsyncKeyState(VK_LBUTTON) & 0x8000);
    g_pEsc = KeyPressed(VK_ESCAPE); g_pBack = PadDown(g_backOffset, g_backXMask, g_backXTrig);
    ToWide(g_message, g_wMsg, 300); ToWide(g_yesText, g_wYes, 120); ToWide(g_noText, g_wNo, 120);
    Log("Jogo pausado; menu Retry? na tela.");
}

static void EndPrompt(uint8_t* m, const char* why)
{
    m[MENU_SHOW_MOUSE] = g_savedShowMouse;
    g_phase = PH_NONE;
    if (g_acceptAfterClose) g_teleportPending = true;
    g_acceptAfterClose = g_acceptAfterClose && false;
    Log("%s", why);
}

// CMenuManager::Process (called every frame, with the menu open or not)
static void ProcessImpl(void* self, void (__thiscall *orig)(void*))
{
    if (g_phase == PH_NONE) { orig(self); return; }
    uint8_t* m = (uint8_t*)self;
    const DWORD tnow = GetTickCount();

    if (g_phase == PH_REQUEST)
    {
        orig(self);                                       // the game opens the pause menu by itself (request already made)
        if (MenuIsActive()) EnterActive(m, tnow);
        else if (tnow - g_phaseTick > 1500 && !g_forcedActivate)
        {
            g_forcedActivate = true; *(uint8_t*)ADDR_REQ_OPEN = 1;
            Log("Pedido de pausa repetido.");
        }
        else if (tnow - g_phaseTick > 5000) { g_phase = PH_NONE; Log("Nao foi possivel pausar o jogo; aviso cancelado."); }
        return;
    }

    if (g_phase == PH_CLOSING)
    {
        *(uint8_t*)ADDR_REQ_CLOSE = 1;                    // asks to close; the game's Process does all the rest
        orig(self);
        if (!MenuIsActive()) EndPrompt(m, "Menu fechado; jogo despausado.");
        else if (tnow - g_phaseTick > 4000) { EndPrompt(m, "Fechamento demorou; seguindo com o processamento normal do menu."); }
        return;
    }

    // PH_ACTIVE / PH_WAITRELEASE
    MenuLite(m);
    if (!MenuIsActive())                                  // closed for another reason (e.g. gamepad Start): finishes the closing
    {
        orig(self);
        g_acceptAfterClose = false;
        EndPrompt(m, "Menu fechado por outro motivo; aviso cancelado.");
        return;
    }
    InputStep(m);
}

template<int N> static void __fastcall Hook_MenuProcessN(void* self, void*)
{
    ProcessImpl(self, (void (__thiscall*)(void*))g_origProc[N]);
}

template<int N> static void __cdecl Hook_BigMessageN(const wchar_t* text, uint32_t time, uint32_t style)
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
    g_origBigMessage[N](text, time, style);
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

BOOL APIENTRY DllMain(HMODULE hm, DWORD reason, LPVOID)
{
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(hm);

    GetModuleFileNameA(hm, g_iniPath, MAX_PATH);
    char* dot = strrchr(g_iniPath, '.');
    if (dot) strcpy(dot, ".ini");

    g_logEnabled = GetPrivateProfileIntA("Retry", "EnableLog", 0, g_iniPath) != 0;
    if (g_logEnabled) { FILE* f = fopen("RetryMissionPauseIII.log", "w"); if (f) fclose(f); }

    if (*(uint32_t*)VERSION_ADDR != VERSION_OK)
    {
        Log("Versao do exe nao reconhecida (esperado GTA III 1.0). Mod desativado.");
        return TRUE;
    }

    LoadConfig();
    ToWide(g_message, g_wMsg, 300); ToWide(g_yesText, g_wYes, 120); ToWide(g_noText, g_wNo, 120);
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (GetFileAttributesExA(g_iniPath, GetFileExInfoStandard, &fad)) g_lastWrite = fad.ftLastWriteTime;

    // The call site must be a "call rel32" (for the menu ones, made on the menu object: mov ecx, 0x8F59D8 right before).
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

    // 2) menu drawing (only called with the menu active)
    if (!hookSite(CALL_DRAW_FRONTEND, FN_DRAW_FRONTEND, (void*)&Hook_DrawFrontEnd, g_origDraw, true))
    { Log("Nao foi possivel ganchar o desenho do menu. Mod desativado."); g_enabled = false; return TRUE; }

    // 3) menu processing (both game paths)
    static void* const procHooks[2] = { (void*)&Hook_MenuProcessN<0>, (void*)&Hook_MenuProcessN<1> };
    int nProc = 0;
    for (int i = 0; i < 2; ++i)
        if (hookSite(CALLS_MENU_PROCESS[i], FN_MENU_PROCESS, procHooks[i], g_origProc[i], true)) ++nProc;
    if (nProc == 0) { Log("Nenhum ponto de CMenuManager::Process pode ser ganchado. Mod desativado."); g_enabled = false; return TRUE; }

    // 3b) menu options' text
    int nPrint = 0;
    if (hookSite(SITE_PRINT_OPTION, FN_FONT_PRINT, (void*)&Hook_PrintOption, g_origPrintOpt, false)) ++nPrint;
    if (hookSite(SITE_PRINT_VALUE,  FN_FONT_PRINT, (void*)&Hook_PrintValue,  g_origPrintVal, false)) ++nPrint;
    if (nPrint < 2) Log("Aviso: so %d dos 2 pontos de texto do menu foram ganchados; o aviso pode aparecer incompleto.", nPrint);

    // 4) "MISSION FAILED" message
    static void* const msgHooks[3] = { (void*)&Hook_BigMessageN<0>, (void*)&Hook_BigMessageN<1>, (void*)&Hook_BigMessageN<2> };
    int nMsg = 0;
    for (int i = 0; i < 3; ++i)
    {
        uintptr_t o = 0;
        if (hookSite(CALLS_BIG_MESSAGE[i], FN_ADD_BIG_MESSAGE, msgHooks[i], o, false)) { g_origBigMessage[i] = (BigMsg_t)o; ++nMsg; }
    }
    if (nMsg == 0) { Log("Nenhum ponto de AddBigMessage pode ser ganchado. Mod desativado."); g_enabled = false; return TRUE; }

    Log("OK: hooks instalados (AddBigMessage x%d, processamento do menu x%d, texto das opcoes x%d).", nMsg, nProc, nPrint);
    Log("Build: %s %s | GTA III 1.0 | sons: navegar=%d confirmar=%d voltar=%d (ajustaveis no .ini).", __DATE__, __TIME__, g_sndNavigate, g_sndConfirm, g_sndBack);
    return TRUE;
}
