// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CET_LevelLoading.h"
#include "ILevelSystem.h"
#include "GameClientChannel.h"
#include <CryNetwork/NetHelpers.h>
#include "GameContext.h"
#include "GameContext.h"
#include <CryThreading/IThreadManager.h>

/*
 * Prepare level
 */

class CCET_PrepareLevel : public CCET_Base
{
public:
	const char*                 GetName() { return "PrepareLevel"; }

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		CGameContext* pGameContext = CCryAction::GetCryAction()->GetGameContext();
		string levelName = pGameContext ? pGameContext->GetLevelName() : string("");
		if (levelName.empty())
		{
			//GameWarning("No level name set");
			return eCETR_Wait;
		}
		if (gEnv->IsClient() && !gEnv->bServer)
		{
			CryLogAlways("============================ PrepareLevel %s ============================", levelName.c_str());

			gEnv->pSystem->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_LOAD_PREPARE, 0, 0);

			CCryAction::GetCryAction()->GetILevelSystem()->PrepareNextLevel(levelName);
		}
		return eCETR_Ok;
	}
};

void AddPrepareLevelLoad(IContextEstablisher* pEst, EContextViewState state)
{
	pEst->AddTask(state, new CCET_PrepareLevel());
}

/*
 * Load a level
 */

class CCET_LoadLevel : public CCET_Base
{
public:
	CCET_LoadLevel() : m_started(false) {}

	const char*                 GetName() { return "LoadLevel"; }

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		CCryAction* pAction = CCryAction::GetCryAction();
		CGameContext* pGameContext = pAction->GetGameContext();
		string levelName = pGameContext ? pGameContext->GetLevelName() : string("");
		if (levelName.empty())
		{
			GameWarning("No level name set");
			return eCETR_Failed;
		}

		pAction->StartNetworkStallTicker(true);
		GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_LOAD_START, reinterpret_cast<UINT_PTR>(levelName.c_str()), 0);
		ILevelInfo* pILevel = pAction->GetILevelSystem()->LoadLevel(levelName);
		GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_LOAD_END, 0, 0);
		pAction->StopNetworkStallTicker();

		if (pILevel == NULL)
		{
			GameWarning("Failed to load level: %s", levelName.c_str());
			return eCETR_Failed;
		}
		m_started = true;
		return eCETR_Ok;
	}

	bool m_started;
};

class CLevelLoadingThread : public IThread
{
public:
	enum EState
	{
		eState_Working,
		eState_Failed,
		eState_Succeeded,
	};

	CLevelLoadingThread(ILevelSystem* pLevelSystem, const char* szLevelName) 
		: m_pLevelSystem(pLevelSystem), m_state(eState_Working), m_levelName(szLevelName) 
	{
	}

	virtual void ThreadEntry() override
	{
		const threadID levelLoadingThreadId = CryGetCurrentThreadId();

		if (gEnv->pRenderer) //Renderer may be unavailable when turned off
		{
			gEnv->pRenderer->SetLevelLoadingThreadId(levelLoadingThreadId);
		}

		const ILevelInfo* pLoadedLevelInfo = m_pLevelSystem->LoadLevel(m_levelName.c_str());
		const bool bResult = (pLoadedLevelInfo != NULL);

		if (gEnv->pRenderer) //Renderer may be unavailable when turned off
		{
			gEnv->pRenderer->SetLevelLoadingThreadId(0);
		}

		m_state = bResult ? eState_Succeeded : eState_Failed;
	}

	EState GetState() const
	{
		return m_state;
	}

private:
	ILevelSystem* m_pLevelSystem;
	volatile EState m_state;
	const string m_levelName;
};

class CCET_LoadLevelAsync : public CCET_Base
{
public:
	CCET_LoadLevelAsync() : m_bStarted(false), m_pLevelLoadingThread(nullptr) {}

	virtual const char* GetName() override 
	{ 
		return "LoadLevelAsync"; 
	}

	virtual EContextEstablishTaskResult OnStep( SContextEstablishState& state ) override
	{
		CCryAction* pAction = CCryAction::GetCryAction();
		CGameContext* pGameContext = pAction->GetGameContext();
		string levelName = pGameContext ? pGameContext->GetLevelName() : string("");
		if (!m_bStarted)
		{
			if (levelName.empty())
			{
				GameWarning("No level name set");
				return eCETR_Failed;
			} 

			pAction->StartNetworkStallTicker(true);
			GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_LOAD_START, reinterpret_cast<UINT_PTR>(levelName.c_str()), 0);

			ILevelSystem* pLevelSystem = pAction->GetILevelSystem();
			m_pLevelLoadingThread = new CLevelLoadingThread(pLevelSystem, levelName.c_str());
			if (!gEnv->pThreadManager->SpawnThread(m_pLevelLoadingThread, "LevelLoadingThread"))
			{
				CryFatalError("Error spawning LevelLoadingThread thread.");
			}

			m_bStarted = true;
		}

		const CLevelLoadingThread::EState threadState = m_pLevelLoadingThread->GetState();
		if(threadState == CLevelLoadingThread::eState_Working)
			return eCETR_Wait;

		gEnv->pThreadManager->JoinThread(m_pLevelLoadingThread, eJM_Join);
		delete m_pLevelLoadingThread;
		m_pLevelLoadingThread = nullptr;

		GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_LOAD_END, 0, 0);
		pAction->StopNetworkStallTicker();
		if(threadState == CLevelLoadingThread::eState_Succeeded)
		{
			return eCETR_Ok;
		}
		else
		{
			GameWarning("Failed to load level: %s", levelName.c_str());
			return eCETR_Failed;
		}
	}

	bool m_bStarted;
	CLevelLoadingThread* m_pLevelLoadingThread;
};


