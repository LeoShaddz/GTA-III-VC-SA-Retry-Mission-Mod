// RetryMissionPause.cpp - GTA San Andreas 1.0 US (inclui exe "compact"/Hoodlum)
// VERSAO ALTERNATIVA do RetryMission: em vez de um aviso com contador, o jogo PAUSA (tela de pausa do
// proprio jogo) e, no lugar das opcoes (Continuar, Mapa, ...), aparece no meio da tela:
//
//        Retry?
//        Yes
//        No
//
// "Yes" e "No" sao opcoes selecionaveis, como no menu de pausa:
//   teclado: setas Cima/Baixo + Enter      mouse: passar o cursor e clicar
//   controle (XInput ou GInput): direcional ou analogico esquerdo Cima/Baixo + Cross (A)
//   Yes = despausa e teleporta para o marcador (restaurando tudo); No (ou Esc) = despausa e cancela.
// Navegacao tambem com o analogico ESQUERDO (cima/baixo). Nao ha atalhos: so escolher e confirmar.
// Sons do menu ORIGINAL do jogo: navegar (setas/direcional/mouse), confirmar (Yes) e voltar (No, Esc, Y do controle).
// O cursor do mouse e o icone ORIGINAL do jogo (textura do proprio menu) e se comporta como no GInput:
// some quando voce usa o controle ou as setas do teclado e volta quando voce mexe no mouse.
// O que voce aperta no menu de Retry nao vaza para a gameplay (nada de soco ao clicar para confirmar).
// Nao ha contador: o jogador decide quando quiser. Os textos sao configuraveis no .ini.
//
// Quando o jogador pisa no marcador e a missao comeca, guarda a posicao, as armas (com a municao
// exata), a vida, o colete, o dinheiro, o nivel de procurado, o horario e o clima desse momento.
//
// Configuracao em RetryMissionPause.ini (recarregado ao salvar).
//
// Compilar como Win32 (x86). A saida deve se chamar RetryMissionPause.asi
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>

// ======================= Enderecos (GTA SA 1.0 US) =======================
static const uintptr_t VERSION_ADDR = 0x82457C;
static const uint32_t  VERSION_OK   = 0x94BF;

// Jogador
static const uintptr_t PLAYERS_BASE   = 0xB7CD98;  // CWorld::Players[], stride 0x190, ped no offset 0
static const uintptr_t PLAYER_FOCUS   = 0xB7CD74;  // CWorld::PlayerInFocus (byte)
static const uintptr_t PLAYER_STRIDE  = 0x190;
static const uintptr_t PED_STATE_OFF  = 0x530;     // CPed::m_ePedState (0x36 morrendo, 0x37 morto, 0x3F preso)
static const uintptr_t ENTITY_AREA_OFF = 0x2F;     // CEntity::m_nAreaCode

// Estado do jogo
static const uintptr_t ADDR_GAME_STATE   = 0x96A8B0;  // CGameLogic::GameState (0 = jogando; 1 = morreu; 2 = preso; ...)
static const uintptr_t ADDR_TIMER_MS     = 0xB7CB84;  // CTimer::m_snTimeInMilliseconds (para quando o jogo pausa)
static const uintptr_t ADDR_CURR_AREA    = 0xB72914;  // CGame::currArea (interior atual)
static const uintptr_t ADDR_CUTSCENE_A   = 0xB5F851;  // CCutsceneMgr::ms_running
static const uintptr_t ADDR_CUTSCENE_B   = 0xB5F852;  // CCutsceneMgr::ms_cutsceneProcessing
// CTheScripts::IsPlayerOnAMission: *(int*)(0xA49960 + *(uint32*)0xA476AC) == 1
static const uintptr_t ADDR_MISSION_OFF  = 0xA476AC;
static const uintptr_t ADDR_SCRIPT_SPACE = 0xA49960;

// Relogio e clima
static const uintptr_t ADDR_CLOCK_SECONDS = 0xB70150;  // CClock::ms_nGameClockSeconds (short)
static const uintptr_t ADDR_CLOCK_MINUTES = 0xB70152;  // CClock::ms_nGameClockMinutes (byte)
static const uintptr_t ADDR_CLOCK_HOURS   = 0xB70153;  // CClock::ms_nGameClockHours (byte)
static const uintptr_t FN_SET_GAME_CLOCK  = 0x52D150;  // CClock::SetGameClock(hours, minutes, dayOfWeek)
static const uintptr_t ADDR_WEATHER_INTERP = 0xC8130C; // CWeather::InterpolationValue (float)
static const uintptr_t ADDR_WEATHER_FORCED = 0xC81318; // CWeather::ForcedWeatherType (short, -1 = nenhum)
static const uintptr_t ADDR_WEATHER_NEW    = 0xC8131C; // CWeather::NewWeatherType (short)
static const uintptr_t ADDR_WEATHER_OLD    = 0xC81320; // CWeather::OldWeatherType (short)

// Dinheiro: CPlayerInfo::m_nMoney (+0xB8, = 0xB7CE50 para o jogador 0) e m_nDisplayMoney (+0xBC, mostrado na tela)
static const uintptr_t PLAYER_MONEY_OFF   = 0xB8;
static const uintptr_t PLAYER_DISPLAY_OFF = 0xBC;

// Procurado: CWanted* = *(Players + 4 + idx*0x190) (a mesma conta de FindPlayerWanted, 0x56E230)
static const uintptr_t WANTED_CHAOS_OFF = 0x00;    // int: nivel de "caos"
static const uintptr_t WANTED_LEVEL_OFF = 0x2C;    // int: estrelas (0-6)
static const uintptr_t FN_WANTED_UPDATE = 0x561C90; // CWanted::UpdateWantedLevel (thiscall, sem argumentos)

// CPed
static const uintptr_t PED_HEALTH_OFF   = 0x540;   // float
static const uintptr_t PED_ARMOUR_OFF   = 0x548;   // float
// Roupas, corte de cabelo e tatuagens (CPedClothesDesc) e stats/skills
static const uintptr_t PED_PLAYERDATA_OFF  = 0x480;    // CPlayerPed::m_pPlayerData
static const uintptr_t PLAYERDATA_CLOTHES_OFF = 0x4;   // CPlayerPedData::m_pPedClothesDesc
static const size_t    CLOTHES_DESC_SIZE   = 0x78;     // chaves de modelo[10] + de textura[18] (roupa, cabelo, tatuagens) + gordura + musculo
static const uintptr_t FN_REBUILD_PLAYER   = 0x5A82C0; // CClothes::RebuildPlayer(CPlayerPed*, bool) (cdecl)
static const uintptr_t STATS_REGION_ADDR   = 0xB78F68; // CStats: inteiros (ids 82..) e floats (ids 0..81), contiguos
static const size_t    STATS_REGION_SIZE   = 0x560;
static const uintptr_t PED_WEAPONS_OFF  = 0x5A0;   // CWeapon[13], 0x1C cada: tipo +0, estado +4, municao no pente +8, municao total +0xC
static const uintptr_t PED_ACTIVE_SLOT  = 0x718;   // byte
static const int       WEAPON_SLOTS     = 13;
static const uintptr_t FN_PED_CLEAR_WEAPONS = 0x5E6320;  // CPed::ClearWeapons (thiscall)
static const uintptr_t FN_PED_GIVE_WEAPON   = 0x5E6080;  // CPed::GiveWeapon(type, ammo, bool) (thiscall)
static const uintptr_t FN_PED_SET_CUR_WEAPON = 0x5E61F0; // CPed::SetCurrentWeapon(slot) (thiscall)

// Menu de pausa (FrontEndMenuManager em 0xBA6748)
static const uintptr_t MENU_OBJ             = 0xBA6748;
static const uintptr_t ADDR_MENU_ACTIVE     = 0xBA67A4;  // m_bMenuActive (= objeto + 0x5C)
static const uintptr_t ADDR_MENU_ACTIVATE   = 0xBA677B;  // m_bActivateMenuNextFrame (= objeto + 0x33)
static const uintptr_t MENU_SHUTDOWN_OFF    = 0x32;      // "pedido de fechar o menu"
static const uintptr_t MENU_ACTIVE_OFF      = 0x5C;
static const uintptr_t MENU_SCREEN_BYTE_OFF = 0x5B;
static const uintptr_t FN_REQUEST_PAUSE     = 0x53BC60;  // pede para abrir o menu de pausa no proximo frame
static const uintptr_t FN_MENU_PROCESS_CALL = 0x53BF44;  // call CMenuManager::Process (em CGame::Process)
static const uintptr_t FN_MENU_PROCESS      = 0x57B440;
static const uintptr_t FN_MENU_CHECK_CLOSE  = 0x576B70;  // trata abrir/fechar o menu
static const uintptr_t FN_MENU_STREAMING    = 0x573CF0;  // thiscall(uint8) - parte do Process que nao e input
static const uintptr_t FN_MENU_AUDIO_A      = 0x730740;  // cdecl(1)
static const uintptr_t FN_MENU_AUDIO_B      = 0x7305E0;  // cdecl(1)
static const uintptr_t FN_FONT_RENDER_BUF   = 0x71A210;  // CFont::RenderFontBuffer
static const uintptr_t FN_DRAW_RECT         = 0x727B60;  // CSprite2d::DrawRect(CRect*, CRGBA*)
static const uintptr_t FN_FONT_SET_DROPCOLOR = 0x719510; // CFont::SetDropColor(CRGBA)

