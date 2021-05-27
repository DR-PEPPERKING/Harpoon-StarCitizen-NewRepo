// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VegetationMap.h"

#include "IEditorImpl.h"
#include "LogFile.h"
#include "VegetationSelectTool.h"
#include "Terrain/Heightmap.h"
#include "Terrain/Layer.h"

#include <Util/TempFileHelper.h>

#include "QT/Widgets/QWaitProgress.h"
#include "Util/ImageTIF.h"
#include "IAIManager.h"

#include <IUndoObject.h>
#include <Controls/QuestionDialog.h>
#include <Util/FileUtil.h>
#include <Util/XmlArchive.h>
#include <CrySystem/ConsoleRegistration.h>

#include <CryPhysics/IPhysics.h>

#pragma push_macro("GetObject")
#undef GetObject

namespace Private_VegetationMap
{

#pragma pack(push,1)
// Structure of vegetation object instance in file.
struct SVegInst_V1_0
{
	Vec3  pos;
	float scale;
	uint8 objectIndex;
	uint8 brightness;
	uint8 angle;
};
#pragma pack(pop)

#pragma pack(push,1)
// Structure of vegetation object instance in file.
// Deprecated (11/07/2014)
struct SVegInst_V2_0
{
	Vec3  pos;
	float scale;
	uint8 objectIndex;
	uint8 brightness;
	float angle;
	float angleX;
	float angleY;
};
#pragma pack(pop)

#pragma pack(push,1)
// Structure of vegetation object instance in file.
struct SVegInst
{
	Vec3  pos;
	float scale;
	int   objectIndex;
	uint8 brightness;
	float angle;
	float angleX;
	float angleY;
};
#pragma pack(pop)

const char* kVegetationMapFile = "VegetationMap.dat";
const int kMaxMapSize = 1024;
const int kMinMaskValue = 32;
}

#define SAFE_RELEASE_NODE(node) if (node) { (node)->ReleaseNode(); node = nullptr; }

//////////////////////////////////////////////////////////////////////////
//! Undo object for vegetation instance operations
class CUndoVegInstance : public IUndoObject
{
public:
	CUndoVegInstance(CVegetationInstance* pInst)
	{
		// Stores the current state of this object.
		assert(pInst != 0);
		m_pInstance = pInst;
		m_pInstance->AddRef();

		ZeroStruct(m_redo);
		m_undo.pos = m_pInstance->pos;
		m_undo.scale = m_pInstance->scale;
		m_undo.objectIndex = m_pInstance->object->GetIndex();
		m_undo.brightness = m_pInstance->brightness;
		m_undo.angle = m_pInstance->angle;
		m_undo.angleX = m_pInstance->angleX;
		m_undo.angleY = m_pInstance->angleY;
	}
	~CUndoVegInstance()
	{
		m_pInstance->Release();
	}
protected:
	virtual const char* GetDescription() { return "Vegetation Modify"; }

	virtual void        Undo(bool bUndo)
	{
		if (bUndo)
		{
			m_redo.pos = m_pInstance->pos;
			m_redo.scale = m_pInstance->scale;
			m_redo.objectIndex = m_pInstance->object->GetIndex();
			m_redo.brightness = m_pInstance->brightness;
			m_redo.angle = m_pInstance->angle;
			m_redo.angleX = m_pInstance->angleX;
			m_redo.angleY = m_pInstance->angleY;
		}
		m_pInstance->scale = m_undo.scale;
		m_pInstance->brightness = m_undo.brightness;
		m_pInstance->angle = m_undo.angle;
		m_pInstance->angleX = m_undo.angleX;
		m_pInstance->angleY = m_undo.angleY;

		// move instance to new position
		GetIEditorImpl()->GetVegetationMap()->MoveInstance(m_pInstance, m_undo.pos, false);

		Update();
	}
	virtual void Redo()
	{
		m_pInstance->scale = m_redo.scale;
		m_pInstance->brightness = m_redo.brightness;
		m_pInstance->angle = m_redo.angle;
		m_pInstance->angleX = m_redo.angleX;
		m_pInstance->angleY = m_redo.angleY;

		// move instance to new position
		GetIEditorImpl()->GetVegetationMap()->MoveInstance(m_pInstance, m_redo.pos, false);
		Update();
	}
	void Update()
	{
		CEditTool* pTool = GetIEditorImpl()->GetLevelEditorSharedState()->GetEditTool();
		if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CVegetationSelectTool)))
		{
			auto pVegetationTool = static_cast<CVegetationSelectTool*>(pTool);
			pVegetationTool->UpdateTransformManipulator();
		}
	}

protected:
	CVegetationInstance* m_pInstance;

private:
	Private_VegetationMap::SVegInst m_undo;
	Private_VegetationMap::SVegInst m_redo;
};

//////////////////////////////////////////////////////////////////////////
//! Undo object for vegetation instance operations with vegetation object relationships.
class CUndoVegInstanceEx : public CUndoVegInstance
{
public:
	CUndoVegInstanceEx(CVegetationInstance* pInst)
		: CUndoVegInstance(pInst)
	{
		m_undo = pInst->object;
	}
protected:

	virtual void Undo(bool bUndo)
	{
		if (bUndo)
		{
			m_redo = m_pInstance->object;
		}
		m_pInstance->object = m_undo;

		CUndoVegInstance::Undo(bUndo);
	}
	virtual void Redo()
	{
		m_pInstance->object = m_redo;
		CUndoVegInstance::Redo();
	}
private:
	TSmartPtr<CVegetationObject> m_undo;
	TSmartPtr<CVegetationObject> m_redo;
};

//////////////////////////////////////////////////////////////////////////
//! Undo object for CBaseObject.
class CUndoVegInstanceCreate : public IUndoObject
{
public:
	CUndoVegInstanceCreate(CVegetationInstance* pInst, bool bDeleted)
	{
		// Stores the current state of this object.
		assert(pInst != 0);
		m_pInstance = pInst;
		m_pInstance->AddRef();
		m_bDeleted = bDeleted;
	}

	~CUndoVegInstanceCreate()
	{
		m_pInstance->Release();
	}

protected:
	virtual const char* GetDescription() { return "Vegetation Create"; }

	virtual void        Undo(bool bUndo)
	{
		if (bUndo)
		{
			ClearThingSelection();
		}

		CVegetationMap* vegMap = GetIEditorImpl()->GetVegetationMap();
		if (m_bDeleted)
		{
			vegMap->AddObjInstance(m_pInstance);
			vegMap->MoveInstance(m_pInstance, m_pInstance->pos, false);
		}
		else
		{
			// invalidate area occupied by deleted instance
			vegMap->DeleteObjInstance(m_pInstance);
		}
		NotifyListeners();
	}

	virtual void Redo()
	{
		ClearThingSelection();
		CVegetationMap* vegMap = GetIEditorImpl()->GetVegetationMap();
		if (!m_bDeleted)
		{
			vegMap->AddObjInstance(m_pInstance);
			vegMap->MoveInstance(m_pInstance, m_pInstance->pos, false);
		}
		else
		{
			vegMap->DeleteObjInstance(m_pInstance);
		}
		NotifyListeners();
	}

	void ClearThingSelection()
	{
		CEditTool* pTool = GetIEditorImpl()->GetLevelEditorSharedState()->GetEditTool();
		if (pTool && pTool->IsKindOf(RUNTIME_CLASS(CVegetationSelectTool)))
		{
			auto pVegetationTool = static_cast<CVegetationSelectTool*>(pTool);
			pVegetationTool->ClearThingSelection(false);
		}
	}

	void NotifyListeners()
	{
		auto pVegetationMap = GetIEditorImpl()->GetVegetationMap();
		pVegetationMap->signalVegetationObjectChanged(m_pInstance->object);
	}

private:
	CVegetationInstance* m_pInstance;
	bool                 m_bDeleted;
};

//////////////////////////////////////////////////////////////////////////
//! Base Undo object for CVegetationObject.
class CUndoVegetationBase : public IUndoObject
{
public:
	CUndoVegetationBase(bool bIsReloadObjectsInPanel)
		: m_bIsReloadObjectsInEditor(bIsReloadObjectsInPanel)
	{
	}

protected:
	virtual const char* GetDescription() { return "Vegetation Modify"; }

	virtual void        Undo(bool bUndo)
	{
		NotifyListeners();
	}

	virtual void Redo()
	{
		NotifyListeners();
	}

	void NotifyListeners()
	{
		auto pVegetationMap = GetIEditorImpl()->GetVegetationMap();
		pVegetationMap->signalAllVegetationObjectsChanged(m_bIsReloadObjectsInEditor);
	}

private:
	bool m_bIsReloadObjectsInEditor;
};

//////////////////////////////////////////////////////////////////////////
//! Undo object for vegetation object.
class CUndoVegetationObject : public CUndoVegetationBase
{
public:
	CUndoVegetationObject(CVegetationObject* pObj)
		: CUndoVegetationBase(true)
	{
		m_pObj = pObj;
		m_undo = m_pObj->GetNumInstances();
	}

protected:
	virtual void Undo(bool bUndo)
	{
		if (bUndo)
		{
			m_redo = m_pObj->GetNumInstances();
		}
		m_pObj->SetNumInstances(m_undo);
		// Set hidden flag
		m_pObj->SetHidden(true);
		// Show instances
		GetIEditorImpl()->GetVegetationMap()->HideObject(m_pObj, false);
		__super::Undo(bUndo);
	}
	virtual void Redo()
	{
		m_pObj->SetNumInstances(m_redo);
		__super::Redo();
	}

private:
	TSmartPtr<CVegetationObject> m_pObj;
	int                          m_undo;
	int                          m_redo;
};

//////////////////////////////////////////////////////////////////////////
//! Undo object for creation and deleting CVegetationObject
class CUndoVegetationObjectCreate : public CUndoVegetationBase
{
public:
	CUndoVegetationObjectCreate(CVegetationObject* pObj, bool bDeleted)
		: CUndoVegetationBase(true)
	{
		// Stores the current state of this object.
		assert(pObj != 0);
		m_pObj = pObj;
		m_pObj->AddRef();
		m_bDeleted = bDeleted;
	}
	~CUndoVegetationObjectCreate()
	{
		m_pObj->Release();
	}
protected:
	virtual const char* GetDescription() { return "Vegetation Object Create"; }

	virtual void        Undo(bool bUndo)
	{
		CVegetationMap* vegMap = GetIEditorImpl()->GetVegetationMap();
		if (m_bDeleted)
		{
			vegMap->InsertObject(m_pObj);
		}
		else
		{
			vegMap->RemoveObject(m_pObj);
		}
		__super::Undo(bUndo);
	}
	virtual void Redo()
	{
		CVegetationMap* vegMap = GetIEditorImpl()->GetVegetationMap();
		if (!m_bDeleted)
		{
			vegMap->InsertObject(m_pObj);
		}
		else
		{
			vegMap->RemoveObject(m_pObj);
		}
		__super::Redo();
	}

private:
	CVegetationObject* m_pObj;
	bool               m_bDeleted;
};

//////////////////////////////////////////////////////////////////////////
CVegetationMap::CVegetationMap()
	: m_sectorSize{0}
	, m_numSectors{0}
	, m_mapSize{0}
	, m_minimalDistance{0.1f}
	, m_worldToSector{0}
	, m_numInstances{0}
	, m_nVersion{1}
	, m_storeBaseUndoState{eStoreUndo_Normal}
{
	srand(GetTickCount());

	REGISTER_COMMAND("ed_GenerateBillboardTextures", GenerateBillboards, 0, "Generate billboard textures for all level vegetations having UseSprites flag enabled. Textures are stored into <Objects> folder");
}

CVegetationMap::~CVegetationMap()
{
	ClearAll();
}

void CVegetationMap::ClearObjects()
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	ClearSectors();
	m_objects.clear();
}

void CVegetationMap::ClearAll()
{
	ClearObjects();

	m_sectors.clear();

	m_idToObject.clear();

	m_sectorSize = 0;
	m_numSectors = 0;
	m_mapSize = 0;
	m_minimalDistance = 0.1f;
	m_worldToSector = 0;
	m_numInstances = 0;
}

