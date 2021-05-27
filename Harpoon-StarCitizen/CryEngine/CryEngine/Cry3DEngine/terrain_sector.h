// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#define ARR_TEX_OFFSETS_SIZE 4

#include "BasicArea.h"
#include "Array2d.h"

enum eTexureType
{
	ett_Diffuse,
	ett_LM
};

class CUpdateTerrainTempData;

struct SSurfaceTypeInfo
{
	SSurfaceTypeInfo() { memset(this, 0, sizeof(SSurfaceTypeInfo)); }
	struct SSurfaceType*    pSurfaceType;
	_smart_ptr<IRenderMesh> arrpRM[3];

	bool HasRM() { return arrpRM[0] || arrpRM[1] || arrpRM[2]; }
	int  GetIndexCount();
	void DeleteRenderMeshes(IRenderer* pRend);

	void GetMemoryUsage(ICrySizer* pSizer) const { /*nothing*/ }
};

// this structure used for storing localized surface type ids and weights
struct SSurfaceTypeLocal
{
	enum
	{
		kMaxSurfaceTypesNum = 3,  // maximum number of surface types allowed to be mixed in single heightmap unit
		kMaxSurfaceTypeId   = 15, // maximum value of localized surface type id
		kMaxVal             = 15  // wights and id's are stored in 4 bit uint
	};

	uint32 GetDominatingSurfaceType() const
	{
		return ty[0];
	}

	const SSurfaceTypeLocal& operator=(int nSurfType)
	{
		ZeroStruct(*this);
		CRY_MATH_ASSERT(nSurfType >= 0 && nSurfType <= kMaxSurfaceTypeId);
		we[0] = kMaxVal;
		ty[0] = nSurfType;
		return *this;
	}

	bool operator!=(const SSurfaceTypeLocal& o) const
	{
		return memcmp(this, &o, sizeof(o)) != 0;
	}

	bool operator==(const SSurfaceTypeLocal& o) const
	{
		return memcmp(this, &o, sizeof(o)) == 0;
	}

	static void EncodeIntoUint32(const SSurfaceTypeLocal& si, uint32& rTypes)
	{
		uint8* p = (uint8*)&rTypes;

		for (int i = 0; i < kMaxSurfaceTypesNum; i++)
		{
			CRY_MATH_ASSERT(si.we[i] <= kMaxVal);
			CRY_MATH_ASSERT(si.ty[i] <= kMaxVal);
		}

		p[0] = (si.ty[0] & kMaxVal) | ((si.ty[1] & kMaxVal) << 4);
		p[1] = (si.ty[2] & kMaxVal) | ((si.we[1] & kMaxVal) << 4);
		p[2] = (si.we[2] & kMaxVal) | (p[2] & (kMaxVal << 4));

		CRY_MATH_ASSERT((si.we[0] + si.we[1] + si.we[2]) == kMaxVal);
	}

	static void DecodeFromUint32(const uint32& rTypes, SSurfaceTypeLocal& si)
	{
		uint8* p = (uint8*)&rTypes;

		si.ty[0] = (((int)p[0])) & kMaxVal;
		si.ty[1] = (((int)p[0]) >> 4) & kMaxVal;
		si.ty[2] = (((int)p[1])) & kMaxVal;
		si.we[1] = (((int)p[1]) >> 4) & kMaxVal;
		si.we[2] = (((int)p[2])) & kMaxVal;
		si.we[0] = CLAMP(kMaxVal - si.we[1] - si.we[2], 0, kMaxVal);

		CRY_MATH_ASSERT((si.we[0] + si.we[1] + si.we[2]) == kMaxVal);
	}

	uint8 ty[kMaxSurfaceTypesNum] = { 0 };
	uint8 we[kMaxSurfaceTypesNum] = { 0 };
};

// heightmap item containing packed surface types and elevation
union SHeightMapItem
{
	static const uint SurfaceBits = 20;
	static const uint HeightBits =  12;

