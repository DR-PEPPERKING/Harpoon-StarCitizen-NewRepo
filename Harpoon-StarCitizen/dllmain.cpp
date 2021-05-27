// dllmain.cpp : Defines the entry point for the DLL application.

#include "CryEngineSDK/CryEntitySystem/CEntity.h"
#include "Memory.h"
#include "CryEngineSDK/CXConsole/IConsole.h"
#include "MinHook/MinHook.h"
// StarCitizen.exe + 415CF6D
bool WriteInfiniteAmmo()
{
	printf("Infinite Ammo...");
	void* pAmmoUpdate = 0;
	pAmmoUpdate = (void*)g_pMemory->m_pAmmoUpdateInstruction;
	if (!pAmmoUpdate)
	{
		printf("FAILED! No Signature Found\n");
		return false;
	}

	DWORD protect = 0;
	VirtualProtect((void*)pAmmoUpdate, 11, PAGE_EXECUTE_READWRITE, &protect); /* cause jle to not set*/
	for (int i = 0; i < 11; i++)
	{
		*((BYTE*)pAmmoUpdate + i) = 0x90;
	}
	*((BYTE*)pAmmoUpdate) = 0xC7;
	*((BYTE*)pAmmoUpdate + 1) = 0x86;
	*((std::uint32_t*)((char*)pAmmoUpdate + 2)) = 0x000003B8;
	*((std::uint32_t*)((char*)pAmmoUpdate + 6)) = 0x000000FF;
	VirtualProtect((void*)pAmmoUpdate, 11, protect, &protect); /* cause jle to not set*/
	printf("Ok\n");
	return true;
}

bool WriteNoCarryWeight()
{
	printf("Carry Weight...");
	void* pAmmoUpdate = 0;
	pAmmoUpdate = (void*)g_pMemory->m_pCarryWeightUpdate;
	if (!pAmmoUpdate)
	{
		printf("FAILED! No Signature Found\n");
		return false;
	}

	DWORD protect = 0;
	VirtualProtect((void*)pAmmoUpdate, 9, PAGE_EXECUTE_READWRITE, &protect); /* cause jle to not set*/
	for (int i = 0; i < 9; i++)
	{
		*((BYTE*)pAmmoUpdate + i) = 0x90;
	}
	VirtualProtect((void*)pAmmoUpdate, 9, protect, &protect); /* cause jle to not set*/
	printf("Ok\n");
	return true;
}
#if 0
class CXConsole;
char __fastcall hk_CXConsoleProcessInput(CXConsole* _this, )
{

}
#endif
class CXConsole;
CXConsole* g_pCXConsole = nullptr;

typedef __int64(__cdecl* CXConsolePrintfFunc_t)(const char* szFormatter, ...);
LPVOID oCXConsoleLoadConfigVar = NULL;
typedef void(__fastcall* CXConsoleLoadConfigVarFunc_t)(_int64*, ICVar*, char*);
void __fastcall hk_CXConsole_LoadConfigVar(__int64* _this, ICVar* pCvar, char* sValue)
{
	if (!pCvar)
		return;

	if (!sValue || !*sValue)
		return;

	g_pCXConsole = (CXConsole*)_this;

	char* szName = (*(char*(__fastcall**)(ICVar*))(*(char**)pCvar + int(112)))(pCvar);
	printf("LoadConfigVar Called For %s to be set to %s\n", szName, sValue);

	if (strstr(szName, "p_max_entity_cells"))
		strcpy_s(sValue, 5, "1000");

	((CXConsolePrintfFunc_t)g_pMemory->m_pCXConsolePrintf)("LoadConfigVar Called For %s to be set to %s\n", szName, sValue);
	((CXConsoleLoadConfigVarFunc_t)oCXConsoleLoadConfigVar)(_this, pCvar, sValue);
}



//E8 ? ? ? ? E9 ? ? ? ? 49 8B BE ? ? ? ? 48 8B 75 E7
LPVOID oCXConsoleExecuteCommand = 0;
typedef __int64(__fastcall* CXConsoleExecuteCommandFunc_t)(__int64 a1, __int64 a2, char** a3, __int64 a4, float _XMM0);
__int64  hk_CXConsole_ExecuteCommand(__int64 a1, __int64 a2, char** a3, __int64 a4, float _XMM0)
{
	g_pCXConsole = (CXConsole*)a1;
	((CXConsolePrintfFunc_t)g_pMemory->m_pCXConsolePrintf)("hk_CXConsole_ExecuteCommand Called For %s\n", *a3);
	printf("hk_CXConsole_ExecuteCommand Called For %s\n", *a3);
	return ((CXConsoleExecuteCommandFunc_t)oCXConsoleExecuteCommand)(a1, a2, a3, a4, _XMM0);
}

LPVOID oLoadAndInitCryModule = 0;
typedef __int64(__fastcall* LoadAndInitCryModuleFunc_t)(const char* szFunctionName, const char* szModuleName, __int64 unkn, __int64 unkn2);
__int64 __fastcall hk_LoadAndInitCryModule(const char* szModuleName, const char* szFunctionName, __int64 unkn, __int64 unkn2)
{
	printf("Module %s is being loaded with the Init function %s\n", szModuleName, szFunctionName);	
	return ((LoadAndInitCryModuleFunc_t)oLoadAndInitCryModule)(szModuleName, szFunctionName, unkn, unkn2);
}

