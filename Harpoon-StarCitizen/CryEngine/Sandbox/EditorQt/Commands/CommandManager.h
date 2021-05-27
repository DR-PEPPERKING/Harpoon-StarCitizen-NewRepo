// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Commands/ICommandManager.h>

class CCustomCommand;
class QCommandAction;

class CEditorCommandManager : public ICommandManager
{
public:
	enum
	{
		CUSTOM_COMMAND_ID_FIRST = 10000,
		CUSTOM_COMMAND_ID_LAST  = 15000
	};

	CEditorCommandManager();
	~CEditorCommandManager();

	void                             RegisterAutoCommands();

	bool                             AddCommand(CCommand* pCommand, TPfnDeleter deleter = NULL) override;
	bool                             AddCommandModule(CCommandModuleDescription* pCommand) override;
	bool                             UnregisterCommand(const char* cmdFullName) override;
	string                           Execute(const string& cmdLine) override;
	string                           Execute(const string& module, const string& name, const CCommand::CArgs& args);
	void                             GetCommandList(std::vector<string>& cmds) const override;
	void                             GetCommandList(std::vector<CCommand*>& cmds) const override;
	void                             RemapCommand(const char* oldModule, const char* oldName, const char* newModule, const char* newName) override;

	bool                             IsCommandDeprecated(const char* cmdFullName) const override;
	const CCommandModuleDescription* FindOrCreateModuleDescription(const char* moduleName) override;
	virtual CCommand*                GetCommand(const char* cmdFullName) const override;
	virtual void                     SetChecked(const char* cmdFullName, bool checked) override;
	virtual QAction*                 GetAction(const char* cmdFullName, const char* text = nullptr) const override;
	virtual QCommandAction*          CreateNewAction(const char* cmdFullName) const override;
	virtual QCommandAction*          GetCommandAction(const char* command, const char* text = nullptr) const override;

	void                             SetEditorUIActionsEnabled(bool bEnabled);

	//! Used in the console dialog
	bool IsRegistered(const char* module, const char* name) const override;
	bool IsRegistered(const char* cmdLine) const override;

	//! Turning off the warning is needed for reloading the ribbon bar.
	void            TurnDuplicateWarningOn()  { m_bWarnDuplicate = true; }
	void            TurnDuplicateWarningOff() { m_bWarnDuplicate = false; }

	void            RegisterAction(QCommandAction* action, const string& command);
	void            SetUiDescription(const char* module, const char* name, const CUiCommand::UiInfo& info) const override;

	void            ResetShortcut(const char* commandFullName);
	void            ResetAllShortcuts();

	void            AddCustomCommand(CCustomCommand* pCommand);
	void            RemoveCustomCommand(CCustomCommand* pCommand);
	void            RemoveAllCustomCommands();
	void            GetCustomCommandList(std::vector<CCommand*>& cmds) const;

protected:
	struct SCommandTableEntry
	{
		CCommand*   pCommand;
		TPfnDeleter deleter;
	};

	//! A full command name to an actual command mapping
	typedef std::map<string, SCommandTableEntry> CommandTable;
	CommandTable m_commands;
	CommandTable m_deprecatedCommands;

	typedef std::map<string, CCommandModuleDescription*> CommandModuleTable;
	CommandModuleTable           m_commandModules;

	std::vector<CCustomCommand*> m_CustomCommands;

	bool                         m_bWarnDuplicate;
	bool                         m_areActionsEnabled;

	static string GetFullCommandName(const string& module, const string& name);
	static void   GetArgsFromString(const string& argsTxt, CCommand::CArgs& argList);
	void          LogCommand(const string& fullCmdName, const CCommand::CArgs& args) const;
	string        ExecuteAndLogReturn(CCommand* pCommand, const CCommand::CArgs& args);
};
