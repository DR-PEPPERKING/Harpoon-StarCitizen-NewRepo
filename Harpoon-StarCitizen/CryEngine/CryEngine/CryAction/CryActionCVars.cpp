// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CryActionCVars.h"
#include "Serialization/XmlSerializeHelper.h"
#include "Network/GameContext.h"
#include <CrySystem/ConsoleRegistration.h>

CCryActionCVars* CCryActionCVars::s_pThis = 0;

CCryActionCVars::CCryActionCVars()
{
	CRY_ASSERT(!s_pThis);
	s_pThis = this;

	REGISTER_CVAR2("g_playerInteractorRadius", &playerInteractorRadius, 1.8f, VF_CHEAT, "Maximum radius at which player can interact with other entities");
	REGISTER_CVAR2("i_itemSystemDebugMemStats", &debugItemMemStats, 0, VF_CHEAT, "Display item memory stats on screen");

	REGISTER_CVAR2("g_debug_stats", &g_debug_stats, 0, VF_NULL, "Enabled massive gameplay events debug");
	REGISTER_CVAR2("g_statisticsMode", &g_statisticsMode, 2, VF_NULL, "Statistics mode\n"
	                                                                  " 0 - disabled\n"
	                                                                  " 1 - enabled crysis mode\n"
	                                                                  " 2 - enabled K01 mode\n");

	REGISTER_CVAR2("cl_useCurrentUserNameAsDefault", &useCurrentUserNameAsDefault, 1, 0, "Use the current user name instead of the default profile's name");

#if !defined(_RELEASE)
	REGISTER_CVAR2("g_userNeverAutoSignsIn", &g_userNeverAutoSignsIn, 0, VF_CHEAT, "for autobuilds never automatically bring up the user sign in window, if the user isn't signed in. Can affect performance and skew performance test results. Has to be added to system.cfg, doesn't work if added to the commandline!");
#endif

	// Currently, GameDLLs should set this cvar - it's a fundamental change in AI//depot/Games/Crysis2/Branches/Develop/MP/Trunk/Engine/Config/multiplayer.cfg
	// 0 is the preferred value, 1 is for Crysis compatibility
	int defaultAiFlowNodeAlertnessCheck = 1;
	REGISTER_CVAR2("ai_FlowNodeAlertnessCheck", &aiFlowNodeAlertnessCheck, defaultAiFlowNodeAlertnessCheck, VF_INVISIBLE, "Enable the alertness check in AI flownodes");

	//Gameplay Analyst
	REGISTER_CVAR2("g_gameplayAnalyst", &g_gameplayAnalyst, 0, VF_REQUIRE_APP_RESTART, "Enable/Disable Gameplay Analyst");
	REGISTER_CVAR2("g_multiplayerEnableVehicles", &g_multiplayerEnableVehicles, 1, 0, "Enable vehicles in multiplayer");

	// Cooperative Animation System
	REGISTER_CVAR(co_coopAnimDebug, 0, 0, "Enable Cooperative Animation debug output");
	REGISTER_CVAR(co_usenewcoopanimsystem, 1, VF_CHEAT, "Uses the new cooperative animation system which works without the animation graph");
	REGISTER_CVAR(co_slideWhileStreaming, 0, 0, "Allows the sliding while the anims are being streamed. Otherwise the sliding step while wait until the anims are streaming");
	// ~Cooperative Animation System

	// AI stances
	ag_defaultAIStance = REGISTER_STRING("ag_defaultAIStance", "combat", 0, "Specifies default stance name for AI");

	REGISTER_CVAR(g_syncClassRegistry, 0, VF_NULL, "synchronize class registry from server to clients");

	REGISTER_CVAR(g_allowSaveLoadInEditor, 0, VF_NULL, "Allow saving and loading games in the editor (DANGEROUS)");
	REGISTER_CVAR(g_saveLoadBasicEntityOptimization, 1, VF_NULL, "Switch basic entity data optimization");
	REGISTER_CVAR(g_debugSaveLoadMemory, 0, VF_CHEAT, "Print debug information about save/load memory usage");
	REGISTER_CVAR(g_saveLoadUseExportedEntityList, 1, VF_NULL, "Only save entities in the editor-generated save list (if available). 0 is the previous behavior");
	REGISTER_CVAR(g_saveLoadExtendedLog, 0, VF_NULL, "Enables the generation of detailed log information regarding saveloads");
	REGISTER_CVAR(g_useXMLCPBinForSaveLoad, 1, VF_REQUIRE_LEVEL_RELOAD, "Use XML compressed binary format for save and loads. DON'T CHANGE THIS DURING RUNTIME!");
	REGISTER_CVAR(g_XMLCPBGenerateXmlDebugFiles, 0, VF_CHEAT, "Activates the generation, for debug purposes, of a text xml file each time that there is a binary save (LastBinarySaved.xml) or load (LastBinaryLoaded.xml).");
	REGISTER_CVAR(g_XMLCPBAddExtraDebugInfoToXmlDebugFiles, 0, VF_CHEAT, "When the xml debug files are activated, this option adds the name and entity class name to every entity reference in the .xml .");
	REGISTER_CVAR(g_XMLCPBSizeReportThreshold, 2048, VF_CHEAT, "defines the minimun size needed for nodes to be shown in the xml report file");
	REGISTER_CVAR(g_XMLCPBUseExtraZLibCompression, 1, VF_CHEAT, "Enables an extra zlib compression pass on the binary saves.");
	REGISTER_CVAR(g_XMLCPBBlockQueueLimit, 6, VF_CHEAT | VF_REQUIRE_APP_RESTART, "Limits the number of blocks to queue for saving, causes a main thread stall if exceeded. 0 for no limit.");

	REGISTER_CVAR(g_allowDisconnectIfUpdateFails, 1, VF_INVISIBLE, "");

	REGISTER_CVAR(g_useSinglePosition, 1, VF_NULL, "Activates the new Single Position update order");
	REGISTER_CVAR(g_handleEvents, 1, VF_NULL, "Activates the registration requirement for GameObjectEvents");

	REGISTER_COMMAND("g_saveLoadDumpEntity", DumpEntitySerializationData, 0, "Print to console the xml data saved for a specified entity");
	REGISTER_COMMAND("g_dumpClassRegistry", DumpClassRegistry, 0, "Print to console the list of classes and their associated ids");

	REGISTER_CVAR2("g_enableMergedMeshRuntimeAreas", &g_enableMergedMeshRuntimeAreas, 0, VF_CHEAT | VF_REQUIRE_APP_RESTART, "Enables the Merged Mesh cluster generation and density precalculations at game/level load");

	REGISTER_CVAR(g_useProfileManager, 1, 0, "Use player profiles manager system. \nDefault is 1.\n");
	REGISTER_CVAR(g_legacyItemSystem,  1, 0, "Initialize Legacy Item System from GameSDK. \nDefault is 1.\n");

	REGISTER_CVAR(g_enableActionGame,  1, 0, "Use CryAction ActionGame and GameContext functionality from GameSDK. \nDefault is 1.\n");
}