	uint32 raw;
	struct
	{
		uint32 surface : SurfaceBits;
		uint32 height  : HeightBits;
	};

	SHeightMapItem()
		: raw(0) {}
	SHeightMapItem(uint32 height, uint32 surface)
		: surface(surface), height(height) {}

	bool operator==(const SHeightMapItem& other)
	{
		return raw == other.raw;
	}
	bool operator!=(const SHeightMapItem& other)
	{
		return raw != other.raw;
	}

	AUTO_STRUCT_INFO;
};

struct SRangeInfo
{
	SRangeInfo()
		: fOffset(0.0f)
		, fRange(0.0f)
		, pHMData(nullptr)
		, nSize(0)
		, nUnitBitShift(0)
		, nModified(0)
		, pSTPalette(nullptr)
	{
	}

	void UpdateBitShift(int nUnitToSectorBS)
	{
		const int nSecSize = 1 << nUnitToSectorBS;
		int n = nSize - 1;
		nUnitBitShift = 0;
		while (n > 0 && nSecSize > n)
		{
			nUnitBitShift++;
			n *= 2;
		}
	}

	inline SHeightMapItem GetRawDataByIndex(uint32 i) const
	{
		CRY_MATH_ASSERT(i < uint32(nSize*nSize));
		CRY_MATH_ASSERT(pHMData);

		return pHMData[i];
	}

	inline SHeightMapItem GetRawData(uint32 x, uint32 y) const
	{
		CRY_MATH_ASSERT(x < nSize);
		CRY_MATH_ASSERT(y < nSize);
		CRY_MATH_ASSERT(pHMData);

		return GetRawDataByIndex(x * nSize + y);
	}

	inline float RawDataToHeight(const SHeightMapItem& data) const
	{
		return fOffset + float(data.height) * fRange;
	}
#ifdef CRY_PLATFORM_SSE2
	inline f32v4 RawDataToHeight(u32v4 data) const
	{
		return convert<f32v4>(data >> Scalar(SHeightMapItem::SurfaceBits)) * to_v4(fRange) + to_v4(fOffset);
	}
#endif

	inline float GetHeightByIndex(uint32 i) const
	{
		CRY_MATH_ASSERT(i < uint32(nSize*nSize));
		CRY_MATH_ASSERT(pHMData);

		return RawDataToHeight(pHMData[i]);
	}

	inline float GetHeight(uint32 x, uint32 y) const
	{
		CRY_MATH_ASSERT(x < nSize);
		CRY_MATH_ASSERT(y < nSize);
		CRY_MATH_ASSERT(pHMData);

		return GetHeightByIndex(x * nSize + y);
	}

	inline uint32 GetSurfaceTypeByIndex(uint32 i) const
	{
		CRY_MATH_ASSERT(i < uint32(nSize*nSize));
		CRY_MATH_ASSERT(pHMData);

		SSurfaceTypeLocal si;
		SSurfaceTypeLocal::DecodeFromUint32(pHMData[i].surface, si);

		return si.GetDominatingSurfaceType() & e_index_hole;
	}

	inline uint32 GetSurfaceType(uint32 x, uint32 y) const
	{
		CRY_MATH_ASSERT(x < nSize);
		CRY_MATH_ASSERT(y < nSize);
		CRY_MATH_ASSERT(pHMData);

		return GetSurfaceTypeByIndex(x * nSize + y);
	}

	inline void SetDataLocal(int nX, int nY, SHeightMapItem usValue)
	{
		CRY_MATH_ASSERT(nX >= 0 && nX < (int)nSize);
		CRY_MATH_ASSERT(nY >= 0 && nY < (int)nSize);
		CRY_MATH_ASSERT(pHMData);
		pHMData[nX * nSize + nY] = usValue;
	}

	inline SHeightMapItem GetDataUnits(int nX_units, int nY_units) const
	{
		int nMask = nSize - 2;
		int nX = nX_units >> nUnitBitShift;
		int nY = nY_units >> nUnitBitShift;
		return GetRawData(nX & nMask, nY & nMask);
	}