// Cursor do mouse do menu (sprite carregado do proprio jogo) e entrada
static const uintptr_t MENU_MOUSE_ON_OFF   = 0xB8;      // 1 = o menu desenha o cursor / usa o mouse
static const uintptr_t MENU_MOUSE_X_OFF    = 0xBC;      // posicao do cursor (pixels da tela do jogo)
static const uintptr_t MENU_MOUSE_Y_OFF    = 0xC0;
static const uintptr_t MENU_SPRITE_OFF     = 0x154;     // CSprite2d "mouse" (textura mouse + mascara mousea)
static const uintptr_t FN_SPRITE_DRAW      = 0x728350;  // CSprite2d::Draw(CRect&, CRGBA&) (thiscall)
static const uintptr_t FN_STRETCH_X        = 0x5733E0;  // CMenuManager::StretchX(float) (thiscall, retorna em st0)
static const uintptr_t FN_STRETCH_Y        = 0x573410;  // CMenuManager::StretchY(float)
static const uintptr_t FN_UPDATE_PADS_CALL = 0x53BEE6;  // call CPad::UpdatePads em CGame::Process
static const uintptr_t FN_UPDATE_PADS      = 0x541DD0;

// Efeitos sonoros do menu (CAudioEngine::ReportFrontendAudioEvent) - os mesmos ids que o menu de pausa usa
static const uintptr_t ADDR_AUDIO_ENGINE   = 0xB6BC90;
static const uintptr_t FN_AUDIO_EVENT      = 0x506EA0;  // thiscall(engine, evento, volumeExtra, velocidade)
static const int SND_CONFIRM  = 1;     // Cross / Enter (selecionar)
static const int SND_BACK     = 2;     // Triangulo / Esc (voltar)
static const int SND_NAVIGATE = 3;     // direcional / setas / passar o mouse (mudar de opcao)

// Procurado: funcao oficial do jogo que define o nivel (limpa tambem a fila de crimes pendentes)
static const uintptr_t FN_WANTED_SET       = 0x562470;  // CWanted::SetWantedLevel(int) (thiscall)
static const uintptr_t ADDR_WANTED_MAXLVL  = 0x8CDEE4;  // nivel maximo de procurado permitido agora
static const uintptr_t ADDR_WANTED_NEVER   = 0x969171;  // cheat "nunca procurado" (SetWantedLevel nao faz nada se ligado)

// Estados dos itens do HUD (peso/estado de exibicao). Cada item: estado, tempo, tempo2
static const uintptr_t HUD_ITEM_BASES[4] = { 0xBAA404, 0xBAA414, 0xBAA424, 0xBAA434 };
static const uintptr_t HUD_MONEY_BASE    = 0xBAA424;    // estado (0 = escondido), tempo, tempo2
static const uintptr_t ADDR_HUD_LAST_MONEY = 0xBAA430;  // ultimo dinheiro exibido

// Controle (CPad 0; GetPad usa stride 0x134; NewState e o primeiro campo)
static const uintptr_t PAD0_ADDR = 0xB73458;

// Funcoes do jogo
static const uintptr_t FN_GAMELOGIC_UPDATE_CALL = 0x53C11D;  // call CGameLogic::Update (em CGame::Process)
static const uintptr_t FN_GAMELOGIC_UPDATE      = 0x442AD0;
static const uintptr_t FN_HUD_DRAW              = 0x58D490;  // CHud::Draw (chamado em 3 lugares)
static const uintptr_t FN_ADD_BIG_MESSAGE       = 0x69F2B0;  // CMessages::AddBigMessage(text, time, style)
static const uintptr_t FN_TEXT_GET              = 0x6A0050;  // CText::Get(const char* key) (thiscall)
static const uintptr_t ADDR_THETEXT             = 0xC1B340;
static const uintptr_t FN_PED_TELEPORT          = 0x5E4110;  // CPed::Teleport(CVector, uint8) (thiscall)
static const uintptr_t FN_LOAD_SCENE            = 0x40ED80;  // CStreaming::LoadScene(CVector*)

// CFont
static const uintptr_t FN_FONT_SET_SCALE   = 0x719380;
static const uintptr_t FN_FONT_SET_COLOR   = 0x719430;
static const uintptr_t FN_FONT_SET_STYLE   = 0x719490;
static const uintptr_t FN_FONT_SET_WRAPX   = 0x7194D0;
static const uintptr_t FN_FONT_SET_CENTRE  = 0x7194E0;  // CentreSize
static const uintptr_t FN_FONT_SET_EDGE    = 0x719590;
static const uintptr_t FN_FONT_SET_PROP    = 0x7195B0;
static const uintptr_t FN_FONT_SET_BG      = 0x7195C0;
static const uintptr_t FN_FONT_SET_JUSTIFY = 0x719600;
static const uintptr_t FN_FONT_SET_ORIENT  = 0x719610;  // 0 = centro, 1 = esquerda, 2 = direita
static const uintptr_t FN_FONT_PRINT       = 0x71A700;  // CFont::PrintString(float x, float y, char* text)

// Tela
static const int* const pScreenW = (const int*)0xC17044;  // RsGlobal.maximumWidth
static const int* const pScreenH = (const int*)0xC17048;  // RsGlobal.maximumHeight

// ======================= Tipos =======================
typedef void (__cdecl *VoidFn_t)();
typedef void (__cdecl *BigMsg_t)(const char*, uint32_t, uint32_t);
typedef const char* (__thiscall *TextGet_t)(void*, const char*);
typedef void (__thiscall *PedTeleport_t)(void*, float, float, float, uint8_t);
typedef void (__cdecl *LoadScene_t)(const float*);
typedef void (__thiscall *PedVoid_t)(void*);
typedef int  (__thiscall *PedGiveWeapon_t)(void*, int, uint32_t, int);
typedef void (__thiscall *PedSetSlot_t)(void*, int);
typedef void (__cdecl *SetClock_t)(uint8_t, uint8_t, uint8_t);
typedef void (__thiscall *WantedUpdate_t)(void*);
typedef void (__thiscall *WantedSet_t)(void*, int);
typedef void (__thiscall *AudioEvent_t)(void*, int, float, float);

// ======================= Configuracao =======================
// offset = campo do CPad do jogo (CControllerState); xmask = botao do XInput; xtrig = gatilho (1 = L2, 2 = R2)
struct PadButton { const char* name; const char* display; int offset; uint16_t xmask; int xtrig; };
static const PadButton PAD_BUTTONS[] = {
    { "DPadUp",    "Dpad Up",    0x10, 0x0001, 0 }, { "DPadDown",  "Dpad Down",  0x12, 0x0002, 0 },
    { "DPadLeft",  "Dpad Left",  0x14, 0x0004, 0 }, { "DPadRight", "Dpad Right", 0x16, 0x0008, 0 },
    { "L1", "L1", 0x08, 0x0100, 0 }, { "L2", "L2", 0x0A, 0, 1 }, { "R1", "R1", 0x0C, 0x0200, 0 }, { "R2", "R2", 0x0E, 0, 2 },
    { "Start",  "Start",  0x18, 0x0010, 0 }, { "Select", "Select", 0x1A, 0x0020, 0 },
    { "Square", "Square", 0x1C, 0x4000, 0 }, { "Triangle", "Triangle", 0x1E, 0x8000, 0 },
    { "Cross",  "Cross",  0x20, 0x1000, 0 }, { "Circle",   "Circle",   0x22, 0x2000, 0 },
    { "L3", "L3", 0x24, 0x0040, 0 }, { "R3", "R3", 0x26, 0x0080, 0 },
};

static bool  g_enabled   = true;
static int   g_vk        = 'R';        // 0 = desativado
static char  g_keyName[32] = "R";
static int   g_cancelVk  = 'N';
static char  g_cancelKeyName[32] = "N";
static int   g_cancelOffset = 0x14;
static uint16_t g_cancelXMask = 0x0004;
static int   g_cancelXTrig = 0;
static char  g_cancelPadName[32] = "Dpad Left";
static bool  g_cancelOnEsc = true;
static int   g_backOffset = 0x1E;      // Triangulo / Y do controle
static uint16_t g_backXMask = 0x8000;
static int   g_backXTrig = 0;
static char  g_backPadName[32] = "Triangle";
static int   g_padOffset = 0x16;       // -1 = desativado
static uint16_t g_padXMask = 0x0008;   // mascara do XInput do botao escolhido
static int   g_padXTrig = 0;
static int   g_padSource = 0;          // 0 = Auto, 1 = XInput, 2 = Game (CPad)
static char  g_padName[32] = "Dpad Right";
static char  g_message[256] = "Retry?";
static char  g_padLabel[64] = "";     // se preenchido, substitui o nome do botao em {PAD}
static bool  g_restoreWeapons = true, g_restoreHealth = true, g_restoreArmour = true, g_restoreMoney = true, g_restoreWanted = true, g_restoreTimeWeather = true, g_restoreClothes = true, g_restoreStats = true;
static float g_promptDelaySec = 3.0f;   // segundos entre a falha da missao e o menu Retry?
static int   g_sCodeSmall = 0x9A, g_sCodeCap = 0x8A;       // codigos do jogo para s e S com caron
static float g_msgY      = 47.0f;      // % da altura da tela (posicao do topo do texto)
static float g_msgOffX = 0.0f, g_msgOffY = 0.0f;   // pixels
static uint32_t g_titleColor = 0xFFEEC8B8u;     // ABGR
static uint32_t g_selColor   = 0xFFFFE0D2u;     // opcao selecionada
static uint32_t g_normColor  = 0xFF9C7F6Eu;     // opcao nao selecionada
static char     g_yesText[64] = "Yes";
static char     g_noText[64]  = "No";
static float    g_lineSpacing = 1.0f;
static bool     g_ownCursor   = true;
static bool     g_hideCursorWithPad = true;
static float g_scaleMul  = 1.0f;

