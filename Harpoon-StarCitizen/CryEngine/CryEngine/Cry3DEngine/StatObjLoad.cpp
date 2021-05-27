// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   statobjconstr.cpp
//  Version:     v1.00
//  Created:     28/5/2001 by Vladimir Kajalin
//  Compilers:   Visual Studio.NET
//  Description: loading
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"

#include "StatObj.h"
#include "IndexedMesh.h"
#include "MatMan.h"

#include "CGF/CGFLoader.h"
#include "CGF/CGFSaver.h"
#include "CGF/ReadOnlyChunkFile.h"

#include <CryMemory/CryMemoryManager.h>

#define GEOM_INFO_FILE_EXT      "ginfo"
#define MESH_NAME_FOR_MAIN      "main"
#define PHYSICS_BREAKABLE_JOINT "$joint"
#define PHYSICS_COMPILED_JOINTS "$compiled_joints"

void CStatObj::Refresh(int nFlags)
{
	if (nFlags & FRO_GEOMETRY)
	{
		if (nFlags & FRO_FORCERELOAD)
		{
			m_bLodsLoaded = false;
		}

		if (m_bCheckGarbage)
		{
			GetObjManager()->UnregisterForGarbage(this);
		}

		const int oldModificationId = m_nModificationId;

		ShutDown();
		Init();
		bool bRes = LoadCGF(m_szFileName, 0);

		// Shutdown/Init sequence might produce same modification id as before, so we make sure to store a different value.
		m_nModificationId = oldModificationId + 1;

		LoadLowLODs(false, 0);
		TryMergeSubObjects(false);

		if (!bRes)
		{
			// load default in case of error
			ShutDown();
			Init();
			LoadCGF(DEFAULT_CGF_NAME, 0);
			m_bDefaultObject = true;
		}

		return;
	}
}

void CStatObj::LoadLowLODs(bool useStreaming, uint32 loadingFlags)
{
	if (!LoadLowLODS_Prepare(loadingFlags))
		return;

	int nLoadedLods = 1;
	CStatObj* loadedLods[MAX_STATOBJ_LODS_NUM] = { nullptr };
	for (int lodLevel = 1; lodLevel < MAX_STATOBJ_LODS_NUM; lodLevel++)
	{
		CStatObj* pLodStatObj = LoadLowLOD(lodLevel, useStreaming, loadingFlags);
		if (!pLodStatObj)
			break;

		++nLoadedLods;
		loadedLods[lodLevel] = pLodStatObj;
	}

	LoadLowLODS_Finalize(nLoadedLods, loadedLods);
}

CStatObj* CStatObj::LoadLowLOD(int lodLevel, bool useStreaming, uint32 loadingFlags)
{
	char szLodFileName[MAX_PATH];
	MakeLodFileName(lodLevel, szLodFileName);

	CStatObj* pLodStatObj = stl::find_in_map(m_pObjManager->m_nameToObjectMap, CONST_TEMP_STRING(szLodFileName), nullptr);
	if (pLodStatObj)
	{
		pLodStatObj->m_pLod0 = this; // Must be here.

		typedef std::set<CStatObj*> LoadedObjects;
		LoadedObjects::iterator it = m_pObjManager->m_lstLoadedObjects.find(pLodStatObj);
		if (it != m_pObjManager->m_lstLoadedObjects.end())
		{
			m_pObjManager->m_lstLoadedObjects.erase(it);
			m_pObjManager->m_nameToObjectMap.erase(CONST_TEMP_STRING(szLodFileName));
		}
	}
	else if (IsValidFile(szLodFileName))
	{
		pLodStatObj = new CStatObj();
		pLodStatObj->m_pLod0 = this;

		if (useStreaming && GetCVars()->e_StreamCgf)
			pLodStatObj->m_bCanUnload = true;

		pLodStatObj->LoadCGF_Prepare(szLodFileName, true);
		CReadOnlyChunkFile chunkFile(false, false);
		if(!pLodStatObj->LoadCGF(&chunkFile, szLodFileName, true, loadingFlags))
		{
			if (GetLodObject(lodLevel) != pLodStatObj)
				SAFE_RELEASE(pLodStatObj);
			SetLodObject(lodLevel, nullptr);
		}
	}
	return pLodStatObj;
}

class CStatObj::AsyncLodLoader : public IStreamCallback
{
public:
	static void Load(CStatObj* pBaseObject, bool useStreaming, uint32 loadingFlags, IStatObjLoadedCallback* pCallback = nullptr)
	{
		new AsyncLodLoader(pBaseObject, useStreaming, loadingFlags, pCallback); // will delete itself
	}

private:
	AsyncLodLoader(CStatObj* pBaseObject, bool useStreaming, uint32 loadingFlags, IStatObjLoadedCallback* pCallback = nullptr) :
		m_pLODs{}, m_pBaseObject(pBaseObject), m_pCallback(pCallback), m_loadingFlags(loadingFlags), m_streamsRunning(0)
	{
		CObjManager* const pObjManager = Cry3DEngineBase::GetObjManager();
		for (int lodLevel = 1; lodLevel < MAX_STATOBJ_LODS_NUM; ++lodLevel)
		{
			char szLodFileName[MAX_PATH];
			pBaseObject->MakeLodFileName(lodLevel, szLodFileName);

			CStatObj* pLodStatObj = stl::find_in_map(pObjManager->m_nameToObjectMap, CONST_TEMP_STRING(szLodFileName), NULL);
			if (pLodStatObj)
			{
				pLodStatObj->m_pLod0 = pBaseObject; // Must be here.

				auto it = pObjManager->m_lstLoadedObjects.find(pLodStatObj);
				if (it != pObjManager->m_lstLoadedObjects.end())
				{
					pObjManager->m_lstLoadedObjects.erase(it);
					pObjManager->m_nameToObjectMap.erase(CONST_TEMP_STRING(szLodFileName));
				}

				m_pLODs[lodLevel - 1] = pLodStatObj;
			}
			else if (Cry3DEngineBase::IsValidFile(szLodFileName))
			{
				pLodStatObj = new CStatObj();
				pLodStatObj->m_pLod0 = pBaseObject; // Must be here.
				if (useStreaming && Cry3DEngineBase::GetCVars()->e_StreamCgf)
					pLodStatObj->m_bCanUnload = true;

				pLodStatObj->LoadCGF_Prepare(szLodFileName, true);

				m_pLODs[lodLevel - 1] = pLodStatObj;
				StreamReadParams params(lodLevel);
				gEnv->pSystem->GetStreamEngine()->StartRead(eStreamTaskTypeGeometry, szLodFileName, this, &params);
				++m_streamsRunning;
			}
			else
			{
				pBaseObject->SetLodObject(lodLevel, nullptr);
				break;
			}
		}

		if (m_streamsRunning == 0)
			Finalize();
	}

	virtual void StreamOnComplete(IReadStream* pStream, unsigned) override
	{
		uint lodLevel = static_cast<uint>(pStream->GetUserData());
		assert(lodLevel < MAX_STATOBJ_LODS_NUM);
		CStatObj* pLodObject = m_pLODs[lodLevel-1];
		assert(m_streamsRunning != 0);
		--m_streamsRunning;
		
		if (!pStream->IsError())
		{
			CReadOnlyChunkFile chunkFile(false, false);
			if (!chunkFile.ReadFromMemory(pStream->GetBuffer(), pStream->GetBytesRead()))
				LodFailed(lodLevel);
			else
				pLodObject->LoadCGF(&chunkFile, pLodObject->GetFilePath(), true, m_loadingFlags);
		}
		else
			LodFailed(lodLevel);

		if (m_streamsRunning == 0)
			Finalize();
	}

	void Finalize()
	{
		int nLoadedLods = 1;
		CStatObj* loadedLods[MAX_STATOBJ_LODS_NUM] = { 0 };
		int lodLevel = 1;
		// gather the successful LODs
		for (; lodLevel < MAX_STATOBJ_LODS_NUM; ++lodLevel)
		{
			if (m_pLODs[lodLevel - 1] == nullptr)
				break;

			++nLoadedLods;
			loadedLods[lodLevel] = m_pLODs[lodLevel - 1];
		}
		// and remove remaining ones (if any)
		for (; lodLevel < MAX_STATOBJ_LODS_NUM; ++lodLevel)
			LodFailed(lodLevel);

		m_pBaseObject->LoadLowLODS_Finalize(nLoadedLods, loadedLods);

		if (m_pCallback)
			m_pCallback->OnLoaded(true, m_pBaseObject);

		delete this;
	}

