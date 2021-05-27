// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "AutoRegister.h"
#include "Command.h"
#include "CommandModuleDescription.h"
#include "BoostPythonMacros.h"

// CryCommon
#include <CrySandbox/CrySignal.h>
#include <CryCore/functor.h>

typedef void (* TPfnDeleter)(void*);

class CCustomCommand;
class QAction;
class QCommandAction;

struct ICommandManager
{
	virtual ~ICommandManager() {}
	virtual bool                             AddCommand(CCommand* pCommand, TPfnDeleter deleter = nullptr) = 0;
	virtual bool                             AddCommandModule(CCommandModuleDescription* pCommand) = 0;
	virtual void                             SetUiDescription(const char* module, const char* name, const CUiCommand::UiInfo& info) const = 0;
	virtual bool                             UnregisterCommand(const char* cmdFullName) = 0;
	virtual bool                             IsRegistered(const char* module, const char* name) const = 0;
	virtual bool                             IsRegistered(const char* cmdLine) const = 0;
	virtual bool                             IsCommandDeprecated(const char* cmdFullName) const = 0;
	virtual const CCommandModuleDescription* FindOrCreateModuleDescription(const char* moduleName) = 0;
	virtual CCommand*                        GetCommand(const char* cmdFullName) const = 0;
	virtual void                             SetChecked(const char* cmdFullName, bool checked) = 0;
	// Searches for a registered command and returns an action. If no UI command is found, we create a new action and use text to set the action text
	virtual QAction*                         GetAction(const char* cmdFullName, const char* text = nullptr) const = 0;
	virtual QCommandAction*                  GetCommandAction(const char* command, const char* text = nullptr) const = 0;
	virtual QCommandAction*                  CreateNewAction(const char* cmdFullName) const = 0;
	virtual string                           Execute(const string& cmdLine) = 0;
	virtual void                             GetCommandList(std::vector<string>& cmds) const = 0;
	virtual void                             GetCommandList(std::vector<CCommand*>& cmds) const = 0;
	virtual void                             RemapCommand(const char* oldModule, const char* oldName, const char* newModule, const char* newName) = 0;
	// Creates a new action for the given command, this action is already populated with predefined icons, text, etc but has independent state
	virtual void                             AddCustomCommand(CCustomCommand* pCommand) = 0;
	virtual void                             RemoveCustomCommand(CCustomCommand* pCommand) = 0;

	// signals
	CCrySignal<void()>                signalChanged;
	CCrySignal<void(CCommand*)>       signalCommandAdded;
	CCrySignal<void(QCommandAction*)> signalCustomCommandAdded;
};

/// A set of helper template methods for an easy registration of commands
namespace CommandManagerHelper
{
bool RegisterCommand(ICommandManager* pCmdMgr, const char* szModule, const char* szName, const CCommandDescription& description, const Functor0& functor);
template<typename RT>
bool RegisterCommand(ICommandManager* pCmdMgr, const char* szModule, const char* szName, const CCommandDescription& description, const Functor0wRet<RT>& functor);
template<LIST(1, typename P)>
bool RegisterCommand(ICommandManager* pCmdMgr, const char* szModule, const char* szName, const CCommandDescription& description, const Functor1<LIST(1, P)>& functor);
template<LIST(1, typename P), typename RT>
bool RegisterCommand(ICommandManager* pCmdMgr, const char* szModule, const char* szName, const CCommandDescription& description, const Functor1wRet<LIST(1, P), RT>& functor);
template<LIST(2, typename P)>
bool RegisterCommand(ICommandManager* pCmdMgr, const char* szModule, const char* szName, const CCommandDescription& description, const Functor2<LIST(2, P)>& functor);
template<LIST(2, typename P), typename RT>
bool RegisterCommand(ICommandManager* pCmdMgr, const char* szModule, const char* szName, const CCommandDescription& description, const Functor2wRet<LIST(2, P), RT>& functor);
template<LIST(3, typename P)>
bool RegisterCommand(ICommandManager* pCmdMgr, const char* szModule, const char* szName, const CCommandDescription& description, const Functor3<LIST(3, P)>& functor);
template<LIST(3, typename P), typename RT>
bool RegisterCommand(ICommandManager* pCmdMgr, const char* szModule, const char* szName, const CCommandDescription& description, const Functor3wRet<LIST(3, P), RT>& functor);
template<LIST(4, typename P)>
bool RegisterCommand(ICommandManager* pCmdMgr, const char* szModule, const char* szName, const CCommandDescription& description, const Functor4<LIST(4, P)>& functor);
template<LIST(4, typename P), typename RT>
bool RegisterCommand(ICommandManager* pCmdMgr, const char* szModule, const char* szName, const CCommandDescription& description, const Functor4wRet<LIST(4, P), RT>& functor);
template<LIST(5, typename P)>
bool RegisterCommand(ICommandManager* pCmdMgr, const char* szModule, const char* szName, const CCommandDescription& description, const Functor5<LIST(5, P)>& functor);

namespace Private
{
template<typename FunctorType, typename CommandType>
bool        RegisterCommand(ICommandManager* pCmdMgr, const char* szModule, const char* szName, const CCommandDescription& description, const FunctorType& functor);

inline void DefaultDelete(void* p)
{
	delete p;
}
}
}

