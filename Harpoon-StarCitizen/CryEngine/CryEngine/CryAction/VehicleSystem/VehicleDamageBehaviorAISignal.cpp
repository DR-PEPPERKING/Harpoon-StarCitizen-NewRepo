// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VehicleDamageBehaviorAISignal.h"

#include "IVehicleSystem.h"

CVehicleDamageBehaviorAISignal::CVehicleDamageBehaviorAISignal()
	: m_pVehicle(nullptr)
	, m_freeSignalRepeat(false)
	, m_isActive(false)
	, m_signalId(0)
	, m_freeSignalRadius(15.0f)
	, m_timeCounter(0.0f)
{}

//------------------------------------------------------------------------
bool CVehicleDamageBehaviorAISignal::Init(IVehicle* pVehicle, const CVehicleParams& table)
{
	m_pVehicle = pVehicle;

	CVehicleParams signalTable = table.findChild("AISignal");
	if (!signalTable)
		return false;

	if (!signalTable.getAttr("signalId", m_signalId))
		return false;

	m_signalText = signalTable.getAttr("signalText");
	m_freeSignalText = signalTable.getAttr("freeSignalText");
	signalTable.getAttr("freeSignalRadius", m_freeSignalRadius);
	signalTable.getAttr("freeSignalRepeat", m_freeSignalRepeat);

	return true;
}

void CVehicleDamageBehaviorAISignal::Reset()
{
	if (m_isActive)
		ActivateUpdate(false);
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorAISignal::ActivateUpdate(bool activate)
{
	if (activate && !m_isActive)
	{
		m_timeCounter = 1.0f;
		m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_AlwaysUpdate);
	}
	else if (!activate && m_isActive)
		m_pVehicle->SetObjectUpdate(this, IVehicle::eVOU_NoUpdate);

	m_isActive = activate;
}

//------------------------------------------------------------------------
void CVehicleDamageBehaviorAISignal::OnDamageEvent(EVehicleDamageBehaviorEvent event, const SVehicleDamageBehaviorEventParams& behaviorParams)
{
	if (event == eVDBE_VehicleDestroyed || event == eVDBE_MaxRatioExceeded)
	{
		ActivateUpdate(false);
		return;
	}

	if (event != eVDBE_Hit)
		return;

	IEntity* pEntity = m_pVehicle->GetEntity();
	if (!pEntity || !pEntity->HasAI())
		return;

	IAISystem* pAISystem = gEnv->pAISystem;
	if (!pAISystem)
		return;

	if (!m_freeSignalText.empty())
	{
		Vec3 entityPosition = pEntity->GetPos();

		AISignals::IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
		pData->point = entityPosition;

		pAISystem->SendAnonymousSignal(gEnv->pAISystem->GetSignalManager()->CreateSignal_DEPRECATED(m_signalId, m_freeSignalText, 0, pData), entityPosition, m_freeSignalRadius);

		if (m_freeSignalRepeat)
			ActivateUpdate(true);
	}

	AISignals::IAISignalExtraData* pExtraData = pAISystem->CreateSignalExtraData();
	CRY_ASSERT(pExtraData);
	pExtraData->iValue = behaviorParams.shooterId;

	pAISystem->SendSignal(AISignals::ESignalFilter::SIGNALFILTER_SENDER, gEnv->pAISystem->GetSignalManager()->CreateSignal_DEPRECATED(m_signalId, m_freeSignalText, pEntity->GetId(), pExtraData));
}

void CVehicleDamageBehaviorAISignal::Update(const float deltaTime)
{
	if (m_pVehicle->IsDestroyed())
	{
		ActivateUpdate(false);
		return;
	}

	m_timeCounter -= deltaTime;
	if (m_timeCounter < 0.0f)
	{
		IEntity* pEntity = m_pVehicle->GetEntity();
		CRY_ASSERT(pEntity);
		IAISystem* pAISystem = gEnv->pAISystem;
		CRY_ASSERT(pAISystem);

		Vec3 entityPosition = pEntity->GetPos();

		AISignals::IAISignalExtraData* pData = gEnv->pAISystem->CreateSignalExtraData();
		pData->point = entityPosition;

		pAISystem->SendAnonymousSignal(gEnv->pAISystem->GetSignalManager()->CreateSignal_DEPRECATED(m_signalId, m_freeSignalText, 0, pData), entityPosition, m_freeSignalRadius);

		m_timeCounter = 1.0f;
	}
}

void CVehicleDamageBehaviorAISignal::GetMemoryUsage(ICrySizer* s) const
{
	s->AddObject(this, sizeof(*this));
	s->AddObject(m_signalText);
	s->AddObject(m_freeSignalText);
}

DEFINE_VEHICLEOBJECT(CVehicleDamageBehaviorAISignal);