LPVOID oCXConsoleCheckCvarWhiteListStatus = NULL;
//E8 ? ? ? ? 0F B6 F0 84 C0 75 72
char __fastcall hk_CXConsole_CheckCvarWhiteListStatus(__int64* _this, __int64 MaybeIsReleaseFlags, int nCvarFlags, char* szFileName)
{
	//printf("hk_CXConsole_CheckCvarWhiteListStatus\n");
	return 1;
}

//GetCvar <-- direct reference : [actual address in first opcode] E8 ? ? ? ? 0F B6 D8 40 84 FF
// CXConsole::Printf : direct reference: [actual address in first opcode] E8 ? ? ? ? 49 2B DF 

template <typename T>
static constexpr auto _relativeToAbsolute(uintptr_t address) noexcept
{
	return (T)(address + 4 + *reinterpret_cast<std::int32_t*>(address));
}

class CryNetwork;

void Initalize(HMODULE hModule)
{
	printf("Initializing.\n");
	printf("Running Byte Patches.\n");
	WriteInfiniteAmmo();
	WriteNoCarryWeight();
	printf("Unlocking Console Vars...");


	
	char* pIsDebugCheck = (char*)MemoryTools::FindPattern("StarCitizen.exe", "48 85 C0 74 10 48 8B D7 48 8B C8 E8 ? ? ? ? 48 8B D8 EB 02 33 DB 48 8B D7");
	DWORD protect = 0;
	VirtualProtect((void*)pIsDebugCheck, 5, PAGE_EXECUTE_READWRITE, &protect); 
	for (int i = 0; i < 5; i++)
		*(BYTE*)(pIsDebugCheck + i) = 0x90;
	VirtualProtect((void*)pIsDebugCheck, 5, protect, &protect); 

	char* ConsoleVar = _relativeToAbsolute<char*>((uintptr_t)((char*)MemoryTools::FindPattern("StarCitizen.exe", "48 8B 0D ? ? ? ? 80 B9 9B ? ? ? ? 0F 84 ? ? ? ? 4C 8B 91 ? ? ? ? 4D 85 D2 74 33 49 8B 02") + int(3)));
	*(BYTE*)(ConsoleVar + 923) = 0xFF;
	*(BYTE*)(ConsoleVar + 200) = 0xFF;

	ConsoleVar = _relativeToAbsolute<char*>((uintptr_t)((char*)MemoryTools::FindPattern("StarCitizen.exe", "48 8B 05 ? ? ? ? 44 38 A8 ? ? ? ? 0F 85 ? ? ? ? 48 8B 88 C8 00 00 00 48 8D 15 37 35 5D 07 48 8B 01 FF 90 C0 00 00 00 48 89 45 D8 48 8B D8") + int(3)));
	*(BYTE*)(ConsoleVar + 922) = 0x00;
	*(BYTE*)(ConsoleVar + 200) = 0xFF;

	bool* bConDebug = _relativeToAbsolute<bool*>((uintptr_t)((char*)MemoryTools::FindPattern("StarCitizen.exe", "83 3D ? ? ? ? 00 49 8B E8 48 8B F2 48 89 54 24 30 48 8B F9") + int(2)));
	*bConDebug = true;

	
	pIsDebugCheck = (char*)MemoryTools::FindPattern("StarCitizen.exe", "40 38 B8 ? ? ? ? 75 2F");
	protect = 0;
	VirtualProtect((void*)pIsDebugCheck, 9, PAGE_EXECUTE_READWRITE, &protect); 
	for (int i = 0; i < 9; i++)
		*(BYTE*)(pIsDebugCheck + i) = 0x90;
	VirtualProtect((void*)pIsDebugCheck, 9, protect, &protect); 


	printf("Ok\n");


	//




	printf("Loading Modules...");
	
	



	printf("Ok\n");

	printf("Hooking Functions...");
	MH_Initialize();
	MH_CreateHook((LPVOID)g_pMemory->m_pLoadConfigVar, hk_CXConsole_LoadConfigVar, &oCXConsoleLoadConfigVar);
	MH_CreateHook((LPVOID)g_pMemory->m_pCXConsoleExecuteCommand, hk_CXConsole_ExecuteCommand, &oCXConsoleExecuteCommand);
	MH_CreateHook((LPVOID)g_pMemory->m_pLoadAndInitCryModule, hk_LoadAndInitCryModule, &oLoadAndInitCryModule);
	MH_CreateHook((LPVOID)g_pMemory->m_pCheckCvarWhileList, hk_CXConsole_CheckCvarWhiteListStatus, &oCXConsoleCheckCvarWhiteListStatus);

	MH_EnableHook(MH_ALL_HOOKS);
	printf("Ok\n");
}


BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		AllocConsole();
		FILE* fDummy;
		freopen_s(&fDummy, "CONOUT$", "w", stdout);
		freopen_s(&fDummy, "CONOUT$", "w", stderr);
		freopen_s(&fDummy, "CONIN$", "r", stdin);
		g_pMemory = new GameMemory();
		Initalize(hModule);
		printf("Harpoon StarCitizen Loaded!\n");
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