class CCET_LoadLevelTimeSliced : public CCET_Base
{
public:
	virtual const char* GetName() override { return "LoadLevelTimeSliced"; }

	virtual EContextEstablishTaskResult OnStep(SContextEstablishState& state) override
	{
		if (!m_started)
		{
			m_started = Start();
			if (!m_started)
			{
				return eCETR_Failed;
			}
		}

		return DoStep();
	}

	bool Start()
	{
		CCryAction* pAction = CCryAction::GetCryAction();
		CGameContext* pGameContext = pAction->GetGameContext();
		m_levelName = pGameContext ? pGameContext->GetLevelName() : string("");
		if (m_levelName.empty())
		{
			GameWarning("No level name set");
			return false;
		}

		GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_LOAD_START, reinterpret_cast<UINT_PTR>(m_levelName.c_str()), 0);
		const bool started = pAction->GetILevelSystem()->StartLoadLevel(m_levelName);
		if (!started)
		{
			GameWarning("Failed to load level: %s", m_levelName.c_str());
			GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_LOAD_END, 0, 0);
		}
		pAction->StartNetworkStallTicker(true);
		return started;
	}

	EContextEstablishTaskResult DoStep()
	{
		CCryAction* pAction = CCryAction::GetCryAction();
		const ILevelSystem::ELevelLoadStatus status = pAction->GetILevelSystem()->UpdateLoadLevelStatus();

		if (status == ILevelSystem::ELevelLoadStatus::InProgress)
		{
			return eCETR_Wait;
		}

		EContextEstablishTaskResult result = eCETR_Ok;
		if (status == ILevelSystem::ELevelLoadStatus::Failed)
		{
			GameWarning("Failed to load level: %s", m_levelName.c_str());
			result = eCETR_Failed;
		}

		GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_LEVEL_LOAD_END, 0, 0);
		pAction->StopNetworkStallTicker();

		return result;
	}


	bool m_started = false;
	string m_levelName;
};


void AddLoadLevel( IContextEstablisher * pEst, EContextViewState state, bool ** ppStarted )
{
	const bool bIsEditor = gEnv->IsEditor();
	const bool bIsDedicated = gEnv->IsDedicated();
	const ICVar* pAsyncLoad = gEnv->pConsole->GetCVar("g_asynclevelload");
	const bool bAsyncLoad = pAsyncLoad && pAsyncLoad->GetIVal() > 0;

	if(!bIsEditor && !bIsDedicated && bAsyncLoad)
	{
		CCET_LoadLevelAsync* pLL = new CCET_LoadLevelAsync;
		pEst->AddTask( state, pLL );
		*ppStarted = &pLL->m_bStarted;
	}
	else
	{
		const ICVar* pTimeSliced = gEnv->pConsole->GetCVar("g_levelLoadTimeSliced");
		const bool isTimeSliced = pTimeSliced && pTimeSliced->GetIVal() > 0;

		if (isTimeSliced)
		{
			CCET_LoadLevelTimeSliced* pLL = new CCET_LoadLevelTimeSliced;
			pEst->AddTask(state, pLL);
			*ppStarted = &pLL->m_started;
		}
		else
		{
			CCET_LoadLevel* pLL = new CCET_LoadLevel;
			pEst->AddTask(state, pLL);
			*ppStarted = &pLL->m_started;
		}
	}
}

/*
 * Loading complete
 */

class CCET_LoadingComplete : public CCET_Base
{
public:
	CCET_LoadingComplete(bool* pStarted) : m_pStarted(pStarted) {}

	const char*                 GetName() { return "LoadingComplete"; }

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		ILevelSystem* pLS = CCryAction::GetCryAction()->GetILevelSystem();
		if (pLS->GetCurrentLevel())
		{
			pLS->OnLoadingComplete(pLS->GetCurrentLevel());
			return eCETR_Ok;
		}
		return eCETR_Failed;
	}
	virtual void OnFailLoading(bool hasEntered)
	{
		ILevelSystem* pLS = CCryAction::GetCryAction()->GetILevelSystem();
		if (m_pStarted && *m_pStarted && !hasEntered)
			pLS->OnLoadingError(NULL, "context establisher failed");
	}

	bool* m_pStarted;
};

void AddLoadingComplete(IContextEstablisher* pEst, EContextViewState state, bool* pStarted)
{
	pEst->AddTask(state, new CCET_LoadingComplete(pStarted));
}

/*
 * Reset Areas
 */
class CCET_ResetAreas : public CCET_Base
{
public:
	const char*                 GetName() { return "ResetAreas"; }

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		gEnv->pEntitySystem->ResetAreas();
		return eCETR_Ok;
	}
};

void AddResetAreas(IContextEstablisher* pEst, EContextViewState state)
{
	pEst->AddTask(state, new CCET_ResetAreas);
}

/*
 * Action events
 */
class CCET_ActionEvent : public CCET_Base
{
public:
	CCET_ActionEvent(const SActionEvent& evt) : m_evt(evt) {}

	const char*                 GetName() { return "ActionEvent"; }

	EContextEstablishTaskResult OnStep(SContextEstablishState& state)
	{
		CCryAction::GetCryAction()->OnActionEvent(m_evt);
		return eCETR_Ok;
	}

private:
	SActionEvent m_evt;
};

void AddActionEvent(IContextEstablisher* pEst, EContextViewState state, const SActionEvent& evt)
{
	pEst->AddTask(state, new CCET_ActionEvent(evt));
}
