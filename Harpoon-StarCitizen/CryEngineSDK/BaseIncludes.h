#pragma once
#include <Windows.h>
#include <cstdint>
#include <TlHelp32.h>
#include <Psapi.h>
#include <string>
#include <sstream>
#include <iomanip>

#define COMBINE1(X,Y) X##Y  // helper macro
#define COMBINE(X,Y) COMBINE1(X,Y)

#define SUBFUNC() virtual void COMBINE(sub_,__LINE__ )() = 0;
#define FUNC_PLACEHOLDER(VAR) void COMBINE(Func, __LINE__)();

#define FUNC_PLACEHOLDER(VAR) void COMBINE(Func, __LINE__)();
#define ClassFunction __forceinline


#if 1
template <class T, class ... Types>
__forceinline T CallVirtualFunction(void* _this, int nIndex, Types ... vals) {
	return ((*reinterpret_cast<T(__thiscall***)(void*, Types...)>(_this))[nIndex])(_this, vals...);
}
#endif

#define CON(VAR, ...) ((CXConsolePrintfFunc_t)g_pMemory->m_pCXConsolePrintf)(VAR, __VA_ARGS__)