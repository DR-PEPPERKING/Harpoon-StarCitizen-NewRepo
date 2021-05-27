// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AssetManagerHelpers.h"

#include "AssetSystem/AssetManager.h"
#include "Loader/AssetLoaderBackgroundTask.h"
#include "Loader/AssetLoaderHelpers.h"
#include "Notifications/NotificationCenter.h"
#include "PathUtils.h"
#include "QtUtil.h"
#include "ThreadingUtils.h"

#include <CrySystem/IProjectManager.h>
#include <CrySystem/File/ICryPak.h>
#include <CryCore/ToolsHelpers/ResourceCompilerHelper.inl>
#include <CryCore/ToolsHelpers/SettingsManagerHelpers.inl>
#include <CryCore/ToolsHelpers/EngineSettingsManager.inl>
#include <CryString/CryPath.h>

#include <QTimer>
#include <deque>

namespace AssetManagerHelpers
{

bool IsFileOpened(const string& path)
{
	if (!gEnv->pCryPak->IsFileExist(path, ICryPak::eFileLocation_OnDisk))
	{
		return false;
	}

	FILE* pFile = GetISystem()->GetIPak()->FOpen(path, "r", ICryPak::FOPEN_ONDISK | ICryPak::FOPEN_LOCKED_OPEN);
	if (!pFile)
	{
		return true;
	}
	GetISystem()->GetIPak()->FClose(pFile);
	return false;
}

std::vector<string> GetListOfMetadataFiles(const std::vector<CAsset*>& assets)
{
	std::vector<string> result;
	result.reserve(assets.size());
	for (CAsset* pAsset : assets)
	{
		result.push_back(pAsset->GetMetadataFile());
	}

	return result;
}

QStringList GetUiNamesFromAssetTypeNames(const std::vector<string>& typeNames)
{
	CAssetManager* const pManager = CAssetManager::GetInstance();
	QStringList uiNames;
	uiNames.reserve(typeNames.size());
	for (const string& typeName : typeNames)
	{
		CAssetType* const pType = pManager->FindAssetType(typeName.c_str());
		if (pType)
		{
			uiNames.push_back(pType->GetUiTypeName());
		}
	}
	return uiNames;
}

QStringList GetUiNamesFromAssetTypes(const std::vector<CAssetType*>& types)
{
	QStringList uiNames;
	uiNames.reserve(types.size());
	std::transform(types.cbegin(), types.cend(), std::back_inserter(uiNames), [](const CAssetType* pType)
	{
		return pType->GetUiTypeName();
	});
	return uiNames;
}

std::vector<CAssetType*> GetAssetTypesFromTypeNames(const std::vector<string>& typeNames)
{
	const CAssetManager* const pManager = CAssetManager::GetInstance();
	std::vector<CAssetType*> types;
	types.reserve(typeNames.size());

	for (const string& typeName : typeNames)
	{
		CAssetType* const pAssetType = pManager->FindAssetType(typeName);
		if (!pAssetType)
		{
			continue;
		}
		types.push_back(pAssetType);
	}
	return types;
}

void RCLogger::OnRCMessage(MessageSeverity severity, const char* szText)
{
	if (severity == MessageSeverity_Error)
	{
		CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_ERROR, "RC: %s", szText);
	}
	else if (severity == MessageSeverity_Warning)
	{
		CryWarning(EValidatorModule::VALIDATOR_MODULE_EDITOR, EValidatorSeverity::VALIDATOR_WARNING, "RC: %s", szText);
	}
}

void CProcessingQueue::ProcessItemAsync(const string& key, Predicate fn)
{
	if (m_queue.empty())
	{
		m_queue.emplace_back(key, std::move(fn));
		ProcessQueue();
	}
	else // When the queue is not empty, a previous call to ProcessQueue() has already triggered the processing.
	{
		m_queue.emplace_back(key, std::move(fn));
	}
}

void CProcessingQueue::ProcessItemUniqueAsync(const string& key, Predicate fn)
{
	const bool bIsQueuedUp = std::any_of(m_queue.begin(), m_queue.end(), [&key](const auto& x)
	{
		return key.CompareNoCase(x.first) == 0;
	});

	if (bIsQueuedUp)
	{
		return;
	}

	ProcessItemAsync(key, std::move(fn));
}

void CProcessingQueue::ProcessQueue()
{
	if (m_queue.empty())
	{
		return;
	}

	auto path = m_queue.front();
		
	// Call the function object.
	const bool isDone = path.second(path.first);
	if (isDone)
	{
		m_queue.pop_front();
	}

	if (!m_queue.empty())
	{
		// Try in 0.5 seconds if the item has not been processed.
		const int timeout = isDone ? 0 : 500;
		QTimer::singleShot(timeout, [this]()
		{
			ProcessQueue();
		});
	}
}

void CAsyncFileListener::OnFileChange(const char* szFilename, EChangeType eType)
{
	// Ignore events for files outside of the current game folder.
	if (GetISystem()->GetIPak()->IsAbsPath(szFilename))
	{
		return;
	}

	if (AcceptFile(szFilename, eType))
	{
		const string assetPath(szFilename);
		m_fileQueue.ProcessItemUniqueAsync(assetPath, [this, eType](const string& assetPath)
		{
			// It can be that the file is still being opened for writing.
			if (AssetManagerHelpers::IsFileOpened(assetPath))
			{
				// Try again
				return false;
			}

			return ProcessFile(assetPath.c_str(), eType);
		});
	}
}

}