template<typename FunctorType, typename CommandType>
bool CommandManagerHelper::Private::RegisterCommand(ICommandManager* pCmdMgr, const char* szModule, const char* szName, const CCommandDescription& description, const FunctorType& functor)
{
	CRY_ASSERT(functor);
	if (!functor)
	{
		return false;
	}

	CommandType* const pCommand = new CommandType(szModule, szName, description, functor);

	if (pCmdMgr->AddCommand(pCommand, DefaultDelete) == false)
	{
		DefaultDelete(pCommand);
		return false;
	}
	return true;
}

inline bool CommandManagerHelper::RegisterCommand(ICommandManager* pCmdMgr, const char* szModule, const char* szName, const CCommandDescription& description, const Functor0& functor)
{
	return Private::RegisterCommand<Functor0, CCommand0>(pCmdMgr, szModule, szName, description, functor);
}

template<typename RT>
bool CommandManagerHelper::RegisterCommand(ICommandManager* pCmdMgr, const char* szModule, const char* szName, const CCommandDescription& description, const Functor0wRet<RT>& functor)
{
	return Private::RegisterCommand<Functor0wRet<RT>, CCommand0wRet<RT>>(pCmdMgr, szModule, szName, description, functor);
}

template<LIST(1, typename P)>
bool CommandManagerHelper::RegisterCommand(ICommandManager* pCmdMgr, const char* szModule, const char* szName, const CCommandDescription& description, const Functor1<LIST(1, P)>& functor)
{
	return Private::RegisterCommand<Functor1<LIST(1, P)>, CCommand1<LIST(1, P)>>(pCmdMgr, szModule, szName, description, functor);
}

template<LIST(1, typename P), typename RT>
bool CommandManagerHelper::RegisterCommand(ICommandManager* pCmdMgr, const char* szModule, const char* szName, const CCommandDescription& description, const Functor1wRet<LIST(1, P), RT>& functor)
{
	return Private::RegisterCommand<Functor1wRet<LIST(1, P), RT>, CCommand1wRet<LIST(1, P), RT>>(pCmdMgr, szModule, szName, description, functor);
}

template<LIST(2, typename P)>
bool CommandManagerHelper::RegisterCommand(ICommandManager* pCmdMgr, const char* szModule, const char* szName, const CCommandDescription& description, const Functor2<LIST(2, P)>& functor)
{
	return Private::RegisterCommand<Functor2<LIST(2, P)>, CCommand2<LIST(2, P)>>(pCmdMgr, szModule, szName, description, functor);
}

template<LIST(2, typename P), typename RT>
bool CommandManagerHelper::RegisterCommand(ICommandManager* pCmdMgr, const char* szModule, const char* szName, const CCommandDescription& description, const Functor2wRet<LIST(2, P), RT>& functor)
{
	return Private::RegisterCommand<Functor2wRet<LIST(2, P), RT>, CCommand2wRet<LIST(2, P), RT>>(pCmdMgr, szModule, szName, description, functor);
}