	void LodFailed(uint lodLevel)
	{
		if (m_pBaseObject->GetLodObject(lodLevel) != m_pLODs[lodLevel - 1])
			SAFE_RELEASE(m_pLODs[lodLevel - 1])
		else
			m_pLODs[lodLevel - 1] = nullptr;

		m_pBaseObject->SetLodObject(lodLevel, nullptr);
	}

	CStatObj* m_pLODs[MAX_STATOBJ_LODS_NUM - 1];
	CStatObj* m_pBaseObject;
	IStatObjLoadedCallback* m_pCallback;
	uint32    m_loadingFlags;
	uint32    m_streamsRunning;
};

void CStatObj::LoadLowLODsAsync(bool useStreaming, uint32 loadingFlags, IStatObjLoadedCallback* pCallback)
{
	if (!LoadLowLODS_Prepare(loadingFlags))
	{
		if(pCallback)
			pCallback->OnLoaded(false, this);
	}
	else
	{
		AsyncLodLoader::Load(this, useStreaming, loadingFlags, pCallback);
	}
}

bool CStatObj::LoadLowLODS_Prepare(uint32 loadingFlags)
{
	m_bLodsLoaded = true;

	if (loadingFlags & ELoadingFlagsIgnoreLoDs)
	{
		return false;
	}

	if (m_nLoadedLodsNum > 1 && GetFlags() & STATIC_OBJECT_COMPOUND)
	{
		for (int nLodLevel = 1; nLodLevel < MAX_STATOBJ_LODS_NUM; nLodLevel++)
		{
			char sLodFileName[MAX_PATH];
			MakeLodFileName(nLodLevel, sLodFileName);
			
			if (IsValidFile(sLodFileName))
			{
				m_nLoadedLodsNum = 1;
				break;
			}
		}
	}

	if (m_nLoadedLodsNum > 1)
		return false;

	m_nLoadedLodsNum = 1;

	if (!GetCVars()->e_Lods)
		return false;

	if (m_bSubObject) // Never do this for sub objects.
		return false;

	return true;
}

void CStatObj::LoadLowLODS_Finalize(int nLoadedLods, CStatObj* loadedLods[MAX_STATOBJ_LODS_NUM])
{
	// Put LODs into main object
	for (int lodLevel = 1; lodLevel < nLoadedLods; lodLevel++)
	{
		SetLodObject(lodLevel, loadedLods[lodLevel]);

		bool bLodCompound = (loadedLods[lodLevel]->GetFlags() & STATIC_OBJECT_COMPOUND) != 0;
		bool bLod0Compund = (GetFlags() & STATIC_OBJECT_COMPOUND) != 0;
		if (bLodCompound != bLod0Compund)
			FileWarning(0, loadedLods[lodLevel]->GetFilePath(), "Invalid LOD%d: Has different merging property from LOD0", lodLevel);
	}

	// Put LODs into the sub objects.
	if (nLoadedLods > 1)
	{
		m_bLodsAreLoadedFromSeparateFile = true;

		for (int i = 0; i < (int)m_subObjects.size(); i++)
		{
			SSubObject* pSubObject = &m_subObjects[i];
			if (!pSubObject->pStatObj || pSubObject->nType != STATIC_SUB_OBJECT_MESH)
				continue;

			CStatObj* pSubStatObj = (CStatObj*)pSubObject->pStatObj;

			for (int nLodLevel = 1; nLodLevel < nLoadedLods; nLodLevel++)
			{
				if (loadedLods[nLodLevel] != 0 && loadedLods[nLodLevel]->m_nSubObjectMeshCount > 0)
				{
					SSubObject* pLodSubObject = loadedLods[nLodLevel]->FindSubObject(pSubObject->name);
					if (pLodSubObject && pLodSubObject->pStatObj && pLodSubObject->nType == STATIC_SUB_OBJECT_MESH)
					{
						pSubStatObj->SetLodObject(nLodLevel, (CStatObj*)pLodSubObject->pStatObj);
					}
				}
			}
			if (pSubStatObj)
				pSubStatObj->CleanUnusedLods();
		}
	}

	CleanUnusedLods();

	for (int nLodLevel = 0; nLodLevel < MAX_STATOBJ_LODS_NUM; nLodLevel++)
	{
		if (loadedLods[nLodLevel])
		{
			GetObjManager()->CheckForGarbage(loadedLods[nLodLevel]);
		}
	}
}

void CStatObj::MakeLodFileName(uint lod, char (&buffer)[MAX_PATH])
{
	assert(lod < MAX_STATOBJ_LODS_NUM);
	cry_strcpy(buffer, m_szFileName.c_str());
	if (lod > 0) // lod 0 is just the original file name
	{
		char* sPointSeparator = strchr(buffer, '.');
		if (sPointSeparator)
			*sPointSeparator = '\0'; // Terminate at the dot
		cry_strcat(buffer, "_lod");
		char sLodNum[8];
		ltoa(lod, sLodNum, 10);
		cry_strcat(buffer, sLodNum);
	}
	cry_strcat(buffer, ".");
	cry_strcat(buffer, PathUtil::GetExt(m_szFileName));
}

//////////////////////////////////////////////////////////////////////////
// Free render resources for unused upper LODs.
void CStatObj::CleanUnusedLods()
{
	_smart_ptr<IRenderMesh> pNullMesh = nullptr;
	if (m_nLoadedLodsNum > 1)
	{
		int nMinLod = GetMinUsableLod();
		nMinLod = clamp_tpl(nMinLod, 0, (int)m_nLoadedLodsNum - 1);
		for (int i = 0; i < nMinLod; i++)
		{
			CStatObj* pStatObj = (CStatObj*)GetLodObject(i);
			if (!pStatObj)
				continue;

			if (pStatObj->m_pRenderMesh)
			{
				pStatObj->SetRenderMesh(pNullMesh);
			}
		}
	}
}