static char     g_iniPath[MAX_PATH];
static FILETIME g_lastWrite = {0, 0};
static DWORD    g_lastCheck = 0;

static bool g_logEnabled = false;      // EnableLog=1 no .ini liga o arquivo de log

static void Log(const char* fmt, ...)
{
    if (!g_logEnabled) return;
    FILE* f = fopen("RetryMissionPause.log", "a");
    if (!f) return;
    va_list ap; va_start(ap, fmt);
    vfprintf(f, fmt, ap); va_end(ap);
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

static int ParseKey(const char* s)
{
    if (!s || !s[0]) return 0;
    char u[32]; size_t n = strlen(s); if (n >= sizeof(u)) n = sizeof(u) - 1;
    for (size_t i = 0; i < n; ++i) u[i] = (char)toupper((unsigned char)s[i]);
    u[n] = 0;
    if (!strcmp(u, "NONE") || !strcmp(u, "OFF")) return 0;
    if (n == 1 && ((u[0] >= 'A' && u[0] <= 'Z') || (u[0] >= '0' && u[0] <= '9'))) return u[0];
    if (u[0] == 'F' && n >= 2 && n <= 3) { int k = atoi(u + 1); if (k >= 1 && k <= 24) return VK_F1 + k - 1; }
    if (!strncmp(u, "NUM", 3) && n == 4 && u[3] >= '0' && u[3] <= '9') return VK_NUMPAD0 + (u[3] - '0');
    struct { const char* n; int vk; } names[] = {
        {"ENTER",VK_RETURN},{"SPACE",VK_SPACE},{"TAB",VK_TAB},{"BACKSPACE",VK_BACK},{"INSERT",VK_INSERT},
        {"DELETE",VK_DELETE},{"HOME",VK_HOME},{"END",VK_END},{"PAGEUP",VK_PRIOR},{"PAGEDOWN",VK_NEXT},
        {"UP",VK_UP},{"DOWN",VK_DOWN},{"LEFT",VK_LEFT},{"RIGHT",VK_RIGHT},{"SHIFT",VK_SHIFT},
        {"CTRL",VK_CONTROL},{"ALT",VK_MENU},{"CAPSLOCK",VK_CAPITAL},
    };
    for (auto& e : names) if (!strcmp(u, e.n)) return e.vk;
    char* end = nullptr; long v = strtol(s, &end, 0);   // aceita 0x52 ou 82
    if (end != s && v > 0 && v < 256) return (int)v;
    return 0;
}

static void ParsePad(const char* s, int& offset, uint16_t& xmask, int& xtrig, char* display, size_t dsize)
{
    offset = -1; xmask = 0; xtrig = 0; display[0] = 0;
    if (!s || !s[0] || !_stricmp(s, "none") || !_stricmp(s, "off")) return;
    for (auto& b : PAD_BUTTONS)
        if (!_stricmp(s, b.name)) { offset = b.offset; xmask = b.xmask; xtrig = b.xtrig; strncpy(display, b.display, dsize - 1); display[dsize - 1] = 0; return; }
    Log("PadButton desconhecido no ini: %s", s);
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

    // Nao ha mais atalhos (R, N, direcional esquerdo/direito): so navegar e confirmar no menu.
    g_vk = 0;            g_padOffset = -1;    g_padXMask = 0;        g_padXTrig = 0;
    g_cancelVk = 0;      g_cancelOffset = -1; g_cancelXMask = 0;     g_cancelXTrig = 0;
    g_cancelOnEsc = GetPrivateProfileIntA("Retry", "CancelOnEscape", 1, g_iniPath) != 0;
    GetPrivateProfileStringA("Retry", "BackPadButton", "Triangle", buf, sizeof(buf), g_iniPath);
    ParsePad(buf, g_backOffset, g_backXMask, g_backXTrig, g_backPadName, sizeof(g_backPadName));

    GetPrivateProfileStringA("Retry", "Message", "Retry?", g_message, sizeof(g_message), g_iniPath);
    GetPrivateProfileStringA("Retry", "PadLabel", "", g_padLabel, sizeof(g_padLabel), g_iniPath);
    g_restoreWeapons     = GetPrivateProfileIntA("Retry", "RestoreWeapons", 1, g_iniPath) != 0;
    g_restoreHealth      = GetPrivateProfileIntA("Retry", "RestoreHealth", 1, g_iniPath) != 0;
    g_restoreArmour      = GetPrivateProfileIntA("Retry", "RestoreArmour", 1, g_iniPath) != 0;
    g_restoreTimeWeather = GetPrivateProfileIntA("Retry", "RestoreTimeWeather", 1, g_iniPath) != 0;
    GetPrivateProfileStringA("Retry", "PadSource", "Auto", buf, sizeof(buf), g_iniPath);
    g_padSource = !_stricmp(buf, "XInput") ? 1 : (!_stricmp(buf, "Game") ? 2 : 0);
    g_restoreMoney       = GetPrivateProfileIntA("Retry", "RestoreMoney", 1, g_iniPath) != 0;
    g_restoreWanted      = GetPrivateProfileIntA("Retry", "RestoreWanted", 1, g_iniPath) != 0;
    g_promptDelaySec     = ReadFloat("PromptDelaySeconds", 3.0f); if (g_promptDelaySec < 0.0f) g_promptDelaySec = 0.0f;
    g_restoreClothes     = GetPrivateProfileIntA("Retry", "RestoreClothes", 1, g_iniPath) != 0;
    g_restoreStats       = GetPrivateProfileIntA("Retry", "RestoreStats", 1, g_iniPath) != 0;
    { char b2[32];
      GetPrivateProfileStringA("Retry", "SmallSCode", "0x9A", b2, sizeof(b2), g_iniPath);  g_sCodeSmall = (int)strtol(b2, nullptr, 0);
      GetPrivateProfileStringA("Retry", "CapitalSCode", "0x8A", b2, sizeof(b2), g_iniPath); g_sCodeCap = (int)strtol(b2, nullptr, 0); }
    g_msgY     = ReadFloat("MessageY", 47.0f);
    g_msgOffX  = ReadFloat("MessageOffsetX", 0.0f);
    g_msgOffY  = ReadFloat("MessageOffsetY", 0.0f);
    g_titleColor = ReadColor("TitleColor", "B8C8EE");
    g_selColor   = ReadColor("SelectedColor", "D2E0FF");
    g_normColor  = ReadColor("NormalColor", "6E7F9C");
    GetPrivateProfileStringA("Retry", "YesText", "Yes", g_yesText, sizeof(g_yesText), g_iniPath);
    GetPrivateProfileStringA("Retry", "NoText", "No", g_noText, sizeof(g_noText), g_iniPath);
    g_lineSpacing = ReadFloat("LineSpacing", 1.0f);
    if (g_lineSpacing <= 0.0f) g_lineSpacing = 1.0f;
    g_ownCursor = GetPrivateProfileIntA("Retry", "ShowMouseCursor", 1, g_iniPath) != 0;
    g_hideCursorWithPad = GetPrivateProfileIntA("Retry", "HideCursorWithController", 1, g_iniPath) != 0;
    g_scaleMul = ReadFloat("TextScale", 1.0f);
    if (g_scaleMul <= 0.0f) g_scaleMul = 1.0f;
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

// ======================= Acesso ao jogo =======================
static inline uint8_t* PlayerPed()
{
    uint8_t focus = *(uint8_t*)PLAYER_FOCUS;
    return *(uint8_t**)(PLAYERS_BASE + (uintptr_t)focus * PLAYER_STRIDE);
}

static bool PedPosition(uint8_t* ped, float out[3])
{
    uint8_t* matrix = *(uint8_t**)(ped + 0x14);          // CMatrix* (pos em +0x30), senao placement em +0x4
    const float* p = matrix ? (const float*)(matrix + 0x30) : (const float*)(ped + 0x4);
    out[0] = p[0]; out[1] = p[1]; out[2] = p[2];
    return true;
}

static bool IsOnMission()
{
    uint32_t off = *(uint32_t*)ADDR_MISSION_OFF;
    return off != 0 && *(int32_t*)(ADDR_SCRIPT_SPACE + off) == 1;
}

static bool IsCutscene()
{
    return *(uint8_t*)ADDR_CUTSCENE_A != 0 || *(uint8_t*)ADDR_CUTSCENE_B != 0;
}

// ---- XInput (carregado dinamicamente; se nao existir, so usa o pad do jogo) ----
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

static bool GameIsForeground()
{
    HWND fg = GetForegroundWindow();
    DWORD pid = 0; if (fg) GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

static bool g_lastPadViaX = false;
static DWORD g_lastKeyTick = 0;
static const DWORD KEY_GRACE_MS = 600;

// true se alguma tecla do teclado esta apertada (ignora mouse e botoes de controle reportados como tecla)
static bool AnyKeyboardKeyDown()
{
    for (int vk = 8; vk < 255; ++vk)
    {
        if (vk >= 0xC3 && vk <= 0xDA) continue;     // VK_GAMEPAD_*
        if (GetAsyncKeyState(vk) & 0x8000) return true;
    }
    return false;
}

// true enquanto o botao escolhido (e SO ele) esta apertado
static bool PadDown(int padOffset, uint16_t xmask, int xtrig)
{
    if (padOffset < 0) return false;

    // 1) direto do XInput: nao sofre interferencia de outros botoes/teclas mapeados no jogo
    if (g_padSource != 2)
    {
        XGamepad xg;
        if (ReadXInput(xg))
        {
            g_lastPadViaX = true;
            if (!GameIsForeground()) return false;
            if (xtrig == 1) return xg.bLT > 100;
            if (xtrig == 2) return xg.bRT > 100;
            return xmask && (xg.wButtons & xmask) != 0;
        }
        if (g_padSource == 1) return false;     // XInput obrigatorio e nao ha controle
    }
    g_lastPadViaX = false;

    // 2) pelo pad do jogo (GInput injeta o estado aqui). Exige que nenhum OUTRO botao esteja apertado,
    //    para nao disparar por engano quando outro botao/tecla mexe no estado do pad.
    // O jogo tambem traduz teclas do teclado para o estado do pad (ex.: a tecla Y), e o pad enxerga a tecla
    // soltar um frame DEPOIS do teclado. Por isso o estado do pad so vale como "controle" se nenhuma tecla do
    // teclado estiver apertada nem tiver sido apertada ha menos de KEY_GRACE_MS.
    const DWORD tnow = GetTickCount();
    if (AnyKeyboardKeyDown()) g_lastKeyTick = tnow;
    if (*(int16_t*)(PAD0_ADDR + padOffset) == 0) return false;
    if (tnow - g_lastKeyTick < KEY_GRACE_MS) return false;
    for (const PadButton& b : PAD_BUTTONS)
    {
        if (b.offset == padOffset) continue;
        if (b.offset >= 0x10 && *(int16_t*)(PAD0_ADDR + b.offset) != 0) return false;   // direcionais, botoes, start/select, L3/R3
    }
    return true;
}

static bool KeyPressed(int vk)
{
    if (!vk) return false;
    HWND fg = GetForegroundWindow();
    DWORD pid = 0; if (fg) GetWindowThreadProcessId(fg, &pid);
    if (pid != GetCurrentProcessId()) return false;       // so quando o jogo esta em foco
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

// ---- qual dispositivo o jogador esta usando (como o GInput): controle ou teclado/mouse ----
static bool  g_usingPad = false;
static long  g_trkMouseX = -100000, g_trkMouseY = -100000;

static bool MouseButtonsDown()
{
    return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) || (GetAsyncKeyState(VK_RBUTTON) & 0x8000) || (GetAsyncKeyState(VK_MBUTTON) & 0x8000);
}

// Como o GInput: o cursor aparece quando voce usa o mouse e some quando voce usa o controle (e, no menu de
// Retry, tambem as setas do teclado). Vale o que aconteceu por ultimo.
static void TrackInputDevice(bool inMenu)
{
    if (!GameIsForeground()) return;

    bool padAct = false;
    XGamepad xg;
    if (ReadXInput(xg))
    {
        padAct = xg.wButtons != 0 || xg.bLT > 40 || xg.bRT > 40 ||
                 abs(xg.lx) > 14000 || abs(xg.ly) > 14000 || abs(xg.rx) > 14000 || abs(xg.ry) > 14000;
    }
    else if (g_padSource != 1)
    {
        // controle sem XInput (GInput/DirectInput): so botoes do pad do jogo, e nunca perto de uma tecla do teclado
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

    if (mouseAct)                    g_usingPad = false;      // mouse: cursor aparece
    else if (padAct || arrowAct)     g_usingPad = true;       // controle ou setas: cursor some
}

// Analogico ESQUERDO na vertical: -1 = cima, +1 = baixo, 0 = neutro
static int LeftStickVertical()
{
    if (g_padSource != 2)
    {
        XGamepad xg;
        if (ReadXInput(xg))
        {
            if (!GameIsForeground()) return 0;
            if (xg.ly >  16000) return -1;           // XInput: positivo = para cima
            if (xg.ly < -16000) return  1;
            return 0;
        }
        if (g_padSource == 1) return 0;
    }
    // pad do jogo (GInput): LeftStickY vai de -128 a 127, negativo = cima. Ignorado perto de teclas (W/S viram analogico).
    const DWORD tnow = GetTickCount();
    if (AnyKeyboardKeyDown()) g_lastKeyTick = tnow;
    if (tnow - g_lastKeyTick < KEY_GRACE_MS) return 0;
    const int v = *(int16_t*)(PAD0_ADDR + 0x02);
    if (v < -64) return -1;
    if (v >  64) return  1;
    return 0;
}

// ---- impede que o que foi apertado no menu "vaze" para a gameplay (ex.: clique de confirmar = soco) ----
static bool  g_suppress = false;
static DWORD g_suppressStart = 0, g_suppressLastHeld = 0;

static bool MenuInputsHeld()
{
    static const int vks[] = { VK_UP, VK_DOWN, VK_RETURN, VK_ESCAPE, VK_SPACE, VK_LBUTTON, VK_RBUTTON };
    for (int vk : vks) if (GetAsyncKeyState(vk) & 0x8000) return true;
    if (g_vk && (GetAsyncKeyState(g_vk) & 0x8000)) return true;
    if (g_cancelVk && (GetAsyncKeyState(g_cancelVk) & 0x8000)) return true;
    XGamepad xg;
    if (ReadXInput(xg) && (xg.wButtons != 0 || xg.bLT > 40 || xg.bRT > 40)) return true;
    return false;
}

static void BeginSuppress()
{
    g_suppress = true; g_suppressStart = g_suppressLastHeld = GetTickCount();
}

typedef void (__cdecl *UpdatePads_t)();
static UpdatePads_t g_origUpdatePads = nullptr;

// Logo depois de o jogo ler os controles: enquanto a supressao esta ativa, zera o estado do pad
// (NewState e OldState), assim nada do que foi apertado no menu chega ao jogador.
static void __cdecl Hook_UpdatePads()
{
    g_origUpdatePads();
    if (!g_suppress) return;
    const DWORD now = GetTickCount();
    if (MenuInputsHeld()) g_suppressLastHeld = now;
    if (now - g_suppressStart > 3000 || (now - g_suppressStart > 250 && now - g_suppressLastHeld > 200))
    {
        g_suppress = false;
        Log("Entrada liberada para a gameplay.");
        return;
    }
    memset((void*)PAD0_ADDR, 0, 0x60);
}

static bool StrEquals(const char* a, const char* b)
{
    if (!a || !b) return false;
    return strcmp(a, b) == 0;
}

// ======================= Estado do mod =======================
static bool     g_haveSaved   = false;
static bool     g_prevOnMission = false;
static DWORD    g_failTime    = 0;    // ultima vez que "mission failed" foi detectado (timer do jogo)
static bool     g_failSeen    = false;
static DWORD    g_missionEndTime = 0;
static bool     g_pending     = false;  // falha detectada, esperando o jogador voltar a jogar
static DWORD    g_lastBusy    = 0;
static bool     g_prompting   = false;
static DWORD    g_promptStart = 0;
static bool     g_prevKey = false, g_prevPad = false;
static bool     g_prevCKey = false, g_prevCPad = false, g_prevEsc = false;

// Fases da pausa do aviso
enum Phase { PH_NONE = 0, PH_REQUEST, PH_ACTIVE, PH_CLOSING };
static Phase    g_phase = PH_NONE;
static DWORD    g_phaseTick = 0;          // GetTickCount (tempo real; o timer do jogo para na pausa)
static bool     g_acceptAfterClose = false;
static bool     g_teleportPending = false;
static bool     g_forcedActivate = false;

// Foto do estado do jogo (armas, vida, colete, procurado, hora, clima, posicao)
struct WeaponSnap { int32_t type, ammoInClip, totalAmmo; };
struct StateSnap
{
    bool       valid;
    float      pos[3];
    int32_t    area;
    float      health, armour;
    int32_t    money;
    WeaponSnap w[WEAPON_SLOTS];
    int8_t     activeSlot;
    int32_t    wantedChaos, wantedLevel;
    uint8_t    hours, minutes;
    int16_t    seconds;
    int16_t    weatherOld, weatherNew, weatherForced;
    float      weatherInterp;
    bool       hasClothes;
    uint8_t    clothes[CLOTHES_DESC_SIZE];
    uint8_t    stats[STATS_REGION_SIZE];
};
static StateSnap g_rolling = {};   // foto continua, atualizada a cada frame enquanto NAO ha missao
static StateSnap g_snap    = {};   // foto "oficial": a do momento em que a missao comecou

static uint8_t* PlayerInfo()
{
    uint8_t focus = *(uint8_t*)PLAYER_FOCUS;
    return (uint8_t*)(PLAYERS_BASE + (uintptr_t)focus * PLAYER_STRIDE);
}

static uint8_t* PlayerWanted()
{
    uint8_t focus = *(uint8_t*)PLAYER_FOCUS;
    return *(uint8_t**)(PLAYERS_BASE + 4 + (uintptr_t)focus * PLAYER_STRIDE);
}

static void TakeSnapshot(uint8_t* ped, StateSnap& o)
{
    PedPosition(ped, o.pos);
    o.area   = *(int32_t*)ADDR_CURR_AREA;
    o.health = *(float*)(ped + PED_HEALTH_OFF);
    o.armour = *(float*)(ped + PED_ARMOUR_OFF);
    o.money  = *(int32_t*)(PlayerInfo() + PLAYER_MONEY_OFF);
    for (int i = 0; i < WEAPON_SLOTS; ++i)
    {
        const int32_t* w = (const int32_t*)(ped + PED_WEAPONS_OFF + i * 0x1C);
        o.w[i].type = w[0]; o.w[i].ammoInClip = w[2]; o.w[i].totalAmmo = w[3];
    }
    o.activeSlot = *(int8_t*)(ped + PED_ACTIVE_SLOT);
    uint8_t* wanted = PlayerWanted();
    o.wantedChaos = wanted ? *(int32_t*)(wanted + WANTED_CHAOS_OFF) : 0;
    o.wantedLevel = wanted ? *(int32_t*)(wanted + WANTED_LEVEL_OFF) : 0;
    o.hours   = *(uint8_t*)ADDR_CLOCK_HOURS;
    o.minutes = *(uint8_t*)ADDR_CLOCK_MINUTES;
    o.seconds = *(int16_t*)ADDR_CLOCK_SECONDS;
    o.weatherOld    = *(int16_t*)ADDR_WEATHER_OLD;
    o.weatherNew    = *(int16_t*)ADDR_WEATHER_NEW;
    o.weatherForced = *(int16_t*)ADDR_WEATHER_FORCED;
    o.weatherInterp = *(float*)ADDR_WEATHER_INTERP;
    o.hasClothes = false;
    uint8_t* pdata = *(uint8_t**)(ped + PED_PLAYERDATA_OFF);
    uint8_t* desc  = pdata ? *(uint8_t**)(pdata + PLAYERDATA_CLOTHES_OFF) : nullptr;
    if (desc) { memcpy(o.clothes, desc, CLOTHES_DESC_SIZE); o.hasClothes = true; }
    memcpy(o.stats, (const void*)STATS_REGION_ADDR, STATS_REGION_SIZE);
    o.valid = true;
}

static void LogSnapshot(const char* title, const StateSnap& o)
{
    Log("%s: vida=%.1f colete=%.1f dinheiro=%d procurado=%d (caos %d) hora=%02d:%02d clima=%d/%d slot=%d",
        title, o.health, o.armour, o.money, o.wantedLevel, o.wantedChaos, o.hours, o.minutes,
        o.weatherOld, o.weatherNew, (int)o.activeSlot);
    for (int i = 0; i < WEAPON_SLOTS; ++i)
        if (o.w[i].type > 0)
            Log("   slot %d: arma %d, municao %d (pente %d)", i, o.w[i].type, o.w[i].totalAmmo, o.w[i].ammoInClip);
}

// Aplica as armas/vida/colete/procurado salvos. Devolve true se algo precisou ser corrigido.
static void ApplyPlayerState(uint8_t* ped, bool firstTime)
{
    const StateSnap& S = g_snap;

    // Roupa, cabelo e tatuagens (e, se ligado, stats/skills): restaura antes das armas, porque reconstruir o
    // personagem recria o modelo.
    if (firstTime && (g_restoreClothes || g_restoreStats))
    {
        if (g_restoreStats) { memcpy((void*)STATS_REGION_ADDR, S.stats, STATS_REGION_SIZE); Log("   stats/skills restaurados."); }
        uint8_t* pdata = *(uint8_t**)(ped + PED_PLAYERDATA_OFF);
        uint8_t* desc  = pdata ? *(uint8_t**)(pdata + PLAYERDATA_CLOTHES_OFF) : nullptr;
        if (g_restoreClothes && desc && S.hasClothes) { memcpy(desc, S.clothes, CLOTHES_DESC_SIZE); Log("   roupa, cabelo e tatuagens restaurados."); }
        ((void (__cdecl*)(void*, int))FN_REBUILD_PLAYER)(ped, 0);       // reconstroi o personagem com o que foi restaurado
    }

    if (g_restoreWeapons)
    {
        if (firstTime)
        {
            ((PedVoid_t)FN_PED_CLEAR_WEAPONS)(ped);
            for (int i = 0; i < WEAPON_SLOTS; ++i)
            {
                const WeaponSnap& ws = S.w[i];
                if (ws.type <= 0) continue;
                ((PedGiveWeapon_t)FN_PED_GIVE_WEAPON)(ped, ws.type, (uint32_t)ws.totalAmmo, 1);
            }
            if (S.activeSlot >= 0 && S.activeSlot < WEAPON_SLOTS)
                ((PedSetSlot_t)FN_PED_SET_CUR_WEAPON)(ped, S.activeSlot);
        }
        // municao exata (pente e total), conferida a cada chamada
        for (int i = 0; i < WEAPON_SLOTS; ++i)
        {
            const WeaponSnap& ws = S.w[i];
            if (ws.type <= 0) continue;
            int32_t* w = (int32_t*)(ped + PED_WEAPONS_OFF + i * 0x1C);
            if (w[0] != ws.type)
            {
                ((PedGiveWeapon_t)FN_PED_GIVE_WEAPON)(ped, ws.type, (uint32_t)ws.totalAmmo, 1);
                if (!firstTime) Log("   arma %d tinha sumido do slot %d; entregue de novo.", ws.type, i);
            }
            if (w[0] == ws.type && (w[2] != ws.ammoInClip || w[3] != ws.totalAmmo))
            {
                if (!firstTime) Log("   municao do slot %d mudou (%d/%d); corrigindo para %d/%d.", i, w[3], w[2], ws.totalAmmo, ws.ammoInClip);
                w[2] = ws.ammoInClip;
                w[3] = ws.totalAmmo;
                if (ws.totalAmmo > 0 && w[1] == 3) w[1] = 0;     // 3 = sem municao
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
            *money = S.money;
            *display = S.money;      // o valor mostrado na tela muda na hora, sem contagem
            *(int32_t*)ADDR_HUD_LAST_MONEY = S.money;   // o HUD nao precisa animar a mudanca
        }
    }
    if (g_restoreWanted)
    {
        uint8_t* wanted = PlayerWanted();
        if (wanted)
        {
            int32_t* chaos = (int32_t*)(wanted + WANTED_CHAOS_OFF);
            int32_t* level = (int32_t*)(wanted + WANTED_LEVEL_OFF);
            if (firstTime || *level != S.wantedLevel)
            {
                const int before = *level, chaosBefore = *chaos;
                // funcao oficial do jogo: ajusta o caos e LIMPA a fila de crimes pendentes (que fazia as estrelas voltarem)
                ((WantedSet_t)FN_WANTED_SET)(wanted, S.wantedLevel);
                if (*level != S.wantedLevel)
                {
                    // o jogo recusou (nivel maximo menor ou cheat "nunca procurado"): mantem o que o jogo definiu
                    Log("   procurado: pediu %d, ficou %d (maximo permitido %d, 'nunca procurado' %d).", S.wantedLevel, *level,
                        *(int32_t*)ADDR_WANTED_MAXLVL, (int)*(uint8_t*)ADDR_WANTED_NEVER);
                }
                if (firstTime || before != *level)
                    Log("   procurado: %d (caos %d) -> %d (caos %d); salvo era %d.", before, chaosBefore, *level, *chaos, S.wantedLevel);
            }
        }
    }
}

static void ApplyWorldState(bool firstTime)
{
    const StateSnap& S = g_snap;
    if (firstTime)
    {
        ((SetClock_t)FN_SET_GAME_CLOCK)(S.hours, S.minutes, 0);
        *(int16_t*)ADDR_CLOCK_SECONDS = S.seconds;
    }
    // Imediato, como um cheat de clima (que chama ForceWeatherNow), mas sem registrar como cheat: o clima que estava
    // visivel quando a missao comecou (o antigo ou o novo, conforme a interpolacao) vira o clima atual nas duas variaveis,
    // sem transicao gradual. O clima forcado volta ao que era (normalmente nenhum).
    const int16_t type = (S.weatherInterp >= 0.5f) ? S.weatherNew : S.weatherOld;
    *(int16_t*)ADDR_WEATHER_OLD    = type;
    *(int16_t*)ADDR_WEATHER_NEW    = type;
    *(int16_t*)ADDR_WEATHER_FORCED = S.weatherForced;
    *(float*)ADDR_WEATHER_INTERP   = 0.0f;
}

// ---- protecao do HUD: o contador de dinheiro (e os outros itens) nao pode ficar escondido por causa da pausa ----
struct HudSnap { int32_t v[4][3]; bool valid; };
static HudSnap g_hudSaved = {};
static DWORD   g_hudGuardUntil = 0;

static void SaveHud()
{
    for (int i = 0; i < 4; ++i)
        for (int k = 0; k < 3; ++k) g_hudSaved.v[i][k] = *(int32_t*)(HUD_ITEM_BASES[i] + k * 4);
    g_hudSaved.valid = true;
    Log("HUD salvo: dinheiro estado=%d (tempos %d/%d).", g_hudSaved.v[2][0], g_hudSaved.v[2][1], g_hudSaved.v[2][2]);
}

static void GuardHud()
{
    if (!g_hudSaved.valid) return;
    for (int i = 0; i < 4; ++i)
    {
        int32_t* cur = (int32_t*)HUD_ITEM_BASES[i];
        if (cur[0] == 0 && g_hudSaved.v[i][0] != 0)       // ficou escondido, mas antes estava aparecendo
        {
            Log("HUD: item %d tinha sumido (estado 0); restaurando estado %d.", i, g_hudSaved.v[i][0]);
            for (int k = 0; k < 3; ++k) cur[k] = g_hudSaved.v[i][k];
        }
    }
}

// por algum tempo depois de recomecar, confere os valores de novo (algo do jogo pode mexer neles)
static bool  g_reapply = false;
static DWORD g_reapplyUntil = 0;

static void StartPrompt(DWORD /*now*/)
{
    g_prompting = true; g_pending = false;
    g_phase = PH_REQUEST; g_phaseTick = GetTickCount(); g_forcedActivate = false;
    if (!*(uint8_t*)ADDR_MENU_ACTIVE) ((VoidFn_t)FN_REQUEST_PAUSE)();     // pede a pausa (como apertar Esc)
    Log("Pausa solicitada para mostrar o aviso.");
}

// Tira o jogador do veiculo usando a mesma tarefa que o jogo usa (CTaskSimpleCarSetPedOut::SetPedOut), do mesmo jeito que
// o proprio jogo faz ao mover o jogador para outro lugar estando dentro de um carro. Sem isso o Teleport nao funciona.
static bool LeaveVehicle(uint8_t* ped)
{
    if (!(*(uint32_t*)(ped + 0x46C) & 0x100)) return false;         // bInVehicle
    void* veh = *(void**)(ped + 0x58C);                              // m_pVehicle
    if (!veh) return false;
    alignas(16) uint8_t task[0x40] = {};                             // objeto CTaskSimpleCarSetPedOut na pilha
    ((void (__thiscall*)(void*, void*, int, int))0x6478B0)(task, veh, 0, 1);   // construtor(veiculo, porta, true)
    task[0x11] = 1;
    ((void (__thiscall*)(void*, void*))0x647D10)(task, ped);        // SetPedOut(ped)
    ((void (__thiscall*)(void*))0x647950)(task);                    // destrutor (solta a referencia ao veiculo)
    return true;
}

static void DoTeleport(uint8_t* ped)
{
    const StateSnap& S = g_snap;
    if (LeaveVehicle(ped)) Log("Jogador estava em um veiculo: removido antes de teleportar.");
    *(int32_t*)ADDR_CURR_AREA = S.area;
    *(uint8_t*)(ped + ENTITY_AREA_OFF) = (uint8_t)S.area;
    ((LoadScene_t)FN_LOAD_SCENE)(S.pos);
    ((PedTeleport_t)FN_PED_TELEPORT)(ped, S.pos[0], S.pos[1], S.pos[2], 0);

    LogSnapshot("Antes de restaurar (estado atual)", [&]{ StateSnap cur = {}; TakeSnapshot(ped, cur); return cur; }());
    ApplyPlayerState(ped, true);
    if (g_restoreTimeWeather) ApplyWorldState(true);
    g_reapply = true;
    g_reapplyUntil = GetTickCount() + 2500;
    g_hudGuardUntil = GetTickCount() + 6000;
    Log("Teleportado para %.1f %.1f %.1f (area %d) e estado restaurado.", S.pos[0], S.pos[1], S.pos[2], S.area);
    LogSnapshot("Estado salvo no marcador", S);
}

// Chamado uma vez por frame de logica (dentro de CGame::Process, logo apos CGameLogic::Update)
static void Tick()
{
    CheckReload();
    if (!g_enabled) return;

    TrackInputDevice(false);
    if (g_hudGuardUntil && GetTickCount() < g_hudGuardUntil) GuardHud();

    uint8_t* ped = PlayerPed();
    if (!ped) return;

    const DWORD now = *(uint32_t*)ADDR_TIMER_MS;
    const bool onMission = IsOnMission();
    const uint8_t gameState = *(uint8_t*)ADDR_GAME_STATE;
    const int32_t pedState = *(int32_t*)(ped + PED_STATE_OFF);
    const bool pedDown = (pedState == 0x36 || pedState == 0x37 || pedState == 0x3F);
    const bool cutscene = IsCutscene();

    // Enquanto NAO ha missao e o jogador esta bem, mantem uma foto atualizada do estado.
    // (assim, no instante em que a missao comeca, usamos a foto do frame anterior, antes de o
    // script da missao mexer em armas, vida, procurado etc.)
    if (!onMission && !cutscene && gameState == 0 && !pedDown && *(float*)(ped + PED_HEALTH_OFF) > 0.5f)
        TakeSnapshot(ped, g_rolling);

    // Missao comecou: grava o estado de quando o jogador pisou no marcador
    if (onMission && !g_prevOnMission)
    {
        if (g_rolling.valid) g_snap = g_rolling;
        else                 TakeSnapshot(ped, g_snap);
        g_haveSaved = true;
        g_pending = false; g_prompting = false; g_failSeen = false; g_reapply = false;
        Log("Missao iniciada. Marcador salvo: %.1f %.1f %.1f (area %d).", g_snap.pos[0], g_snap.pos[1], g_snap.pos[2], g_snap.area);
        LogSnapshot("Estado salvo", g_snap);
    }

    // Logo depois de recomecar: confere por 1,5 s se o estado restaurado continua certo
    if (g_reapply)
    {
        if ((int32_t)(GetTickCount() - g_reapplyUntil) > 0) { g_reapply = false; Log("Conferencia pos-restauracao encerrada."); }
        else
        {
            ApplyPlayerState(ped, false);
            if (g_restoreTimeWeather) ApplyWorldState(false);
        }
    }

    // Morreu / foi preso durante a missao conta como falha
    if (onMission && (gameState != 0 || pedDown))
    {
        g_failSeen = true; g_failTime = now;
    }

    // Missao terminou
    if (!onMission && g_prevOnMission)
    {
        g_missionEndTime = now;
        if (g_failSeen && now - g_failTime < 8000 && g_haveSaved)
        {
            g_pending = true;
            Log("Missao terminou com falha.");
        }
        else
        {
            Log("Missao terminou sem falha (ou sem marcador salvo).");
        }
    }
    g_prevOnMission = onMission;

    // Mensagem "MISSION FAILED" chegou depois do fim da missao (ou durante): ver hook abaixo
    if (!onMission && g_failSeen && g_haveSaved && !g_pending && !g_prompting &&
        now - g_failTime < 8000 && now - g_missionEndTime < 8000)
        g_pending = true;

    if (gameState != 0 || pedDown || cutscene || onMission)
        g_lastBusy = now;

    // Mostra o aviso quando o jogador voltou a controlar o personagem
    if (g_pending && !g_prompting && now - g_lastBusy >= 1000 && now - g_failTime >= (DWORD)(g_promptDelaySec * 1000.0f))
    {
        StartPrompt(now);
        g_failSeen = false;
    }

    // Fase de pedido (ainda nao pausou): cancela se algo atrapalhar
    if (g_phase == PH_REQUEST && (onMission || cutscene || gameState != 0 || pedDown))
    {
        g_phase = PH_NONE; g_prompting = false;
        Log("Aviso cancelado antes de pausar.");
    }

    // Depois que o menu fechou com "sim": teleporta e restaura
    if (g_teleportPending && !*(uint8_t*)ADDR_MENU_ACTIVE)
    {
        g_teleportPending = false;
        DoTeleport(ped);
    }
}

// ======================= Texto na tela =======================
static void ExpandMessage(char* out, size_t outSize)
{
    size_t len = 0; out[0] = 0;
    auto append = [&](const char* t) {
        size_t n = strlen(t);
        if (len + n < outSize) { memcpy(out + len, t, n); len += n; out[len] = 0; }
    };
    for (const char* m = g_message; *m; )
    {
        if (!strncmp(m, "{KEY}", 5))      { append(g_vk ? g_keyName : "-");                 m += 5; }
        else if (!strncmp(m, "{PAD}", 5)) { append(g_padOffset < 0 ? "-" : (g_padLabel[0] ? g_padLabel : g_padName)); m += 5; }
        else                              { char c[2] = { *m, 0 }; append(c);                ++m;   }
    }
}

// Converte o texto do .ini (UTF-8 ou ANSI) para os codigos de caractere do jogo. "s" e "S" com caron (U+0161/U+0160)
// viram SmallSCode/CapitalSCode; as demais letras acentuadas viram a letra simples.
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

static void ToGameText(const char* s, char* out, size_t outCount)
{
    size_t o = 0;
    while (*s && o + 1 < outCount)
    {
        const unsigned char c = (unsigned char)*s;
        unsigned cp = 0; int len = 0;
        if (c < 0x80) { out[o++] = (char)c; ++s; continue; }
        if (c >= 0xC2 && c <= 0xDF && (s[1] & 0xC0) == 0x80)                                   { cp = ((c & 0x1F) << 6) | (s[1] & 0x3F); len = 2; }
        else if (c >= 0xE0 && c <= 0xEF && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80)     { cp = ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); len = 3; }
        unsigned code;
        if (len) { code = (cp == 0x161) ? (unsigned)g_sCodeSmall : (cp == 0x160) ? (unsigned)g_sCodeCap : StripAccent(cp); s += len; }
        else     { code = (c == 0x9A) ? (unsigned)g_sCodeSmall : (c == 0x8A) ? (unsigned)g_sCodeCap : StripAccent(c); ++s; }   // ANSI: 0x9A/0x8A
        out[o++] = (char)code;
    }
    out[o] = 0;
}

struct RectF { float left, bottom, right, top; };    // CRect do jogo (bottom = y maior)
struct Rgba8 { uint8_t r, g, b, a; };

// ---- mouse (coordenadas do jogo) ----
static bool GetMouseGame(float& mx, float& my)
{
    HWND fg = GetForegroundWindow();
    DWORD pid = 0; if (fg) GetWindowThreadProcessId(fg, &pid);
    if (!fg || pid != GetCurrentProcessId()) return false;
    POINT p;
    if (!GetCursorPos(&p) || !ScreenToClient(fg, &p)) return false;
    RECT r;
    if (!GetClientRect(fg, &r) || r.right <= 0 || r.bottom <= 0) return false;
    mx = (float)p.x * (float)*pScreenW / (float)r.right;
    my = (float)p.y * (float)*pScreenH / (float)r.bottom;
    return true;
}

static void FillRectPx(float l, float t, float r, float b, uint8_t cr, uint8_t cg, uint8_t cb)
{
    RectF rc = { l, b, r, t };
    Rgba8 col = { cr, cg, cb, 255 };
    ((void (__cdecl*)(RectF*, Rgba8*))FN_DRAW_RECT)(&rc, &col);
}

// Cursor em forma de seta, feito de retangulos: so e usado se a textura original do jogo nao estiver carregada
static void DrawArrowCursor(float x, float y)
{
    const float u = (float)*pScreenH / 1080.0f * 2.0f;     // 1 "pixel" da seta
    auto shape = [&](float grow, uint8_t c) {
        for (int i = 0; i < 17; ++i)
        {
            const float top = y + i * u;
            float left = x, right = x + (i < 12 ? (i * 0.55f + 1.5f) * u : 0.0f);
            if (i >= 8) { left = x + 1.5f * u; right = x + (i < 12 ? (i * 0.55f + 1.5f) * u : 4.0f * u); if (i >= 12) right = x + 4.0f * u; }
            if (i >= 8 && i < 12) left = x;
            FillRectPx(left - grow, top - grow, right + grow, top + u + grow, c, c, c);
        }
    };
    shape(1.2f * u, 0);
    shape(0.0f, 255);
}

// Cursor ORIGINAL do jogo: o proprio menu carrega a textura "mouse" (com a mascara "mousea") dos arquivos do
// jogo em um CSprite2d. Desenhamos esse sprite com o mesmo tamanho e a mesma sombra que o menu usa.
static void DrawGameCursor(float mx, float my)
{
    void* menu = (void*)MENU_OBJ;
    uint8_t* sprite = (uint8_t*)(MENU_OBJ + MENU_SPRITE_OFF);
    if (!*(void**)sprite) { DrawArrowCursor(mx, my); return; }       // textura ainda nao carregada

    auto stretchX = [&](float v) { return ((float (__thiscall*)(void*, float))FN_STRETCH_X)(menu, v); };
    auto stretchY = [&](float v) { return ((float (__thiscall*)(void*, float))FN_STRETCH_Y)(menu, v); };
    auto draw = [&](float left, float top, float right, float bottom, Rgba8 color) {
        RectF rc = { left, bottom, right, top };
        ((void (__thiscall*)(void*, RectF*, Rgba8*))FN_SPRITE_DRAW)(sprite, &rc, &color);
    };

    const float w = stretchX(18.0f), h = stretchY(18.0f);
    Rgba8 shadow = { 100, 100, 100, 50 };
    draw(mx + stretchX(6.0f), my + stretchY(3.0f), mx + stretchX(24.0f), my + stretchY(21.0f), shadow);   // sombra
    Rgba8 white = { 255, 255, 255, 255 };
    draw(mx, my, mx + w, my + h, white);                                                                    // seta
}

// ---- layout do menu: titulo, Yes, No (centralizados) ----
struct MenuLayout { float step, top[3], bandL, bandR; };
static MenuLayout GetLayout(float W, float H)
{
    MenuLayout L;
    L.step = H * 0.0676f * g_lineSpacing;                 // espacamento entre as opcoes do menu de pausa
    const float y0 = H * g_msgY / 100.0f + g_msgOffY;     // topo da linha "Yes"
    L.top[0] = y0 - L.step; L.top[1] = y0; L.top[2] = y0 + L.step;
    L.bandL = W * 0.5f - W * 0.17f + g_msgOffX;
    L.bandR = W * 0.5f + W * 0.17f + g_msgOffX;
    return L;
}

static int HitItem(const MenuLayout& L, float mx, float my)      // 0 = Yes, 1 = No, -1 = nenhum
{
    if (mx < L.bandL || mx > L.bandR) return -1;
    for (int i = 0; i < 2; ++i)
        if (my >= L.top[i + 1] - L.step * 0.10f && my < L.top[i + 1] + L.step * 0.90f) return i;
    return -1;
}

static int   g_sel = 0;                   // 0 = Yes, 1 = No
static float g_mx = -1, g_my = -1;
static float g_lastMouseX = -1000, g_lastMouseY = -1000;
static bool  g_mouseSeen = false;
static bool  g_cursorVisible = true;          // some quando o jogador usa o controle (como no GInput)
static uint8_t g_savedMouseFlag = 0;

static void PlayMenuSound(int ev)
{
    ((AudioEvent_t)FN_AUDIO_EVENT)((void*)ADDR_AUDIO_ENGINE, ev, 0.0f, 1.0f);
}

static void PrintMenuLine(const char* text, float x, float y, uint32_t color)
{
    char gtext[300]; ToGameText(text, gtext, sizeof(gtext));
    ((void (__cdecl*)(uint32_t))FN_FONT_SET_COLOR)(color);
    ((void (__cdecl*)(float, float, const char*))FN_FONT_PRINT)(x, y, gtext);
}

// Cobre a area das opcoes do menu de pausa (fundo preto) e escreve "Retry? / Yes / No" no centro
static void DrawPauseMenu(const MenuLayout& L)
{
    const float W = (float)*pScreenW, H = (float)*pScreenH;

    // descarrega o texto que o menu ja enfileirou (as opcoes), para que o retangulo preto fique POR CIMA dele
    ((VoidFn_t)FN_FONT_RENDER_BUF)();
    FillRectPx(W * 0.18f, H * 0.27f, W * 0.82f, H * 0.83f, 0, 0, 0);

    char title[600]; ExpandMessage(title, sizeof(title));

    ((void (__cdecl*)(uint8_t))FN_FONT_SET_PROP)(1);
    ((void (__cdecl*)(uint8_t, uint8_t))FN_FONT_SET_BG)(0, 0);
    ((void (__cdecl*)(uint8_t))FN_FONT_SET_JUSTIFY)(0);
    ((void (__cdecl*)(uint8_t))FN_FONT_SET_ORIENT)(0);            // centralizado
    ((void (__cdecl*)(float))FN_FONT_SET_WRAPX)(W);
    ((void (__cdecl*)(float))FN_FONT_SET_CENTRE)(W);
    ((void (__cdecl*)(uint8_t))FN_FONT_SET_STYLE)(2);             // fonte das opcoes do menu
    ((void (__cdecl*)(uint8_t))FN_FONT_SET_EDGE)(2);              // sombra, como nas opcoes
    ((void (__cdecl*)(uint32_t))FN_FONT_SET_DROPCOLOR)(0xFF000000u);
    ((void (__cdecl*)(float, float))FN_FONT_SET_SCALE)(0.70f * W / 640.0f * g_scaleMul, H / 448.0f * g_scaleMul);

    const float cx = W * 0.5f + g_msgOffX;
    PrintMenuLine(title,    cx, L.top[0], g_titleColor);
    PrintMenuLine(g_yesText, cx, L.top[1], g_sel == 0 ? g_selColor : g_normColor);
    PrintMenuLine(g_noText,  cx, L.top[2], g_sel == 1 ? g_selColor : g_normColor);

    // desenha o texto agora para o cursor ficar por cima
    ((VoidFn_t)FN_FONT_RENDER_BUF)();

    if (g_mouseSeen)
    {
        *(int32_t*)(MENU_OBJ + MENU_MOUSE_X_OFF) = (int32_t)g_mx;
        *(int32_t*)(MENU_OBJ + MENU_MOUSE_Y_OFF) = (int32_t)g_my;
        if (g_ownCursor && g_cursorVisible) DrawGameCursor(g_mx, g_my);
    }
}

static void BeginClose(bool accept, const char* why)
{
    PlayMenuSound(accept ? SND_CONFIRM : SND_BACK);   // Yes = som de confirmar; No/Esc/Y = som de voltar
    BeginSuppress();                       // o clique/tecla de confirmar nao pode virar soco na gameplay
    g_acceptAfterClose = accept;
    g_phase = PH_CLOSING; g_phaseTick = GetTickCount();
    Log("%s -> %s.", why, accept ? "recomecar (teleportar)" : "cancelar");
}

// estados anteriores dos comandos (para detectar o "apertou agora")
static bool g_pUp = false, g_pDown = false, g_pEnter = false, g_pOk = false, g_pClick = false;
static bool g_pYes = false, g_pNo = false, g_pEsc = false, g_pBack = false;

static bool MouseClickDown()
{
    HWND fg = GetForegroundWindow();
    DWORD pid = 0; if (fg) GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId() && (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
}

// Quando a tela de Retry termina (por qualquer motivo): devolve o cursor do menu ao normal e protege o HUD
static void EndPauseCleanup()
{
    *(uint8_t*)(MENU_OBJ + MENU_MOUSE_ON_OFF) = g_savedMouseFlag;
    g_hudGuardUntil = GetTickCount() + 6000;
    GuardHud();
}

// Chamado a cada frame desenhado (inclusive com o jogo pausado), logo depois de CHud::Draw
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
            g_sel = 0; g_mouseSeen = false; g_lastMouseX = -1000; g_lastMouseY = -1000;
            SaveHud();                                      // guarda o estado do HUD (contador de dinheiro etc.)
            g_hudGuardUntil = tnow + 600000;                // vigia ate o fim da pausa e um pouco depois
            g_savedMouseFlag = *(uint8_t*)(MENU_OBJ + MENU_MOUSE_ON_OFF);
            *(uint8_t*)(MENU_OBJ + MENU_MOUSE_ON_OFF) = 0;  // o cursor passa a ser desenhado so por este mod
            g_cursorVisible = !(g_hideCursorWithPad && g_usingPad);
            // o que ja estiver apertado agora nao conta (so um aperto novo)
            const int stick0 = LeftStickVertical();
            g_pUp   = KeyPressed(VK_UP)     || PadDown(0x10, 0x0001, 0) || stick0 < 0;
            g_pDown = KeyPressed(VK_DOWN)   || PadDown(0x12, 0x0002, 0) || stick0 > 0;
            g_pEnter = KeyPressed(VK_RETURN);
            g_pOk   = PadDown(0x20, 0x1000, 0);
            g_pClick = MouseClickDown();
            g_pYes = g_pNo = false;
            g_pEsc  = KeyPressed(VK_ESCAPE);
            g_pBack = PadDown(g_backOffset, g_backXMask, g_backXTrig);
            Log("Jogo pausado; menu Retry? na tela.");
        }
        else if (tnow - g_phaseTick > 1500 && !g_forcedActivate)
        {
            g_forcedActivate = true;
            *(uint8_t*)ADDR_MENU_ACTIVATE = 1;    // a funcao normal recusou o pedido; forca a ativacao
            Log("Pedido normal de pausa nao funcionou; forcando a ativacao do menu.");
        }
        else if (tnow - g_phaseTick > 5000)
        {
            g_phase = PH_NONE; g_prompting = false;
            Log("Nao foi possivel pausar o jogo; aviso cancelado.");
        }
        return;
    }

    if (g_phase == PH_ACTIVE)
    {
        if (!menuActive)
        {
            g_phase = PH_NONE; g_prompting = false;
            EndPauseCleanup();
            Log("Menu fechado por outro motivo; aviso cancelado.");
            return;
        }

        const float W = (float)*pScreenW, H = (float)*pScreenH;
        const MenuLayout L = GetLayout(W, H);

        // ---- entrada ----
        TrackInputDevice(true);
        g_cursorVisible = !(g_hideCursorWithPad && g_usingPad);
        const int  stick = LeftStickVertical();                      // analogico esquerdo
        const bool up    = KeyPressed(VK_UP)   || PadDown(0x10, 0x0001, 0) || stick < 0;
        const bool down  = KeyPressed(VK_DOWN) || PadDown(0x12, 0x0002, 0) || stick > 0;
        const bool enter = KeyPressed(VK_RETURN);
        const bool ok    = PadDown(0x20, 0x1000, 0);                 // Cross / A
        const bool click = MouseClickDown();
        const bool yes = false, no = false;      // sem atalhos
        const bool esc   = g_cancelOnEsc && KeyPressed(VK_ESCAPE);
        const bool back  = PadDown(g_backOffset, g_backXMask, g_backXTrig);          // Triangulo / Y do controle

        // mouse: o cursor se move livremente; passar por cima de uma opcao a seleciona
        float mx, my;
        int hit = -1;
        if (GetMouseGame(mx, my))
        {
            g_mx = mx; g_my = my; g_mouseSeen = true;
            hit = g_cursorVisible ? HitItem(L, mx, my) : -1;       // com o cursor escondido o mouse nao seleciona nada
            if (fabsf(mx - g_lastMouseX) + fabsf(my - g_lastMouseY) > 3.0f)
            {
                g_lastMouseX = mx; g_lastMouseY = my;
                if (hit >= 0 && hit != g_sel) { g_sel = hit; PlayMenuSound(SND_NAVIGATE); }
            }
        }

        const bool armed = (tnow - g_phaseTick) > 300;      // ignora o que ainda estava apertado ao pausar
        if (armed)
        {
            if ((up && !g_pUp) || (down && !g_pDown)) { g_sel = 1 - g_sel; PlayMenuSound(SND_NAVIGATE); }   // 2 opcoes: alterna

            if ((esc && !g_pEsc) || (back && !g_pBack))
                BeginClose(false, (esc && !g_pEsc) ? "Esc: No" : "Voltar do controle (Y): No");
            else if (click && !g_pClick && hit >= 0)             { g_sel = hit; BeginClose(hit == 0, hit == 0 ? "Mouse: Yes" : "Mouse: No"); }
            else if ((enter && !g_pEnter) || (ok && !g_pOk))     BeginClose(g_sel == 0, g_sel == 0 ? "Confirmar: Yes" : "Confirmar: No");
        }
        g_pUp = up; g_pDown = down; g_pEnter = enter; g_pOk = ok; g_pClick = click;
        g_pYes = yes; g_pNo = no; g_pEsc = esc; g_pBack = back;

        if (g_phase == PH_ACTIVE) DrawPauseMenu(L);
    }
}

// ======================= Hooks =======================
static VoidFn_t  g_origGameLogicUpdate = nullptr;
static VoidFn_t  g_origHudDraw = nullptr;
static BigMsg_t  g_origBigMessage = nullptr;

static void __cdecl Hook_GameLogicUpdate()
{
    g_origGameLogicUpdate();
    Tick();
}

static void __cdecl Hook_HudDraw()
{
    g_origHudDraw();
    PauseFrame();
}

typedef void (__thiscall *MenuProcess_t)(void*);
static MenuProcess_t g_origMenuProcess = nullptr;

// Substitui a chamada de CMenuManager::Process. Enquanto a mensagem esta na tela, o menu NAO recebe
// entrada (para ninguem escolher uma opcao invisivel); so fazemos a manutencao que o jogo faz por frame.
static void __fastcall Hook_MenuProcess(void* self, void* /*edx*/)
{
    if (g_phase == PH_ACTIVE)
    {
        ((void (__thiscall*)(void*, uint32_t))FN_MENU_STREAMING)(self, *((uint8_t*)self + MENU_SCREEN_BYTE_OFF));
        ((void (__cdecl*)(int))FN_MENU_AUDIO_A)(1);
        ((void (__cdecl*)(int))FN_MENU_AUDIO_B)(1);
        return;
    }
    if (g_phase == PH_CLOSING)
    {
        *((uint8_t*)self + MENU_SHUTDOWN_OFF) = 1;                        // pede para fechar o menu
        ((void (__thiscall*)(void*))FN_MENU_CHECK_CLOSE)(self);           // a rotina do jogo que fecha e despausa
        if (*((uint8_t*)self + MENU_ACTIVE_OFF) == 0)
        {
            g_phase = PH_NONE; g_prompting = false;
            if (g_acceptAfterClose) g_teleportPending = true;
            EndPauseCleanup();
            Log("Menu fechado; jogo despausado.");
        }
        else if (GetTickCount() - g_phaseTick > 4000)
        {
            Log("Fechamento demorou; usando o processamento normal do menu.");
            g_phase = PH_NONE; g_prompting = false;
            if (g_acceptAfterClose) g_teleportPending = true;
            g_origMenuProcess(self);
            EndPauseCleanup();
        }
        return;
    }
    g_origMenuProcess(self);
}

static void __cdecl Hook_BigMessage(const char* text, uint32_t time, uint32_t style)
{
    if (text && g_enabled)
    {
        const char* fail = ((TextGet_t)FN_TEXT_GET)((void*)ADDR_THETEXT, "M_FAIL");
        if (StrEquals(text, fail))
        {
            g_failSeen = true;
            g_failTime = *(uint32_t*)ADDR_TIMER_MS;
            Log("Mensagem de missao falhada detectada.");
        }
    }
    g_origBigMessage(text, time, style);
}

// Troca o destino de um "call rel32" e devolve o destino original
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

// Procura todos os "call" para 'target' na area de codigo principal e troca por 'hook'
static int PatchAllCalls(uintptr_t target, void* hook)
{
    int count = 0;
    const uintptr_t begin = 0x401000, end = 0x401000 + 0x455E00;
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
    if (g_logEnabled) { FILE* f = fopen("RetryMissionPause.log", "w"); if (f) fclose(f); }

    if (*(uint32_t*)VERSION_ADDR != VERSION_OK)
    {
        Log("Versao do exe nao reconhecida (esperado 1.0 US). Mod desativado.");
        return TRUE;
    }

    LoadConfig();
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (GetFileAttributesExA(g_iniPath, GetFileExInfoStandard, &fad)) g_lastWrite = fad.ftLastWriteTime;

    // 1) logica por frame: logo depois de CGameLogic::Update
    uintptr_t o1 = PatchCall(FN_GAMELOGIC_UPDATE_CALL, (void*)&Hook_GameLogicUpdate);
    if (o1 != FN_GAMELOGIC_UPDATE) { Log("Ponto de chamada de CGameLogic::Update nao confere. Mod desativado."); return TRUE; }
    g_origGameLogicUpdate = (VoidFn_t)o1;

    // 2) desenho do aviso: depois de CHud::Draw
    g_origHudDraw = (VoidFn_t)FN_HUD_DRAW;
    int nDraw = PatchAllCalls(FN_HUD_DRAW, (void*)&Hook_HudDraw);

    // 3) detectar "MISSION FAILED"
    g_origBigMessage = (BigMsg_t)FN_ADD_BIG_MESSAGE;
    int nMsg = PatchAllCalls(FN_ADD_BIG_MESSAGE, (void*)&Hook_BigMessage);

    // 4) menu de pausa: entrada bloqueada enquanto a mensagem esta na tela
    uintptr_t o4 = PatchCall(FN_MENU_PROCESS_CALL, (void*)&Hook_MenuProcess);
    if (o4 != FN_MENU_PROCESS) { Log("Ponto de chamada de CMenuManager::Process nao confere. Mod desativado."); g_enabled = false; return TRUE; }
    g_origMenuProcess = (MenuProcess_t)o4;

    // 5) leitura dos controles: permite impedir que o que foi apertado no menu vaze para a gameplay
    uintptr_t o5 = PatchCall(FN_UPDATE_PADS_CALL, (void*)&Hook_UpdatePads);
    if (o5) g_origUpdatePads = (UpdatePads_t)o5;
    else    Log("Aviso: ponto de chamada de CPad::UpdatePads nao confere; a protecao contra 'vazamento' de entrada ficou desligada.");

    Log("OK: hooks instalados (CHud::Draw x%d, AddBigMessage x%d, menu de pausa, UpdatePads %s).", nDraw, nMsg, o5 ? "ok" : "nao");
    Log("Build: %s %s | cursor do jogo + GInput-like, protecao de entrada e do HUD.", __DATE__, __TIME__);
    return TRUE;
}
