// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

//  Description: Python stabs for ui actions

#include <StdAfx.h>

#include "IEditorImpl.h"
#include "CryEdit.h"
#include "QtMainFrame.h"
#include "Util/BoostPythonHelpers.h"

#include <EditorFramework/Preferences.h>
#include <CryAudio/IAudioSystem.h>

#include <QApplication>
#include <QtEvents>

class QAction;

DECLARE_PYTHON_MODULE(ui_action);

extern CCryEditApp theApp;
namespace
{
CCryEditApp* GetEditApp() { return &theApp; }
}

class UiActionCommandModuleDescription : public CCommandModuleDescription
{
public:

	UiActionCommandModuleDescription(const char* name, const char* uiName, const char* description)
		: CCommandModuleDescription(name, uiName, description)
	{}

	virtual string FormatCommandForUI(const char* command) const override
	{
		string forUi(command);

		if (forUi.find("action") == 0)
		{
			forUi = forUi.substr(6);
		}

		return CCommandModuleDescription::FormatCommandForUI(forUi);
	}
};

REGISTER_EDITOR_COMMAND_MODULE_WITH_DESC(UiActionCommandModuleDescription, ui_action, "Menus and Toolbars", "");

#define REGISTER_EDIT_APP_UI_ACTION(EditAppMemberFuncName, action_name)                  \
	void PyBind_ui_action_ ## action_name() { GetEditApp()-> ## EditAppMemberFuncName(); } \
	REGISTER_PYTHON_COMMAND(PyBind_ui_action_ ## action_name, ui_action, action_name, "");

REGISTER_EDIT_APP_UI_ACTION(OnFileEditLogFile, actionShow_Log_File);

REGISTER_EDIT_APP_UI_ACTION(OnResourcesReduceworkingset, actionReduce_Working_Set);

REGISTER_EDIT_APP_UI_ACTION(OnRuler, actionRuler);

namespace
{
void PyOpenViewPane(const char* viewClassName)
{
	GetIEditorImpl()->OpenView(viewClassName);
}

std::vector<std::string> PyGetViewPaneClassNames()
{
	IEditorClassFactory* pClassFactory = GetIEditorImpl()->GetClassFactory();
	std::vector<IClassDesc*> classDescs;
	pClassFactory->GetClassesBySystemID(ESYSTEM_CLASS_VIEWPANE, classDescs);

	std::vector<std::string> classNames;

	for (auto const& classDesc : classDescs)
	{
		classNames.emplace_back(classDesc->ClassName());
	}

	return classNames;
}

void PyExit()
{
	if (CEditorMainFrame::m_pInstance)
	{
		CEditorMainFrame::m_pInstance->window()->close();
	}
	else
	{
		qApp->exit();
	}
}
}

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyExit, general, exit,
                                     "Exits the editor.",
                                     "general.exit()");
REGISTER_EDITOR_UI_COMMAND_DESC(general, exit, "Exit", "", "icons:General/Folder_Open.ico", false)

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyOpenViewPane, general, open_pane,
                                     "Opens a view pane specified by the pane class name.",
                                     "general.open_pane(str paneClassName)");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetViewPaneClassNames, general, get_pane_class_names,
                                          "Get all available class names for use with open_pane & close_pane.",
                                          "[str] general.get_pane_class_names()");

//////////////////////////////////////////////////////////////////////////
//TODO : move this out of python files, this does not belong here
struct SAudioGeneralPreferences : public SPreferencePage
{
	SAudioGeneralPreferences()
		: SPreferencePage("General", "Audio/General")
		, bMuteAudio(false)
	{
	}

	virtual bool Serialize(yasli::Archive& ar) override
	{
		ar.openBlock("general", "General");
		ar(bMuteAudio, "muteAudio", "Mute Audio");
		ar.closeBlock();

		if (ar.isInput())
		{
			if (bMuteAudio)
			{
				GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_AUDIO_MUTE, 0, 0);
			}
			else
			{
				GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_AUDIO_UNMUTE, 0, 0);
			}
		}

		return true;
	}

	bool bMuteAudio;
};

SAudioGeneralPreferences gAudioGeneralPreferences;
REGISTER_PREFERENCES_PAGE_PTR(SAudioGeneralPreferences, &gAudioGeneralPreferences)

//////////////////////////////////////////////////////////////////////////

namespace
{
//////////////////////////////////////////////////////////////////////////
void PyStopAllSounds()
{
	CryLogAlways("<Audio> Executed \"Stop All Sounds\" command.");
	gEnv->pAudioSystem->StopAllSounds();
}

//////////////////////////////////////////////////////////////////////////
void PyRefreshAudioSystem()
{
	GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_AUDIO_REFRESH, reinterpret_cast<UINT_PTR>(GetIEditorImpl()->GetLevelName()), 0);
}

//////////////////////////////////////////////////////////////////////////
void PyToggleMuteAudioSystem()
{
	gAudioGeneralPreferences.bMuteAudio = !gAudioGeneralPreferences.bMuteAudio;

	if (gAudioGeneralPreferences.bMuteAudio)
	{
		GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_AUDIO_MUTE, 0, 0);
	}
	else
	{
		GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_AUDIO_UNMUTE, 0, 0);
	}
}
};

REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyStopAllSounds, ui_action, actionStop_All_Sounds, "Stop all playing sounds.", "");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyRefreshAudioSystem, ui_action, actionRefresh_Audio, "Send refresh event to the audio system.", "");
REGISTER_PYTHON_COMMAND_WITH_EXAMPLE(PyToggleMuteAudioSystem, ui_action, actionMute_Audio, "Toggle Mute of the Sound System.", "");

namespace
{
void PyExecute(const char* pythonCmd)
{
	PyScript::Execute(pythonCmd);
}
}

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyExecute, python, execute, "Executes python code", "");