void TransformMesh(CMesh& mesh, Matrix34 tm)
{
	const int nVerts = mesh.GetVertexCount();
	if (mesh.m_pPositions)
	{
		for (int i = 0; i < nVerts; i++)
		{
			mesh.m_pPositions[i] = tm.TransformPoint(mesh.m_pPositions[i]);
		}
	}
	else if (mesh.m_pPositionsF16)
	{
		for (int i = 0; i < nVerts; i++)
		{
			mesh.m_pPositionsF16[i] = tm.TransformPoint(mesh.m_pPositionsF16[i].ToVec3());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
bool CStatObj::LoadStreamRenderMeshes(bool bLod, const void* pData, const int nDataSize)
{
	MEMSTAT_CONTEXT(EMemStatContextType::CGF, m_szFileName.c_str());
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);

	CLoaderCGF cgfLoader(util::pool_allocate, util::pool_free, GetCVars()->e_StatObjTessellationMode != 2 || bLod);
	CContentCGF cgfContent(m_szFileName);
	CExportInfoCGF* pExportInfo = cgfContent.GetExportInfo();

	bool wasLoadSuccessful = false;
	CReadOnlyChunkFile chunkFile(false, true);  // Chunk file must exist until CGF is completely loaded, and if loading from file do not make a copy of it.
	if(pData && nDataSize > 0)
		wasLoadSuccessful = chunkFile.ReadFromMemory(pData, nDataSize);
	else
	{
		stack_string streamFilename;
		GetStreamFilePath(streamFilename);
		wasLoadSuccessful = chunkFile.Read(streamFilename);
	}

	if (wasLoadSuccessful)
		wasLoadSuccessful = cgfLoader.LoadCGF(&cgfContent, m_szFileName, chunkFile, nullptr, 0);

	if (!wasLoadSuccessful)
	{
		FileWarning(0, m_szFileName, "CGF Streaming Failed: %s", cgfLoader.GetLastError());
		return false;
	}

	//////////////////////////////////////////////////////////////////////////

	int nSubObjCount = (int)m_subObjects.size();
	bool bMeshAssigned = false;
	bool bBreakNodeLoop = false;

	for (int i = 0; i < cgfContent.GetNodeCount() && !bBreakNodeLoop; i++)
	{
		CNodeCGF* pNode = cgfContent.GetNode(i);
		if (!pNode->pMesh)
			continue;

		bool bNodeIsValidMesh = (pNode->type == CNodeCGF::NODE_MESH || (pNode->type == CNodeCGF::NODE_HELPER && pNode->helperType == HP_GEOMETRY));
		if (!bNodeIsValidMesh)
			continue;

		CStatObj* pStatObj = 0;
		for (int s = 0; s < nSubObjCount && !pStatObj; s++)
		{
			CStatObj* pSubStatObj = (CStatObj*)m_subObjects[s].pStatObj;
			if (!pSubStatObj)
				continue;
			for (int nLod = 0; nLod < MAX_STATOBJ_LODS_NUM; nLod++)
			{
				CStatObj* pSubLod = (CStatObj*)pSubStatObj->GetLodObject(nLod);
				if (!pSubLod)
					continue;
				if (0 == strcmp(pSubLod->m_cgfNodeName.c_str(), pNode->name))
				{
					pStatObj = pSubLod;
					break;
				}
			}
		}

		if (!pStatObj && m_nSubObjectMeshCount <= 1)
		{
			// If we do not have sub objects, assign the root StatObj to be used, and then not check anymore other nodes.
			for (int nLod = 0; nLod < MAX_STATOBJ_LODS_NUM && !pStatObj; nLod++)
			{
				CStatObj* pLod = (CStatObj*)GetLodObject(nLod);
				if (!pLod)
					continue;
				if (0 == strcmp(pLod->m_cgfNodeName.c_str(), pNode->name))
				{
					pStatObj = pLod;
					break;
				}
			}
		}
		if (pStatObj)
		{
			// add mesh to sync setup queue
			pStatObj->m_pStreamedRenderMesh = pStatObj->MakeRenderMesh(pNode->pMesh, true);
			if (pStatObj->m_pStreamedRenderMesh)
			{
				bMeshAssigned = true;

				//////////////////////////////////////////////////////////////////////////
				// FIXME: Qtangents not generated for foliage in RC, we must do that here.
				//////////////////////////////////////////////////////////////////////////
				if (pStatObj->m_nSpines && pStatObj->m_pSpines) // foliage
					pStatObj->m_pStreamedRenderMesh->GenerateQTangents();

				if (!bLod && (pExportInfo->bMergeAllNodes || m_nSubObjectMeshCount == 0))
				{
					AnalyzeFoliage(pStatObj->m_pStreamedRenderMesh, &cgfContent);
				}
			}
		}
	}
	if (!bMeshAssigned && gEnv->pRenderer)
	{
		Warning("RenderMesh not assigned %s", m_szFileName.c_str());
	}

	//////////////////////////////////////////////////////////////////////////
	// Merge sub-objects for the new lod.
	if (GetCVars()->e_StatObjMerge)
	{
		CStatObj* pLod0 = (m_pLod0 != nullptr) ? (CStatObj*)m_pLod0 : this;
		pLod0->TryMergeSubObjects(true);
	}
	//////////////////////////////////////////////////////////////////////////

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::CommitStreamRenderMeshes()
{
	if (m_pStreamedRenderMesh)
	{
		CryAutoCriticalSection lock(m_streamingMeshLock);
		SetRenderMesh(m_pStreamedRenderMesh);
		m_pStreamedRenderMesh = nullptr;
	}
	if (m_pLODs)
	{
		for (int nLod = 0; nLod < MAX_STATOBJ_LODS_NUM; nLod++)
		{
			CStatObj* pLodObj = m_pLODs[nLod];
			if (pLodObj && pLodObj->m_pStreamedRenderMesh)
			{
				CryAutoCriticalSection lock(pLodObj->m_streamingMeshLock);

				pLodObj->SetRenderMesh(pLodObj->m_pStreamedRenderMesh);
				pLodObj->m_pStreamedRenderMesh = 0;
			}
		}
	}

	for (size_t i = 0, num = m_subObjects.size(); i < num; ++i)
	{
		CStatObj* pSubObj = (CStatObj*)m_subObjects[i].pStatObj;
		if (pSubObj)
		{
			pSubObj->CommitStreamRenderMeshes();
		}
	}
}

bool CStatObj::IsDeformable()
{
	if (Cry3DEngineBase::GetCVars()->e_MergedMeshes == 0)
		return false;

	// Create deformable subobject is present
	if (m_isDeformable)
	{
		return true;
	}
	else
		for (int i = 0, n = GetSubObjectCount(); i < n; ++i)
		{
			IStatObj::SSubObject* subObject = GetSubObject(i);
			if (!subObject)
				continue;
			if (CStatObj* pChild = static_cast<CStatObj*>(GetSubObject(i)->pStatObj))
			{
				if (pChild->m_isDeformable)
					return true;
			}
		}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CStatObj::LoadCGF(const char* filename, uint32 loadingFlags)
{
	if (m_bSubObject) // Never execute this on the sub objects.
		return true;

	FUNCTION_PROFILER_3DENGINE;
	CRY_DEFINE_ASSET_SCOPE("CGF", filename);
	MEMSTAT_CONTEXT(EMemStatContextType::CGF, filename);

	LoadCGF_Prepare(filename, false);
	CReadOnlyChunkFile chunkFile(false, false);
	return LoadCGF(&chunkFile, filename, false, loadingFlags);
}

//////////////////////////////////////////////////////////////////////////
static _smart_ptr<IMaterial> LoadCGFMaterial(CMatMan* pMatMan, const char* szMaterialName, const char* szCgfFilename, uint32 loadingFlags)
{
	_smart_ptr<IMaterial> pMaterial = pMatMan->LoadCGFMaterial(szMaterialName, szCgfFilename, loadingFlags);
	if (pMaterial->IsDefault())
	{
		// If the material file is not found then let's try to use material file
		// with the name that is the same as the name of the .cgf file.
		const string cgfBasedMaterialName = PathUtil::GetFileName(szCgfFilename);
		pMaterial = pMatMan->LoadCGFMaterial(cgfBasedMaterialName.c_str(), szCgfFilename, loadingFlags);
		if (pMaterial->IsDefault())
		{
			pMatMan->FileWarning(0, szCgfFilename, "CGF is unable to load its material, see XML reader error above for material info.");
		}
		else
		{
			CryLog("Loaded material \"%s\" instead of missing \"%s\" (file \"%s\")", cgfBasedMaterialName.c_str(), szMaterialName, szCgfFilename);
		}
	}

#if defined(FEATURE_SVO_GI)
	if ((Cry3DEngineBase::GetCVars()->e_svoTI_Active >= 0) && (gEnv->IsEditor() || Cry3DEngineBase::GetCVars()->e_svoTI_Apply))
		pMaterial->SetKeepLowResSysCopyForDiffTex();
#endif

	return pMaterial;
}

class CStatObj::CStatObjAsyncCGFLoader : public IStreamCallback
{
public:
	CStatObjAsyncCGFLoader(CStatObj* pObject, const char* szFilename, uint32 loadingFlags, IStatObjLoadedCallback* pCallback) :
		m_pObject(pObject), m_pCallback(pCallback), m_loadingFlags(loadingFlags)
	{
		gEnv->pSystem->GetStreamEngine()->StartRead(eStreamTaskTypeGeometry, szFilename, this);
	}

	virtual void StreamOnComplete(IReadStream* pStream, unsigned) override
	{
		bool succeeded = false;
		if (pStream->IsError())
		{
			if (!(m_loadingFlags & CStatObj::ELoadingFlagsNoErrorIfFail))
				Cry3DEngineBase::FileWarning(0, m_pObject->GetFilePath(), "CGF Streaming Failed: %s", pStream->GetErrorName());
		}
		else
		{
			CReadOnlyChunkFile chunkFile(false, false);
			if (chunkFile.ReadFromMemory(pStream->GetBuffer(), pStream->GetBytesRead()))
				succeeded = m_pObject->LoadCGF(&chunkFile, m_pObject->GetFilePath(), false, m_loadingFlags);
		}

		if (m_pCallback)
			m_pCallback->OnLoaded(succeeded, m_pObject);
		
		delete this;
	}

private:
	CStatObj* m_pObject;
	IStatObjLoadedCallback* m_pCallback;
	uint32    m_loadingFlags;
};

void CStatObj::LoadCGFAsync(const char* filename, uint32 loadingFlags, IStatObjLoadedCallback* pCallback)
{
	if (m_bSubObject) // Never execute this on the sub objects.
		return;
	
	FUNCTION_PROFILER_3DENGINE;
	CRY_DEFINE_ASSET_SCOPE("CGF", filename);
	MEMSTAT_CONTEXT(EMemStatContextType::CGF, filename);

	LoadCGF_Prepare(filename, false);
	new CStatObjAsyncCGFLoader(this, filename, loadingFlags, pCallback);
}

void CStatObj::LoadCGF_Prepare(const char* filename, bool isLod)
{
	PrintComment("Loading %s", filename);
	if (!isLod)
		GetConsole()->TickProgressBar();

	m_nRenderTrisCount = m_nLoadedTrisCount = 0;
	m_nLoadedVertexCount = 0;
	m_szFileName = filename;
	m_szFileName.replace('\\', '/');

	// Determine if stream only cgf is available
	stack_string streamPath;
	GetStreamFilePath(streamPath);
	m_bHasStreamOnlyCGF = gEnv->pCryPak->IsFileExist(streamPath.c_str());
}

bool CStatObj::LoadCGF(IChunkFile* chunkFile, const char* filename, bool isLod, uint32 loadingFlags)
{
	//////////////////////////////////////////////////////////////////////////
	// Load CGF.
	//////////////////////////////////////////////////////////////////////////
	class Listener : public ILoaderCGFListener
	{
	public:
		virtual void Warning(const char* format) override { Cry3DEngineBase::Warning("%s", format); }
		virtual void Error(const char* format)   override { Cry3DEngineBase::Error("%s", format); }
		
		virtual bool IsValidationEnabled()       override { return Cry3DEngineBase::GetCVars()->e_StatObjValidate != 0; }
	};

	Listener listener;
	CLoaderCGF cgfLoader(util::pool_allocate, util::pool_free, GetCVars()->e_StatObjTessellationMode != 2 || isLod);
	CContentCGF cgfContent(filename);
	
	if (!cgfLoader.LoadCGF(&cgfContent, filename, *chunkFile, &listener, loadingFlags))
	{
		if (!(loadingFlags & IStatObj::ELoadingFlagsNoErrorIfFail))
		{
			FileWarning(0, filename, "CGF Loading Failed: %s", cgfLoader.GetLastError());
		}
		return false;
	}

#if !defined(_RELEASE)
	if (GetCVars()->e_CGFMaxFileSize >= 0 && !stristr(filename, DEFAULT_CGF_NAME))
	{
		size_t fileSize = gEnv->pCryPak->FGetSize(filename, true);
		if (fileSize > (size_t)GetCVars()->e_CGFMaxFileSize << 10)
		{
			FileWarning(0, filename, "CGF Loading Failed: file '%s' (size %3.3f kb) exceeds size limit (max %3.3f kb)",
			            filename, fileSize / 1024.f, (GetCVars()->e_CGFMaxFileSize << 10) / 1024.f);
			return false;
		}
	}
#endif
		
	//////////////////////////////////////////////////////////////////////////

	INDENT_LOG_DURING_SCOPE(true, "While loading static object geometry '%s'", filename);

	CExportInfoCGF* pExportInfo = cgfContent.GetExportInfo();
	CNodeCGF* pFirstMeshNode = nullptr;
	CMesh* pFirstMesh = nullptr;
	m_nSubObjectMeshCount = 0;

	if (!pExportInfo->bCompiledCGF)
	{
		FileWarning(0, m_szFileName, "CGF is not compiled, use RC");
		return false;
	}

	m_bMeshStrippedCGF = pExportInfo->bNoMesh;

	bool bHasJoints = false;
	if (loadingFlags & ELoadingFlagsForceBreakable)
		m_nFlags |= STATIC_OBJECT_DYNAMIC;

	m_nNodeCount = cgfContent.GetNodeCount();

	//////////////////////////////////////////////////////////////////////////
	// Find out number of meshes, and get pointer to the first found mesh.
	//////////////////////////////////////////////////////////////////////////
	for (int i = 0; i < cgfContent.GetNodeCount(); i++)
	{
		CNodeCGF* pNode = cgfContent.GetNode(i);
		if (pNode->type == CNodeCGF::NODE_MESH)
		{
			if (m_szProperties.empty())
			{
				m_szProperties = pNode->properties; // Take properties from the first mesh node.
				m_szProperties.MakeLower();
			}
			m_nSubObjectMeshCount++;
			if (!pFirstMeshNode)
			{
				pFirstMeshNode = pNode;
				pFirstMesh = pNode->pMesh;
			}
		}
		else if (!strncmp(pNode->name, PHYSICS_BREAKABLE_JOINT, strlen(PHYSICS_BREAKABLE_JOINT)) || !strcmp(pNode->name, PHYSICS_COMPILED_JOINTS))
			bHasJoints = true;
	}

	bool bIsLod0Merged = false;
	if (isLod && m_pLod0)
	{
		// This is a lod object, check if parent was merged or not.
		bIsLod0Merged = m_pLod0->m_nSubObjectMeshCount == 0;
	}

	if (pExportInfo->bMergeAllNodes || (m_nSubObjectMeshCount <= 1 && !bHasJoints && (!isLod || bIsLod0Merged)))
	{
		// If we merging all nodes, ignore sub object meshes.
		m_nSubObjectMeshCount = 0;

		if (cgfContent.GetCommonMaterial())
		{
			if (loadingFlags & ELoadingFlagsPreviewMode)
			{
				m_pMaterial = GetMatMan()->GetDefaultMaterial();
				m_pMaterial->AddRef();
			}
			else
			{
				m_pMaterial = ::LoadCGFMaterial(GetMatMan(), cgfContent.GetCommonMaterial()->name, m_szFileName.c_str(), loadingFlags);
			}
		}
	}

	// Prepare material and mesh for the billboards
	if (GetCVars()->e_VegetationBillboards >= 0)
	{
		CheckCreateBillboardMaterial();
	}

	// Fail if mesh was not complied by RC
	if (pFirstMesh && pFirstMesh->GetFaceCount() > 0)
	{
		FileWarning(0, filename, "CGF is not compiled");
		return false;
	}

	if (GetCVars()->e_StatObjValidate)
	{
		const char* pErrorDescription = nullptr;
		if (pFirstMesh && (!pFirstMesh->Validate(&pErrorDescription)))
		{
			FileWarning(0, filename, "CGF has invalid merged mesh (%s)", pErrorDescription);
			assert(!"CGF has invalid merged mesh");
			return false;
		}
		if (!cgfContent.ValidateMeshes(&pErrorDescription))
		{
			FileWarning(0, filename, "CGF has invalid meshes (%s)", pErrorDescription);
			assert(!"CGF has invalid meshes");
			return false;
		}
	}

	// Common of all sub nodes bbox.
	AABB commonBBox;
	commonBBox.Reset();

	bool bHaveMeshNamedMain = false;
	bool bHasBreakableJoints = false;
	bool bHasCompiledJoints = false;
	bool bRenderMeshLoaded = false; // even if streaming is disabled we may load now from stripped cgf so meshes will fail to load - in this case we will stream it later

	//////////////////////////////////////////////////////////////////////////
	// Create StatObj from Mesh.
	//////////////////////////////////////////////////////////////////////////

	_smart_ptr<IRenderMesh> pMainMesh;

	if (pExportInfo->bMergeAllNodes || m_nSubObjectMeshCount == 0)
	{
		if (pFirstMeshNode)
		{
			m_AABB.min = pFirstMeshNode->meshInfo.bboxMin;
			m_AABB.max = pFirstMeshNode->meshInfo.bboxMax;
			m_fGeometricMeanFaceArea = pFirstMeshNode->meshInfo.fGeometricMean;
			commonBBox = m_AABB;
			m_nRenderTrisCount = m_nLoadedTrisCount = pFirstMeshNode->meshInfo.nIndices / 3;
			m_nLoadedVertexCount = pFirstMeshNode->meshInfo.nVerts;
			m_cgfNodeName = pFirstMeshNode->name;
			CalcRadiuses();

			if (pFirstMesh)
			{
				// Assign mesh to this static object.
				_smart_ptr<IRenderMesh> pRenderMesh = MakeRenderMesh(pFirstMesh, !m_bCanUnload);
				SetRenderMesh(pRenderMesh);
				pMainMesh = m_pRenderMesh;
				bRenderMeshLoaded |= (m_pRenderMesh != nullptr);
			}
			else
			{
				// If mesh not known now try to estimate its memory usage.
				m_nRenderMeshMemoryUsage = CMesh::ApproximateRenderMeshMemoryUsage(pFirstMeshNode->meshInfo.nVerts, pFirstMeshNode->meshInfo.nIndices);
				CalcRadiuses();
			}

			//////////////////////////////////////////////////////////////////////////
			// Physicalize merged geometry.
			//////////////////////////////////////////////////////////////////////////
			if (!isLod)
			{
				PhysicalizeCompiled(pFirstMeshNode);
			}
		}
	}
	//////////////////////////////////////////////////////////////////////////

	scratch_vector<CNodeCGF*> nodes;
	static const size_t lodNamePrefixLength = strlen(CGF_NODE_NAME_LOD_PREFIX);

	//////////////////////////////////////////////////////////////////////////
	// Create SubObjects.
	//////////////////////////////////////////////////////////////////////////
	if (cgfContent.GetNodeCount() > 1 || m_nSubObjectMeshCount > 0)
	{
		nodes.reserve(cgfContent.GetNodeCount());

		scratch_vector<std::pair<CNodeCGF*, CStatObj*>> meshToObject;
		meshToObject.reserve(cgfContent.GetNodeCount());

		//////////////////////////////////////////////
		// Count required subobjects and reserve space
		//////////////////////////////////////////////
		size_t nSubObjects = 0;
		for (int ii = 0; ii < cgfContent.GetNodeCount(); ii++)
		{
			CNodeCGF* pNode = cgfContent.GetNode(ii);

			if (pNode->bPhysicsProxy)
				continue;

			if (pNode->type == CNodeCGF::NODE_MESH)
			{
				if (pExportInfo->bMergeAllNodes || m_nSubObjectMeshCount == 0) // Only add helpers, ignore meshes.
					continue;
			}
			else if (pNode->type == CNodeCGF::NODE_HELPER)
			{
				switch (pNode->helperType)
				{
				case HP_GEOMETRY:
					{
						if (strnicmp(pNode->name, CGF_NODE_NAME_LOD_PREFIX, lodNamePrefixLength) == 0)
							continue;
					}
					break;
				}
			}

			++nSubObjects;
		}
		m_subObjects.reserve(nSubObjects);
		//////////////////////////////////////////////

		int nNumMeshes = 0;
		for (int ii = 0; ii < cgfContent.GetNodeCount(); ii++)
		{
			CNodeCGF* pNode = cgfContent.GetNode(ii);

			if (pNode->bPhysicsProxy)
				continue;

			SSubObject subObject;
			subObject.pStatObj = 0;
			subObject.bIdentityMatrix = pNode->bIdentityMatrix;
			subObject.bHidden = false;
			subObject.tm = pNode->worldTM;
			subObject.localTM = pNode->localTM;
			subObject.name = pNode->name;
			subObject.properties = pNode->properties;
			subObject.nParent = -1;
			subObject.pWeights = nullptr;
			subObject.pFoliage = nullptr;
			subObject.helperSize.Set(0, 0, 0);

			if (pNode->type == CNodeCGF::NODE_MESH)
			{
				if (pExportInfo->bMergeAllNodes || m_nSubObjectMeshCount == 0) // Only add helpers, ignore meshes.
					continue;

				nNumMeshes++;
				subObject.nType = STATIC_SUB_OBJECT_MESH;

				if (stristr(pNode->name, "shadowproxy") != nullptr)
					subObject.bShadowProxy = true;

				if (stricmp(pNode->name, MESH_NAME_FOR_MAIN) == 0)
					bHaveMeshNamedMain = true;
			}
			else if (pNode->type == CNodeCGF::NODE_LIGHT)
				subObject.nType = STATIC_SUB_OBJECT_LIGHT;
			else if (pNode->type == CNodeCGF::NODE_HELPER)
			{
				if (!bHasBreakableJoints && !strncmp(pNode->name, PHYSICS_BREAKABLE_JOINT, strlen(PHYSICS_BREAKABLE_JOINT)))
					bHasBreakableJoints = true;
				if (!bHasCompiledJoints && !strcmp(pNode->name, PHYSICS_COMPILED_JOINTS))
					bHasBreakableJoints = bHasCompiledJoints = true;

				switch (pNode->helperType)
				{
				case HP_POINT:
					subObject.nType = STATIC_SUB_OBJECT_POINT;
					break;
				case HP_DUMMY:
					subObject.nType = STATIC_SUB_OBJECT_DUMMY;
					subObject.helperSize = (pNode->helperSize * 0.01f);
					break;
				case HP_XREF:
					subObject.nType = STATIC_SUB_OBJECT_XREF;
					break;
				case HP_CAMERA:
					subObject.nType = STATIC_SUB_OBJECT_CAMERA;
					break;
				case HP_GEOMETRY:
					{
						subObject.nType = STATIC_SUB_OBJECT_HELPER_MESH;
						subObject.bHidden = true; // Helpers are not rendered.
					}
					break;
				default:
					CRY_ASSERT(0, "Unknown CGFNode Helper type %d", pNode->helperType);
				}
			}

			// Only when multiple meshes inside.
			// If only 1 mesh inside, Do not create a separate CStatObj for it.
			if ((m_nSubObjectMeshCount > 0 && pNode->type == CNodeCGF::NODE_MESH) ||
			    (subObject.nType == STATIC_SUB_OBJECT_HELPER_MESH))
			{
				if (pNode->pSharedMesh)
				{
					// Try to find already create StatObj for a shred mesh node
					for (int k = 0, num = (int)meshToObject.size(); k < num; k++)
					{
						if (pNode->pSharedMesh == meshToObject[k].first)
						{
							subObject.pStatObj = meshToObject[k].second;
							break;
						}
					}
				}

				if (!subObject.pStatObj)
				{
					// Create a StatObj from the CGF node.
					subObject.pStatObj = MakeStatObjFromCgfNode(&cgfContent, pNode, isLod, loadingFlags, commonBBox);
					if (pNode->pSharedMesh)
						meshToObject.push_back(std::make_pair(pNode->pSharedMesh, static_cast<CStatObj*>(subObject.pStatObj)));
					else
						meshToObject.push_back(std::make_pair(pNode, static_cast<CStatObj*>(subObject.pStatObj)));
					bRenderMeshLoaded |= (((CStatObj*)subObject.pStatObj)->m_pRenderMesh != 0);
				}
			}

			//////////////////////////////////////////////////////////////////////////
			// Check if helper object is a LOD
			//////////////////////////////////////////////////////////////////////////
			if ((subObject.nType == STATIC_SUB_OBJECT_HELPER_MESH) &&
			    (strnicmp(pNode->name, CGF_NODE_NAME_LOD_PREFIX, lodNamePrefixLength) == 0))
			{
				// Check if helper object is a LOD

				if (!subObject.pStatObj)
				{
					continue;
				}

				CStatObj* pLodStatObj = (CStatObj*)subObject.pStatObj;
				CStatObj* pStatObjParent = this;
				if (!pExportInfo->bMergeAllNodes && m_nSubObjectMeshCount > 0 && pNode->pParent)
				{
					// We are attached to some object, find it.
					for (int i = 0, num = nodes.size(); i < num; i++)
					{
						if (nodes[i] == pNode->pParent)
						{
							pStatObjParent = (CStatObj*)m_subObjects[i].pStatObj;
							break;
						}
					}
				}
				if (!pStatObjParent)
				{
					continue;
				}

				const int nLodLevel = atoi(pNode->name + lodNamePrefixLength);
				if ((nLodLevel >= 1) && (nLodLevel < MAX_STATOBJ_LODS_NUM))
				{
					if (!pStatObjParent->m_pLODs || !pStatObjParent->m_pLODs[nLodLevel])
					{
						pStatObjParent->SetLodObject(nLodLevel, pLodStatObj);
					}
					else
					{
						const char* existingGeoName = pStatObjParent->m_pLODs[nLodLevel]->GetGeoName();
						FileWarning(0, m_szFileName.c_str(), "Duplicated LOD helper %s (%s). Existing geometry name: %s", pNode->name, m_szFileName.c_str(), existingGeoName);
					}
				}

				continue;
			}
			//////////////////////////////////////////////////////////////////////////

			if (subObject.pStatObj)
			{
				subObject.pStatObj->AddRef();
			}

			m_subObjects.push_back(subObject);
			nodes.push_back(pNode);
		}

		// Delete not assigned stat objects.
		for (int k = 0, num = (int)meshToObject.size(); k < num; k++)
		{
			if (meshToObject[k].second->m_nUsers == 0)
			{
				delete meshToObject[k].second;
			}
		}

		// Assign SubObject parent pointers.
		int nNumCgfNodes = (int)nodes.size();
		if (nNumCgfNodes > 0)
		{
			CNodeCGF** pNodes = &nodes[0];

			//////////////////////////////////////////////////////////////////////////
			// Move meshes to beginning, Sort sub-objects so that meshes are first.
			for (int i = 0; i < nNumCgfNodes; i++)
			{
				if (pNodes[i]->type != CNodeCGF::NODE_MESH)
				{
					// check if any more meshes exist.
					if (i < nNumMeshes)
					{
						// Try to find next mesh and place it here.
						for (int j = i + 1; j < nNumCgfNodes; j++)
						{
							if (pNodes[j]->type == CNodeCGF::NODE_MESH)
							{
								// Swap objects at j to i.
								std::swap(pNodes[i], pNodes[j]);
								std::swap(m_subObjects[i], m_subObjects[j]);
								break;
							}
						}
					}
				}
			}
			//////////////////////////////////////////////////////////////////////////

			// Assign Parent nodes.
			for (int i = 0; i < nNumCgfNodes; i++)
			{
				CNodeCGF* pParentNode = pNodes[i]->pParent;
				if (pParentNode)
				{
					for (int j = 0; j < nNumCgfNodes; j++)
					{
						if (pNodes[j] == pParentNode)
						{
							m_subObjects[i].nParent = j;
							break;
						}
					}
				}
			}

			//////////////////////////////////////////////////////////////////////////
			// Handle Main/Remain meshes used for Destroyable Objects.
			//////////////////////////////////////////////////////////////////////////
			if (bHaveMeshNamedMain)
			{
				// If have mesh named main, then mark all sub object hidden except the one called "Main".
				for (auto& subObject : m_subObjects)
				{
					if (subObject.nType == STATIC_SUB_OBJECT_MESH)
					{
						subObject.bHidden = (stricmp(subObject.name, MESH_NAME_FOR_MAIN) != 0);
					}
				}
			}
			//////////////////////////////////////////////////////////////////////////
		}
	}

	if (m_nSubObjectMeshCount > 0)
	{
		m_AABB = commonBBox;
		CalcRadiuses();
	}

	//////////////////////////////////////////////////////////////////////////
	// Physicalize physics proxy nodes.
	//////////////////////////////////////////////////////////////////////////
	if (!isLod)
	{
		for (int i = 0, numNodes = cgfContent.GetNodeCount(); i < numNodes; i++)
		{
			CNodeCGF* pNode = cgfContent.GetNode(i);
			if (pNode->bPhysicsProxy)
			{
				CStatObj* pStatObjParent = this;
				if (pNode->pParent)
					for (int j = nodes.size() - 1; j >= 0; j--)
						if (nodes[j] == pNode->pParent && m_subObjects[j].pStatObj)
						{
							pStatObjParent = (CStatObj*)m_subObjects[j].pStatObj;
							break;
						}
				pStatObjParent->PhysicalizeCompiled(pNode, 1);
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Analyze foliage info.
	//////////////////////////////////////////////////////////////////////////
	if (!isLod && (pExportInfo->bMergeAllNodes || m_nSubObjectMeshCount == 0) && !m_bCanUnload)
	{
		AnalyzeFoliage(pMainMesh, &cgfContent);
	}
	//////////////////////////////////////////////////////////////////////////

	for (int i = 0; i < cgfContent.GetNodeCount(); i++)
		if (strstr(cgfContent.GetNode(i)->properties, "deformable"))
			m_nFlags |= STATIC_OBJECT_DEFORMABLE;

	if (m_nSubObjectMeshCount > 0)
		m_nFlags |= STATIC_OBJECT_COMPOUND;
	else
		m_nFlags &= ~STATIC_OBJECT_COMPOUND;

	if (!isLod && !m_szProperties.empty())
	{
		ParseProperties();
	}

	if (!isLod)
	{
		CPhysicalizeInfoCGF* pPi = cgfContent.GetPhysicalizeInfo();
		if (pPi->nRetTets)
		{
			m_pLattice = GetPhysicalWorld()->GetGeomManager()->CreateTetrLattice(pPi->pRetVtx, pPi->nRetVtx, pPi->pRetTets, pPi->nRetTets);
		}
	}

	if (m_bHasDeformationMorphs)
	{
		int i, j;
		for (i = GetSubObjectCount() - 1; i >= 0; i--)
			if ((j = SubobjHasDeformMorph(i)) >= 0)
				GetSubObject(i)->pStatObj->SetDeformationMorphTarget(GetSubObject(j)->pStatObj);
		m_bUnmergable = 1;
	}

	// Only objects with breakable physics joints can be merged.
	if (!bHasBreakableJoints)
	{
		m_bUnmergable = true;
	}

	// sub meshes merging
	if (GetCVars()->e_StatObjMerge)
	{
		if (!m_bUnmergable)
		{
			if (!CanMergeSubObjects())
			{
				m_bUnmergable = true;
			}
		}
	}

	// Merging always produces 16 bit meshes, so disable for 32 bit meshes for now
	if (pFirstMesh && pFirstMesh->m_pPositions && !bHasCompiledJoints)
	{
		m_bUnmergable = true;
	}

	if (!m_bCanUnload && bRenderMeshLoaded)
		m_eStreamingStatus = ecss_Ready;

	const std::vector<CStatObj*> allObjects = GatherAllObjects();
	for (CStatObj* obj : allObjects)
	{
		// Determine if the cgf is deformable
		if (stristr(obj->m_szGeomName.c_str(), "bendable") && stristr(obj->m_szProperties.c_str(), "mergedmesh_deform"))
		{
			obj->m_isDeformable = 1;
			obj->DisableStreaming();
		}

		// Read the depth sort offset
		Vec3 depthSortOffset;
		if (std::sscanf(obj->m_szProperties.c_str(), "depthoffset(x:%f,y:%f,z:%f)", &depthSortOffset.x, &depthSortOffset.y, &depthSortOffset.z) == 3)
		{
			obj->m_depthSortOffset = depthSortOffset;
		}
	}

	// Recursive computation of m_fLODDistance for compound- and sub-objects
	ComputeAndStoreLodDistances();

	if (!m_bCanUnload && m_bHasStreamOnlyCGF)
	{
		return LoadStreamRenderMeshes(isLod);
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
std::vector<CStatObj*> CStatObj::GatherAllObjects()
{
	const int subObjectsCount = GetSubObjectCount();

	std::vector<CStatObj*> allObjects;
	allObjects.reserve(subObjectsCount + 1);

	allObjects.push_back(this);

	for (int i = 0; i < subObjectsCount; ++i)
	{
		IStatObj::SSubObject* subObject = GetSubObject(i);
		if (subObject && subObject->pStatObj)
		{
			allObjects.push_back(static_cast<CStatObj*>(subObject->pStatObj));
		}
	}

	return allObjects;
}

//////////////////////////////////////////////////////////////////////////
CStatObj* CStatObj::MakeStatObjFromCgfNode(CContentCGF* pCGF, CNodeCGF* pNode, bool bLod, int loadingFlags, AABB& commonBBox)
{
	CNodeCGF* pTMNode = pNode;
	if (pNode->pSharedMesh)
	{
		pNode = pNode->pSharedMesh;
	}

	// Calc bbox.
	if (pNode->type == CNodeCGF::NODE_MESH)
	{
		AABB box(pNode->meshInfo.bboxMin, pNode->meshInfo.bboxMax);
		box.SetTransformedAABB(pTMNode->worldTM, box);
		commonBBox.Add(box.min);
		commonBBox.Add(box.max);
	}

	CStatObj* pStatObj = new CStatObj;

	pStatObj->m_szFileName = m_szFileName;
	pStatObj->m_szGeomName = pNode->name;
	pStatObj->m_bSubObject = true;

	if (pNode->type == CNodeCGF::NODE_MESH)
	{
		pStatObj->m_pParentObject = this;
	}

	pStatObj->m_szProperties = pNode->properties;
	pStatObj->m_szProperties.MakeLower();
	if (!bLod && !pStatObj->m_szProperties.empty())
	{
		pStatObj->ParseProperties();
	}

	if (pNode->pMaterial)
	{
		if (loadingFlags & ELoadingFlagsPreviewMode)
		{
			pStatObj->m_pMaterial = GetMatMan()->GetDefaultMaterial();
			pStatObj->m_pMaterial->AddRef();
		}
		else
		{
			pStatObj->m_pMaterial = ::LoadCGFMaterial(GetMatMan(), pNode->pMaterial->name, m_szFileName.c_str(), loadingFlags);
		}
		if (!m_pMaterial || m_pMaterial->IsDefault())
			m_pMaterial = pStatObj->m_pMaterial; // take it as a general stat obj material.
	}
	if (!pStatObj->m_pMaterial)
		pStatObj->m_pMaterial = m_pMaterial;

	pStatObj->m_AABB.min = pNode->meshInfo.bboxMin;
	pStatObj->m_AABB.max = pNode->meshInfo.bboxMax;
	pStatObj->m_nRenderMatIds = pNode->meshInfo.nSubsets;
	pStatObj->m_nRenderTrisCount = pStatObj->m_nLoadedTrisCount = pNode->meshInfo.nIndices / 3;
	pStatObj->m_nLoadedVertexCount = pNode->meshInfo.nVerts;
	pStatObj->m_fGeometricMeanFaceArea = pNode->meshInfo.fGeometricMean;
	pStatObj->CalcRadiuses();

	if (loadingFlags & ELoadingFlagsForceBreakable)
		pStatObj->m_nFlags |= STATIC_OBJECT_DYNAMIC;

	if (pNode->pMesh)
	{
		_smart_ptr<IRenderMesh> pRenderMesh = pStatObj->MakeRenderMesh(pNode->pMesh, !m_bCanUnload);
		pStatObj->SetRenderMesh(pRenderMesh);
	}
	else
	{
		// If mesh not known now try to estimate its memory usage.
		pStatObj->m_nRenderMeshMemoryUsage = CMesh::ApproximateRenderMeshMemoryUsage(pNode->meshInfo.nVerts, pNode->meshInfo.nIndices);
	}
	pStatObj->m_cgfNodeName = pNode->name;

	if (!bLod)
	{
		pStatObj->PhysicalizeCompiled(pNode);
		pStatObj->AnalyzeFoliage(pStatObj->m_pRenderMesh, pCGF);
	}
	if (pNode->pSkinInfo)
	{
		pStatObj->m_pSkinInfo = (SSkinVtx*)pNode->pSkinInfo;
		pStatObj->m_hasSkinInfo = 1;
		pNode->pSkinInfo = 0;
	}

	return pStatObj;
}

//////////////////////////////////////////////////////////////////////////
_smart_ptr<IRenderMesh> CStatObj::MakeRenderMesh(CMesh* pMesh, bool bDoRenderMesh)
{
	FUNCTION_PROFILER_3DENGINE;

	if (!pMesh)
		return 0;

	m_AABB = pMesh->m_bbox;
	m_fGeometricMeanFaceArea = pMesh->m_geometricMeanFaceArea;

	CalcRadiuses();

	m_nLoadedTrisCount = pMesh->GetIndexCount() / 3;
	m_nLoadedVertexCount = pMesh->GetVertexCount();
	if (!m_nLoadedTrisCount)
		return nullptr;

	m_nRenderTrisCount = 0;
	m_nRenderMatIds = 0;
	//////////////////////////////////////////////////////////////////////////
	// Initialize Mesh subset material flags.
	//////////////////////////////////////////////////////////////////////////
	for (int i = 0; i < pMesh->GetSubSetCount(); i++)
	{
		SMeshSubset& subset = pMesh->m_subsets[i];
		IMaterial* pMtl = m_pMaterial->GetSafeSubMtl(subset.nMatID);
		subset.nMatFlags = pMtl->GetFlags();
		if (subset.nPhysicalizeType == PHYS_GEOM_TYPE_NONE && pMtl->GetSurfaceType()->GetPhyscalParams().pierceability >= 10)
			subset.nMatFlags |= MTL_FLAG_NOPHYSICALIZE;
		if (!(subset.nMatFlags & MTL_FLAG_NODRAW) && (subset.nNumIndices > 0))
		{
			m_nRenderMatIds++;
			m_nRenderTrisCount += subset.nNumIndices / 3;
		}
	}
	//////////////////////////////////////////////////////////////////////////

	if (!m_nRenderTrisCount)
		return 0;

	_smart_ptr<IRenderMesh> pOutRenderMesh;

	// Create renderable mesh.
	if (gEnv->pRenderer)
	{
		if (!pMesh)
			return nullptr;
		if (pMesh->GetSubSetCount() == 0)
			return nullptr;

		size_t nRenderMeshSize = ~0U;
		if (bDoRenderMesh)
		{
			pOutRenderMesh = GetRenderer()->CreateRenderMesh("StatObj", m_szFileName.c_str());

			if (m_idmatBreakable >= 0 || m_bBreakableByGame)
			{
				// need to keep mesh data in system memory for breakable meshes
				pOutRenderMesh->KeepSysMesh(true);
			}

			// we cannot use FSM_CREATE_DEVICE_MESH flag since we can have an async call to the renderer!
			{
				uint32 nFlags = 0;
				nFlags |= GetCVars()->e_StreamCgf ? 0 : FSM_CREATE_DEVICE_MESH;
				nFlags |= (!GetCVars()->e_StreamCgf && Get3DEngine()->m_bInLoad) ? FSM_SETMESH_ASYNC : 0;
#ifdef MESH_TESSELLATION_ENGINE
				nFlags |= FSM_ENABLE_NORMALSTREAM;
#endif
				nRenderMeshSize = pOutRenderMesh->SetMesh(*pMesh, 0, nFlags, nullptr, true);
				if (nRenderMeshSize == ~0U)
					return nullptr;
			}

			bool arrMaterialSupportsTeselation[32];
			ZeroStruct(arrMaterialSupportsTeselation);
		}

		m_nRenderMeshMemoryUsage = (nRenderMeshSize == ~0U) ? pMesh->EstimateRenderMeshMemoryUsage() : nRenderMeshSize;
		//m_nRenderMeshMemoryUsage = pMesh->EstimateRenderMeshMemoryUsage();
	}

	return pOutRenderMesh;
}

static inline CNodeCGF* CreateNodeCGF(CContentCGF* pCGF, CStatObj* pStatObj, const char* name, CNodeCGF* pParent, CMaterialCGF* pMaterial, const Matrix34& localTM = Matrix34(IDENTITY), const char* properties = 0)
{
	CNodeCGF* pNode = nullptr;

	// Add single node for merged mesh.
	pNode = new CNodeCGF;

	if (!pNode)
	{
		CryLog("SaveToCgf: failed to allocate CNodeCGF aborting");
		return nullptr;
	}

	cry_sprintf(pNode->name, "%s", name);
	pNode->properties = properties;
	pNode->localTM = localTM;
	pNode->worldTM = pParent ? pParent->worldTM * localTM : localTM;
	pNode->bIdentityMatrix = localTM.IsIdentity();
	pNode->pParent = pParent;
	pNode->pMaterial = pMaterial;
	pNode->nPhysicalizeFlags = 0;

	if (pStatObj && pStatObj->GetIndexedMesh(true))	// GetIndexedMesh(true) will create indexed mesh from render mesh if needed
	{
		pNode->pMesh = new CMesh;
		pNode->pMesh->CopyFrom(*(pStatObj->GetIndexedMesh()->GetMesh()));
		pNode->pMesh->m_bbox = pStatObj->GetAABB();
		pNode->type = CNodeCGF::NODE_MESH;
	}
	else
		pNode->type = CNodeCGF::NODE_HELPER;

	if (pStatObj)
		pStatObj->SavePhysicalizeData(pNode);
	pCGF->AddNode(pNode);

	const int subobjCount = pStatObj ? pStatObj->GetSubObjectCount() : 0;
	std::vector<CNodeCGF*> nodes;
	nodes.resize(subobjCount);
	for (int subidx = 0; subidx < subobjCount; ++subidx)
		nodes[subidx] = pNode;
	for (int subidx = 0; subidx < subobjCount; ++subidx)
	{
		IStatObj::SSubObject* pSubObj = pStatObj->GetSubObject(subidx);
		if (!(nodes[subidx] = CreateNodeCGF(pCGF, (CStatObj*)pSubObj->pStatObj, pSubObj->pStatObj ? pSubObj->pStatObj->GetGeoName() : pSubObj->name.c_str(),
		                                    pSubObj->nParent >= 0 ? nodes[pSubObj->nParent] : pNode, pMaterial, pSubObj->localTM, pSubObj->properties)))
			pNode = nullptr;
	}
	return pNode;
}

//////////////////////////////////////////////////////////////////////////
// Save statobj to the CGF file.
bool CStatObj::SaveToCGF(const char* sFilename, IChunkFile** pOutChunkFile, bool bHavePhiscalProxy)
{
#if defined(INCLUDE_SAVECGF)
	CContentCGF cgfContent(sFilename);

	cgfContent.GetExportInfo()->bCompiledCGF = true;
	cgfContent.GetExportInfo()->bMergeAllNodes = (GetSubObjectCount() <= 0);
	cgfContent.GetExportInfo()->bHavePhysicsProxy = bHavePhiscalProxy;
	cry_fixed_size_strcpy(cgfContent.GetExportInfo()->rc_version_string, "From Sandbox");

	CChunkFile* pChunkFile = new CChunkFile();
	if (pOutChunkFile)
		*pOutChunkFile = pChunkFile;

	CMaterialCGF* pMaterialCGF = new CMaterialCGF;
	// if material name uses an absolute path, check if it's in the same folder as the cgf
	// if so, leave just the name, otherwise remove the engine path from the material path
	if (m_pMaterial)
		cry_strcpy(pMaterialCGF->name, 
			PathUtil::IsRelativePath(m_pMaterial->GetName()) ? m_pMaterial->GetName() :	// store non-absolute path as it is
			(stricmp(PathUtil::GetPathWithoutFilename(GetFilePath()), PathUtil::GetPathWithoutFilename(m_pMaterial->GetName())) ?
			m_pMaterial->GetName() + PathUtil::GetEnginePath().length() + 1 :
			PathUtil::GetFileName(m_pMaterial->GetName()).c_str())
		);
	else
		pMaterialCGF->name[0] = 0;
	pMaterialCGF->nPhysicalizeType = PHYS_GEOM_TYPE_DEFAULT;
	pMaterialCGF->bOldMaterial = false;
	pMaterialCGF->nChunkId = 0;

	// Array of sub materials.
	//std::vector<CMaterialCGF*> subMaterials;

	bool bResult = false;
	if (CreateNodeCGF(&cgfContent, this, GetGeoName() ? GetGeoName() : "Merged", nullptr, pMaterialCGF))
	{
		CSaverCGF cgfSaver(*pChunkFile);

		const bool bNeedEndianSwap = false;
		const bool bUseQtangents = false;
		const bool bStorePositionsAsF16 = false;
		const bool bStoreIndicesAsU16 = (sizeof(vtx_idx) == sizeof(uint16));

		cgfSaver.SaveContent(&cgfContent, bNeedEndianSwap, bStorePositionsAsF16, bUseQtangents, bStoreIndicesAsU16);

		bResult = true;
	}

	if (!pOutChunkFile && bResult)
	{
		bResult = pChunkFile->Write(sFilename);
		pChunkFile->Release();
	}

	return bResult;
#else // #if defined(INCLUDE_SAVECGF)
	CRY_FUNCTION_NOT_IMPLEMENTED;
	return false;
#endif
}

//////////////////////////////////////////////////////////////////////////
inline char* trim_whitespaces(char* str, char* strEnd)
{
	char* first = str;
	while (first < strEnd && (*first == ' ' || *first == '\t'))
		first++;
	char* s = strEnd - 1;
	while (s >= first && (*s == ' ' || *s == '\t'))
		*s-- = 0;
	return first;
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::ParseProperties()
{
	FUNCTION_PROFILER_3DENGINE;

	int nLen = m_szProperties.size();
	if (nLen >= 4096)
	{
		Warning("CGF '%s' has a geometry info file of more than 4kB", m_szFileName.c_str());
		nLen = 4096;
	}

	char properties[4096];
	memcpy(properties, m_szProperties.c_str(), nLen);
	properties[nLen] = 0;

	char* str = properties;
	char* strEnd = str + nLen;
	while (str < strEnd)
	{
		char* line = str;
		while (str < strEnd && *str != '\n' && *str != '\r')
			str++;
		char* lineEnd = str;
		*lineEnd = 0;
		str++;
		while (str < strEnd && (*str == '\n' || *str == '\r')) // Skip all \r\n at end.
			str++;

		if (*line == '/' || *line == '#') // skip comments
		{
			continue;
		}

		if (line < lineEnd)
		{
			// Parse line.
			char* l = line;
			while (l < lineEnd && *l != '=')
				l++;
			if (l < lineEnd)
			{
				*l = 0;
				char* left = line;
				char* right = l + 1;

				// remove white spaces from left and right.
				left = trim_whitespaces(left, l);
				right = trim_whitespaces(right, lineEnd);

				//////////////////////////////////////////////////////////////////////////
				if (0 == strcmp(left, "mass"))
				{
					m_phys_mass = (float)atof(right);
				}
				else if (0 == strcmp(left, "density"))
				{
					m_phys_density = (float)atof(right);
				}
				//////////////////////////////////////////////////////////////////////////
			}
			else
			{
				// There`s no = on the line, must be a flag.
				//////////////////////////////////////////////////////////////////////////
				if (0 == strcmp(line, "entity"))
				{
					// pickable
					m_nFlags |= STATIC_OBJECT_SPAWN_ENTITY;
				}
				else if (0 == strcmp(line, "no_player_collide"))
				{
					m_nFlags |= STATIC_OBJECT_NO_PLAYER_COLLIDE;
				}
				else if (0 == strcmp(line, "pickable"))
				{
					m_nFlags |= STATIC_OBJECT_PICKABLE;
				}
				else if (0 == strcmp(line, "no_auto_hidepoints"))
				{
					m_nFlags |= STATIC_OBJECT_NO_AUTO_HIDEPOINTS;
				}
				else if (0 == strcmp(line, "dynamic"))
				{
					m_nFlags |= STATIC_OBJECT_DYNAMIC;
				}
				else if (0 == strcmp(line, "no_hit_refinement"))
				{
					m_bNoHitRefinement = true;
					for (int i = m_arrPhysGeomInfo.GetGeomCount() - 1; i >= 0; i--)
						m_arrPhysGeomInfo[i]->pGeom->SetForeignData(nullptr, 0);
				}
				else if (0 == strcmp(line, "no_explosion_occlusion"))
				{
					m_bDontOccludeExplosions = true;
				}
				//////////////////////////////////////////////////////////////////////////
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CStatObj::CheckCreateBillboardMaterial()
{
	// Check if billboard textures exist
	const char * arrTextureSuffixes[2] = { "_billbalb.dds", "_billbnorm.dds" };
	int nBillboardTexturesFound = 0;
	static_assert(EFTT_DIFFUSE == 0 && EFTT_NORMALS == 1, "This code relies on specific enum values.");
	for (int nSlot = EFTT_DIFFUSE; nSlot <= EFTT_NORMALS; nSlot++)
	{
		string szTextureName = m_szFileName;
		if (szTextureName.find(".cgf") != string::npos)
		{
			szTextureName.replace(".cgf", arrTextureSuffixes[nSlot]);
			if (gEnv->pCryPak->IsFileExist(szTextureName.c_str()))
				nBillboardTexturesFound++;
		}
	}

	// create billboard material and cgf
	if (nBillboardTexturesFound == 2)
	{
		m_pBillboardMaterial = GetMatMan()->LoadMaterial("%ENGINE%/EngineAssets/Materials/billboard_default", false);

		if (m_pBillboardMaterial)
		{
			// clone reference material and assign new textures
			m_pBillboardMaterial = GetMatMan()->CloneMaterial(m_pBillboardMaterial);

			SShaderItem& shaderItem = m_pBillboardMaterial->GetShaderItem();
			SInputShaderResources* inputShaderResources = gEnv->pRenderer->EF_CreateInputShaderResource(shaderItem.m_pShaderResources);
			for (int nSlot = EFTT_DIFFUSE; nSlot <= EFTT_NORMALS; nSlot++)
			{
				string szTextureName = m_szFileName;
				szTextureName.replace(".cgf", arrTextureSuffixes[nSlot]);
				inputShaderResources->m_Textures[nSlot].m_Name = szTextureName;
			}
			SShaderItem newShaderItem = gEnv->pRenderer->EF_LoadShaderItem(shaderItem.m_pShader->GetName(), false, 0, inputShaderResources, shaderItem.m_pShader->GetGenerationMask());
			m_pBillboardMaterial->AssignShaderItem(newShaderItem);
		}
	}
}
