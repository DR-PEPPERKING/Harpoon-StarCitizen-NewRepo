// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameTokenManager.h"

#include <CryGame/IGameTokens.h>
#include "GameTokenItem.h"
#include "GameTokenLibrary.h"

#include "GameEngine.h"

#include "DataBaseDialog.h"
#include "GameTokenDialog.h"
#include <Util/EditorUtils.h>
#include <Util/FileUtil.h>
#include <Util/TempFileHelper.h>

#define GAMETOCKENS_LIBS_PATH "Libs/GameTokens/"

namespace Private_GameTokenManager
{
const char* kGameTokensFile = "GameTokens.xml";
const char* kGameTokensRoot = "GameTokens";
};

//////////////////////////////////////////////////////////////////////////
// CGameTokenManager implementation.
//////////////////////////////////////////////////////////////////////////
CGameTokenManager::CGameTokenManager()
{
	m_pLevelLibrary = (CBaseLibrary*)AddLibrary("Level");
	m_pLevelLibrary->SetLevelLibrary(true);
	m_bUniqGuidMap = false;
	m_bUniqNameMap = true;
}

//////////////////////////////////////////////////////////////////////////
CGameTokenManager::~CGameTokenManager()
{
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenManager::ClearAll()
{
	CBaseLibraryManager::ClearAll();
	m_pLevelLibrary = (CBaseLibrary*)AddLibrary("Level");
	m_pLevelLibrary->SetLevelLibrary(true);
}

//////////////////////////////////////////////////////////////////////////
CBaseLibraryItem* CGameTokenManager::MakeNewItem()
{
	return new CGameTokenItem;
}
//////////////////////////////////////////////////////////////////////////
CBaseLibrary* CGameTokenManager::MakeNewLibrary()
{
	return new CGameTokenLibrary(this);
}
//////////////////////////////////////////////////////////////////////////
string CGameTokenManager::GetRootNodeName()
{
	return "GameTokensLibrary";
}
//////////////////////////////////////////////////////////////////////////
string CGameTokenManager::GetLibsPath()
{
	if (m_libsPath.IsEmpty())
		m_libsPath = GAMETOCKENS_LIBS_PATH;
	return m_libsPath;
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenManager::Export(XmlNodeRef& node)
{
	XmlNodeRef libs = node->newChild("GameTokensLibraryReferences");
	for (int i = 0; i < GetLibraryCount(); i++)
	{
		IDataBaseLibrary* pLib = GetLibrary(i);
		// Level libraries are saved in level.
		if (pLib->IsLevelLibrary())
			continue;
		// For Non-Level libs (Global) we save a reference (name)
		// as these are stored in the Game/Libs directory
		XmlNodeRef libNode = libs->newChild("Library");
		libNode->setAttr("Name", pLib->GetName());
	}
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenManager::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	__super::OnEditorNotifyEvent(event);

	switch (event)
	{
	case eNotify_OnBeginGameMode:
	case eNotify_OnBeginSimulationMode:
		// ensure all game tokens are set with their default values on game start.
		// this should not send a notification to other systems like FG
		{
			IGameTokenSystem* pGTS = GetIEditor()->GetGameEngine()->GetIGameTokenSystem();

			pGTS->SetGoingIntoGame(true); // so that GTS knows not to send token changed notifications at this point

			// set all game tokens to their default values
			for (ItemsNameMap::iterator it = m_itemsNameMap.begin(); it != m_itemsNameMap.end(); ++it)
			{
				CGameTokenItem* pToken = (CGameTokenItem*)&*it->second;

				TFlowInputData data;
				pToken->GetValue(data);
				pToken->SetValue(data, true);
			}

			pGTS->TriggerTokensAsChanged();

			pGTS->SetGoingIntoGame(false);
		}
		break;
	}
}

const char* CGameTokenManager::GetDataFilename() const
{
	return Private_GameTokenManager::kGameTokensFile;
}

//////////////////////////////////////////////////////////////////////////
void CGameTokenManager::Save(bool bBackup)
{
	using namespace Private_GameTokenManager;
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	CTempFileHelper helper(GetIEditorImpl()->GetLevelDataFolder() + kGameTokensFile);

	XmlNodeRef root = XmlHelpers::CreateXmlNode(kGameTokensRoot);
	Serialize(root, false);
	root->saveToFile(helper.GetTempFilePath());

	helper.UpdateFile(bBackup);
}

//////////////////////////////////////////////////////////////////////////
bool CGameTokenManager::Load()
{
	using namespace Private_GameTokenManager;
	string filename = GetIEditorImpl()->GetLevelDataFolder() + kGameTokensFile;
	XmlNodeRef root = XmlHelpers::LoadXmlFromFile(filename);
	if (root && !stricmp(root->getTag(), kGameTokensRoot))
	{
		Serialize(root, true);
		return true;
	}
	return false;
}