	// Finds or selects a 4-bit index in this sector (0-14) to represent the given global surface type index (0-126).
	uint16 GetLocalSurfaceTypeID(uint16 usGlobalSurfaceTypeID);

	float           fOffset;
	float           fRange;
	SHeightMapItem* pHMData;

	uint16          nSize;
	uint8           nUnitBitShift;
	uint8           nModified;
	uchar*          pSTPalette; // Maps the local surface type indices from the HM to the global ones in CTerrain

	enum
	{
		e_index_undefined = 14,
		e_index_hole,
		e_palette_size,

		e_undefined = 127,
		e_hole,
		e_max_surface_types
	};
};

template<class T> class TPool
{
public:

	TPool(int nPoolSize)
	{
		m_nPoolSize = nPoolSize;
		m_pPool = new T[nPoolSize];
		m_lstFree.PreAllocate(nPoolSize, 0);
		m_lstUsed.PreAllocate(nPoolSize, 0);
		for (int i = 0; i < nPoolSize; i++)
			m_lstFree.Add(&m_pPool[i]);
	}

	~TPool()
	{
		delete[] m_pPool;
	}

	void ReleaseObject(T* pInst)
	{
		if (m_lstUsed.Delete(pInst))
			m_lstFree.Add(pInst);
	}

	int GetUsedInstancesCount(int& nAll)
	{
		nAll = m_nPoolSize;
		return m_lstUsed.Count();
	}

	T* GetObject()
	{
		T* pInst = NULL;
		if (m_lstFree.Count())
		{
			pInst = m_lstFree.Last();
			m_lstFree.DeleteLast();
			m_lstUsed.Add(pInst);
		}

		return pInst;
	}

	void GetMemoryUsage(class ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_lstFree);
		pSizer->AddObject(m_lstUsed);

		if (m_pPool)
			for (int i = 0; i < m_nPoolSize; i++)
				m_pPool[i].GetMemoryUsage(pSizer);
	}

	PodArray<T*> m_lstFree;
	PodArray<T*> m_lstUsed;
	T*           m_pPool;
	int          m_nPoolSize;
};

struct STerrainNodeLeafData
{
	STerrainNodeLeafData() { memset(this, 0, sizeof(*this)); }
	~STerrainNodeLeafData();
	float                   m_arrTexGen[MAX_RECURSION_LEVELS][ARR_TEX_OFFSETS_SIZE];
	int                     m_arrpNonBorderIdxNum[SRangeInfo::e_max_surface_types][4];
	_smart_ptr<IRenderMesh> m_pRenderMesh;
};

enum ETextureEditingState : unsigned int
{
	eTES_SectorIsUnmodified = 0,
	eTES_SectorIsModified_AtlasIsUpToDate,
	eTES_SectorIsModified_AtlasIsDirty
};

const float kGeomErrorNotSet = -1;

struct CTerrainNode final : public Cry3DEngineBase, public IRenderNode, public IStreamCallback
{
public:

	// IRenderNode implementation
	virtual const char*             GetName() const                         override { return "TerrainNode"; }
	virtual const char*             GetEntityClassName() const              override { return "TerrainNodeClass"; }
	virtual Vec3                    GetPos(bool bWorldOnly = true) const    override { return Vec3(m_nOriginX, m_nOriginY, 0); }
	virtual void                    SetBBox(const AABB& WSBBox)             override {}
	virtual struct IPhysicalEntity* GetPhysics() const                      override { return NULL; }
	virtual void                    SetPhysics(IPhysicalEntity* pPhys)      override {}
	virtual void                    SetMaterial(IMaterial* pMat)            override {}
	virtual IMaterial*              GetMaterial(Vec3* pHitPos = NULL) const override { return NULL; }
	virtual IMaterial*              GetMaterialOverride() const             override { return NULL; }
	virtual float                   GetMaxViewDist() const                  override { return 1000000.f; }
	virtual EERType                 GetRenderNodeType() const               override { return eERType_TerrainSector;  }

