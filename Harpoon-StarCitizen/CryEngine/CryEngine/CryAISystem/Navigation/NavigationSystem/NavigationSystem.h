// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __NavigationSystem_h__
#define __NavigationSystem_h__

#pragma once

#include <CryAISystem/INavigationSystem.h>
#include <CryAISystem/NavigationSystem/MNMNavMesh.h>

#include "../MNM/AgentSettings.h"
#include "../MNM/Tile.h"
#include "../MNM/NavMesh.h"
#include "../MNM/TileGenerator.h"
#include "../MNM/MarkupVolume.h"

#include "WorldMonitor.h"
#include "OffMeshNavigationManager.h"
#include "VolumesManager.h"
#include "IslandConnectionsManager.h"
#include "NavigationUpdatesManager.h"
#include "AnnotationsLibrary.h"

#include <CryCore/Containers/CryListenerSet.h>

#include <CryThreading/IThreadManager.h>
#include <CryThreading/IJobManager.h>

#define NAV_MESH_REGENERATION_ENABLED 1

#if CRY_PLATFORM_WINDOWS && NAV_MESH_REGENERATION_ENABLED
	#define NAVIGATION_SYSTEM_EDITOR_BACKGROUND_UPDATE 1
#else
	#define NAVIGATION_SYSTEM_EDITOR_BACKGROUND_UPDATE 0
#endif

struct RayCastRequest;

namespace MNM
{
	class CNavMeshQueryManager;
}

#if DEBUG_MNM_ENABLED || NAVIGATION_SYSTEM_EDITOR_BACKGROUND_UPDATE
class NavigationSystem;
struct NavigationMesh;
#endif

#if DEBUG_MNM_ENABLED
class NavigationSystem;
struct NavigationMesh;

class NavigationSystemDebugDraw
{
	struct NavigationSystemWorkingProgress
	{
		NavigationSystemWorkingProgress()
			: m_initialQueueSize(0)
			, m_currentQueueSize(0)
			, m_timeUpdating(0.0f)
		{

		}

		void Update(const float frameTime, const size_t queueSize);
		void Draw();

	private:

		void BeginDraw();
		void EndDraw();
		void DrawQuad(const Vec2& origin, const Vec2& size, const ColorB& color);

		float               m_timeUpdating;
		size_t              m_initialQueueSize;
		size_t              m_currentQueueSize;
		SAuxGeomRenderFlags m_oldRenderFlags;
	};

	struct DebugDrawSettings
	{
		DebugDrawSettings()
			: meshID(0)
			, selectedX(0)
			, selectedY(0)
			, selectedZ(0)
			, forceGeneration(false)
		{

		}

		inline bool Valid() const { return (meshID != NavigationMeshID(0)); }

		NavigationMeshID meshID;

		size_t           selectedX;
		size_t           selectedY;
		size_t           selectedZ;

		bool             forceGeneration;
	};

public:

	NavigationSystemDebugDraw()
		: m_agentTypeID(0)
	{

	}

	inline void SetAgentType(const NavigationAgentTypeID agentType)
	{
		m_agentTypeID = agentType;
	}

	inline NavigationAgentTypeID GetAgentType() const
	{
		return m_agentTypeID;
	}

	void DebugDraw(NavigationSystem& navigationSystem);
	void UpdateWorkingProgress(const float frameTime, const size_t queueSize);

private:

	MNM::TileID       DebugDrawTileGeneration(NavigationSystem& navigationSystem, const DebugDrawSettings& settings);
	void              DebugDrawRayCast(NavigationSystem& navigationSystem, const DebugDrawSettings& settings);
	void              DebugDrawPathFinder(NavigationSystem& navigationSystem, const DebugDrawSettings& settings);
	void              DebugDrawClosestPoint(NavigationSystem& navigationSystem, const DebugDrawSettings& settings);
	void              DebugDrawGroundPoint(NavigationSystem& navigationSystem, const DebugDrawSettings& settings);
	void              DebugDrawSnapToNavmesh(NavigationSystem& navigationSystem, const DebugDrawSettings& settings);
	void              DebugDrawIslandConnection(NavigationSystem& navigationSystem, const DebugDrawSettings& settings);

	void              DebugDrawNavigationMeshesForSelectedAgent(NavigationSystem& navigationSystem, MNM::TileID excludeTileID);
	void              DebugDrawNavigationSystemState(NavigationSystem& navigationSystem);
	void              DebugDrawMemoryStats(NavigationSystem& navigationSystem);
	void              DebugDrawTriangleOnCursor(NavigationSystem& navigationSystem);
	void              DebugDrawMeshBorders(NavigationSystem& navigationSystem);

	DebugDrawSettings GetDebugDrawSettings(NavigationSystem& navigationSystem);

	inline Vec3       TriangleCenter(const Vec3& a, const Vec3& b, const Vec3& c)
	{
		return (a + b + c) / 3.f;
	}

	const INavMeshQueryFilter* GetDebugQueryFilter(const char* szName) const;