void CVegetationMap::RegisterInstance(CVegetationInstance* pInst)
{
	// re-create vegetation render node
	SAFE_RELEASE_NODE(pInst->pRenderNode);

	I3DEngine* p3DEngine = GetIEditorImpl()->Get3DEngine();

	if (pInst->object && !pInst->object->IsHidden())
		pInst->pRenderNode = p3DEngine->GetITerrain()->AddVegetationInstance(pInst->object->GetId(), pInst->pos, pInst->scale, pInst->brightness, RAD2BYTE(pInst->angle), RAD2BYTE(pInst->angleX), RAD2BYTE(pInst->angleY));

	if (pInst->pRenderNode && !pInst->object->IsAutoMerged())
	{
		GetIEditorImpl()->GetAIManager()->OnAreaModified(pInst->pRenderNode->GetBBox());
		p3DEngine->OnObjectModified(pInst->pRenderNode, pInst->pRenderNode->GetRndFlags());
	}

	// re-create ground decal render node
	UpdateGroundDecal(pInst);
}

const char* CVegetationMap::GetDataFilename() const
{
	using namespace Private_VegetationMap;
	return kVegetationMapFile;
}

void CVegetationMap::UpdateGroundDecal(CVegetationInstance* pInst)
{
	SAFE_RELEASE_NODE(pInst->pRenderNodeGroundDecal);
	if (pInst->object && pInst->object->m_pMaterialGroundDecal && pInst->pRenderNode)
	{
		pInst->pRenderNodeGroundDecal = (IDecalRenderNode*)GetIEditorImpl()->Get3DEngine()->CreateRenderNode(eERType_Decal);

		// update basic entity render flags
		unsigned int renderFlags = 0;

		pInst->pRenderNodeGroundDecal->SetRndFlags(renderFlags);

		// set properties
		SDecalProperties decalProperties;
		decalProperties.m_projectionType = SDecalProperties::eProjectOnTerrain;

		Matrix34 wtm;
		wtm.SetIdentity();
		float fRadiusXY = ((pInst->object && pInst->object->GetObject()) ? pInst->object->GetObject()->GetRadiusHors() : 1.f) * pInst->scale * 0.25f;
		wtm.SetScale(Vec3(fRadiusXY, fRadiusXY, fRadiusXY));
		wtm = wtm * Matrix34::CreateRotationZ(pInst->angle);
		wtm.SetTranslation(pInst->pos);

		// get normalized rotation (remove scaling)
		Matrix33 rotation(wtm);
		rotation.SetRow(0, rotation.GetRow(0).GetNormalized());
		rotation.SetRow(1, rotation.GetRow(1).GetNormalized());
		rotation.SetRow(2, rotation.GetRow(2).GetNormalized());

		decalProperties.m_pos = wtm.TransformPoint(Vec3(0, 0, 0));
		decalProperties.m_normal = wtm.TransformVector(Vec3(0, 0, 1));
		decalProperties.m_pMaterialName = pInst->object->m_pMaterialGroundDecal->GetName();
		decalProperties.m_radius = decalProperties.m_normal.GetLength();
		decalProperties.m_explicitRightUpFront = rotation;
		decalProperties.m_sortPrio = 10;
		pInst->pRenderNodeGroundDecal->SetDecalProperties(decalProperties);

		pInst->pRenderNodeGroundDecal->SetMatrix(wtm);
		pInst->pRenderNodeGroundDecal->SetViewDistRatio(200);
	}
}

float CVegetationMap::GenerateRotation(CVegetationObject* pObject, const Vec3& vPos)
{
	const bool bRandomRotation = pObject->IsRandomRotation();
	const int nRotationRange = pObject->GetRotationRangeToTerrainNormal();

	if (bRandomRotation || nRotationRange >= 360)
	{
		return (float)rand();
	}
	else if (nRotationRange > 0)
	{
		const Vec3 vTerrainNormal = GetIEditorImpl()->Get3DEngine()->GetTerrainSurfaceNormal(vPos);

		if (abs(vTerrainNormal.x) == 0.f && abs(vTerrainNormal.y) == 0.f)
		{
			return (float)rand();
		}
		else
		{
			const float rndDegree = (float)-nRotationRange * 0.5f + (float)(rand() % nRotationRange);
			const float finaleDegree = RAD2DEG(atan2f(vTerrainNormal.y, vTerrainNormal.x)) + rndDegree;
			return DEG2RAD(finaleDegree);
		}
	}
	return 0.f;
}

void CVegetationMap::ClearSectors()
{
	RemoveObjectsFromTerrain();

	for (auto& sector : m_sectors)
	{
		for (auto& pInst : sector)
		{
			pInst->pRenderNode = nullptr;
			SAFE_RELEASE_NODE(pInst->pRenderNodeGroundDecal);
			pInst->Release();
		}
		sector.clear();
	}
	m_numInstances = 0;
}

void CVegetationMap::Allocate(int nMapSize, bool bKeepData)
{
	using namespace Private_VegetationMap;
	CXmlArchive ar("Temp");
	if (bKeepData)
	{
		Serialize(ar);
	}

	ClearAll();

	m_mapSize = nMapSize;
	m_sectorSize = m_mapSize < kMaxMapSize ? 1 : m_mapSize / kMaxMapSize;
	m_numSectors = m_mapSize / m_sectorSize;
	m_worldToSector = 1.0f / m_sectorSize;

	// Allocate sectors map
	m_sectors.resize(m_numSectors * m_numSectors);

	if (bKeepData)
	{
		ar.bLoading = true;
		Serialize(ar);
	}
}

int CVegetationMap::GetSize() const
{
	return m_numSectors * m_sectorSize;
}

void CVegetationMap::PlaceObjectsOnTerrain()
{
	I3DEngine* p3DEngine = GetIEditorImpl()->Get3DEngine();
	if (!p3DEngine)
		return;

	// Clear all objects from 3d Engine.
	RemoveObjectsFromTerrain();

	CWaitProgress progress(_T("Placing Objects on Terrain"));

	int i = 0;
	for (auto& sector : m_sectors)
	{
		for (auto& pInst : sector)
		{
			if (!pInst->object->IsHidden())
			{
				// Stick vegetation to terrain.
				if (!pInst->object->IsAffectedByBrushes() && pInst->object->IsAffectedByTerrain())
					pInst->pos.z = p3DEngine->GetTerrainElevation(pInst->pos.x, pInst->pos.y);
				pInst->pRenderNode = nullptr;
				SAFE_RELEASE_NODE(pInst->pRenderNodeGroundDecal);
				RegisterInstance(pInst);
			}
		}

		++i;
		progress.Step(100 * i / (m_numSectors * m_numSectors));
	}
}

void CVegetationMap::RemoveObjectsFromTerrain()
{
	if (GetIEditorImpl()->Get3DEngine())
		GetIEditorImpl()->Get3DEngine()->RemoveAllStaticObjects();
}

void CVegetationMap::HideObject(CVegetationObject* object, bool bHide)
{
	if (object->IsHidden() == bHide)
		return;

	object->SetHidden(bHide);

	if (object->GetNumInstances() == 0)
	{
		return;
	}

	I3DEngine* p3DEngine = GetIEditorImpl()->Get3DEngine();

	for (auto& sector : m_sectors)
	{
		for (auto& pInst : sector)
		{
			if (pInst->object == object)
			{
				// Remove/Add to terrain.
				if (bHide)
				{
					SAFE_RELEASE_NODE(pInst->pRenderNode);
					SAFE_RELEASE_NODE(pInst->pRenderNodeGroundDecal);
				}
				else
				{
					// Stick vegetation to terrain.
					if (!pInst->object->IsAffectedByBrushes() && pInst->object->IsAffectedByTerrain())
						pInst->pos.z = p3DEngine->GetTerrainElevation(pInst->pos.x, pInst->pos.y);
					RegisterInstance(pInst);
				}
			}
		}
	}
}

void CVegetationMap::HideAllObjects(bool bHide)
{
	for (int i = 0; i < m_objects.size(); i++)
	{
		HideObject(m_objects[i], bHide);
	}
}

void CVegetationMap::MergeObjects(CVegetationObject* object, CVegetationObject* objectMerged)
{
	if (objectMerged->GetNumInstances() > 0)
	{
		if (CUndo::IsRecording())
		{
			CUndo::Record(new CUndoVegetationObject(object));
			CUndo::Record(new CUndoVegetationObject(objectMerged));
		}
		HideObject(object, true);
		HideObject(objectMerged, true);

		for (auto& sector : m_sectors)
		{
			for (auto& pInst : sector)
			{
				if (pInst->object == objectMerged)
				{
					if (CUndo::IsRecording())
						CUndo::Record(new CUndoVegInstanceEx(pInst));
					pInst->object = object;
					object->SetNumInstances(object->GetNumInstances() + 1);
					objectMerged->SetNumInstances(objectMerged->GetNumInstances() - 1);
				}
			}
		}
		HideObject(object, false);
		HideObject(objectMerged, false);
	}
	assert(objectMerged->GetNumInstances() == 0);
}

inline CVegetationMap::SectorInfo& CVegetationMap::GetVegSector(int x, int y)
{
	assert(x >= 0 && x < m_numSectors && y >= 0 && y < m_numSectors);
	return m_sectors[y * m_numSectors + x];
}

inline CVegetationMap::SectorInfo* CVegetationMap::GetVegSector(const Vec3& worldPos)
{
	int x = int(worldPos.x * m_worldToSector);
	int y = int(worldPos.y * m_worldToSector);
	if (x >= 0 && x < m_numSectors && y >= 0 && y < m_numSectors)
		return &m_sectors[y * m_numSectors + x];
	else
		return nullptr;
}

void CVegetationMap::AddObjInstance(CVegetationInstance* pInst)
{
	SectorInfo* si = GetVegSector(pInst->pos);
	if (!si)
	{
		return;
	}
	pInst->AddRef();

	si->push_back(pInst);

	// Increase number of instances.
	CVegetationObject* object = pInst->object;
	object->SetNumInstances(object->GetNumInstances() + 1);
	m_numInstances++;
}

CVegetationInstance* CVegetationMap::CreateObjInstance(CVegetationObject* object, const Vec3& pos, CVegetationInstance* pCopy, SectorInfo* si)
{
	if (!si)
	{
		si = GetVegSector(pos);
		if (!si)
			return nullptr;
	}

	CVegetationInstance* pInst = new CVegetationInstance;
	pInst->m_refCount = 1; // Starts with 1 reference.
	pInst->object = object;
	pInst->pos = pos;
	pInst->pRenderNode = nullptr;

	if (pCopy)
	{
		pInst->scale = pCopy->scale;
		pInst->brightness = pCopy->brightness;
		pInst->angleX = pCopy->angleX;
		pInst->angleY = pCopy->angleY;
		pInst->angle = pCopy->angle;
		pInst->pRenderNodeGroundDecal = pCopy->pRenderNodeGroundDecal;
		pInst->m_boIsSelected = pCopy->m_boIsSelected;
	}
	else
	{
		pInst->scale = 1;
		pInst->brightness = 255;
		pInst->angleX = 0;
		pInst->angleY = 0;
		pInst->angle = 0;
		pInst->pRenderNodeGroundDecal = 0;
		pInst->m_boIsSelected = false;
	}

	if (CUndo::IsRecording())
		CUndo::Record(new CUndoVegInstanceCreate(pInst, false));

	// Add object to end of the list of instances in sector.
	// Increase number of instances.
	object->SetNumInstances(object->GetNumInstances() + 1);
	m_numInstances++;

	si->push_back(pInst);
	return pInst;
}

void CVegetationMap::DeleteObjInstanceImpl(CVegetationInstance* pInst)
{
	if (CUndo::IsRecording())
		CUndo::Record(new CUndoVegInstanceCreate(pInst, true));

	if (pInst->pRenderNode)
	{
		GetIEditorImpl()->GetAIManager()->OnAreaModified(pInst->pRenderNode->GetBBox());
		GetIEditorImpl()->Get3DEngine()->OnObjectModified(pInst->pRenderNode, ERF_CASTSHADOWMAPS);
	}

	SAFE_RELEASE_NODE(pInst->pRenderNode);
	SAFE_RELEASE_NODE(pInst->pRenderNodeGroundDecal);
	pInst->object->SetNumInstances(pInst->object->GetNumInstances() - 1);
	m_numInstances--;
	assert(m_numInstances >= 0);
	pInst->Release();
}

void CVegetationMap::DeleteObjInstance(CVegetationInstance* pInst)
{
	SectorInfo* sector = GetVegSector(pInst->pos);
	if (sector)
	{
		sector->remove(pInst);
		DeleteObjInstanceImpl(pInst);
	}
}