	friend class CTerrain;
	friend class CTerrainUpdateDispatcher;

	virtual void                       Render(const SRendParams& RendParams, const SRenderingPassInfo& passInfo) override;
	virtual const AABB                 GetBBox() const override { return AABB(m_boxHeigtmapLocal.min, m_boxHeigtmapLocal.max + Vec3(0, 0, m_fBBoxExtentionByObjectsIntegration)); }
	virtual void                       FillBBox(AABB& aabb) const override { aabb = GetBBox(); }
	virtual struct ICharacterInstance* GetEntityCharacter(Matrix34A* pMatrix = NULL, bool bReturnOnlyVisible = false) override { return NULL; }

	//////////////////////////////////////////////////////////////////////////
	// IStreamCallback
	//////////////////////////////////////////////////////////////////////////
	// streaming
	virtual void StreamOnComplete(IReadStream* pStream, unsigned nError) override;
	virtual void StreamAsyncOnComplete(IReadStream* pStream, unsigned nError) override;
	//////////////////////////////////////////////////////////////////////////
	void         StartSectorTexturesStreaming(bool bFinishNow);

	void         Init(int x1, int y1, int nNodeSize, CTerrainNode* pParent, bool bBuildErrorsTable);
	CTerrainNode()
	{
		s_nodesCounter++;
	}
	virtual ~CTerrainNode();

	static void   ResetStaticData();
	bool          CheckVis(bool bAllIN, bool bAllowRenderIntoCBuffer, const SRenderingPassInfo& passInfo, FrustumMaskType passCullMask);
	void          SetupTexturing(bool bMakeUncompressedForEditing, const SRenderingPassInfo& passInfo);
	void          RequestTextures(const SRenderingPassInfo& passInfo);
	void          EnableTextureEditingMode(unsigned int textureId);
	void          UpdateNodeTextureFromEditorData();
	void          UpdateNodeNormalMapFromHeightMap();
	static void   SaveCompressedMipmapLevel(const void* data, size_t size, void* userData);
	void          CheckNodeGeomUnload(const SRenderingPassInfo& passInfo);
	void          RenderNodeHeightmap(const SRenderingPassInfo& passInfo, FrustumMaskType passCullMask);
	bool          CheckUpdateProcObjects();
	void          IntersectTerrainAABB(const AABB& aabbBox, PodArray<CTerrainNode*>& lstResult);
	void          UpdateDetailLayersInfo(bool bRecursive);
	void          RemoveProcObjects(bool bRecursive = false);
	void          IntersectWithBox(const AABB& aabbBox, PodArray<CTerrainNode*>* plstResult);
	CTerrainNode* FindMinNodeContainingBox(const AABB& aabbBox);
	bool          RenderSector(const SRenderingPassInfo& passInfo); // returns true only if the sector rendermesh is valid and does not need to be updated
	CTerrainNode* GetTexuringSourceNode(int nTexMML, eTexureType eTexType);
	CTerrainNode* GetReadyTexSourceNode(int nTexMML, eTexureType eTexType);
	int           GetData(byte*& pData, int& nDataSize, EEndian eEndian, SHotUpdateInfo* pExportInfo);
	void          CalculateTexGen(const CTerrainNode* pTextureSourceNode, float& fTexOffsetX, float& fTexOffsetY, float& fTexScale);
	void          FillSectorHeightMapTextureData(Array2d<float>& arrHmData);

	template<class T>
	int                          Load_T(T*& f, int& nDataSize, EEndian eEndian, bool bSectorPalettes, SHotUpdateInfo* pExportInfo);
	int                          Load(uint8*& f, int& nDataSize, EEndian eEndian, bool bSectorPalettes, SHotUpdateInfo* pExportInfo);
	int                          Load(FILE*& f, int& nDataSize, EEndian eEndian, bool bSectorPalettes, SHotUpdateInfo* pExportInfo);
	int                          ReloadModifiedHMData(FILE* f);
	void                         ReleaseHoleNodes();