template<LIST(3, typename P)>
bool CommandManagerHelper::RegisterCommand(ICommandManager* pCmdMgr, const char* szModule, const char* szName, const CCommandDescription& description, const Functor3<LIST(3, P)>& functor)
{
	return Private::RegisterCommand<Functor3<LIST(3, P)>, CCommand3<LIST(3, P)>>(pCmdMgr, szModule, szName, description, functor);
}

template<LIST(3, typename P), typename RT>
bool CommandManagerHelper::RegisterCommand(ICommandManager* pCmdMgr, const char* szModule, const char* szName, const CCommandDescription& description, const Functor3wRet<LIST(3, P), RT>& functor)
{
	return Private::RegisterCommand<Functor3wRet<LIST(3, P), RT>, CCommand3wRet<LIST(3, P), RT>>(pCmdMgr, szModule, szName, description, functor);
}

template<LIST(4, typename P)>
bool CommandManagerHelper::RegisterCommand(ICommandManager* pCmdMgr, const char* szModule, const char* szName, const CCommandDescription& description, const Functor4<LIST(4, P)>& functor)
{
	return Private::RegisterCommand<Functor4<LIST(4, P)>, CCommand4<LIST(4, P)>>(pCmdMgr, szModule, szName, description, functor);
}

template<LIST(4, typename P), typename RT>
bool CommandManagerHelper::RegisterCommand(ICommandManager* pCmdMgr, const char* szModule, const char* szName, const CCommandDescription& description, const Functor4wRet<LIST(4, P), RT>& functor)
{
	return Private::RegisterCommand<Functor4wRet<LIST(4, P), RT>, CCommand4wRet<LIST(4, P), RT>>(pCmdMgr, szModule, szName, description, functor);
}

template<LIST(5, typename P)>
bool CommandManagerHelper::RegisterCommand(ICommandManager* pCmdMgr, const char* szModule, const char* szName, const CCommandDescription& description, const Functor5<LIST(5, P)>& functor)
{
	return Private::RegisterCommand<Functor5<LIST(5, P)>, CCommand5<LIST(5, P)>>(pCmdMgr, szModule, szName, description, functor);
}

//////////////////////////////////////////////////////////////////////////

typedef CAutoRegister<ICommandManager> CAutoRegisterCommandHelper;