	NavigationAgentTypeID           m_agentTypeID;
	NavigationSystemWorkingProgress m_progress;
};
#else
class NavigationSystemDebugDraw
{
public:
	inline void                  SetAgentType(const NavigationAgentTypeID agentType)                  {};
	inline NavigationAgentTypeID GetAgentType() const                                                 { return NavigationAgentTypeID(0); };
	inline void                  DebugDraw(const NavigationSystem& navigationSystem)                  {};
	inline void                  UpdateWorkingProgress(const float frameTime, const size_t queueSize) {};
};
#endif

#if NAVIGATION_SYSTEM_EDITOR_BACKGROUND_UPDATE
class NavigationSystemBackgroundUpdate : public ISystemEventListener
{
	class Thread : public IThread
	{
	public:
		Thread(NavigationSystem& navigationSystem)
			: m_navigationSystem(navigationSystem)
			, m_requestedStop(false)
		{

		}

		// Start accepting work on thread
		virtual void ThreadEntry();

		// Signals the thread that it should not accept anymore work and exit
		void SignalStopWork();

	private:
		NavigationSystem& m_navigationSystem;

		volatile bool     m_requestedStop;
	};

public:
	NavigationSystemBackgroundUpdate(NavigationSystem& navigationSystem)
		: m_pBackgroundThread(NULL)
		, m_navigationSystem(navigationSystem)
		, m_enabled(gEnv->IsEditor())
		, m_paused(false)
	{
		RegisterAsSystemListener();
	}

	~NavigationSystemBackgroundUpdate()
	{
		RemoveAsSystemListener();
		Stop();
	}

	bool IsRunning() const
	{
		return (m_pBackgroundThread != NULL);
	}

	void Pause(const bool pause)
	{
		if (pause)
		{
			if (Stop())   // Stop and synch if necessary
			{
				CryLog("NavMesh generation background thread stopped");
			}
		}

		m_paused = pause;
	}

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam);
	// ~ISystemEventListener

private:

	bool Start();
	bool Stop();

	void RegisterAsSystemListener();
	void RemoveAsSystemListener();

	bool IsEnabled() const { return m_enabled; }

	Thread*           m_pBackgroundThread;
	NavigationSystem& m_navigationSystem;

	bool              m_enabled;
	bool              m_paused;
};
#else
class NavigationSystemBackgroundUpdate
{
public:
	NavigationSystemBackgroundUpdate(NavigationSystem& navigationSystem)
	{}
	bool IsRunning() const                     { return false; }
	void Pause(const bool readingOrSavingMesh) {}
};
#endif

typedef MNM::BoundingVolume NavigationBoundingVolume;

struct NavigationMesh
{	
	NavigationMesh(const NavigationAgentTypeID _agentTypeID)
		: agentTypeID(_agentTypeID)
		, version(0)
	{
	}

#if DEBUG_MNM_ENABLED
	struct ProfileMemoryStats
	{
		ProfileMemoryStats(const MNM::CNavMesh::ProfilerType& _navMeshProfiler)
			: navMeshProfiler(_navMeshProfiler)
			, totalNavigationMeshMemory(0)
		{

		}

		const MNM::CNavMesh::ProfilerType& navMeshProfiler;
		size_t                             totalNavigationMeshMemory;
	};

	ProfileMemoryStats GetMemoryStats(ICrySizer* pSizer) const;
#endif

	NavigationAgentTypeID agentTypeID;
	size_t                version;

	MNM::CNavMesh         navMesh;
	NavigationVolumeID    boundary;
	CEnumFlags<INavigationSystem::EMeshFlag> flags;

	typedef std::vector<NavigationVolumeID> Markups;
	Markups markups;

#ifdef SW_NAVMESH_USE_GUID
	NavigationVolumeGUID boundaryGUID;
#endif
	typedef std::vector<NavigationVolumeID> ExclusionVolumes;
	ExclusionVolumes exclusions;
#ifdef SW_NAVMESH_USE_GUID
	typedef std::vector<NavigationVolumeGUID> ExclusionVolumesGUID;
	ExclusionVolumesGUID exclusionsGUID;
#endif

	string name; // TODO: this is currently duplicated for all agent types
};

struct AgentType
{
	// Copying the AgentType in a multi threaded environment
	// is not safe due to our string class being implemented as a
	// not thread safe Copy-on-Write
	// Using a custom assignment operator and a custom copy constructor
	// gives us thread safety without the usage of code locks

	AgentType() {}

	AgentType(const AgentType& other)
	{
		MakeDeepCopy(other);
	}

	struct Settings
	{
		Settings()
			: voxelSize(Vec3Constants<float>::fVec3_Zero)
		{}

		Vec3 voxelSize;
		MNM::SAgentSettings agent;
	};

