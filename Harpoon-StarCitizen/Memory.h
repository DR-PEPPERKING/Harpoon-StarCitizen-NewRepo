#pragma once
#include "CryEngineSDK/BaseIncludes.h"

namespace MemoryTools
{
	void* FindPattern(const char* moduleName, const char* pattern, const char* szName = nullptr);
	void HexPrint(const char* desc, const void* addr, const int len, int perLine);
	std::string hexStr(BYTE* data, int len);

	template <typename T>
	static constexpr auto relativeToAbsolute(uintptr_t address) noexcept
	{
		return (T)(address + 4 + *reinterpret_cast<std::int32_t*>(address));
	}

	template <typename T>
	static constexpr auto relativeToAbsolute(char* address) noexcept
	{
		return relativeToAbsolute<T>(reinterpret_cast<uintptr_t>(address));
	}
};



class GameMemory
{
public:
	GameMemory();



	std::uintptr_t m_pAmmoUpdateInstruction = 0;
	std::uintptr_t m_pCXConsolePrintf = 0;
	std::uintptr_t m_pLoadConfigVar = 0;
	std::uintptr_t m_pCXConsoleExecuteCommand = 0;
	std::uintptr_t m_pLoadAndInitCryModule = 0;
	std::uintptr_t m_pCheckCvarWhileList = 0;
private:
};

inline GameMemory g_GameMemory;
inline GameMemory* g_pMemory = &g_GameMemory;