void CVegetationMap::RemoveDuplVegetation(int x1, int y1, int x2, int y2)
{
	CUndo undo("Vegetation Remove Duplicates");
	// We keep maps of all items that were not removed sorted by both X and Y to easily search for nearby items
	std::multimap<float, CVegetationInstance*> xPositions;
	std::multimap<float, CVegetationInstance*> yPositions;
	const float minimalDistance = m_minimalDistance;
	const float minDistSq = minimalDistance * minimalDistance;
	auto canAddItem = [&xPositions, &yPositions, minimalDistance, minDistSq](CVegetationInstance* pInst) -> bool
	{
		const float x = pInst->pos.x;
		const float y = pInst->pos.y;

		// By using lower/upper_bound we can quickly find all items that match the x/y ranges.
		auto lowerXIt = xPositions.lower_bound(x - minimalDistance);
		auto upperXIt = xPositions.upper_bound(x + minimalDistance);

		auto lowerYIt = yPositions.lower_bound(y - minimalDistance);
		auto upperYIt = yPositions.upper_bound(y + minimalDistance);

		// If an item matches in X, we add its instance to an unordered set for fast lookup
		std::unordered_set<CVegetationInstance*> seen;
		for (auto xIt = lowerXIt; xIt != upperXIt; ++xIt)
			seen.insert(xIt->second);
		// If the item matches in Y, we see if it was added to the set (meaning it also matches in X)
		for (auto yIt = lowerYIt; yIt != upperYIt; ++yIt)
		{
			// If found, we still need to do a proper distance calculation, returning false means item has to be removed from the sector
			if (seen.find(yIt->second) != seen.end() &&
				Vec2(x, y).GetSquaredDistance(Vec2(yIt->second->pos)) < minDistSq)
			{
				return false;
			}
		}
		// Items that had no other items in the x&y range will be added to the persistent position maps and can remain in the sector
		xPositions.insert({ x, pInst });
		yPositions.insert({ y, pInst });
		return true;
	};

	if (x1 == -1)
	{
		x1 = 0;
		y1 = 0;
		x2 = m_numSectors - 1;
		y2 = m_numSectors - 1;
	}

	for (int j = y1; j <= y2; j++)
	{
		for (int i = x1; i <= x2; i++)
		{
			SectorInfo& sector = m_sectors[i + j * m_numSectors];
			for (auto it = sector.begin(); it != sector.end(); )
			{
				if (!canAddItem(*it))
				{
					DeleteObjInstanceImpl(*it);
					it = sector.erase(it);
				}
				else
				{
					++it;
				}
			}
		}
	}
	signalAllVegetationObjectsChanged(false);
}

CVegetationInstance* CVegetationMap::GetNearestInstance(const Vec3& pos, float radius)
{
	// check all sectors intersected by radius.
	float r = radius * m_worldToSector;
	float px = pos.x * m_worldToSector;
	float py = pos.y * m_worldToSector;
	int sx1 = int(px - r);
	sx1 = max(sx1, 0);
	int sx2 = int(px + r);
	sx2 = min(sx2, m_numSectors - 1);
	int sy1 = int(py - r);
	sy1 = max(sy1, 0);
	int sy2 = int(py + r);
	sy2 = min(sy2, m_numSectors - 1);

	CVegetationInstance* nearest = 0;
	float minDist = FLT_MAX;

	for (int y = sy1; y <= sy2; y++)
	{
		for (int x = sx1; x <= sx2; x++)
		{
			// For each sector check if any object is nearby.
			SectorInfo& si = GetVegSector(x, y);
			for (auto& pInst : si)
			{
				if (fabs(pInst->pos.x - pos.x) < radius && fabs(pInst->pos.y - pos.y) < radius)
				{
					float dist = pos.GetSquaredDistance(pInst->pos);
					if (dist < minDist)
					{
						minDist = dist;
						nearest = pInst;
					}
				}
			}
		}
	}
	return nearest;
}

void CVegetationMap::GetObjectInstances(float x1, float y1, float x2, float y2, std::vector<CVegetationInstance*>& instances)
{
	instances.reserve(100);
	// check all sectors intersected by radius.
	int sx1 = int(x1 * m_worldToSector);
	sx1 = max(sx1, 0);
	int sx2 = int(x2 * m_worldToSector);
	sx2 = min(sx2, m_numSectors - 1);
	int sy1 = int(y1 * m_worldToSector);
	sy1 = max(sy1, 0);
	int sy2 = int(y2 * m_worldToSector);
	sy2 = min(sy2, m_numSectors - 1);

	for (int y = sy1; y <= sy2; y++)
	{
		for (int x = sx1; x <= sx2; x++)
		{
			// For each sector check if any object is nearby.
			SectorInfo& si = GetVegSector(x, y);
			for (auto& pInst : si)
			{
				if (pInst->pos.x >= x1 && pInst->pos.x <= x2 && pInst->pos.y >= y1 && pInst->pos.y <= y2)
				{
					instances.push_back(pInst);
				}
			}
		}
	}
}

void CVegetationMap::GetAllInstances(std::vector<CVegetationInstance*>& instances)
{
	int k = 0;
	instances.resize(m_numInstances);
	for (const auto& sector : m_sectors)
	{
		for (const auto& pInst : sector)
		{
			instances[k++] = pInst;
		}
	}
}

bool CVegetationMap::IsPlaceEmpty(const Vec3& pos, float radius, CVegetationInstance* ignore)
{
	// check all sectors intersected by radius.
	if (pos.x < 0 || pos.y < 0 || pos.x > m_mapSize || pos.y > m_mapSize)
		return false;

	// check all sectors intersected by radius.
	float r = radius * m_worldToSector;
	float px = pos.x * m_worldToSector;
	float py = pos.y * m_worldToSector;
	int sx1 = int(px - r);
	sx1 = max(sx1, 0);
	int sx2 = int(px + r);
	sx2 = min(sx2, m_numSectors - 1);
	int sy1 = int(py - r);
	sy1 = max(sy1, 0);
	int sy2 = int(py + r);
	sy2 = min(sy2, m_numSectors - 1);
	for (int y = sy1; y <= sy2; y++)
	{
		for (int x = sx1; x <= sx2; x++)
		{
			// For each sector check if any object is within this radius.
			SectorInfo& si = GetVegSector(x, y);
			for (auto& pInst : si)
			{
				if (pInst != ignore && fabs(pInst->pos.x - pos.x) < radius && fabs(pInst->pos.y - pos.y) < radius)
				{
					return false;
				}
			}
		}
	}
	return true;
}

bool CVegetationMap::MoveInstance(CVegetationInstance* pInst, const Vec3& newPos, bool bTerrainAlign)
{
	if (!IsPlaceEmpty(newPos, m_minimalDistance, pInst))
	{
		return false;
	}

	if (pInst->pos != newPos)
		RecordUndo(pInst);

	// Then delete object.
	uint32 dwOldFlags = 0;
	if (pInst->pRenderNode)
		dwOldFlags = pInst->pRenderNode->m_nEditorSelectionID;

	SAFE_RELEASE_NODE(pInst->pRenderNode);
	SAFE_RELEASE_NODE(pInst->pRenderNodeGroundDecal);

	SectorInfo* from = GetVegSector(pInst->pos);
	SectorInfo* to = GetVegSector(newPos);

	if (!from || !to)
		return true;

	if (from != to)
	{
		// Relink object between sectors.
		from->remove(pInst);
		if (to)
		{
			to->push_back(pInst);
		}
	}

	pInst->pos = newPos;

	// Stick vegetation to terrain.
	if (bTerrainAlign)
	{
		if (!pInst->object->IsAffectedByBrushes() && pInst->object->IsAffectedByTerrain())
			pInst->pos.z = GetIEditorImpl()->Get3DEngine()->GetTerrainElevation(pInst->pos.x, pInst->pos.y);
	}

	RegisterInstance(pInst);

	if (pInst->pRenderNode)
	{
		pInst->pRenderNode->m_nEditorSelectionID = dwOldFlags;
	}

	return true;
}

bool CVegetationMap::CanPlace(CVegetationObject* object, const Vec3& pos, float radius)
{
	// check all sectors intersected by radius.
	if (pos.x < 0 || pos.y < 0 || pos.x > m_mapSize || pos.y > m_mapSize)
		return false;

	float r = radius * m_worldToSector;
	float px = pos.x * m_worldToSector;
	float py = pos.y * m_worldToSector;
	int sx1 = int(px - r);
	sx1 = max(sx1, 0);
	int sx2 = int(px + r);
	sx2 = min(sx2, m_numSectors - 1);
	int sy1 = int(py - r);
	sy1 = max(sy1, 0);
	int sy2 = int(py + r);
	sy2 = min(sy2, m_numSectors - 1);
	const float radiusSq = radius * radius;
	const float minDistSq = m_minimalDistance * m_minimalDistance;
	for (int y = sy1; y <= sy2; y++)
	{
		for (int x = sx1; x <= sx2; x++)
		{
			// For each sector check if any object is within this radius.
			SectorInfo& si = GetVegSector(x, y);
			for (auto& pInst : si)
			{
				// Only check objects that we need.
				if (pInst->object == object)
				{
					if (Vec2(pInst->pos).GetSquaredDistance(Vec2(pos)) < radiusSq)
						return false;
				}
				else
				{
					if (Vec2(pInst->pos).GetSquaredDistance(Vec2(pos)) < minDistSq)
						return false;
				}
			}
		}
	}
	return true;
}

CVegetationInstance* CVegetationMap::PlaceObjectInstance(const Vec3& worldPos, CVegetationObject* object)
{
	if (!CanPlace(object, worldPos, m_minimalDistance))
	{
		return nullptr;
	}

	CVegetationInstance* pInst = CreateObjInstance(object, worldPos);
	if (!pInst)
	{
		return nullptr;
	}

	pInst->angle = GenerateRotation(object, worldPos);
	pInst->scale = object->CalcVariableSize();

	// Stick vegetation to terrain.
	if (!pInst->object->IsAffectedByBrushes())
		pInst->pos.z = GetIEditorImpl()->Get3DEngine()->GetTerrainElevation(pInst->pos.x, pInst->pos.y);

	RegisterInstance(pInst);
	signalVegetationObjectChanged(object);

	return pInst;
}

void CVegetationMap::PaintBrush(CRect& rc, bool bCircle, CVegetationObject* object, Vec3* pPos)
{
	assert(object != 0);

	CHeightmap* pHeightmap = GetIEditorImpl()->GetHeightmap();

	GetIEditorImpl()->SetModifiedFlag();

	float unitSize = pHeightmap->GetUnitSize();

	int mapSize = m_numSectors * m_sectorSize;

	bool bProgress = rc.right - rc.left >= mapSize;
	CWaitProgress wait("Distributing objects", bProgress);

	// Intersect with map rectangle.
	// Offset by 1 from each side.
	float brushRadius2 = (rc.right - rc.left) * (rc.right - rc.left) / 4;
	rc &= CRect(1, 1, mapSize - 2, mapSize - 2);

	float AltMin = object->GetElevationMin();
	float AltMax = object->GetElevationMax();

	// HeightMap slope is defined as a ratio not as an angle. So we must convert user input in degrees to slope ratio
	float SlopeMin = tan(DEG2RAD(object->GetSlopeMin()));
	float SlopeMax = tan(DEG2RAD(object->GetSlopeMax()));

	float density = object->GetDensity();
	if (density <= 0)
		density = m_minimalDistance;

	int area = rc.Width() * rc.Height();
	int count = area / (density * density);

	// Limit from too much objects.
	if (count > area)
		count = area;

	bool bUndoStored = false;
	int j = 0;

	float x1 = rc.left;
	float y1 = rc.top;
	float width2 = (rc.right - rc.left) / 2.0f;
	float height2 = (rc.bottom - rc.top) / 2.0f;

	float cx = (rc.right + rc.left) / 2.0f;
	float cy = (rc.bottom + rc.top) / 2.0f;

	// Calculate the vegetation for every point in the area marked by the brush
	for (int i = 0; i < count; i++)
	{
		if (bProgress)
		{
			j++;
			if (j > 200 && (!wait.Step(100 * i / count)))
				break;
		}

		Vec3 p(x1 + cry_random(0.0f, 2.0f) * width2, y1 + cry_random(0.0f, 2.0f) * height2, 0);

		// Skip all coordinates outside the brush circle
		if (bCircle && (((p.x - cx) * (p.x - cx) + (p.y - cy) * (p.y - cy)) > brushRadius2))
			continue;

		// Get the height of the current point
		// swap x/y
		int hy = int(p.x / unitSize);
		int hx = int(p.y / unitSize);

		float currHeight = pHeightmap->GetXY(hx, hy);
		// Check if height value is within brush min/max altitude.
		if (currHeight < AltMin || currHeight > AltMax)
			continue;

		// Calculate the slope for this spot
		float slope = pHeightmap->GetAccurateSlope(hx, hy);

		// Check if slope is within brush min/max slope.
		if (slope < SlopeMin || slope > SlopeMax)
			continue;

		float fScale = object->CalcVariableSize();
		float placeRadius = fScale * object->GetObjectSize() * 0.5f;
		// Check if this place is empty.
		if (!CanPlace(object, p, placeRadius))
		{
			continue;
		}

		if (pPos && object->IsAffectedByBrushes())
		{
			p.z = pPos->z;
			float brushRadius = float(rc.right - rc.left) / 2;
			Vec3 pointerPos(p.x, p.y, pPos->z);
			Vec3 posUpper = pointerPos + Vec3(0, 0, 0.1f + brushRadius / 2);
			bool bHitBrush = false;
			p.z = CalcHeightOnBrushes(p, posUpper, bHitBrush);
			if (!bHitBrush && !object->IsAffectedByTerrain())
				return;
		}
		else
		{
			p.z = GetIEditorImpl()->Get3DEngine()->GetTerrainElevation(p.x, p.y);
		}

		if (!bUndoStored)
		{
			StoreBaseUndo();
			bUndoStored = true;
		}

		CVegetationInstance* pInst = CreateObjInstance(object, p);
		if (pInst)
		{
			pInst->angle = GenerateRotation(object, p);
			pInst->scale = fScale;
			RegisterInstance(pInst);
		}
	}

	// if undo stored, store info for redo
	if (bUndoStored)
		StoreBaseUndo();

	signalVegetationObjectChanged(object);
}