	struct MeshInfo
	{
#ifdef SW_NAVMESH_USE_GUID
		MeshInfo(NavigationMeshGUID& _guid, const NavigationMeshID& _id, uint32 _name)
			: guid(_guid)
			, id(_id)
			, name(_name)
		{
		}
		NavigationMeshGUID guid;
#else
		MeshInfo(const NavigationMeshID& _id, uint32 _name)
			: id(_id)
			, name(_name)
		{
		}
#endif

		NavigationMeshID id;
		uint32           name;
	};

	AgentType& operator=(const AgentType& other)
	{
		MakeDeepCopy(other);

		return *this;
	}

	void MakeDeepCopy(const AgentType& other)
	{
		settings = other.settings;

		meshes = other.meshes;

		exclusions = other.exclusions;

		callbacks = other.callbacks;

		meshEntityCallback = other.meshEntityCallback;

		smartObjectUserClasses.reserve(other.smartObjectUserClasses.size());
		SmartObjectUserClasses::const_iterator end = other.smartObjectUserClasses.end();
		for (SmartObjectUserClasses::const_iterator it = other.smartObjectUserClasses.begin(); it != end; ++it)
			smartObjectUserClasses.push_back(it->c_str());

		name = other.name.c_str();
	}

	Settings settings;

	typedef std::vector<MeshInfo> Meshes;
	Meshes meshes;

	typedef std::vector<NavigationVolumeID> ExclusionVolumes;
	ExclusionVolumes exclusions;

	//////////////////////////////////////////////////////////////////////////
	// Temporary - need to find better structures
	// potentially there can be many markup volumes, for each agent type
	typedef std::vector<NavigationVolumeID> MarkupVolumes;
	MarkupVolumes markups;
	//////////////////////////////////////////////////////////////////////////

	CFunctorsList<NavigationMeshChangeCallback> callbacks;
	CFunctorsList<NavigationMeshChangeCallback> annotationCallbacks;

	NavigationMeshEntityCallback                meshEntityCallback;

	typedef std::vector<string> SmartObjectUserClasses;
	SmartObjectUserClasses smartObjectUserClasses;

	string                 name;
};

class NavigationSystem : public INavigationSystem
{
	friend class NavigationSystemDebugDraw;
	friend class NavigationSystemBackgroundUpdate;
	friend class CMNMUpdatesManager;

public:
	// BAI navigation file version history
	// Changes in version 12
	// - Saving Markups entity guids and names
	// Changes in version 11
	// - Fixed saving/loading markups when its' container is dynamic
	// Changes in version 10
	// - Navigation markup volumes and markup volumes data are stored
	// Changes in version 9
	//  - Navigation volumes storage is changed:
	//    * all used navigation volumes are saved (including exclusion volumes, which were missing before);
	//    * navigation area names saved together with volume data;
	//    * volumes stored only onces, instead of storing them together with each mesh.
	// Changes in version 8
	//  - struct MNM::Tile::STriangle layout is changed - now it has triangle flags

	enum eBAINavigationFileVersion : uint16
	{
		INCOMPATIBLE = 10,
		FIRST_COMPATIBLE,
		ENTITY_MARKUP_GUIDS,
		UPDATE_STATE,
		MESH_FLAGS,

		// Add new versions before NEXT
		NEXT,
		CURRENT = NEXT - 1,
	};

	NavigationSystem(const char* configName);
	~NavigationSystem();

	virtual NavigationAgentTypeID      CreateAgentType(const char* name, const SCreateAgentTypeParams& params) override;
	virtual NavigationAgentTypeID      GetAgentTypeID(const char* name) const override;
	virtual NavigationAgentTypeID      GetAgentTypeID(size_t index) const override;
	virtual const char*                GetAgentTypeName(NavigationAgentTypeID agentTypeID) const override;
	virtual size_t                     GetAgentTypeCount() const override;
	bool                               GetAgentTypeProperties(const NavigationAgentTypeID agentTypeID, AgentType& agentTypeProperties) const;

	virtual MNM::AreaAnnotation        GetAreaTypeAnnotation(const NavigationAreaTypeID areaTypeID) const override;
	virtual void                       SetGlobalFilterFlags(const MNM::AreaAnnotation::value_type includeFlags, const MNM::AreaAnnotation::value_type excludeFlags) override;
	virtual void                       GetGlobalFilterFlags(MNM::AreaAnnotation::value_type& includeFlags, MNM::AreaAnnotation::value_type& excludeFlags) const override;

	virtual const MNM::IAnnotationsLibrary& GetAnnotationLibrary() const override { return m_annotationsLibrary; }
	const MNM::CAnnotationsLibrary&         GetAnnotations()     { return m_annotationsLibrary; }

#ifdef SW_NAVMESH_USE_GUID
	virtual NavigationMeshID CreateMesh(const char* name, NavigationAgentTypeID agentTypeID, const SCreateMeshParams& params, NavigationMeshGUID guid) override;
	virtual NavigationMeshID CreateMesh(const char* name, NavigationAgentTypeID agentTypeID, const SCreateMeshParams& params, NavigationMeshID requestedId, NavigationMeshGUID guid) override;
#else
	virtual NavigationMeshID CreateMesh(const char* name, NavigationAgentTypeID agentTypeID, const SCreateMeshParams& params) override;
	virtual NavigationMeshID CreateMesh(const char* name, NavigationAgentTypeID agentTypeID, const SCreateMeshParams& params, NavigationMeshID requestedId) override;
#endif
	virtual NavigationMeshID CreateMeshForVolumeAndUpdate(const char* name, NavigationAgentTypeID agentTypeID, const SCreateMeshParams& params, const NavigationVolumeID volumeID) override;

