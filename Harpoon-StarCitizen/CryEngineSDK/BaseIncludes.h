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