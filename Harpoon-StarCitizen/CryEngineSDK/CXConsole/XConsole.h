#pragma once 
#include "../BaseIncludes.h"
#include "IConsole.h"
//forward declaration
struct INetwork;
class CSystem;


#define MAX_HISTORY_ENTRIES 50
#define LINE_BORDER 10

enum ScrollDir
{
    sdDOWN,
    sdUP,
    sdNONE
};

//////////////////////////////////////////////////////////////////////////
// Console command holds information about commands registered to console.
//////////////////////////////////////////////////////////////////////////
struct CConsoleCommand
{
    std::string m_sName;            // Console command name
    std::string m_sCommand;         // lua code that is executed when this command is invoked
    std::string m_sHelp;            // optional help string - can be shown in the console with "<commandname> ?"
    int    m_nFlags;           // bitmask consist of flag starting with VF_ e.g. VF_CHEAT
    ConsoleCommandFunc m_func; // Pointer to console command.

    //////////////////////////////////////////////////////////////////////////
    CConsoleCommand()
        : m_func(0)
        , m_nFlags(0) {}
    size_t sizeofThis () const {return sizeof(*this) + m_sName.capacity() + 1 + m_sCommand.capacity() + 1; }
    void GetMemoryUsage (class ICrySizer* pSizer) const
    {
        sizeof(std::string);
        _asm int 3
       // pSizer->AddObject(m_sName);
       // pSizer->AddObject(m_sCommand);
       // pSizer->AddObject(m_sHelp);
    }
};