	virtual void             DestroyMesh(NavigationMeshID meshID) override;
	virtual void             SetMeshFlags(NavigationMeshID meshID, const CEnumFlags<EMeshFlag> flags) override;
	virtual void             RemoveMeshFlags(NavigationMeshID meshID, const CEnumFlags<EMeshFlag> flags) override;
	virtual CEnumFlags<EMeshFlag> GetMeshFlags(NavigationMeshID meshID) const override;

	virtual void             SetMeshEntityCallback(NavigationAgentTypeID agentTypeID, const NavigationMeshEntityCallback& callback) override;
	virtual void             AddMeshChangeCallback(NavigationAgentTypeID agentTypeID, const NavigationMeshChangeCallback& callback) override;
	virtual void             RemoveMeshChangeCallback(NavigationAgentTypeID agentTypeID, const NavigationMeshChangeCallback& callback) override;
	virtual void             AddMeshAnnotationChangeCallback(NavigationAgentTypeID agentTypeID, const NavigationMeshChangeCallback& callback) override;
	virtual void             RemoveMeshAnnotationChangeCallback(NavigationAgentTypeID agentTypeID, const NavigationMeshChangeCallback& callback) override;

#ifdef SW_NAVMESH_USE_GUID
	virtual NavigationVolumeID CreateVolume(Vec3* vertices, size_t vertexCount, float height, NavigationVolumeGUID guid) override;
#else
	virtual NavigationVolumeID CreateVolume(Vec3* vertices, size_t vertexCount, float height) override;
#endif
	virtual NavigationVolumeID CreateVolume(Vec3* vertices, size_t vertexCount, float height, NavigationVolumeID requestedID) override;
	virtual void               DestroyVolume(NavigationVolumeID volumeID) override;
	virtual void               SetVolume(NavigationVolumeID volumeID, Vec3* vertices, size_t vertexCount, float height) override;
	virtual bool               ValidateVolume(NavigationVolumeID volumeID) const override;
	virtual NavigationVolumeID GetVolumeID(NavigationMeshID meshID) const override;

	virtual NavigationVolumeID CreateMarkupVolume(NavigationVolumeID requestedID) override;
	virtual void               SetMarkupVolume(const NavigationAgentTypesMask enabledAgentTypesMask, const Vec3* vertices, size_t vertexCount, const NavigationVolumeID volumeID, const MNM::SMarkupVolumeParams& params) override;
	virtual void               DestroyMarkupVolume(NavigationVolumeID volumeID) override;
	virtual void               SetAnnotationForMarkupTriangles(NavigationVolumeID volumeID, const MNM::AreaAnnotation& areaAnotation) override;

#ifdef SW_NAVMESH_USE_GUID
	virtual void SetMeshBoundaryVolume(NavigationMeshID meshID, NavigationVolumeID volumeID, NavigationVolumeGUID volumeGUID) override;
	virtual void SetExclusionVolume(const NavigationAgentTypeID* agentTypeIDs, size_t agentTypeIDCount,
	                                NavigationVolumeID volumeID, NavigationVolumeGUID volumeGUID) override;
#else
	virtual void SetMeshBoundaryVolume(NavigationMeshID meshID, NavigationVolumeID volumeID) override;
	virtual void SetExclusionVolume(const NavigationAgentTypeID* agentTypeIDs, size_t agentTypeIDCount,
	                                NavigationVolumeID volumeID) override;
#endif

	virtual NavigationMeshID      GetMeshID(const char* name, NavigationAgentTypeID agentTypeID) const override;
	virtual DynArray<NavigationMeshID> GetMeshIDsForAgentType(const NavigationAgentTypeID agentTypeID) const override;
	virtual const char*           GetMeshName(NavigationMeshID meshID) const override;
	virtual void                  SetMeshName(NavigationMeshID meshID, const char* name) override;

	virtual EWorkingState         GetState() const override;
	virtual EWorkingState         Update(const CTimeValue frameStartTime, const float frameTime, bool blocking) override;
	virtual void                  PauseNavigationUpdate() override;
	virtual void                  RestartNavigationUpdate() override;

	virtual uint32                GetWorkingQueueSize() const override;

	virtual void                  ProcessQueuedMeshUpdates() override;

	virtual void                  Clear() override;
	virtual void                  ClearAndNotify() override;
	virtual bool                  ReloadConfig() override;
	virtual void                  DebugDraw() override;
	virtual void                  Reset() override;