CCryActionCVars::~CCryActionCVars()
{
	CRY_ASSERT(s_pThis != 0);
	s_pThis = 0;

	IConsole* pConsole = gEnv->pConsole;

	pConsole->UnregisterVariable("g_useSinglePosition", true);
	pConsole->UnregisterVariable("g_handleEvents", true);

	pConsole->UnregisterVariable("g_playerInteractorRadius", true);
	pConsole->UnregisterVariable("i_itemSystemDebugMemStats", true);
	pConsole->UnregisterVariable("g_debug_stats", true);
	pConsole->UnregisterVariable("g_statisticsMode", true);

	pConsole->UnregisterVariable("ai_FlowNodeAlertnessCheck", true);

	pConsole->UnregisterVariable("co_usenewcoopanimsystem", true);

	pConsole->UnregisterVariable("g_allowDisconnectIfUpdateFails", true);

	pConsole->UnregisterVariable("sw_gridSize");
	pConsole->UnregisterVariable("sw_debugInfo");
	pConsole->UnregisterVariable("sw_draw_bbox");
	pConsole->UnregisterVariable("g_enableMergedMeshRuntimeAreas", true);
	pConsole->UnregisterVariable("g_useProfileManager", true);
	pConsole->UnregisterVariable("g_legacyItemSystem", true);

	pConsole->UnregisterVariable("cl_useCurrentUserNameAsDefault", true);
#if !defined(_RELEASE)
	pConsole->UnregisterVariable("g_userNeverAutoSignsIn", true);
#endif
	pConsole->UnregisterVariable("g_gameplayAnalyst", true);
	pConsole->UnregisterVariable("g_multiplayerEnableVehicles", true);
	pConsole->UnregisterVariable("co_coopAnimDebug", true);
	pConsole->UnregisterVariable("co_slideWhileStreaming", true);
	pConsole->UnregisterVariable("ag_defaultAIStance", true);
	pConsole->UnregisterVariable("g_syncClassRegistry", true);
	pConsole->UnregisterVariable("g_allowSaveLoadInEditor", true);
	pConsole->UnregisterVariable("g_saveLoadBasicEntityOptimization", true);
	pConsole->UnregisterVariable("g_debugSaveLoadMemory", true);
	pConsole->UnregisterVariable("g_saveLoadUseExportedEntityList", true);
	pConsole->UnregisterVariable("g_saveLoadExtendedLog", true);
	pConsole->UnregisterVariable("g_useXMLCPBinForSaveLoad", true);
	pConsole->UnregisterVariable("g_XMLCPBGenerateXmlDebugFiles", true);
	pConsole->UnregisterVariable("g_XMLCPBAddExtraDebugInfoToXmlDebugFiles", true);
	pConsole->UnregisterVariable("g_XMLCPBSizeReportThreshold", true);
	pConsole->UnregisterVariable("g_XMLCPBUseExtraZLibCompression", true);
	pConsole->UnregisterVariable("g_XMLCPBBlockQueueLimit", true);
	pConsole->UnregisterVariable("g_saveLoadDumpEntity", true);
	pConsole->UnregisterVariable("g_dumpClassRegistry", true);
	pConsole->UnregisterVariable("g_enableActionGame", true);
}