float CVegetationMap::CalcHeightOnBrushes(const Vec3& p, const Vec3& posUpper, bool& bHitBrush)
{
	IPhysicalWorld* world = GetIEditorImpl()->GetSystem()->GetIPhysicalWorld();
	ray_hit hit;

	bHitBrush = false;
	int numSkipEnts = 0;
	typedef IPhysicalEntity* PIPhysicalEntity;
	PIPhysicalEntity pSkipEnts[3];
	int col = 0;
	Vec3 vDir(0, 0, -1);
	int flags = rwi_stop_at_pierceable | rwi_ignore_terrain_holes;
	for (int chcnt = 0; chcnt < 3; chcnt++)
	{
		hit.pCollider = nullptr;
		col = world->RayWorldIntersection(posUpper, vDir * 1000, ent_all, flags, &hit, 1, pSkipEnts, numSkipEnts);
		IRenderNode* pRenderNode = nullptr;
		if (!hit.pCollider)
			break;
		pRenderNode = (IRenderNode*)hit.pCollider->GetForeignData(PHYS_FOREIGN_ID_STATIC);
		// if we hit terrain (!pRenderNone) or something else but not vegetation
		if (!pRenderNode || pRenderNode->GetRenderNodeType() != eERType_Vegetation)
		{
			bHitBrush = pRenderNode != nullptr;
			break;
		}

		pSkipEnts[numSkipEnts++] = hit.pCollider;
	}
	if (col && !hit.pt.IsZero())
		return hit.pt.z;

	return p.z;
}

void CVegetationMap::PaintBrightness(float x, float y, float w, float h, uint8 brightness, uint8 brightness_shadowmap)
{
	// Find sector range from world positions.
	int startSectorX = int(x * m_worldToSector);
	int startSectorY = int(y * m_worldToSector);
	int endSectorX = int((x + w) * m_worldToSector);
	int endSectorY = int((y + h) * m_worldToSector);

	// Clamp start and end sectors to valid range.
	if (startSectorX < 0)
		startSectorX = 0;
	if (startSectorY < 0)
		startSectorY = 0;
	if (endSectorX >= m_numSectors)
		endSectorX = m_numSectors - 1;
	if (endSectorY >= m_numSectors)
		endSectorY = m_numSectors - 1;

	float x2 = x + w;
	float y2 = y + h;

	// Iterate all sectors in range.
	for (int sy = startSectorY; sy <= endSectorY; sy++)
	{
		for (int sx = startSectorX; sx <= endSectorX; sx++)
		{
			// Iterate all objects in sector.
			SectorInfo& si = GetVegSector(sx, sy);
			for (auto& pInst : si)
			{
				if (pInst->pos.x >= x && pInst->pos.x < x2 && pInst->pos.y >= y && pInst->pos.y <= y2)
				{
					bool bNeedUpdate = false;
					if (pInst->object->HasDynamicDistanceShadows())
					{
						if (pInst->brightness != brightness_shadowmap)
							bNeedUpdate = true;
						// If object is not casting precalculated shadow (small grass etc..) affect it by shadow map.
						pInst->brightness = brightness_shadowmap;
					}
					else
					{
						if (pInst->brightness != brightness)
							bNeedUpdate = true;
						pInst->brightness = brightness;
					}
				}
			}
		}
	}
}

void CVegetationMap::ClearBrush(CRect& rc, bool bCircle, CVegetationObject* pObject)
{
	StoreBaseUndo();

	//GetISystem()->VTuneResume();
	GetIEditorImpl()->SetModifiedFlag();

	Vec3 p(0, 0, 0);

	// Intersect with map rectangle.
	// Offset by 1 from each side.
	float brushRadius2 = (rc.right - rc.left) * (rc.right - rc.left) / 4;
	float cx = (rc.right + rc.left) / 2;
	float cy = (rc.bottom + rc.top) / 2;

	float x1 = rc.left;
	float y1 = rc.top;
	float x2 = rc.right;
	float y2 = rc.bottom;

	// check all sectors intersected by radius.
	int sx1 = int(x1 * m_worldToSector);
	int sx2 = int(x2 * m_worldToSector);
	int sy1 = int(y1 * m_worldToSector);
	int sy2 = int(y2 * m_worldToSector);
	sx1 = max(sx1, 0);
	sx2 = min(sx2, m_numSectors - 1);
	sy1 = max(sy1, 0);
	sy2 = min(sy2, m_numSectors - 1);

	for (int y = sy1; y <= sy2; y++)
	{
		for (int x = sx1; x <= sx2; x++)
		{
			// For each sector check if any object is within this radius.
			SectorInfo& si = GetVegSector(x, y);

			for (auto it = si.begin(); it != si.end(); )
			{
				auto pInst = *it;
				if (pInst->object != pObject && pObject != nullptr)
				{
					++it;
					continue;
				}

				if (bCircle)
				{
					// Skip objects outside the brush circle
					if (((pInst->pos.x - cx) * (pInst->pos.x - cx) + (pInst->pos.y - cy) * (pInst->pos.y - cy)) > brushRadius2)
					{
						++it;
						continue;
					}
				}
				else
				{
					// Within rectangle.
					if (pInst->pos.x < x1 || pInst->pos.x > x2 || pInst->pos.y < y1 || pInst->pos.y > y2)
					{
						++it;
						continue;
					}
				}

				// Then delete object.
				StoreBaseUndo(eStoreUndo_Once);
				DeleteObjInstanceImpl(pInst);
				it = si.erase(it);
			}
		}
	}

	if (pObject)
		signalVegetationObjectChanged(pObject);
	else
		signalAllVegetationObjectsChanged(false);

	StoreBaseUndo();
}

void CVegetationMap::ClearMask(const string& maskFile)
{
	using namespace Private_VegetationMap;
	CLayer layer;
	layer.SetAutoGen(false);
	//	layer.SetSmooth(false);
	if (!layer.LoadMask(maskFile))
	{
		string str;
		str.Format("Error loading mask file %s", (const char*)maskFile);
		AfxGetMainWnd()->MessageBox(str, "Warning", MB_OK | MB_ICONEXCLAMATION);
		return;
	}
	int layerSize = m_numSectors;
	layer.PrecacheTexture();
	if (!layer.IsValid())
		return;

	GetIEditorImpl()->SetModifiedFlag();
	CWaitProgress wait("Clearing mask");

	for (int y = 0; y < layerSize; y++)
	{
		if (!wait.Step(100 * y / layerSize))
			break;

		for (int x = 0; x < layerSize; x++)
		{
			if (layer.GetLayerMaskPoint(x, y) > kMinMaskValue)
			{
				// Find sector.
				// Swap X/Y.
				SectorInfo& sector = m_sectors[y + x * m_numSectors];

				// Delete all instances in this sectors.
				for (auto& pInst : sector)
				{
					DeleteObjInstanceImpl(pInst);
				}
				sector.clear();
			}
		}
	}
	signalAllVegetationObjectsChanged(false);
}

CVegetationObject* CVegetationMap::CreateObject(CVegetationObject* prev)
{
	int id = GenerateVegetationObjectId();

	if (id < 0)
	{
		// Free id not found
		CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("Vegetation objects limit is reached."));
		return nullptr;
	}

	CVegetationObject* obj = new CVegetationObject(id);
	if (prev)
		obj->CopyFrom(*prev);

	m_objects.push_back(obj);
	m_idToObject[id] = obj;

	if (CUndo::IsRecording())
		CUndo::Record(new CUndoVegetationObjectCreate(obj, false));

	return obj;
}

bool CVegetationMap::InsertObject(CVegetationObject* obj)
{
	int id(GenerateVegetationObjectId());

	if (id < 0)
	{
		// Free id not found, created more then 256 objects
		CQuestionDialog::SWarning(QObject::tr(""), QObject::tr("Vegetation objects limit is reached."));
		return false;
	}

	// Assign the new Id to the vegetation object.
	obj->SetId(id);

	// Add it to the list.
	m_objects.push_back(obj);
	m_idToObject[id] = obj;

	if (CUndo::IsRecording())
		CUndo::Record(new CUndoVegetationObjectCreate(obj, false));

	return true;
}

CVegetationObject* CVegetationMap::GetObjectById(int id) const
{
	auto findIt = m_idToObject.find(id);
	if (findIt == m_idToObject.cend())
	{
		return nullptr;
	}
	return findIt->second;
}

void CVegetationMap::SetAIRadius(IStatObj* pObj, float radius)
{
	if (!pObj)
		return;
	pObj->SetAIVegetationRadius(radius);
	for (unsigned i = m_objects.size(); i-- != 0;)
	{
		CVegetationObject* object = m_objects[i];
		IStatObj* pSO = object->GetObject();
		if (pSO == pObj)
			object->SetAIRadius(radius);
	}
}

std::vector<CVegetationObject*> CVegetationMap::GetSelectedObjects() const
{
	std::vector<CVegetationObject*> selectedObjects;
	for (int i = 0; i < GetObjectCount(); ++i)
	{
		auto pVegetationObject = GetObject(i);
		if (pVegetationObject->IsSelected())
		{
			selectedObjects.push_back(pVegetationObject);
		}
	}
	return selectedObjects;
}

void CVegetationMap::RemoveObject(CVegetationObject* object)
{
	// Free id for this object.
	m_idToObject.erase(object->GetId());

	// First delete instances
	// Undo will be stored in DeleteObjInstance()
	if (object->GetNumInstances() > 0)
	{
		for (auto& sector : m_sectors)
		{
			for (auto it = sector.begin(); it != sector.end(); )
			{
				if ((*it)->object == object)
				{
					DeleteObjInstanceImpl(*it);
					it = sector.erase(it);
				}
				else
				{
					++it;
				}
			}
		}
	}

	// Store undo for object after deleting of instances. For correct queue in undo stack.
	if (CUndo::IsRecording())
		CUndo::Record(new CUndoVegetationObjectCreate(object, true));

	m_objects.erase(std::remove(m_objects.begin(), m_objects.end(), object), m_objects.end());
}

void CVegetationMap::ReplaceObject(CVegetationObject* pOldObject, CVegetationObject* pNewObject)
{
	if (pOldObject->GetNumInstances() > 0)
	{
		pNewObject->SetNumInstances(pNewObject->GetNumInstances() + pOldObject->GetNumInstances());

		for (auto& sector : m_sectors)
		{
			for (auto& pInst : sector)
			{
				if (pInst->object == pOldObject)
				{
					pInst->object = pNewObject;
				}
			}
		}
	}
	RemoveObject(pOldObject);
}