	void                          GetMemoryStatistics(ICrySizer* pSizer);

	virtual void                  SetDebugDisplayAgentType(NavigationAgentTypeID agentTypeID) override;
	virtual NavigationAgentTypeID GetDebugDisplayAgentType() const override;

	const NavigationMesh&         GetMesh(const NavigationMeshID& meshID) const;
	NavigationMesh&               GetMesh(const NavigationMeshID& meshID);

	virtual NavigationMeshID      GetEnclosingMeshID(const NavigationAgentTypeID agentTypeID, const Vec3& location) const override;
	virtual NavigationMeshID      FindEnclosingMeshID(const NavigationAgentTypeID agentTypeID, const Vec3& position) const override;
	virtual NavigationMeshID      FindEnclosingMeshID(const NavigationAgentTypeID agentTypeID, const Vec3& position, const MNM::SSnappingMetric& snappingMetric) const override;
	virtual NavigationMeshID      FindEnclosingMeshID(const NavigationAgentTypeID agentTypeID, const Vec3& position, const MNM::SOrderedSnappingMetrics& snappingMetrics) const override;

	virtual bool                  IsLocationInMeshVolume(const NavigationMeshID meshID, const Vec3& location) const override;
	virtual bool                  IsLocationInMeshVolume(const NavigationMeshID meshID, const Vec3& location, const MNM::SSnappingMetric& snapMetric) const override;
	virtual bool                  IsLocationInMeshVolume(const NavigationMeshID meshID, const Vec3& location, const MNM::SOrderedSnappingMetrics& snappingMetrics) const override;

	virtual bool                             GetClosestPointInNavigationMesh(const NavigationAgentTypeID agentID, const Vec3& location, float vrange, float hrange, Vec3* meshLocation, const INavMeshQueryFilter* pFilter) const override;

	virtual bool                             IsPointReachableFromPosition(const NavigationAgentTypeID agentID, const IEntity* pEntityToTestOffGridLinks, const Vec3& startLocation, const Vec3& endLocation, const INavMeshQueryFilter* pFilter) const override;
	virtual bool                             IsPointReachableFromPosition(const NavigationAgentTypeID agentID, const IEntity* pEntityToTestOffGridLinks, const Vec3& startLocation, const Vec3& endLocation, const MNM::SOrderedSnappingMetrics& snappingMetrics, const INavMeshQueryFilter* pFilter) const override;
	virtual bool                             IsPointReachableFromPosition(const IEntity* pEntityToTestOffGridLinks, const NavigationMeshID startMeshID, const MNM::TriangleID startTriangleID, const NavigationMeshID endMeshID, const MNM::TriangleID endTriangleID, const INavMeshQueryFilter* pFilter) const override;
	
	virtual bool                             IsLocationValidInNavigationMesh(const NavigationAgentTypeID agentID, const Vec3& location, const INavMeshQueryFilter* pFilter, float downRange = 1.0f, float upRange = 1.0f) const override;

	virtual INavigationSystem::NavMeshBorderWithNormalArray QueryTriangleBorders(const NavigationMeshID meshID, const MNM::aabb_t& localAabb) const override;
	virtual INavigationSystem::NavMeshBorderWithNormalArray QueryTriangleBorders(const NavigationMeshID meshID, const MNM::aabb_t& localAabb, MNM::ENavMeshQueryOverlappingMode overlappingMode, const INavMeshQueryFilter* pQueryFilter, const INavMeshQueryFilter* pAnnotationFilter) const override;

	virtual DynArray<Vec3>                   QueryTriangleCenterLocationsInMesh(const NavigationMeshID meshID, const MNM::aabb_t& localAabb) const override;
	virtual DynArray<Vec3>                   QueryTriangleCenterLocationsInMesh(const NavigationMeshID meshID, const MNM::aabb_t& localAabb, MNM::ENavMeshQueryOverlappingMode overlappingMode, const INavMeshQueryFilter* pFilter) const override;

	virtual bool                             GetTriangleVertices(const NavigationMeshID meshID, const MNM::TriangleID triangleID, Triangle& outTriangleVertices) const override;

	virtual MNM::GlobalIslandID              GetGlobalIslandIdAtPosition(const NavigationAgentTypeID agentID, const Vec3& location) const override;
	virtual MNM::GlobalIslandID              GetGlobalIslandIdAtPosition(const NavigationAgentTypeID agentID, const Vec3& location, const MNM::SOrderedSnappingMetrics& snappingMetrics) const override;
	virtual MNM::GlobalIslandID              GetGlobalIslandIdAtPosition(const NavigationMeshID meshID, const MNM::TriangleID triangleID) const override;

	virtual bool                             ReadFromFile(const char* fileName, bool bAfterExporting) override;
	virtual bool                             SaveToFile(const char* fileName) const override;

	virtual void                             RegisterListener(INavigationSystemListener* pListener, const char* name = NULL) override { m_listenersList.Add(pListener, name); }
	virtual void                             UnRegisterListener(INavigationSystemListener* pListener) override                        { m_listenersList.Remove(pListener); }

