// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include <CrySystem/IConsole.h>

class CEntity;
struct ICVar;
struct IConsoleCmdArgs;

//////////////////////////////////////////////////////////////////////////
// Console variables local to EntitySystem.
//////////////////////////////////////////////////////////////////////////
struct SEntityWithCharacterInstanceAutoComplete : public IConsoleArgumentAutoComplete
{
	SEntityWithCharacterInstanceAutoComplete();

	virtual int         GetCount() const override;
	virtual const char* GetValue(int index) const override;
};

struct CVar
{
	enum class EEntityDebugDrawType
	{
		Off,
		BoundingBoxWithName,
		BoundingBoxWithPositionPhysics,
		BoundingBoxWithEntityId,
		Hierarchies,
		Links,
		Components
	};

	// General entity CVars
	static ICVar*      pUpdateScript;
	static ICVar*      pUpdateEntities;
	static ICVar*      pEntityBBoxes;

	static ICVar*      pFlowgraphComponents;

	static EEntityDebugDrawType es_EntityDebugDraw;

	static int         es_DebugFindEntity;
	static int         es_debugEntityLifetime;
	static int         es_DebugEntityUsage;
	static const char* es_DebugEntityUsageFilter;
	static int         es_DebugEntityUsageSortMode;
	static int         es_debugDrawEntityIDs;

	static int         es_LayerSaveLoadSerialization;
	static int         es_SaveLoadUseLUANoSaveFlag;
	static int         es_LayerDebugInfo;

	static int         es_profileComponentUpdates;

	// Physics CVars
	static ICVar* pMinImpulseVel;
	static ICVar* pImpulseScale;
	static ICVar* pMaxImpulseAdjMass;
	static ICVar* pDebrisLifetimeScale;
	static ICVar* pHitCharacters;
	static ICVar* pHitDeadBodies;
	static ICVar* pLogCollisions;

	static int    es_UsePhysVisibilityChecks;

	static float  es_MaxPhysDist;
	static float  es_MaxPhysDistInvisible;
	static float  es_MaxPhysDistCloth;
	static float  es_FarPhysTimeout;

	static int    es_MaxJointFx;

	// Area manager CVars
	static ICVar* pDrawAreas;
	static ICVar* pDrawAreaGrid;
	static ICVar* pDrawAreaGridCells;
	static ICVar* pDrawAreaDebug;
	static ICVar* pLogAreaDebug;
	static ICVar* pUpdateAreas;

	static float  es_EntityUpdatePosDelta;

	static int    es_UseProximityTriggerSystem;

	// Script CVars
	static ICVar* pEnableFullScriptSave;
	static ICVar* pSysSpecLight;

	// Initialize console variables.
	static void Init();

	// Dump Entities
	static void DumpEntities(IConsoleCmdArgs*);
	static void DumpEntityClassesInUse(IConsoleCmdArgs*);

	// Recompile area grid
	static void CompileAreaGrid(IConsoleCmdArgs*);

	static void EnableDebugAnimText(IConsoleCmdArgs* args);
	static void SetDebugAnimText(CEntity* entity, const bool enable);

	// Console commands to enable/disable layers
	static void ConsoleCommandToggleLayer(IConsoleCmdArgs* pArgs);

	// Legacy CVar mapping
	static void MapEntityBBoxesCVar(ICVar* pBBoxCVar);
	static void MapDrawEntityIDCVar(ICVar* pIDCVar);
};
