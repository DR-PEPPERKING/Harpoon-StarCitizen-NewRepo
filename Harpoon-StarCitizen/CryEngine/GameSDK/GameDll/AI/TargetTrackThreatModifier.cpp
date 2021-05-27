// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  $Id$
  $DateTime$
  Description: Game-side logic for controlling the threat level of an AI
				agent's current target
  
 -------------------------------------------------------------------------
  History:
  - 09:08:2010: Created by Kevin Kirst

*************************************************************************/

#include "StdAfx.h"
#include "TargetTrackThreatModifier.h"
#include "GameCVars.h"
#include <CryAISystem/IAIObject.h>
#include <CryAISystem/IAIActor.h>
#include <CryAISystem/IAIActorProxy.h>

//////////////////////////////////////////////////////////////////////////
CTargetTrackThreatModifier::CTargetTrackThreatModifier()
{
	ITargetTrackManager *pTargetTrackManager = gEnv->pAISystem->GetTargetTrackManager();
	if (pTargetTrackManager)
		pTargetTrackManager->SetTargetTrackThreatModifier(this);
}

//////////////////////////////////////////////////////////////////////////
CTargetTrackThreatModifier::~CTargetTrackThreatModifier()
{
	ITargetTrackManager *pTargetTrackManager = gEnv->pAISystem->GetTargetTrackManager();
	if (pTargetTrackManager)
		pTargetTrackManager->ClearTargetTrackThreatModifier();
}

//////////////////////////////////////////////////////////////////////////
void CTargetTrackThreatModifier::ModifyTargetThreat(IAIObject &ownerAI, IAIObject &targetAI, const ITargetTrack &track, float &outThreatRatio, EAITargetThreat &outThreat) const
{
	if (track.GetTargetType() > AITARGET_SOUND && outThreat >= AITHREAT_AGGRESSIVE)
	{
		outThreatRatio = 1.0f;
	}
}