	virtual void                             RegisterUser(INavigationSystemUser* pUser, const char* name = NULL) override             { m_users.Add(pUser, name); }
	virtual void                             UnRegisterUser(INavigationSystemUser* pUser) override                                    { m_users.Remove(pUser); }

	virtual bool                             RegisterArea(const char* shapeName, NavigationVolumeID& outVolumeId) override;
	virtual void                             UnRegisterArea(const char* shapeName) override;
	virtual NavigationVolumeID               GetAreaId(const char* shapeName) const override;
	virtual void                             SetAreaId(const char* shapeName, NavigationVolumeID id) override;
	virtual void                             UpdateAreaNameForId(const NavigationVolumeID id, const char* newShapeName) override;
	virtual void                             RemoveLoadedMeshesWithoutRegisteredAreas() override;

	virtual bool                             RegisterEntityMarkups(const IEntity& owningEntity, const char** shapeNamesArray, const size_t count, NavigationVolumeID* pOutIdsArray) override;
	virtual void                             UnregisterEntityMarkups(const IEntity& owningEntity) override;

	virtual void                             StartWorldMonitoring() override;
	virtual void                             StopWorldMonitoring() override;

	virtual bool                             IsInUse() const override;
	virtual void                             CalculateAccessibility() override;

	void                                     RequestUpdateMeshAccessibility(const NavigationMeshID meshId);
	void                                     RequestUpdateAccessibilityAfterSeedChange(const Vec3& oldPosition, const Vec3& newPosition);
	void                                     UpdatePendingAccessibilityRequests();
	void                                     RemoveAllTrianglesByFlags(const MNM::AreaAnnotation::value_type flags);

	void                                     OffsetBoundingVolume(const Vec3& additionalOffset, const NavigationVolumeID volumeId);
	void                                     OffsetAllMeshes(const Vec3& additionalOffset);

	void                                     ComputeAllIslands();
	void                                     ComputeIslandsForMeshes(const NavigationMeshID* pUpdatedMeshes, const size_t count);
	void                                     ComputeMeshesAccessibility(const NavigationMeshID* pUpdatedMeshes, const size_t count);
	void                                     OnMeshesUpdateCompleted();

	void                                     AddOffMeshLinkIslandConnectionsBetweenTriangles(const NavigationMeshID& meshID, const MNM::TriangleID startingTriangleID, const MNM::TriangleID endingTriangleID, const MNM::OffMeshLinkID& linkID);
	void                                     RemoveOffMeshLinkIslandConnection(const MNM::OffMeshLinkID offMeshLinkId);

	MNM::TriangleID                          GetClosestMeshLocation(const NavigationMeshID meshID, const Vec3& location, float vrange, float hrange, const INavMeshQueryFilter* pFilter,
		Vec3* meshLocation, float* distance) const;
	
	virtual MNM::TileID                      GetTileIdWhereLocationIsAtForMesh(const NavigationMeshID meshID, const Vec3& location, const INavMeshQueryFilter* pFilter) override;
	virtual void                             GetTileBoundsForMesh(const NavigationMeshID meshID, const MNM::TileID tileID, AABB& bounds) const override;
	virtual MNM::TriangleID                  GetTriangleIDWhereLocationIsAtForMesh(const NavigationAgentTypeID agentID, const Vec3& location, const INavMeshQueryFilter* pFilter) override;
	virtual bool SnapToNavMesh(
		const NavigationAgentTypeID agentTypeID, const Vec3& position, const MNM::SSnappingMetric& snappingMetric, const INavMeshQueryFilter* pFilter,
		Vec3* pOutSnappedPosition, MNM::TriangleID* pOutTriangleID, NavigationMeshID* pOutNavMeshID) const override;

	virtual bool SnapToNavMesh(
		const NavigationAgentTypeID agentTypeID, const Vec3& position, const MNM::SOrderedSnappingMetrics& snappingMetrics, const INavMeshQueryFilter* pFilter,
		Vec3* pOutSnappedPosition, MNM::TriangleID* pOutTriangleID, NavigationMeshID* pOutNavMeshID) const override;

	virtual MNM::SPointOnNavMesh SnapToNavMesh(
		const NavigationAgentTypeID agentTypeID, const Vec3& position, const MNM::SSnappingMetric& snappingMetric, const INavMeshQueryFilter* pFilter, NavigationMeshID* pOutNavMeshID) const override;

	virtual MNM::SPointOnNavMesh SnapToNavMesh(
		const NavigationAgentTypeID agentTypeID, const Vec3& position, const MNM::SOrderedSnappingMetrics& snappingMetrics, const INavMeshQueryFilter* pFilter, NavigationMeshID* pOutNavMeshID) const override;

	virtual const MNM::INavMesh*             GetMNMNavMesh(const NavigationMeshID meshID) const override;
	virtual NavigationAgentTypeID            GetAgentTypeOfMesh(const NavigationMeshID meshID) const override;

