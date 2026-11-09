// AmbulanceFull.asi - GTA III v1.0 PC (gta3.exe 2,383,872 bytes)
// Mostra o aviso "Ambulance full!!" (A_FULL) assim que o ultimo paciente
// senta na ambulancia, sem precisar tentar pegar mais um paciente.
//
// Compilar: MSVC x86 (Win32), DLL renomeada para .asi (ver main.yml).
// Requer um ASI Loader (ex.: Ultimate ASI Loader / dinput8.dll).

#include <windows.h>
#include <stdint.h>
#include <string.h>

// ---------------- Enderecos (GTA III 1.0) ----------------
static const uintptr_t ADDR_HOOK_CALL      = 0x48E42F; // call CMessages::Display (em Render2dStuff)
static const uintptr_t ADDR_MSG_DISPLAY    = 0x529800; // CMessages::Display
static const uintptr_t ADDR_MSG_JUMPQ      = 0x529A10; // CMessages::AddMessageJumpQ(wchar*, uint32 time, uint16 flag)
static const uintptr_t ADDR_TEXT_GET       = 0x52C5A0; // CText::Get(const char*)  [thiscall]
static const uintptr_t ADDR_THETEXT        = 0x941520; // TheText
static const uintptr_t ADDR_FIND_PLAYERVEH = 0x4A10C0; // FindPlayerVehicle()

// ---------------- Offsets ----------------
static const int OFF_MODEL_INDEX   = 0x5C;   // CEntity::m_modelIndex (int16)
static const int OFF_DRIVER        = 0x1A4;  // CVehicle::pDriver
static int       g_fullPeople      = 4;      // motorista + passageiros (AmbulanceFull.ini)
static const int OFF_NUM_PASSENGER = 0x1C8;  // CVehicle::m_nNumPassengers
static const int OFF_NUM_MAX_PASS  = 0x1CC;  // CVehicle::m_nNumMaxPassengers
static const int MI_AMBULANCE      = 137;    // 0x89

typedef void*    (__cdecl   *FindPlayerVehicle_t)();
typedef wchar_t* (__fastcall *TextGet_t)(void* thisPtr, int edxDummy, const char* key);
typedef void     (__cdecl   *AddMessageJumpQ_t)(wchar_t* text, uint32_t time, uint16_t flag);
typedef void     (__cdecl   *MessagesDisplay_t)();

static const uintptr_t ADDR_TIMER_MS   = 0x885B48; // CTimer::m_snTimeInMilliseconds
static const uintptr_t ADDR_MSG_SLOT0  = 0x8786E0; // CMessages::BriefMessages[0] (text, flag, time, start)

static void ShowAmbulanceFull()
{
    wchar_t* text = ((TextGet_t)ADDR_TEXT_GET)((void*)ADDR_THETEXT, 0, "A_FULL");
    if (!text) return;

    // Mantem o aviso na tela enquanto a ambulancia estiver lotada:
    // so recoloca a mensagem quando ela nao esta mais no slot 0 ou esta perto de expirar.
    wchar_t*  cur   = *(wchar_t**)ADDR_MSG_SLOT0;
    uint32_t  time  = *(uint32_t*)(ADDR_MSG_SLOT0 + 8);
    uint32_t  start = *(uint32_t*)(ADDR_MSG_SLOT0 + 12);
    uint32_t  now   = *(uint32_t*)ADDR_TIMER_MS;
    if (cur == text && (now - start) + 300 < time)
        return;
    ((AddMessageJumpQ_t)ADDR_MSG_JUMPQ)(text, 1500, 1);
}

static void Tick()
{
    void* veh = ((FindPlayerVehicle_t)ADDR_FIND_PLAYERVEH)();
    if (!veh || *(int16_t*)((uint8_t*)veh + OFF_MODEL_INDEX) != MI_AMBULANCE)
        return;

    int people = (*(void**)((uint8_t*)veh + OFF_DRIVER) ? 1 : 0)
               + *((uint8_t*)veh + OFF_NUM_PASSENGER);
    if (people >= g_fullPeople)
        ShowAmbulanceFull();   // some sozinho quando deixa de estar lotada (para de ser recolocado)
}

static void __cdecl HookedDisplay()
{
    Tick();
    ((MessagesDisplay_t)ADDR_MSG_DISPLAY)();
}

static bool PatchCall()
{
    uint8_t* p = (uint8_t*)ADDR_HOOK_CALL;
    if (p[0] != 0xE8) return false;
    int32_t rel = *(int32_t*)(p + 1);
    if ((uintptr_t)(p + 5 + rel) != ADDR_MSG_DISPLAY) return false; // nao e o v1.0

    DWORD old;
    if (!VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old)) return false;
    *(int32_t*)(p + 1) = (int32_t)((uintptr_t)&HookedDisplay - (uintptr_t)(p + 5));
    VirtualProtect(p, 5, old, &old);
    return true;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);

        char path[MAX_PATH];
        GetModuleFileNameA(hModule, path, MAX_PATH);
        char* slash = strrchr(path, '\\');
        if (slash) {
            strcpy(slash + 1, "AmbulanceFull.ini");
            g_fullPeople = GetPrivateProfileIntA("Settings", "PeopleToShowWarning", 4, path);
            if (g_fullPeople < 1) g_fullPeople = 1;
        }

        PatchCall();
    }
    return TRUE;
}