void CVegetationMap::RepositionObject(CVegetationObject* object)
{
	for (auto& sector : m_sectors)
	{
		for (auto& pInst : sector)
		{
			if (pInst->object == object)
			{
				RegisterInstance(pInst);
			}
		}
	}
}

void CVegetationMap::OnHeightMapChanged()
{
	// Iterator over all sectors.
	I3DEngine* p3DEngine = GetIEditorImpl()->Get3DEngine();
	for (auto& sector : m_sectors)
	{
		for (auto& pInst : sector)
		{
			if (!pInst->object->IsAffectedByBrushes() && pInst->object->IsAffectedByTerrain())
				pInst->pos.z = p3DEngine->GetTerrainElevation(pInst->pos.x, pInst->pos.y);

			if (pInst->pRenderNode && !pInst->object->IsAutoMerged())
			{
				// fix vegetation position
				{
					p3DEngine->UnRegisterEntityAsJob(pInst->pRenderNode);
					Matrix34 mat;
					mat.SetIdentity();
					mat.SetTranslation(pInst->pos);
					pInst->pRenderNode->SetMatrix(mat);
					p3DEngine->RegisterEntity(pInst->pRenderNode);
				}

				// fix ground decal position
				if (pInst->pRenderNodeGroundDecal)
				{
					p3DEngine->UnRegisterEntityAsJob(pInst->pRenderNodeGroundDecal);
					Matrix34 wtm;
					wtm.SetIdentity();
					float fRadiusXY = ((pInst->object && pInst->object->GetObject()) ? pInst->object->GetObject()->GetRadiusHors() : 1.f) * pInst->scale * 0.25f;
					wtm.SetScale(Vec3(fRadiusXY, fRadiusXY, fRadiusXY));
					wtm = wtm * Matrix34::CreateRotationZ(pInst->angle);
					wtm.SetTranslation(pInst->pos);

					pInst->pRenderNodeGroundDecal->SetMatrix(wtm);
					p3DEngine->RegisterEntity(pInst->pRenderNodeGroundDecal);
				}
			}
		}
	}
}

void CVegetationMap::ScaleObjectInstances(CVegetationObject* object, float fScale, AABB* outModifiedArea)
{
	for (auto& sector : m_sectors)
	{
		for (auto& pInst : sector)
		{
			if (pInst->object == object)
			{
				// add the box before instance scaling
				if (nullptr != outModifiedArea)
				{
					outModifiedArea->Add(pInst->pRenderNode->GetBBox());
				}

				// scale instance
				pInst->scale *= fScale;
				RegisterInstance(pInst);

				// add the box after the instance scaling
				if (nullptr != outModifiedArea)
				{
					outModifiedArea->Add(pInst->pRenderNode->GetBBox());
				}
			}
		}
	}
}

void CVegetationMap::RandomRotateInstances(CVegetationObject* object)
{
	bool bUndo = CUndo::IsRecording();
	for (auto& sector : m_sectors)
	{
		for (auto& pInst : sector)
		{
			if (pInst->object == object)
			{
				if (bUndo)
					RecordUndo(pInst);
				pInst->angle = rand();
				RegisterInstance(pInst);
			}
		}
	}
}

void CVegetationMap::ClearRotateInstances(CVegetationObject* object)
{
	bool bUndo = CUndo::IsRecording();
	for (auto& sector : m_sectors)
	{
		for (auto& pInst : sector)
		{
			if (pInst->object == object && pInst->angle != 0)
			{
				if (bUndo)
					RecordUndo(pInst);
				pInst->angle = 0;
				RegisterInstance(pInst);
			}
		}
	}
}

void CVegetationMap::DistributeVegetationObject(CVegetationObject* object)
{
	CRect rc(0, 0, GetSize(), GetSize());
	// emits signalVegetationObjectChanged
	PaintBrush(rc, false, object);
}

void CVegetationMap::ClearVegetationObject(CVegetationObject* object)
{
	CRect rc(0, 0, GetSize(), GetSize());
	// emits signalVegetationObjectChanged
	ClearBrush(rc, false, object);
}

void CVegetationMap::MergeVegetationObjects(const std::vector<CVegetationObject*>& objects)
{
	if (objects.size() < 2)
	{
		return;
	}

	CUndo undo("Vegetation Merge");
	std::vector<CVegetationObject*> deleteVegetationObjects;
	CVegetationObject* pMergeVegetationObject = objects.front();
	for (int i = 1; i < objects.size(); ++i)
	{
		MergeObjects(pMergeVegetationObject, objects[i]);
		deleteVegetationObjects.push_back(objects[i]);
	}

	for (auto pDeleteObject : deleteVegetationObjects)
	{
		RemoveObject(pDeleteObject);
	}
	signalVegetationObjectsMerged();
}

void CVegetationMap::Serialize(CXmlArchive& xmlAr)
{
	if (xmlAr.bLoading)
	{
		CLogFile::WriteLine("Loading Vegetation Map...");

		ClearObjects();

		CWaitProgress progress(_T("Loading Static Objects"));

		XmlNodeRef mainNode = xmlAr.root;

		if (mainNode && !stricmp(mainNode->getTag(), "VegetationMap"))
		{
			m_nVersion = 1;
			mainNode->getAttr("Version", m_nVersion);

			XmlNodeRef objects = mainNode->findChild("Objects");
			if (objects)
			{
				int numObjects = objects->getChildCount();
				for (int i = 0; i < numObjects; i++)
				{
					CVegetationObject* obj = CreateObject();
					if (obj)
						obj->Serialize(objects->getChild(i), xmlAr.bLoading);
				}
			}
		}

		SerializeInstances(xmlAr);
		LoadOldStuff(xmlAr);

		// Now display all objects on terrain
		PlaceObjectsOnTerrain();
	}
	else
	{
		CLogFile::WriteLine("Saving Vegetation Map...");

		xmlAr.root = XmlHelpers::CreateXmlNode("VegetationMap");
		XmlNodeRef mainNode = xmlAr.root;

		m_nVersion = 3;
		mainNode->setAttr("Version", m_nVersion);

		XmlNodeRef objects = mainNode->newChild("Objects");
		SerializeObjects(objects);
		SerializeInstances(xmlAr);
	}
}

void CVegetationMap::SerializeObjects(XmlNodeRef& vegetationNode)
{
	for (size_t i = 0, cnt = m_objects.size(); i < cnt; ++i)
	{
		XmlNodeRef node = vegetationNode->newChild("Object");
		m_objects[i]->Serialize(node, false);
	}
}

void CVegetationMap::SerializeInstances(CXmlArchive& xmlAr, CRect* saveRect)
{
	using namespace Private_VegetationMap;
	if (xmlAr.bLoading)
	{
		Vec3 posofs(0, 0, 0);
		if (!saveRect)
		{
			ClearSectors();
		}
		else
		{
			posofs.x = saveRect->left;
			posofs.y = saveRect->top;
		}

		int numObjects = m_objects.size();
		int arraySize;
		void* pData = nullptr;
		if (!xmlAr.pNamedData->GetDataBlock("VegetationInstancesArray", pData, arraySize))
			return;

		if (m_nVersion == 1)
		{
			SVegInst_V1_0* pVegInst = (SVegInst_V1_0*)pData;
			int numInst = arraySize / sizeof(SVegInst_V1_0);
			for (int i = 0; i < numInst; i++)
			{
				if (pVegInst[i].objectIndex >= numObjects)
					continue;
				CVegetationObject* object = m_objects[pVegInst[i].objectIndex];
				CVegetationInstance* obj = CreateObjInstance(object, pVegInst[i].pos + posofs);
				if (obj)
				{
					obj->scale = pVegInst[i].scale;
					obj->brightness = pVegInst[i].brightness;
					obj->angle = BYTE2RAD(pVegInst[i].angle);
					obj->angleX = 0;
					obj->angleY = 0;
				}
			}
		}
		else if (m_nVersion == 2)
		{
			SVegInst_V2_0* pVegInst = (SVegInst_V2_0*)pData;
			int numInst = arraySize / sizeof(SVegInst_V2_0);
			for (int i = 0; i < numInst; i++)
			{
				if (pVegInst[i].objectIndex >= numObjects)
				{
					continue;
				}
				CVegetationObject* object = m_objects[pVegInst[i].objectIndex];
				CVegetationInstance* pInst = CreateObjInstance(object, pVegInst[i].pos + posofs);
				if (pInst)
				{
					pInst->scale = pVegInst[i].scale;
					pInst->brightness = pVegInst[i].brightness;
					pInst->angle = pVegInst[i].angle;
					pInst->angleX = pVegInst[i].angleX;
					pInst->angleY = pVegInst[i].angleY;
				}
			}
		}
		else
		{
			SVegInst* pVegInst = (SVegInst*)pData;
			int numInst = arraySize / sizeof(SVegInst);
			for (int i = 0; i < numInst; i++)
			{
				if (pVegInst[i].objectIndex < numObjects)
				{
					CVegetationObject* object = m_objects[pVegInst[i].objectIndex];
					CVegetationInstance* pInst = CreateObjInstance(object, pVegInst[i].pos + posofs);
					if (pInst)
					{
						pInst->scale = pVegInst[i].scale;
						pInst->brightness = pVegInst[i].brightness;
						pInst->angle = pVegInst[i].angle;
						pInst->angleX = pVegInst[i].angleX;
						pInst->angleY = pVegInst[i].angleY;
					}
				}
			}
		}
		signalAllVegetationObjectsChanged(false);
	}
	else
	{
		if (m_numInstances == 0)
			return;

		int arraySize = sizeof(SVegInst) * m_numInstances;
		SVegInst* array = (SVegInst*)malloc(arraySize);

		// Assign indices to objects.
		for (int i = 0; i < GetObjectCount(); ++i)
		{
			GetObject(i)->SetIndex(i);
		}

		float x1, y1, x2, y2;
		if (saveRect)
		{
			x1 = saveRect->left;
			y1 = saveRect->top;
			x2 = saveRect->right;
			y2 = saveRect->bottom;
		}

		// Fill array.
		int k = 0;
		for (auto& sector : m_sectors)
		{
			for (auto& pInst : sector)
			{
				if (saveRect)
				{
					if (pInst->pos.x < x1 || pInst->pos.x > x2 || pInst->pos.y < y1 || pInst->pos.y > y2)
						continue;
				}
				array[k].pos = pInst->pos;
				array[k].scale = pInst->scale;
				array[k].brightness = pInst->brightness;
				array[k].angle = pInst->angle;
				array[k].angleX = pInst->angleX;
				array[k].angleY = pInst->angleY;
				array[k].objectIndex = pInst->object->GetIndex();
				k++;
			}
		}
		// Save this array.
		if (k > 0)
			xmlAr.pNamedData->AddDataBlock("VegetationInstancesArray", array, k * sizeof(SVegInst));
		free(array);
	}
}

void CVegetationMap::ClearSegment(const AABB& bb)
{
	for (auto& sector : m_sectors)
	{
		for (auto it = sector.begin(); it != sector.end(); )
		{
			auto pInst = *it;
			if (pInst->pos.x < bb.min.x || pInst->pos.x >= bb.max.x || pInst->pos.y < bb.min.y || pInst->pos.y >= bb.max.y)
			{
				++it;
			}
			else
			{
				DeleteObjInstanceImpl(pInst);
				it = sector.erase(it);
			}
		}
	}

	signalAllVegetationObjectsChanged(false);
}