	virtual MNM::ERayCastResult              NavMeshRayCast(const NavigationAgentTypeID agentTypeID, const Vec3& startPos, const Vec3& endPos, const INavMeshQueryFilter* pFilter, MNM::SRayHitOutput* pOutHit) const override;
	virtual MNM::ERayCastResult              NavMeshRayCast(const NavigationMeshID meshID, const MNM::TriangleID startTriangleId, const Vec3& startPos, const MNM::TriangleID endTriangleId, const Vec3& endPos, const INavMeshQueryFilter* pFilter, MNM::SRayHitOutput* pOutHit) const override;
	virtual MNM::ERayCastResult              NavMeshRayCast(const NavigationMeshID meshID, const MNM::SPointOnNavMesh& startPointOnNavMesh, const MNM::SPointOnNavMesh& endPointOnNavMesh, const INavMeshQueryFilter* pFilter, MNM::SRayHitOutput* pOutHit) const override;

	virtual const IOffMeshNavigationManager& GetIOffMeshNavigationManager() const override { return m_offMeshNavigationManager; }
	virtual IOffMeshNavigationManager&       GetIOffMeshNavigationManager() override       { return m_offMeshNavigationManager; }

	bool                                     AgentTypeSupportSmartObjectUserClass(const NavigationAgentTypeID agentTypeID, const char* smartObjectUserClass) const;
	uint16                                   GetAgentRadiusInVoxelUnits(const NavigationAgentTypeID agentTypeID) const;
	uint16                                   GetAgentHeightInVoxelUnits(const NavigationAgentTypeID agentTypeID) const;

	virtual TileGeneratorExtensionID         RegisterTileGeneratorExtension(MNM::TileGenerator::IExtension& extension) override;
	virtual bool                             UnRegisterTileGeneratorExtension(const TileGeneratorExtensionID extensionId) override;

	virtual MNM::INavMeshQueryManager*       GetNavMeshQueryManager() override;

	virtual INavigationUpdatesManager*       GetUpdateManager() override { return &m_updatesManager; }

	inline const WorldMonitor*               GetWorldMonitor() const
	{
		return &m_worldMonitor;
	}

	inline WorldMonitor* GetWorldMonitor()
	{
		return &m_worldMonitor;
	}

	inline const OffMeshNavigationManager* GetOffMeshNavigationManager() const
	{
		return &m_offMeshNavigationManager;
	}

	inline OffMeshNavigationManager* GetOffMeshNavigationManager()
	{
		return &m_offMeshNavigationManager;
	}

	inline const IslandConnectionsManager* GetIslandConnectionsManager() const
	{
		return &m_islandConnectionsManager;
	}

	inline IslandConnectionsManager* GetIslandConnectionsManager()
	{
		return &m_islandConnectionsManager;
	}

	struct TileTaskResult
	{
		TileTaskResult()
			: state(Running)
			, hashValue(0)
		{
		};

		enum State
		{
			Running = 0,
			Completed,
			NoChanges,
			Failed,
		};

		JobManager::SJobState          jobState;
		uint32                         hashValue;
		MNM::STile                     tile;
		MNM::CTileGenerator::SMetaData metaData;

		NavigationMeshID               meshID;

		uint16                         x;
		uint16                         y;
		uint16                         z;
		uint16                         volumeCopy;

		volatile uint16                state; // communicated over thread boundaries
		uint16                         next;  // next free
	};

private:
	struct VolumeDefCopy
	{
		VolumeDefCopy()
			: version(~0ul)
			, refCount(0)
			, meshID(0)
		{
		}

		size_t                           version;
		size_t                           refCount;

		NavigationMeshID                 meshID;

		MNM::BoundingVolume              boundary;
		std::vector<MNM::BoundingVolume> exclusions;
		std::vector<MNM::SMarkupVolume>  markups;
		std::vector<NavigationVolumeID>  markupIds;
	};

#if NAV_MESH_REGENERATION_ENABLED
	void UpdateMeshes(const CTimeValue frameStartTime, const float frameTime, const bool blocking, const bool multiThreaded, const bool bBackground);
	void UpdateMeshesFromEditor(const bool blocking, const bool multiThreaded, const bool bBackground);
	void SetupGenerator(const NavigationMeshID meshID, const MNM::CNavMesh::SGridParams& paramsGrid,
	                    uint16 x, uint16 y, uint16 z, MNM::CTileGenerator::Params& params, const VolumeDefCopy& pDef, bool bMarkupUpdate);
	bool SpawnJob(TileTaskResult& result, NavigationMeshID meshID, const MNM::CNavMesh::SGridParams& paramsGrid,
	              uint16 x, uint16 y, uint16 z, bool bMt, bool bNoHasTest);
	void CommitTile(TileTaskResult& result);
	void CommitMarkupData(const TileTaskResult& result, const MNM::TileID tileId);
#endif

	void ResetAllNavigationSystemUsers();

