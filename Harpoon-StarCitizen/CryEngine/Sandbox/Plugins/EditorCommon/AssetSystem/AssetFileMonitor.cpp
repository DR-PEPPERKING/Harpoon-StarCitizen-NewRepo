// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AssetFileMonitor.h"

#include "AssetSystem/AssetManager.h"
#include "Loader/AssetLoaderBackgroundTask.h"
#include "Loader/AssetLoaderHelpers.h"
#include "PathUtils.h"
#include "QtUtil.h"
#include <IEditor.h>

#include <CrySystem/File/ICryPak.h>
#include <CryString/CryPath.h>

namespace AssetManagerHelpers
{

void CAssetFileMonitor::RegisterFileListener()
{
	static CAssetFileMonitor theInstance;
}

void CAssetFileMonitor::OnFileChange(const char* szFilename, EChangeType changeType)
{
	// Ignore events for files outside of the current game folder (which is essentially the asset root folder).
	if (GetISystem()->GetIPak()->IsAbsPath(szFilename))
	{
		return;
	}

	const char* const szExt = PathUtil::GetExt(szFilename);
	if (stricmp(szExt, "cryasset"))
	{
		return;
	}

	// TODO: Check if we need this.
	if (!strcmp(szFilename, "_tmp"))
	{
		return;
	}

	CAssetManager* const pAssetManager = CAssetManager::GetInstance();
	if (!pAssetManager)
	{
		return;
	}

	const string assetPath = PathUtil::MakeGamePath(szFilename); // Relative to asset directory.

	switch (changeType)
	{
	case IFileChangeListener::eChangeType_Deleted:
	case IFileChangeListener::eChangeType_RenamedOldName:
		m_fileQueue.ProcessItemAsync(assetPath, [](const string& assetPath)
		{
			if (!GetISystem()->GetIPak()->IsFileExist(assetPath))
			{
				RemoveAsset(assetPath);
			}
			return true;
		});
		break;
	case IFileChangeListener::eChangeType_Created:
	case IFileChangeListener::eChangeType_Modified:
	case IFileChangeListener::eChangeType_RenamedNewName:
		m_fileQueue.ProcessItemAsync(assetPath, [](const string& assetPath)
		{
			// It can be that the file is still being opened for writing.
			if (IsFileOpened(assetPath))
			{
				// Try again
				return false;
			}
			LoadAsset(assetPath);
			return true;
		});
		break;
	}
}

void CAssetFileMonitor::RemoveAsset(const string& assetPath)
{
	CAsset* const pAsset = CAssetManager::GetInstance()->FindAssetForMetadata(assetPath);
	if (!pAsset)
	{
		return;
	}

	CAssetManager::GetInstance()->DeleteAssetsOnlyFromData({ pAsset });
}

void CAssetFileMonitor::LoadAsset(const string& assetPath)
{
	CAssetPtr pAsset = AssetLoader::CAssetFactory::LoadAssetFromXmlFile(assetPath);
	if (!pAsset)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "%s: Cannot load asset '%s'", __FUNCTION__, assetPath.c_str());
		return;
	}

	CAssetManager::GetInstance()->MergeAssets({ pAsset.ReleaseOwnership() });
}

CAssetFileMonitor::CAssetFileMonitor()
{
	GetIEditor()->GetFileMonitor()->RegisterListener(this, "", "cryasset");
}

}