void CVegetationMap::ImportSegment(CMemoryBlock& mem, const Vec3& vOfs)
{
	using namespace Private_VegetationMap;
	//TODO: load vegetation objects info, update m_objects, remap indices
	int iInstances = 0;

	typedef std::map<int, CryGUID>  TIntGuidMap;
	typedef std::pair<int, CryGUID> TIntGuidPair;

	typedef std::map<int, int>      TIntIntMap;
	typedef std::pair<int, int>     TIntIntPair;

	bool bIsVegetationObjectInfoSaved = false;
	unsigned char* p = (unsigned char*)mem.GetBuffer();
	int curr = 0;

	if (mem.GetSize() > sizeof(int))
	{
		int bckwCompatabiliyFlag = 0;// = 0xFFFFFFFF;
		// flag for backward compatibility
		memcpy(&bckwCompatabiliyFlag, &p[curr], sizeof(int));
		curr += sizeof(int);

		if (bckwCompatabiliyFlag == 0xFFFFFFFF)
			bIsVegetationObjectInfoSaved = true;
	}

	TIntIntMap tIndexMap;
	if (bIsVegetationObjectInfoSaved)
	{
		// IntGuidMap
		TIntGuidMap VegetationIndexGuidMap;

		int numMapObjects = 0;
		// num objects
		memcpy(&numMapObjects, &p[curr], sizeof(int));
		curr += sizeof(int);

		for (int i = 0; i < numMapObjects; i++)
		{
			TIntGuidPair tPair;
			//object index
			memcpy(&tPair.first, &p[curr], sizeof(int));
			curr += sizeof(int);

			// GUID;
			memcpy(&tPair.second, &p[curr], sizeof(GUID));
			curr += sizeof(GUID);

			VegetationIndexGuidMap.insert(tPair);
		}

		//correct mapping
		TIntGuidMap::iterator it;

		for (it = VegetationIndexGuidMap.begin(); it != VegetationIndexGuidMap.end(); ++it)
		{
			for (int i = 0; i < GetObjectCount(); i++)
			{
				if (GetObject(i)->GetGUID() == it->second)
				{
					TIntIntPair tIndexPair;
					tIndexPair.first = it->first;
					tIndexPair.second = i;
					tIndexMap.insert(tIndexPair);
					break;
				}
			}
		}

		// instances
		memcpy(&iInstances, &p[curr], sizeof(int)); // num of instances
		curr += sizeof(int);
	}

	SVegInst* array = bIsVegetationObjectInfoSaved ? (SVegInst*) &p[curr] : (SVegInst*) mem.GetBuffer();
	int numInst = bIsVegetationObjectInfoSaved ? iInstances : mem.GetSize() / sizeof(SVegInst);
	int numObjects = m_objects.size();
	for (int i = 0; i < numInst; i++)
	{
		int objectIndex = array[i].objectIndex;

		if (bIsVegetationObjectInfoSaved)
		{
			TIntIntMap::iterator it;
			it = tIndexMap.find(objectIndex);
			if (it == tIndexMap.end())
			{
				continue;
			}
			else
			{
				objectIndex = it->second;
			}
		}

		if (objectIndex >= numObjects)
			continue;
		CVegetationObject* object = m_objects[objectIndex];
		CVegetationInstance* pInst = CreateObjInstance(object, array[i].pos + vOfs);
		if (pInst)
		{
			pInst->scale = array[i].scale;
			pInst->brightness = array[i].brightness;
			pInst->angle = array[i].angle;
			if (!object->IsHidden())
				RegisterInstance(pInst);
		}
	}
	signalAllVegetationObjectsChanged(false);
}

void CVegetationMap::ExportSegment(CMemoryBlock& mem, const AABB& bb, const Vec3& vOfs)
{
	using namespace Private_VegetationMap;
	//TODO: save vegetation objects info with number of instances per object

	// Assign indices to objects.
	for (int i = 0; i < GetObjectCount(); i++)
	{
		GetObject(i)->SetIndex(i);
	}

	int totalInstanceCount = 0;
	std::map<int, CryGUID> vegetationIndexGuidMap;

	for (auto& sector : m_sectors)
	{
		for (auto& pInst : sector)
		{
			if (pInst->pos.x < bb.min.x || pInst->pos.x >= bb.max.x || pInst->pos.y < bb.min.y || pInst->pos.y >= bb.max.y)
				continue;

			++totalInstanceCount;

			if (vegetationIndexGuidMap.find(pInst->object->GetIndex()) == vegetationIndexGuidMap.end())
			{
				vegetationIndexGuidMap.insert(std::make_pair<>(pInst->object->GetIndex(), pInst->object->GetGUID()));
			}
		}
	}

	if (totalInstanceCount == 0)
	{
		mem.Free();
		return;
	}

	//calculating size of map
	int sizeOfMem = 0;
	sizeOfMem += sizeof(int); // flag for backward compatibility
	sizeOfMem += sizeof(int); // num of objects

	sizeOfMem += vegetationIndexGuidMap.size() * (sizeof(int) + sizeof(GUID));

	sizeOfMem += sizeof(totalInstanceCount);            // num of instances
	sizeOfMem += sizeof(SVegInst) * totalInstanceCount; // instances

	int numElems = vegetationIndexGuidMap.size();

	CMemoryBlock tmp;
	tmp.Allocate(sizeOfMem);
	unsigned char* p = (unsigned char*)tmp.GetBuffer();
	int curr = 0;

	int bckwCompatabiliyFlag = 0xFFFFFFFF;
	memcpy(&p[curr], &bckwCompatabiliyFlag, sizeof(int)); // flag for backward compatibility
	curr += sizeof(int);

	memcpy(&p[curr], &numElems, sizeof(int)); // num of objects
	curr += sizeof(int);

	for (const auto& it : vegetationIndexGuidMap)
	{
		memcpy(&p[curr], &(it.first), sizeof(int));
		curr += sizeof(int); //object index

		memcpy(&p[curr], &(it.second), sizeof(GUID));
		curr += sizeof(GUID); // GUID;
	}

	// save instances
	memcpy(&p[curr], &totalInstanceCount, sizeof(int)); // num of instances
	curr += sizeof(int);

	SVegInst* array = (SVegInst*) &p[curr];
	int k = 0;
	for (auto& sector : m_sectors)
	{
		for (auto& pInst : sector)
		{
			if (pInst->pos.x < bb.min.x || pInst->pos.x >= bb.max.x || pInst->pos.y < bb.min.y || pInst->pos.y >= bb.max.y)
				continue;

			array[k].pos = pInst->pos + vOfs;
			array[k].scale = pInst->scale;
			array[k].brightness = pInst->brightness;
			array[k].angle = pInst->angle;
			array[k].objectIndex = pInst->object->GetIndex();
			k++;
		}
	}
	assert(k == totalInstanceCount);
	tmp.Compress(mem);
}

void CVegetationMap::LoadOldStuff(CXmlArchive& xmlAr)
{
	if (!xmlAr.root)
		return;

	{
		XmlNodeRef mainNode = xmlAr.root->findChild("VegetationMap");
		if (mainNode)
		{
			XmlNodeRef objects = mainNode->findChild("Objects");
			if (objects)
			{
				int numObjects = objects->getChildCount();
				for (int i = 0; i < numObjects; i++)
				{
					CVegetationObject* obj = CreateObject();
					if (obj)
						obj->Serialize(objects->getChild(i), xmlAr.bLoading);
				}
			}
			SerializeInstances(xmlAr);
		}
	}

	XmlNodeRef mainNode = xmlAr.root->findChild("VegetationMap");
	if (!mainNode)
		return;

	void* pData = 0;
	int nSize;
	if (xmlAr.pNamedData->GetDataBlock("StatObjectsArray", pData, nSize))
	{
		if (nSize != 2048 * 2048)
			return;

		CVegetationObject* usedObjects[256];
		ZeroStruct(usedObjects);

		for (int i = 0; i < m_objects.size(); ++i)
		{
			usedObjects[m_objects[i]->GetIndex()] = m_objects[i];
		}

		Vec3 pos;
		uint8* pMap = (uint8*)pData;
		for (int y = 0; y < 2048; ++y)
		{
			for (int x = 0; x < 2048; ++x)
			{
				int i = x + 2048 * y;
				if (!pMap[i])
					continue;

				unsigned int objIndex = pMap[i] - 1;
				if (!usedObjects[objIndex])
					continue;

				//Swap x/y
				pos.x = y;
				pos.y = x;
				pos.z = GetIEditorImpl()->Get3DEngine()->GetTerrainElevation(pos.x, pos.y);
				CVegetationInstance* pInst = CreateObjInstance(usedObjects[objIndex], pos);
				if (pInst)
				{
					pInst->scale = usedObjects[objIndex]->CalcVariableSize();
				}
			}
		}
		signalAllVegetationObjectsChanged(false);
	}
}

//! Generate shadows from static objects and place them in shadow map bitarray.
void CVegetationMap::GenerateShadowMap(CByteImage& shadowmap, float shadowAmmount, const Vec3& sunVector)
{
	CHeightmap* pHeightmap = GetIEditorImpl()->GetHeightmap();

	int width = shadowmap.GetWidth();

	//@FIXME: Hardcoded.
	int sectorSizeInMeters = 64;

	float unitSize = pHeightmap->GetUnitSize();
	int numSectors = (pHeightmap->GetWidth() * unitSize) / sectorSizeInMeters;

	int sectorSize = shadowmap.GetWidth() / numSectors;
	int sectorSize2 = sectorSize * 2;
	assert(sectorSize > 0);

	uint32 shadowValue = shadowAmmount;

	bool bProgress = width >= 2048;
	CWaitProgress wait("Calculating Objects Shadows", bProgress);

	unsigned char* sectorImage = (unsigned char*)malloc(sectorSize * sectorSize * 3);
	unsigned char* sectorImage2 = (unsigned char*)malloc(sectorSize2 * sectorSize2 * 3);

	for (int y = 0; y < numSectors; y++)
	{
		if (bProgress)
		{
			if (!wait.Step(y * 100 / numSectors))
				break;
		}

		for (int x = 0; x < numSectors; x++)
		{
			//			GetIEditorImpl()->Get3DEngine()->MakeTerrainLightMap( y*sectorSizeInMeters,x*sectorSizeInMeters,sectorSizeInMeters,sectorImage2,sectorSize2 );
			memset(sectorImage2, 255, sectorSize2 * sectorSize2 * 3);

			// Scale sector image down and store into the shadow map.
			{
				int pos;
				uint32 color;
				int x1 = x * sectorSize;
				int y1 = y * sectorSize;
				for (int j = 0; j < sectorSize; j++)
				{
					int sx1 = x1 + (sectorSize - j - 1);
					for (int i = 0; i < sectorSize; i++)
					{
						pos = (i + j * sectorSize2) * 2 * 3;
						color = (shadowValue *
						         ((uint32)
						          (255 - sectorImage2[pos]) +
						          (255 - sectorImage2[pos + 3]) +
						          (255 - sectorImage2[pos + sectorSize2 * 3]) +
						          (255 - sectorImage2[pos + sectorSize2 * 3 + 3])
						         )) >> 10;
						//						color = color*shadowValue >> 8;
						// swap x/y
						//color = (255-sectorImage2[(i+j*sectorSize)*3]);
						shadowmap.ValueAt(sx1, y1 + i) = color;
					}
				}
			}
		}
	}
	free(sectorImage);
	free(sectorImage2);
}

int CVegetationMap::ExportObject(CVegetationObject* object, XmlNodeRef& node, CRect* saveRect)
{
	int numSaved = 0;
	assert(object != 0);
	object->Serialize(node, false);
	if (object->GetNumInstances() > 0)
	{
		// Export instances
		XmlNodeRef instancesNode = node->newChild("Instances");
		for (const auto& sector : m_sectors)
		{
			for (const auto& pInst : sector)
			{
				if (pInst->object == object)
				{
					if (saveRect)
					{
						if (pInst->pos.x < saveRect->left || pInst->pos.x > saveRect->right || pInst->pos.y < saveRect->top || pInst->pos.y > saveRect->bottom)
							continue;
					}
					numSaved++;
					XmlNodeRef node = instancesNode->newChild("Instance");
					node->setAttr("Pos", pInst->pos);
					if (pInst->scale != 1)
						node->setAttr("Scale", pInst->scale);
					if (pInst->brightness != 255)
						node->setAttr("Brightness", (int)pInst->brightness);
					if (pInst->angle != 0)
						node->setAttr("Angle", (int)pInst->angle);
				}
			}
		}
	}
	return numSaved;
}