	void WaitForAllNavigationSystemUsersCompleteTheirReadingAsynchronousTasks();
	void UpdateNavigationSystemUsersForSynchronousWritingOperations();
	void UpdateNavigationSystemUsersForSynchronousOrAsynchronousReadingOperations();
	void UpdateInternalNavigationSystemData(const bool blocking);
	void UpdateInternalSubsystems();

	void ComputeWorldAABB();
	void SetupTasks();
	void StopAllTasks();
	void SetStateAndSendEvent(const EWorkingState newState);

	void UpdateAllListeners(const ENavigationEvent event);
	void ApplyAnnotationChanges();

	bool IsLocationInMeshVolume(const NavigationMesh& mesh, const Vec3& position, const MNM::SSnappingMetric& snapMetric) const;
	inline AABB GetAABBFromSnappingMetric(const Vec3& position, const MNM::SSnappingMetric& snappingMetric, const NavigationAgentTypeID agentID) const;
	inline bool IsOverlappingWithMeshVolume(const NavigationMesh& mesh, const AABB& aabb) const;

	void GatherNavigationVolumesToSave(std::vector<NavigationVolumeID>& usedVolumes) const;

	bool GrowMarkupsIfNeeded();

	// Returns false when the capacity is reached and no new element can be inserted
	template<class IdMap>
	bool GrowIdMapIfNeeded(IdMap& idMap)
	{
		if (idMap.size() >= idMap.capacity())
		{
			return idMap.grow(idMap.capacity()) > 0;
		}
		return true;
	}

	typedef std::vector<uint16> RunningTasks;
	RunningTasks m_runningTasks;
	size_t       m_maxRunningTaskCount;
	float        m_cacheHitRate;
	float        m_throughput;

	typedef stl::aligned_vector<TileTaskResult, alignof(TileTaskResult)> TileTaskResults;
	TileTaskResults m_results;
	uint16          m_free;
	EWorkingState   m_state;

	typedef id_map<uint32, NavigationMesh> Meshes;
	Meshes m_meshes;

	typedef id_map<uint32, MNM::BoundingVolume> Volumes;
	Volumes m_volumes;

	// Markup volumes
	typedef id_map<uint32, MNM::SMarkupVolume> MarkupVolumes;
	MarkupVolumes                                   m_markupVolumes;
	id_map<uint32, MNM::SMarkupVolumeData>          m_markupsData;
	std::unordered_map<NavigationVolumeID, MNM::AreaAnnotation> m_markupAnnotationChangesToApply;
	
	// vector of meshes ids, that were recently updated with newly generated tile
	std::vector<NavigationMeshID> m_recentlyUpdatedMeshIds;

	// vector of meshes ids, that were requested to updated their accessibility
	std::vector<NavigationMeshID> m_accessibilityUpdateRequestForMeshIds;

#ifdef SW_NAVMESH_USE_GUID
	typedef std::map<NavigationMeshGUID, NavigationMeshID> MeshMap;
	MeshMap m_swMeshes;

	typedef std::map<NavigationVolumeGUID, NavigationVolumeID> VolumeMap;
	VolumeMap m_swVolumes;

	int       m_nextFreeMeshId;
	int       m_nextFreeVolumeId;
#endif

	typedef std::vector<AgentType> AgentTypes;
	AgentTypes                        m_agentTypes;

	MNM::CAnnotationsLibrary          m_annotationsLibrary;

	uint32                            m_configurationVersion;

	NavigationSystemDebugDraw         m_debugDraw;

	NavigationSystemBackgroundUpdate* m_pEditorBackgroundUpdate;

	AABB                              m_worldAABB;
	WorldMonitor                      m_worldMonitor;

	OffMeshNavigationManager          m_offMeshNavigationManager;
	IslandConnectionsManager          m_islandConnectionsManager;

	std::vector<VolumeDefCopy>        m_volumeDefCopy;

	string                            m_configName;

	typedef CListenerSet<INavigationSystemListener*> NavigationListeners;
	NavigationListeners m_listenersList;

	typedef CListenerSet<INavigationSystemUser*> NavigationSystemUsers;
	NavigationSystemUsers                  m_users;

	CMNMUpdatesManager                     m_updatesManager;
	CVolumesManager                        m_volumesManager;
	bool                                   m_isNavigationUpdatePaused;

	MNM::STileGeneratorExtensionsContainer m_tileGeneratorExtensionsContainer;
	MNM::CNavMeshQueryManager*             m_pNavMeshQueryManager;

	CTimeValue                             m_frameStartTime;
	float                                  m_frameDeltaTime;
};

namespace NavigationSystemUtils
{
inline bool IsDynamicObjectPartOfTheMNMGenerationProcess(IPhysicalEntity* pPhysicalEntity)
{
	if (pPhysicalEntity)
	{
		pe_status_dynamics dyn;
		if (pPhysicalEntity->GetStatus(&dyn) && (dyn.mass <= 1e-6f))
			return true;
	}

	return false;
}
}

#endif
