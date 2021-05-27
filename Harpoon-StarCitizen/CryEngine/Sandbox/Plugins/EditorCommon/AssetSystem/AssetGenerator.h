// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include "AssetManagerHelpers.h"
#include <CrySystem/File/IFileChangeMonitor.h>
#include <CryRenderer/IRenderer.h>
#include <CryCore/ToolsHelpers/ResourceCompilerHelper.h>

#include <mutex>

class CProgressNotification;

namespace AssetManagerHelpers
{

// ! Creates and updates cryassets for asset data files.
class CAssetGenerator : public IFileChangeListener, public IAsyncTextureCompileListener
{
public:
	static void RegisterFileListener();

	virtual void OnFileChange(const char* szFilename, EChangeType changeType) override;

	virtual void OnCompilationStarted(const char* szSource, const char* szTarget, int nPending) override;
	virtual void OnCompilationFinished(const char* szSource, const char* szTarget, ERcExitCode eReturnCode) override;
	virtual void OnCompilationQueueTriggered(int nPending) override;
	virtual void OnCompilationQueueDepleted() override;

	void GenerateCryasset(const string& filePath, const string& destFolder = "");

	//! Generates/repair *.cryasset files for the current project.
	static bool GenerateCryassets();

private:
	CAssetGenerator();

private:
	CProcessingQueue m_fileQueue;
	std::unique_ptr<CProgressNotification> m_pProgress;
	std::unique_ptr<CProgressNotification> m_pTextureCompilerProgress;
	string m_rcSettings;
	std::mutex m_textureCompilerMutex;
};

//! Generates/repair *.cryasset files for the asset file. 
//! \param filePath string with path to the data file. The path can be absolute or relative to the project assets root folder.
EDITOR_COMMON_API void GenerateCryasset(const string& filePath);

};