void CVegetationMap::ImportObject(XmlNodeRef& node, const Vec3& offset)
{
	CVegetationObject* object = CreateObject();
	if (!object)
		return;
	object->Serialize(node, true);

	// Check if object with this GUID. already exists.
	for (int i = 0; i < GetObjectCount(); i++)
	{
		CVegetationObject* pOldObject = GetObject(i);
		if (pOldObject == object)
			continue;
		if (pOldObject->GetGUID() == object->GetGUID())
		{
			ReplaceObject(pOldObject, object);
			/*
			   // 2 objects have same GUID;
			   string msg;
			   msg.Format( _T("Vegetation Object %s %s already exists in the level!.\r\nOverride existing object?"),
			              GuidUtil::ToString(pOldObject->GetGUID()),(const char*)pOldObject->GetFileName() );

			   if (QDialogButtonBox::StandardButton::Yes == CQuestionDialog::SQuestion(QObject::tr(msg), QObject::tr()))
			   {
			   ReplaceObject( pOldObject,object );
			   }
			   else
			   {
			   RemoveObject( object );
			   object = pOldObject;
			   }
			 */
			break;
		}
	}

	XmlNodeRef instancesNode = node->findChild("Instances");
	if (instancesNode)
	{
		int numChilds = instancesNode->getChildCount();
		for (int i = 0; i < numChilds; i++)
		{
			Vec3 pos(0, 0, 0);
			float scale = 1;
			int brightness = 255;
			int angle = 0;

			XmlNodeRef inst = instancesNode->getChild(i);
			inst->getAttr("Pos", pos);
			inst->getAttr("Scale", scale);
			inst->getAttr("Brightness", brightness);
			inst->getAttr("Angle", angle);

			pos += offset;
			CVegetationInstance* obj = GetNearestInstance(pos, 0.01f);
			if (obj && obj->pos == pos)
			{
				// Delete previous object at same position.
				DeleteObjInstance(obj);
			}
			obj = CreateObjInstance(object, pos);
			if (obj)
			{
				obj->scale = scale;
				obj->brightness = brightness;
				obj->angle = angle;
				// Stick vegetation to terrain.
				if (!obj->object->IsAffectedByBrushes() && obj->object->IsAffectedByTerrain())
					obj->pos.z = GetIEditorImpl()->Get3DEngine()->GetTerrainElevation(obj->pos.x, obj->pos.y);
			}
		}
	}
	RepositionObject(object);
}

void CVegetationMap::ImportObjectsFromXml(const string& filename)
{
	auto pXmlRoot = XmlHelpers::LoadXmlFromFile(filename);
	if (!pXmlRoot)
	{
		return;
	}

	for (int i = 0; i < pXmlRoot->getChildCount(); ++i)
	{
		ImportObject(pXmlRoot->getChild(i));
	}
	signalAllVegetationObjectsChanged(true);
}

void CVegetationMap::ExportObjectsToXml(const string& filename)
{
	const auto selectedObjects = GetSelectedObjects();

	if (selectedObjects.size() > 0)
	{
		auto pRoot = XmlHelpers::CreateXmlNode("Vegetation");

		for (auto pSelectedObject : selectedObjects)
		{
			auto node = pRoot->newChild("VegetationObject");
			ExportObject(pSelectedObject, node);
		}
		XmlHelpers::SaveXmlNode(pRoot, filename);
	}
}

void CVegetationMap::DrawToTexture(uint32* texture, int texWidth, int texHeight, int srcX, int srcY)
{
	assert(texture != 0);

	for (int y = 0; y < texHeight; y++)
	{
		int trgYOfs = y * texWidth;
		for (int x = 0; x < texWidth; x++)
		{
			int sx = x + srcX;
			int sy = y + srcY;
			// Swap X/Y
			SectorInfo& si = m_sectors[sy + sx * m_numSectors];
			if (si.empty())
				texture[x + trgYOfs] = 0;
			else
				texture[x + trgYOfs] = 0xFFFFFFFF;
		}
	}
}

int CVegetationMap::WorldToSector(float worldCoord) const
{
	return int(worldCoord * m_worldToSector);
}

void CVegetationMap::UnloadObjectsGeometry()
{
	for (int i = 0; i < GetObjectCount(); i++)
	{
		GetObject(i)->UnloadObject();
	}
}

void CVegetationMap::ExportBlock(const CRect& subRc, CXmlArchive& ar)
{
	XmlNodeRef mainNode = ar.root->newChild("VegetationMap");
	mainNode->setAttr("X1", subRc.left);
	mainNode->setAttr("Y1", subRc.top);
	mainNode->setAttr("X2", subRc.right);
	mainNode->setAttr("Y2", subRc.bottom);

	CRect rect = subRc;
	for (int i = 0; i < GetObjectCount(); i++)
	{
		XmlNodeRef vegObjNode = mainNode->createNode("VegetationObject");
		CVegetationObject* pObject = GetObject(i);
		int numSaved = ExportObject(pObject, vegObjNode, &rect);
		if (numSaved > 0)
		{
			mainNode->addChild(vegObjNode);
		}
	}

	//SerializeInstances( ar,&rect );
}

void CVegetationMap::ImportBlock(CXmlArchive& ar, CPoint placeOffset)
{
	XmlNodeRef mainNode = ar.root->findChild("VegetationMap");
	if (!mainNode)
		return;

	CRect subRc(0, 0, 1, 1);
	mainNode->getAttr("X1", subRc.left);
	mainNode->getAttr("Y1", subRc.top);
	mainNode->getAttr("X2", subRc.right);
	mainNode->getAttr("Y2", subRc.bottom);

	subRc.OffsetRect(placeOffset);

	// Clear all vegetation instances in this rectangle.
	ClearBrush(subRc, false, NULL);

	// Serialize vegetation objects
	for (int i = 0; i < mainNode->getChildCount(); i++)
	{
		XmlNodeRef vegObjNode = mainNode->getChild(i);
		ImportObject(vegObjNode, Vec3(placeOffset.x, placeOffset.y, 0));
	}
	// Clear object in this rectangle.
	CRect rect(placeOffset.x, placeOffset.y, placeOffset.x, placeOffset.y);
	SerializeInstances(ar, &rect);

	// Now display all objects on terrain.
	PlaceObjectsOnTerrain();

	//SegmentedWorld Notification
	AABB destBox;
	destBox.min.x = subRc.left;
	destBox.min.y = subRc.top;
	destBox.min.z = 0;

	destBox.max.x = subRc.right;
	destBox.max.y = subRc.bottom;
	destBox.max.z = 0;
}

void CVegetationMap::RepositionArea(const AABB& box, const Vec3& offset, int nRot, bool isCopy)
{
	Vec3 src = (box.min + box.max) / 2;
	Vec3 dst = src + offset;
	float alpha = 3.141592653589793f / 2 * nRot;
	float cosa = cos(alpha);
	float sina = sin(alpha);

	float x1 = min(box.min.x, box.max.x);
	float y1 = min(box.min.y, box.max.y);
	float x2 = max(box.min.x, box.max.x);
	float y2 = max(box.min.y, box.max.y);

	// check all sectors intersected by radius.
	int sx1 = int(x1 * m_worldToSector);
	int sx2 = int(x2 * m_worldToSector);
	int sy1 = int(y1 * m_worldToSector);
	int sy2 = int(y2 * m_worldToSector);
	sx1 = max(sx1, 0);
	sx2 = min(sx2, m_numSectors - 1);
	sy1 = max(sy1, 0);
	sy2 = min(sy2, m_numSectors - 1);

	//Objects will be moved or copied here
	//After all sectors will be processed, current and this area will be swapped
	std::vector<SectorInfo> tempArea(m_sectors.size());

	// 1. Move/Copy objects from map to tempArea
	for (int sectY = 0; sectY < m_numSectors; ++sectY)
	{
		for (int sectX = 0; sectX < m_numSectors; ++sectX)
		{
			//Entire sector is out of AABB
			if (sectY < sy1 || sectY > sy2 || sectX < sx1 || sectX > sx2)
			{
				//Out of moving area: move entire sector and process the next one
				// Swap is not good: when if something is moved to tempArea, it will returned back to m_sectors
				const int sectorIndex = sectY * m_numSectors + sectX;

				tempArea[sectorIndex].splice(tempArea[sectorIndex].begin(), m_sectors[sectorIndex]);
				continue;
			}

			SectorInfo& sector = GetVegSector(sectX, sectY);
			for (auto pInst : sector)
			{
				if (pInst->object == nullptr)
				{
					//No attached Vegetation object: will not process it
					continue;
				}

				if (x1 <= pInst->pos.x && pInst->pos.x <= x2 && y1 <= pInst->pos.y && pInst->pos.y <= y2)
				{
					// Inside of AABB
					const Vec3 pos = pInst->pos - src;
					Vec3 newPos = Vec3(cosa * pos.x - sina * pos.y, sina * pos.x + cosa * pos.y, pos.z) + dst;

					const int newSectorX = int(newPos.x * m_worldToSector);
					const int newSectorY = int(newPos.y * m_worldToSector);
					if (newSectorX < 0 || newSectorX >= m_numSectors || newSectorY < 0 || newSectorY >= m_numSectors)
					{
						//New position is out of map, the object will be cut. Need to store it for undo
						SAFE_RELEASE_NODE(pInst->pRenderNode);
						RecordUndo(pInst);
						continue;
					}

					const int newSectorId = newSectorX + newSectorY * m_numSectors;

					if (!pInst->object->IsAffectedByBrushes() && pInst->object->IsAffectedByTerrain())
					{
						newPos.z = GetIEditorImpl()->Get3DEngine()->GetTerrainElevation(newPos.x, newPos.y);
					}

					if (isCopy)
					{
						// Keep this instance in same position in tempArea
						tempArea[sectY * m_numSectors + sectX].push_back(pInst);

						// New instance with new position
						auto* pSi = &tempArea[newSectorId];
						auto* pNewInstance = CreateObjInstance(pInst->object, newPos, pInst, pSi);
						RegisterInstance(pNewInstance);
					}
					else
					{
						// Move the instance to the new position in tempArea
						RecordUndo(pInst);

						pInst->pos = newPos;
						tempArea[newSectorId].push_back(pInst);
						RegisterInstance(pInst);
					}
				}
				else
				{
					// Outside of AABB: just copy without modifications at the same sector
					tempArea[sectY * m_numSectors + sectX].push_back(pInst);
				}
			}
		}
	}

	//2. Swap
	std::swap(m_sectors, tempArea);

	//3. Remove objects that are too close
	int px1 = int((x1 + offset.x) * m_worldToSector);
	int px2 = int((x2 + offset.x) * m_worldToSector);
	int py1 = int((y1 + offset.y) * m_worldToSector);
	int py2 = int((y2 + offset.y) * m_worldToSector);

	if (nRot == 1 || nRot == 3)
	{
		px1 = int((dst.x - (box.max.y - box.min.y)) * m_worldToSector);
		px2 = int((dst.x + (box.max.y - box.min.y)) * m_worldToSector);
		py1 = int((dst.y - (box.max.x - box.min.x)) * m_worldToSector);
		py2 = int((dst.y + (box.max.x - box.min.x)) * m_worldToSector);
	}

	px1 = max(px1, 0);
	px2 = min(px2, m_numSectors - 1);
	py1 = max(py1, 0);
	py2 = min(py2, m_numSectors - 1);

	RemoveDuplVegetation(px1, py1, px2, py2);
}

void CVegetationMap::RecordUndo(CVegetationInstance* pInst)
{
	if (CUndo::IsRecording())
		CUndo::Record(new CUndoVegInstance(pInst));
}

int CVegetationMap::GetTexureMemoryUsage(bool bOnlySelectedObjects)
{
	ICrySizer* pSizer = GetISystem()->CreateSizer();

	for (int i = 0; i < GetObjectCount(); i++)
	{
		CVegetationObject* pObject = GetObject(i);
		if (!pObject->IsSelected() && bOnlySelectedObjects)
			continue;

		pObject->GetTextureMemUsage(pSizer);
	}
	int nSize = pSizer->GetTotalSize();
	pSizer->Release();
	return nSize;
}

void CVegetationMap::GetMemoryUsage(ICrySizer* pSizer)
{
	pSizer->Add(*this);

	pSizer->Add(m_idToObject);

	{
		pSizer->Add(m_objects);

		for (auto& obj : m_objects)
		{
			pSizer->Add(*obj);
		}
	}

	{
		pSizer->Add(m_sectors);
		for (const auto& sector : m_sectors)
		{
			for (const auto& pInst : sector)
			{
				pSizer->Add(*pInst);
			}
		}
	}
}

void CVegetationMap::SetEngineObjectsParams()
{
	for (int i = 0; i < GetObjectCount(); i++)
	{
		CVegetationObject* obj = GetObject(i);
		obj->SetEngineParams();
	}
}

int CVegetationMap::GenerateVegetationObjectId()
{
	// Generate New id.
	for (int i = 0; i < std::numeric_limits<int>::max(); ++i)
	{
		if (m_idToObject.find(i) == m_idToObject.end())
		{
			return i;
		}
	}

	return -1;
}

void CVegetationMap::UpdateConfigSpec()
{
	for (int i = 0; i < GetObjectCount(); i++)
	{
		CVegetationObject* obj = GetObject(i);
		obj->OnConfigSpecChange();
	}
}