void CCryActionCVars::DumpEntitySerializationData(IConsoleCmdArgs* pArgs)
{
	if (pArgs->GetArgCount() != 2)
	{
		CryLog("Format: es_dumpEntitySerializationData [id | name]");
	}

	IEntity* pEntity = NULL;
	const char* name = pArgs->GetArg(1);
	pEntity = gEnv->pEntitySystem->FindEntityByName(name);
	if (!pEntity)
	{
		EntityId id = (EntityId)atoi(name);
		pEntity = gEnv->pEntitySystem->GetEntity(id);
	}
	if (!pEntity)
	{
		CryLog("Unable to find entity %s", name);
		return;
	}

	XmlNodeRef node = gEnv->pSystem->CreateXmlNode("root");
	if (node)
	{
		CXmlSerializeHelper serializer;
		TSerialize ser(serializer.GetWriter(node));

		// first entity properties
		int32 iEntityFlags = pEntity->GetFlags();
		if ((iEntityFlags & ENTITY_SERIALIZE_PROPERTIES) && !(iEntityFlags & ENTITY_FLAG_UNREMOVABLE))
		{
			ser.BeginGroup("EntityProperties");
			pEntity->Serialize(ser, ENTITY_SERIALIZE_PROPERTIES);
			ser.EndGroup();
		}

		// then extra data
		ser.BeginGroup("ExtraEntityData");
		if (iEntityFlags & ENTITY_FLAG_MODIFIED_BY_PHYSICS)
		{
			ser.BeginGroup("EntityGeometry");
			pEntity->Serialize(ser, ENTITY_SERIALIZE_GEOMETRIES);
			ser.EndGroup();
		}
		pEntity->Serialize(ser, ENTITY_SERIALIZE_PROXIES);
		ser.EndGroup();

		CryLogAlways("Serializing entity %s", pEntity->GetName());
		string xml = node->getXML().c_str();
		int pos = 0;
		string line = xml.Tokenize("\n", pos);
		for (; (pos >= 0) && !line.empty(); line = xml.Tokenize("\n", pos))
		{
			CryLogAlways("%s", line.c_str());
		}
	}
}

void CCryActionCVars::DumpClassRegistry(IConsoleCmdArgs* pArgs)
{
	CGameContext* pGameContext = CCryAction::GetCryAction()->GetGameContext();
	if (pGameContext)
	{
		pGameContext->DumpClasses();
	}
	else
	{
		CryLogAlways("Unable to dump class registry, game context is NULL");
	}
}