	void                         UnloadNodeTexture(bool bRecursive);
	float                        GetSurfaceTypeAmount(Vec3 vPos, int nSurfType);
	virtual void                 GetMemoryUsage(ICrySizer* pSizer) const override;
	void                         GetResourceMemoryUsage(ICrySizer* pSizer, const AABB& cstAABB);

	void                         SetLOD(const SRenderingPassInfo& passInfo);
	uint8                        GetTextureLOD(float fDistance, const SRenderingPassInfo& passInfo);

	void                         ReleaseHeightMapGeometry(bool bRecursive = false, const AABB* pBox = NULL);
	void                         ResetHeightMapGeometry(bool bRecursive = false, const AABB* pBox = NULL);
	int                          GetSecIndex();

	void                         DrawArray(const SRenderingPassInfo& passInfo);

	void                         UpdateRenderMesh(struct CStripsInfo* pArrayInfo);
	void                         BuildVertices(float stepSize);
	void                         SetVertexSurfaceType(float x, float y, float stepSize, CTerrain* pTerrain, SVF_P2S_N4B_C4B_T1F& vert);
	void                         SetVertexNormal(float x, float y, const float iLookupRadius, CTerrain* pTerrain, const int nTerrainSize, SVF_P2S_N4B_C4B_T1F& vert, Vec3* pTerrainNorm = nullptr);
	void                         AppendTrianglesFromObjects(const int nOriginX, const int nOriginY, CTerrain* pTerrain, const float stepSize, const int nTerrainSize);

	int                          GetMML(int dist, int mmMin, int mmMax);

	uint32                       GetLastTimeUsed() { return m_nLastTimeUsed; }

	static void                  GenerateIndicesForAllSurfaces(IRenderMesh* pRM, int arrpNonBorderIdxNum[SRangeInfo::e_max_surface_types][4], int nBorderStartIndex, SSurfaceTypeInfo* pSurfaceTypeInfos, CUpdateTerrainTempData* pUpdateTerrainTempData = NULL);
	void                         BuildIndices(CStripsInfo& si, const SRenderingPassInfo& passInfo);

	void                         BuildIndices_Wrapper(SRenderingPassInfo passInfo);
	void                         BuildVertices_Wrapper();
	void                         RenderSectorUpdate_Finish(const SRenderingPassInfo& passInfo);

	static void                  UpdateSurfaceRenderMeshes(const _smart_ptr<IRenderMesh> pSrcRM, struct SSurfaceType* pSurface, _smart_ptr<IRenderMesh>& pMatRM, int nProjectionId, PodArray<vtx_idx>& lstIndices, const char* szComment, int nNonBorderIndicesCount, const SRenderingPassInfo& passInfo);
	static void                  SetupTexGenParams(SSurfaceType* pLayer, float* pOutParams, uint8 ucProjAxis, bool bOutdoor, float fTexGenScale = 1.f);

	int                          CreateSectorTexturesFromBuffer(float* pSectorHeightMap);

	bool                         CheckUpdateDiffuseMap();
	bool                         AssignTextureFileOffset(int16*& pIndices, int16& nElementsNum);
	void                         UpdateDistance(const SRenderingPassInfo& passInfo);
	const float                  GetDistance(const SRenderingPassInfo& passInfo);
	bool                         IsProcObjectsReady() { return m_bProcObjectsReady != 0; }
	void                         UpdateRangeInfoShift();
	int                          GetSectorSizeInHeightmapUnits() const;
	void                         CheckLeafData();
	inline STerrainNodeLeafData* GetLeafData() { return m_pLeafData; }
	virtual void                 OffsetPosition(const Vec3& delta) override;
	_smart_ptr<IRenderMesh>      GetSharedRenderMesh();
	uint32                       GetMaterialsModificationId();
	void                         SetTraversalFrameId(uint32 onePassTraversalFrameId, int shadowFrustumLod);
	void                         UpdateGeomError();
	void                         InvalidateCachedShadowData();