void CVegetationMap::Save(bool bBackup)
{
	using namespace Private_VegetationMap;
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	CTempFileHelper helper(GetIEditorImpl()->GetLevelDataFolder() + kVegetationMapFile);

	CXmlArchive xmlAr;
	Serialize(xmlAr);
	xmlAr.Save(helper.GetTempFilePath());

	helper.UpdateFile(bBackup);
}

bool CVegetationMap::Load()
{
	using namespace Private_VegetationMap;
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	string filename = GetIEditorImpl()->GetLevelDataFolder() + kVegetationMapFile;
	CXmlArchive xmlAr;
	xmlAr.bLoading = true;
	if (!xmlAr.Load(filename))
	{
		return false;
	}

	Serialize(xmlAr);
	return true;
}

void CVegetationMap::ReloadGeometry()
{
	for (int i = 0; i < GetObjectCount(); i++)
	{
		IStatObj* pObject = GetObject(i)->GetObject();
		if (pObject)
			pObject->Refresh(FRO_SHADERS | FRO_TEXTURES | FRO_GEOMETRY);
	}
}

void CVegetationMap::GenerateBillboards(IConsoleCmdArgs*)
{
	CVegetationMap* vegMap = GetIEditorImpl()->GetVegetationMap();

	struct SBillboardInfo
	{
		IStatObj* pModel;
		float     fDistRatio;
	};

	std::vector<SBillboardInfo> Billboards;
	for (int i = 0; i < vegMap->GetObjectCount(); i++)
	{
		CVegetationObject* pVO = vegMap->GetObject(i);
		if (!pVO->IsUseSprites())
			continue;

		IStatObj* pObject = pVO->GetObject();
		int j = 0;
		for (; j < Billboards.size(); j++)
		{
			SBillboardInfo& BI = Billboards[j];
			if (BI.pModel == pObject)
				continue;
		}
		if (j == Billboards.size())
		{
			SBillboardInfo B;
			B.pModel = pObject;
			B.fDistRatio = pVO->mv_SpriteDistRatio;
			Billboards.push_back(B);
		}
	}

	uint32 nRenderingFlags = SHDF_ALLOWHDR | SHDF_ALLOWPOSTPROCESS;

	const Vec3 vEye(0, 0, 0);
	const Vec3 vAt(-1, 0, 0);
	const Vec3 vUp(0, 0, 1);

	int nSpriteResFinal = 128;
	int nSpriteResInt = nSpriteResFinal << 1;
	int nLine = (int)sqrtf(FAR_TEX_COUNT);
	int nAtlasRes = nLine * nSpriteResFinal;
	ITexture* pAtlasD = gEnv->pRenderer->CreateTexture("$BillboardsAtlasD", nAtlasRes, nAtlasRes, 1, NULL, eTF_B8G8R8A8, FT_USAGE_RENDERTARGET);
	ITexture* pAtlasN = gEnv->pRenderer->CreateTexture("$BillboardsAtlasN", nAtlasRes, nAtlasRes, 1, NULL, eTF_B8G8R8A8, FT_USAGE_RENDERTARGET);
	int nProducedTexturesCounter = 0;

	for (int i = 0; i < Billboards.size(); i++)
	{
		SBillboardInfo& BI = Billboards[i];
		IStatObj* pObj = BI.pModel;

		gEnv->pLog->Log("Generating billboards for %s", pObj->GetFilePath());

		CCamera tmpCam = GetISystem()->GetViewCamera();

		for (int j = 0; j < FAR_TEX_COUNT; j++)
		{
			float fRadiusHors = pObj->GetRadiusHors();
			float fRadiusVert = pObj->GetRadiusVert();
			float fAngle = FAR_TEX_ANGLE * (float)j + 90.f;

			//float fCustomDistRatio = 1.0f;
			float fGenDist = 18.f; // *max(0.5f, BI.fDistRatio * fCustomDistRatio);

			float fFOV = 0.565f / fGenDist * 200.f * (gf_PI / 180.0f);
			float fDrawDist = fRadiusVert * fGenDist;

			Matrix33 matRot = Matrix33::CreateOrientation(vAt - vEye, vUp, 0);
			tmpCam.SetMatrix(Matrix34(matRot, Vec3(0, 0, 0)));
			tmpCam.SetFrustum(nSpriteResInt, nSpriteResInt, fFOV, max(0.1f, fDrawDist - fRadiusHors), fDrawDist + fRadiusHors);

			IRenderer::SGraphicsPipelineDescription pipelineDesc;
			pipelineDesc.type = EGraphicsPipelineType::Standard;
			pipelineDesc.shaderFlags = nRenderingFlags;

			SRenderingPassInfo passInfo = SRenderingPassInfo::CreateBillBoardGenPassRenderingInfo(passInfo.GetGraphicsPipelineKey(), tmpCam, nRenderingFlags);
			IRenderView* pView = passInfo.GetIRenderView();
			pView->SetCameras(&tmpCam, 1);

			gEnv->pRenderer->EF_StartEf(passInfo);

			Matrix34A matTrans;
			matTrans.SetIdentity();
			matTrans.SetTranslation(Vec3(-fDrawDist, 0, 0));

			Matrix34A matRotation;
			matRotation = matRotation.CreateRotationZ(fAngle * (gf_PI / 180.0f));

			Vec3 vCenter = pObj->GetVegCenter();
			Matrix34A matCenter;
			matCenter.SetIdentity();
			matCenter.SetTranslation(-vCenter);
			matCenter = matRotation * matCenter;
			Matrix34 InstMatrix = matTrans * matCenter;

			SRendParams rParams;
			rParams.pMatrix = &InstMatrix;
			rParams.lodValue = CLodValue(0);
			pObj->Render(rParams, passInfo);

			pView->SwitchUsageMode(IRenderView::eUsageModeWritingDone);

			gEnv->pRenderer->EF_EndEf3D(0, 0, passInfo, nRenderingFlags);

			RectI rcDst;
			rcDst.x = (j % nLine) * nSpriteResFinal;
			rcDst.y = (j / nLine) * nSpriteResFinal;
			rcDst.w = nSpriteResFinal;
			rcDst.h = nSpriteResFinal;

			std::shared_ptr<CGraphicsPipeline> pGraphicsPipeline = gEnv->pRenderer->FindGraphicsPipeline(passInfo.GetGraphicsPipelineKey());
			if (!gEnv->pRenderer->StoreGBufferToAtlas(rcDst, nSpriteResInt, nSpriteResInt, nSpriteResFinal, nSpriteResFinal, pAtlasD, pAtlasN, pGraphicsPipeline.get()))
			{
				assert(0);
			}
		}
		const char* szName = pObj->GetFilePath();

		CString fileName = PathUtil::GetFileName(szName);
		CString pathName = PathUtil::GetPathWithoutFilename(szName);
		CString fullGameFolder = PathUtil::AddSlash(PathUtil::GetGameFolder());

		CString fullFolder = fullGameFolder + pathName;
		CString fullFilename = fullFolder + fileName;
		CString nameAlbedo = fullFilename + "_billbAlb.tif";
		CString nameNormal = fullFilename + "_billbNorm.tif";
		CFileUtil::CreateDirectory(fullFolder);

		vegMap->SaveBillboardTIFF(nameAlbedo, pAtlasD, "Diffuse_highQ", true);
		vegMap->SaveBillboardTIFF(nameNormal, pAtlasN, "Normalmap_highQ", false);

		nProducedTexturesCounter += 2;
	}
	SAFE_RELEASE(pAtlasD);
	SAFE_RELEASE(pAtlasN);

	gEnv->pLog->Log("%d billboard textures produced", nProducedTexturesCounter);
}

bool CVegetationMap::SaveBillboardTIFF(const CString& fileName, ITexture* pTexture, const char* szPreset, bool bConvertToSRGB)
{
	int nWidth = pTexture->GetWidth();
	int nHeight = pTexture->GetHeight();
	int nPitch = nWidth * 4;

	uint8* pSrcData = new uint8[nPitch * nHeight];
	pTexture->GetData32(0, 0, pSrcData, eTF_B8G8R8A8);

	if (bConvertToSRGB)
	{
		byte* pSrc = pSrcData;
		for (int i = 0; i < nWidth * nHeight; i++)
		{
			byte r = pSrc[0];
			byte g = pSrc[1];
			byte b = pSrc[2];
			pSrc[2] = (byte)(powf((float)r / 255.0f, 1.0f / 2.2f) * 255.0f);
			pSrc[1] = (byte)(powf((float)g / 255.0f, 1.0f / 2.2f) * 255.0f);
			pSrc[0] = (byte)(powf((float)b / 255.0f, 1.0f / 2.2f) * 255.0f);

			pSrc += 4;
		}
	}
	/*else
	   {
	   byte *pSrc = pSrcData;
	   for (int i = 0; i < nWidth * nHeight; i++)
	   {
	    byte r = pSrc[0];
	    pSrc[0] = pSrc[2];
	    pSrc[2] = r;

	    pSrc += 4;
	   }
	   }*/

	// save data to tiff

	CImageTIF tif;
	const bool res = tif.SaveRAW(fileName.GetString(), pSrcData, nWidth, nHeight, 1, 4, true, szPreset);
	assert(res);

	SAFE_DELETE_ARRAY(pSrcData);

	return res;
}

bool CVegetationMap::IsAreaEmpty(const AABB& bbox)
{
	std::vector<CVegetationInstance*> foundVegetationIstances;
	foundVegetationIstances.clear();
	bool bHasVeg = false;

	GetObjectInstances(bbox.min.x, bbox.min.y, bbox.max.x, bbox.max.y, foundVegetationIstances);

	for (int vegNo = 0; vegNo < foundVegetationIstances.size(); ++vegNo)
	{
		CVegetationInstance* pVegInstance = (CVegetationInstance*)foundVegetationIstances[vegNo];
		if (!pVegInstance)
			continue;

		if (!pVegInstance->object)
			continue;

		IStatObj* pStatObj = pVegInstance->object->GetObject();

		if (!pStatObj)
			continue;

		Vec3 expandMin = pVegInstance->pos + pStatObj->GetBoxMin() * pVegInstance->scale;
		Vec3 expandMax = pVegInstance->pos + pStatObj->GetBoxMax() * pVegInstance->scale;

		AABB vegBBox(expandMin, expandMax);

		if (bbox.ContainsBox(vegBBox))
			bHasVeg = true;

		if (pVegInstance->pRenderNode && bHasVeg)
			pVegInstance->object->SetSelected(true);
	}

	return !(foundVegetationIstances.size() > 0 && bHasVeg);
}

void CVegetationMap::StoreBaseUndo(EStoreUndo state)
{
	bool bRecordUndo = false;

	switch (state)
	{
	case eStoreUndo_Normal:
		if (eStoreUndo_Normal == m_storeBaseUndoState)
			bRecordUndo = true;
		break;

	case eStoreUndo_Begin:
		m_storeBaseUndoState = eStoreUndo_Begin;
		break;

	case eStoreUndo_Once:
		if (eStoreUndo_Begin == m_storeBaseUndoState)
		{
			bRecordUndo = true;
			m_storeBaseUndoState = eStoreUndo_Once;
		}
		break;

	case eStoreUndo_End:
		if (eStoreUndo_Once == m_storeBaseUndoState)
			bRecordUndo = true;
		m_storeBaseUndoState = eStoreUndo_Normal;
		break;
	}

	if (bRecordUndo)
	{
		if (CUndo::IsRecording())
			CUndo::Record(new CUndoVegetationBase(false));
	}
}

CVegetationInstance* CVegetationMap::CloneInstance(CVegetationInstance* pOriginal)
{
	SectorInfo* si = GetVegSector(pOriginal->pos);

	if (!si)
		return nullptr;

	CVegetationInstance* pInst = new CVegetationInstance;
	pInst->m_refCount = 1; // Starts with 1 reference.
	pInst->pos = pOriginal->pos;
	pInst->scale = pOriginal->scale;
	pInst->object = pOriginal->object;
	pInst->brightness = pOriginal->brightness;
	pInst->angle = pOriginal->angle;
	pInst->pRenderNode = 0;
	pInst->pRenderNodeGroundDecal = 0;
	pInst->m_boIsSelected = false;

	if (CUndo::IsRecording())
		CUndo::Record(new CUndoVegInstanceCreate(pInst, false));

	// Add object to end of the list of instances in sector.
	// Increase number of instances.
	pInst->object->SetNumInstances(pInst->object->GetNumInstances() + 1);
	m_numInstances++;
	si->push_front(pInst);
	return pInst;
}

#pragma pop_macro("GetObject")
