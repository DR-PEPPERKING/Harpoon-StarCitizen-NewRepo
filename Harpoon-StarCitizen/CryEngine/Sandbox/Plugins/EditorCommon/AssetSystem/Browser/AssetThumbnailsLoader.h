// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <AssetSystem/Asset.h>
#include <CryCore/Containers/MiniQueue.h>
#include <CrySandbox/CrySignal.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <unordered_set>

class CAsset;
struct ICVar;

class CAssetThumbnailsLoader
{
private:
	class CAssetThumbnailsFinalizer;
	friend CAssetThumbnailsFinalizer;

public:
	static CAssetThumbnailsLoader& GetInstance();

	//! Enqueues an asset for thumbnail loading.
	void PostAsset(CAsset* pAsset);

	//! Increases priority of an asset for thumbnail loading. Request is ignored when thumbnail of asset is not being loaded.
	void TouchAsset(CAsset* pAsset);

	//! Attach to this signal to get notified when an asset's thumbnail has been loaded.
	//! Thumbnails are loaded lazily and only loaded once they need to be displayed.
	CCrySignal<void(CAsset&)> signalAssetThumbnailLoaded;

private:
	CAssetThumbnailsLoader();

	void ThreadFunc();

	void SetGenerateThumbnais(bool bValue) { m_bGenerateThumbnais = bValue; }
	bool GetGenerateThumbnais() const { return m_bGenerateThumbnais; }

	static void OnCVarChanged(ICVar* const pCVar);

	void NotifyThumbnailLoaded(CAsset& asset);

private:
	//! Defines size of the loader ring buffer. 
	//! The optimal value should be slightly larger than the usual number of thumbnails displayed in one view.
	static const uint8 m_queueSize = 128;

	std::unique_ptr<CAssetThumbnailsFinalizer> m_pFinalizer;
	std::vector<std::unique_ptr<std::thread>> m_threads;
	std::condition_variable m_conditionVariable;
	MiniQueue<CAssetPtr, m_queueSize> m_assets;
	std::mutex m_assetsMutex;
	std::unordered_set<CAsset*> m_activeRequests;

	volatile bool m_bGenerateThumbnais;
};