	//////////////////////////////////////////////////////////////////////////
	// Member variables
	//////////////////////////////////////////////////////////////////////////
public:
	IReadStreamPtr       m_pReadStream;
	EFileStreamingStatus m_eTexStreamingStatus;

	CTerrainNode*        m_pChilds = nullptr; // 4 childs or NULL

	// flags
	uint8 m_bProcObjectsReady : 1;
	uint8 m_bHasHoles         : 2;                 // sector has holes in the ground
	uint8 m_bNoOcclusion      : 1;                 // sector has visareas under terrain surface

#ifndef _RELEASE
	ETextureEditingState m_eTextureEditingState, m_eElevTexEditingState;
#endif // _RELEASE

	uint8 // LOD's
	       m_cNodeNewTexMML, m_cNodeNewTexMML_Min;
	uint8  m_nTreeLevel;

	uint16 m_nOriginX, m_nOriginY;             // sector origin
	int    m_nLastTimeUsed = 0;                // basically last time rendered
	int    m_nSetLodFrameId = 0;
	float  m_geomError = kGeomErrorNotSet;     // maximum height difference comparing to next more detailed lod

protected:

	// temp data for terrain generation
	CUpdateTerrainTempData* m_pUpdateTerrainTempData = nullptr;

public:

	PodArray<SSurfaceTypeInfo> m_lstSurfaceTypeInfo;
	SRangeInfo                 m_rangeInfo;
	STerrainNodeLeafData*      m_pLeafData = nullptr;
	PodArray<CVegetation*>     m_arrProcObjects;
	SSectorTextureSet          m_nNodeTexSet, m_nTexSet; // texture id's
	uint16                     m_nNodeTextureLastUsedSec4;
	AABB                       m_boxHeigtmapLocal;
	float                      m_fBBoxExtentionByObjectsIntegration;
	struct CTerrainNode*       m_pParent = nullptr;
	float                      m_arrfDistance[MAX_RECURSION_LEVELS] = {};
	int                        m_nNodeTextureOffset = -1;
	int                        m_nNodeHMDataOffset = -1;
	int FTell(uint8*& f);
	int FTell(FILE*& f);
	static PodArray<vtx_idx>       s_arrIndices[SRangeInfo::e_max_surface_types][4];
	static PodArray<SSurfaceType*> s_lstReadyTypes;
	static int                     s_nodesCounter;
	OcclusionTestClient            m_occlusionTestClient;
	bool                           m_bHMDataIsModified = false;
};

// Container to manager temp memory as well as running update jobs
class CTerrainUpdateDispatcher : public Cry3DEngineBase
{
public:
	CTerrainUpdateDispatcher();
	~CTerrainUpdateDispatcher();

	void QueueJob(CTerrainNode*, const SRenderingPassInfo& passInfo);
	void SyncAllJobs(bool bForceAll, const SRenderingPassInfo& passInfo);
	bool Contains(CTerrainNode* pNode)
	{ return (m_queuedJobs.Find(pNode) != -1 || m_arrRunningJobs.Find(pNode) != -1); }

	void GetMemoryUsage(ICrySizer* pSizer) const;

	void RemoveJob(CTerrainNode* pNode);

private:
	bool AddJob(CTerrainNode*, bool executeAsJob, const SRenderingPassInfo& passInfo);

	static const size_t            TempPoolSize = (4U << 20);

	void*                          m_pHeapStorage;
	_smart_ptr<IGeneralMemoryHeap> m_pHeap;

	PodArray<CTerrainNode*>        m_queuedJobs;
	PodArray<CTerrainNode*>        m_arrRunningJobs;
};

#pragma pack(push,4)

struct STerrainNodeChunk
{
	int16 nChunkVersion;
	int16 bHasHoles;
	AABB  boxHeightmap;
	float fOffset;
	float fRange;
	int   nSize;
	int   nSurfaceTypesNum;

	AUTO_STRUCT_INFO;
};

#pragma pack(pop)

#include "terrain.h"