#define REGISTER_EDITOR_COMMAND(functionPtr, moduleName, functionName, description)                                                             \
	namespace Private_CryCommand                                                                                                                  \
	{                                                                                                                                             \
	void RegisterCommand ## moduleName ## functionName()                                                                                          \
	{                                                                                                                                             \
		CommandManagerHelper::RegisterCommand(GetIEditor()->GetICommandManager(), # moduleName, # functionName, description, functor(functionPtr)); \
	}                                                                                                                                             \
	CAutoRegisterCommandHelper g_AutoRegCmdHelper ## moduleName ## functionName(RegisterCommand ## moduleName ## functionName);                   \
	}

#define KEYBOARD_FOCUS_COMMAND_CALLBACK(moduleName, functionName)         \
	namespace Private_EditorFocusCommand                                    \
	{                                                                       \
	void EditorFocusCommand ## moduleName ## functionName()                 \
	{                                                                       \
		CommandEvent( # moduleName "." # functionName).SendToKeyboardFocus(); \
	}                                                                       \
	}                                                                       \

#define REGISTER_EDITOR_AND_SCRIPT_KEYBOARD_FOCUS_COMMAND(moduleName, functionName, description)                                               \
	KEYBOARD_FOCUS_COMMAND_CALLBACK(moduleName, functionName)                                                                                    \
	REGISTER_EDITOR_COMMAND(Private_EditorFocusCommand::EditorFocusCommand ## moduleName ## functionName, moduleName, functionName, description) \
	REGISTER_ONLY_PYTHON_COMMAND(Private_EditorFocusCommand::EditorFocusCommand ## moduleName ## functionName, moduleName, functionName, description.GetDescription())

#define REGISTER_EDITOR_AND_SCRIPT_COMMAND(functionPtr, moduleName, functionName, description) \
	REGISTER_EDITOR_COMMAND(functionPtr, moduleName, functionName, description)                  \
	REGISTER_ONLY_PYTHON_COMMAND(functionPtr, moduleName, functionName, description.GetDescription())

#define REGISTER_COMMAND_REMAPPING(oldModuleName, oldFunctionName, newModuleName, newFunctionName)                                                                                                                  \
	namespace Private_CryCommand                                                                                                                                                                                      \
	{                                                                                                                                                                                                                 \
	void RemapCommand ## oldModuleName ## oldFunctionName ## newModuleName ## newFunctionName()                                                                                                                       \
	{                                                                                                                                                                                                                 \
		ICommandManager* pCmdMgr = GetIEditor()->GetICommandManager();                                                                                                                                                  \
		pCmdMgr->RemapCommand( # oldModuleName, # oldFunctionName, # newModuleName, # newFunctionName);                                                                                                                 \
	}                                                                                                                                                                                                                 \
	CAutoRegisterCommandHelper g_AutoRegCmdRemapHelper ## oldModuleName ## oldFunctionName ## newModuleName ## newFunctionName(RemapCommand ## oldModuleName ## oldFunctionName ## newModuleName ## newFunctionName); \
	}

#define REGISTER_EDITOR_COMMAND_MODULE(moduleName, moduleUiName, description)                             \
	namespace Private_CryCommand                                                                            \
	{                                                                                                       \
	void RegisterCommandModule ## moduleName()                                                              \
	{                                                                                                       \
		static CCommandModuleDescription s_desc( # moduleName, moduleUiName, description);                    \
		ICommandManager* cmdMgr = GetIEditor()->GetICommandManager();                                         \
		cmdMgr->AddCommandModule(&s_desc);                                                                    \
	}                                                                                                       \
	CAutoRegisterCommandHelper g_AutoRegCmdModuleHelper ## moduleName(RegisterCommandModule ## moduleName); \
	}

#define REGISTER_EDITOR_COMMAND_MODULE_WITH_DESC(moduleDescType, moduleName, moduleUiName, description)   \
	namespace Private_CryCommand                                                                            \
	{                                                                                                       \
	void RegisterCommandModule ## moduleName()                                                              \
	{                                                                                                       \
		static moduleDescType s_desc( # moduleName, moduleUiName, description);                               \
		ICommandManager* cmdMgr = GetIEditor()->GetICommandManager();                                         \
		cmdMgr->AddCommandModule(&s_desc);                                                                    \
	}                                                                                                       \
	CAutoRegisterCommandHelper g_AutoRegCmdModuleHelper ## moduleName(RegisterCommandModule ## moduleName); \
	}

typedef CAutoRegister<CUiCommand::UiInfo> CAutoRegisterUiCommandHelper;

#define REGISTER_EDITOR_UI_COMMAND_DESC(moduleName, functionName, buttonText, shortcut, icon, checkable)                          \
	namespace Private_CryUiCommandDesc                                                                                              \
	{                                                                                                                               \
	void RegisterUiDesc ## moduleName ## functionName()                                                                             \
	{                                                                                                                               \
		CUiCommand::UiInfo s_uiInfo(buttonText, icon, shortcut, checkable);                                                           \
		ICommandManager* cmdMgr = GetIEditor()->GetICommandManager();                                                                 \
		cmdMgr->SetUiDescription( # moduleName, # functionName, s_uiInfo);                                                            \
	}                                                                                                                               \
	CAutoRegisterUiCommandHelper g_AutoRegUIDescHelper ## moduleName ## functionName(RegisterUiDesc ## moduleName ## functionName); \
	}

#define REGISTER_EDITOR_COMMAND_SHORTCUT(moduleName, functionName, shortcut) REGISTER_EDITOR_UI_COMMAND_DESC(moduleName, functionName, "", shortcut, "", false)
#define REGISTER_EDITOR_COMMAND_ICON(moduleName, functionName, icon)         REGISTER_EDITOR_UI_COMMAND_DESC(moduleName, functionName, "", CKeyboardShortcut(), icon, false)
#define REGISTER_EDITOR_COMMAND_TEXT(moduleName, functionName, buttonText)   REGISTER_EDITOR_UI_COMMAND_DESC(moduleName, functionName, buttonText, CKeyboardShortcut(), "", false)
