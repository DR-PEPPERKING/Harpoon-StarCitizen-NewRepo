#include "Memory.h"


#define INRANGE(x,a,b)    (x >= a && x <= b) 
#define getBits( x )    (INRANGE((x&(~0x20)),'A','F') ? ((x&(~0x20)) - 'A' + 0xa) : (INRANGE(x,'0','9') ? x - '0' : 0))
#define getByte( x )    (getBits(x[0]) << 4 | getBits(x[1]))


namespace MemoryTools {
	void* FindPattern(const char* moduleName, const char* pattern, const char* szName)
	{
		const char* pat = pattern;
		BYTE* firstMatch = 0;
		BYTE* rangeStart = (BYTE*)GetModuleHandleA(moduleName);

		if (!rangeStart)
			printf("Unable To Find Module %s!\n", moduleName);

		MODULEINFO miModInfo; GetModuleInformation(GetCurrentProcess(), (HMODULE)rangeStart, &miModInfo, sizeof(MODULEINFO));
		BYTE* rangeEnd = rangeStart + miModInfo.SizeOfImage;

		if (!miModInfo.SizeOfImage)
			printf("Unable To Find Module %s!\n", moduleName);

		if (rangeStart >= rangeEnd)
			printf("Bad Range! %s\n", moduleName);

		for (BYTE* pCur = rangeStart; pCur < rangeEnd; pCur++)
		{
			if (!*pat)
				return firstMatch;

			if (*(PBYTE)pat == '\?' || *(BYTE*)pCur == getByte(pat))
			{
				if (!firstMatch)
					firstMatch = pCur;

				if (!pat[2])
					return firstMatch;

				if (*(PWORD)pat == '\?\?' || *(PBYTE)pat != '\?')
					pat += 3;

				else
					pat += 2;    //one ?
			}
			else
			{
				pat = pattern;
				firstMatch = 0;
			}
		}

		if (szName)
			printf("Failed to find pattern for %s in %s. (%s)\n", szName, moduleName, pat);
		else
			printf("Failed to find pattern in %s. (%s)\n", moduleName, pat);
		return NULL;
	}

	void HexPrint(
		const char* desc,
		const void* addr,
		const int len,
		int perLine
	) {
		// Silently ignore silly per-line values.

		if (perLine < 4 || perLine > 64) perLine = 16;

		int i;
		unsigned char* buff = (unsigned char*)malloc(perLine + 1);
		const unsigned char* pc = (const unsigned char*)addr;

		// Output description if given.

		if (desc != NULL) printf("%s:\n", desc);

		// Length checks.

		if (len == 0) {
			printf("  ZERO LENGTH\n");
			free(buff);
			return;
		}
		if (len < 0) {
			printf("  NEGATIVE LENGTH: %d\n", len);
			free(buff);
			return;
		}

		// Process every byte in the data.

		for (i = 0; i < len; i++) {
			// Multiple of perLine means new or first line (with line offset).

			if ((i % perLine) == 0) {
				// Only print previous-line ASCII buffer for lines beyond first.

				if (i != 0) printf("  %s\n", buff);

				// Output the offset of current line.

				printf("  %04x ", i);
			}

			// Now the hex code for the specific character.

			printf(" %02x", pc[i]);

			// And buffer a printable ASCII character for later.

			if ((pc[i] < 0x20) || (pc[i] > 0x7e)) // isprint() may be better.
				buff[i % perLine] = '.';
			else
				buff[i % perLine] = pc[i];
			buff[(i % perLine) + 1] = '\0';
		}

		// Pad out last line if not exactly perLine characters.

		while ((i % perLine) != 0) {
			printf("   ");
			i++;
		}

		// And print the final ASCII buffer.

		printf("  %s\n", buff);
		free(buff);
	}


	std::string hexStr(BYTE* data, int len)
	{
		std::stringstream ss;
		ss << std::hex;

		for (int i(0); i < len; ++i)
			ss << std::setw(2) << std::setfill('0') << (int)data[i] << " ";

		return ss.str();
	}

}

GameMemory::GameMemory()
{
	m_pAmmoUpdateInstruction = (std::uintptr_t)MemoryTools::FindPattern("StarCitizen.exe", "89 BE ? ? ? ? E8 ? ? ? ? 48 8D 48 38 E8 ? ? ? ? 41 BE ? ? ? ? 84 C0", "UpdateAmmoInstruction");
	m_pCXConsolePrintf = (std::uintptr_t)MemoryTools::relativeToAbsolute<std::uintptr_t>((char*)MemoryTools::FindPattern("StarCitizen.exe", "E8 ? ? ? ? B3 07 E9 ? ? ? ? 48 8B", "CXConsole::Printf") + int(1));	
	m_pLoadConfigVar = (std::uintptr_t)MemoryTools::relativeToAbsolute<std::uintptr_t>((char*)MemoryTools::FindPattern("StarCitizen.exe", "E8 ? ? ? ? EB 59 48 8B D6", "InitConvars") + int(1));
	m_pCXConsoleExecuteCommand = (std::uintptr_t)MemoryTools::relativeToAbsolute<std::uintptr_t>((char*)MemoryTools::FindPattern("StarCitizen.exe", "E8 ? ? ? ? E9 ? ? ? ? 49 8B BE ? ? ? ? 48 8B 75 E7", "CXConsole::ExecuteCommand") + int(1));
	m_pLoadAndInitCryModule = (std::uintptr_t)MemoryTools::relativeToAbsolute<std::uintptr_t>((char*)MemoryTools::FindPattern("StarCitizen.exe", "E8 ? ? ? ? 49 89 46 40 48 85 C0", "LoadAndInitCryModule") + int(1));
	m_pCheckCvarWhileList = (std::uintptr_t)MemoryTools::relativeToAbsolute<std::uintptr_t>((char*)MemoryTools::FindPattern("StarCitizen.exe", "E8 ? ? ? ? 0F B6 F0 84 C0 75 72", "CXConsole::CheckCvarWhiteList") + int(1));
	m_pCarryWeightUpdate = (std::uintptr_t)((char*)MemoryTools::FindPattern("StarCitizen.exe", "EB 04 4C 8B 75 67 48 85 FF 74 2F 8B C6", "Carry Weight Update") - int(9));
	m_pLoadCryModule = (std::uintptr_t)MemoryTools::relativeToAbsolute<std::uintptr_t>((char*)MemoryTools::FindPattern("StarCitizen.exe", "E8 ? ? ? ? FF D0 8B 15 ? ? ? ?", "LoadCryModule") + int(1));
	m_pMalloc = (StarEngineMallocFunc_t)MemoryTools::relativeToAbsolute<std::uintptr_t>((char*)MemoryTools::FindPattern("StarCitizen.exe", "E8 ? ? ? ? 8B 4D 9B", "StarEngineMalloc") + int(1));
	m_pFree = (StarEngineFreeFunc_t)MemoryTools::relativeToAbsolute<std::uintptr_t>((char*)MemoryTools::FindPattern("StarCitizen.exe", "E8 ? ? ? ? 33 C0 48 8B 5C 24 30 48 83 C4 20", "StarEngineFree") + int(1));



}