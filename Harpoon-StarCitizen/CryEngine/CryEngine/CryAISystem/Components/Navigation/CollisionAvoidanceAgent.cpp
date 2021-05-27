// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CollisionAvoidanceAgent.h"
#include "NavigationComponent.h"

using namespace Cry::AI::CollisionAvoidance;

CCollisionAvoidanceAgent::~CCollisionAvoidanceAgent()
{
	CRY_ASSERT_MESSAGE(!m_pAttachedEntity, "Collision Agent was never unregistered from the Collision Avoidance System.");
}

// If agent is already registered, calling this function will have no effect.
bool CCollisionAvoidanceAgent::Register(IEntity* pEntity)
{
	if (!pEntity)
	{
		CRY_ASSERT(pEntity, "Parameter 'pEntity' must be non-null.");
		return false;
	}

	const bool registeredSuccessfully = gEnv->pAISystem->GetCollisionAvoidanceSystem()->RegisterAgent(this);
	if (!registeredSuccessfully)
	{
		return false;
	}

	m_pAttachedEntity = pEntity;
	return true;
}

bool CCollisionAvoidanceAgent::Unregister()
{
	const bool unregisteredSuccessfully = gEnv->pAISystem->GetCollisionAvoidanceSystem()->UnregisterAgent(this);
	if (!unregisteredSuccessfully)
	{
		return false;
	}

	m_pAttachedEntity = nullptr;
	return true;
}

NavigationAgentTypeID CCollisionAvoidanceAgent::GetNavigationTypeId() const
{
	return m_pOwningNavigationComponent->GetNavigationTypeId();
}

const INavMeshQueryFilter* CCollisionAvoidanceAgent::GetNavigationQueryFilter() const
{
	return m_pOwningNavigationComponent->GetNavigationQueryFilter();
}

const char* CCollisionAvoidanceAgent::GetDebugName() const
{
	return m_pAttachedEntity->GetName();
}

ETreatType CCollisionAvoidanceAgent::GetTreatmentDuringUpdateTick(SAgentParams& outAgent, SObstacleParams& outObstacle) const
{
	static_assert(int(ETreatType::Count) == 3, "Unexpected enum count!");
	switch (m_pOwningNavigationComponent->GetCollisionAvoidanceProperties().type)
	{
	case CEntityAINavigationComponent::SCollisionAvoidanceProperties::EType::Active:
		// Don't actively avoid collisions when not moving (target destination is probably reached). 
		// Break is omitted here on purpose. 
		if (m_pOwningNavigationComponent->GetRequestedVelocity().GetLengthSquared() > 0.00001f)
		{
			// Set current state
			outAgent.currentLocation = m_pOwningNavigationComponent->GetPosition();
			outAgent.currentVelocity = m_pOwningNavigationComponent->GetVelocity();
			outAgent.desiredVelocity = m_pOwningNavigationComponent->GetRequestedVelocity();

			// Set agent properties
			outAgent.maxSpeed = m_pOwningNavigationComponent->GetMovementProperties().maxSpeed;
			outAgent.maxAcceleration = m_pOwningNavigationComponent->GetMovementProperties().maxAcceleration;
			outAgent.radius = m_pOwningNavigationComponent->GetCollisionAvoidanceProperties().radius;
			outAgent.height = m_pOwningNavigationComponent->GetCollisionAvoidanceProperties().height;

			return ETreatType::Agent;
		}
	case CEntityAINavigationComponent::SCollisionAvoidanceProperties::EType::Passive:
		outObstacle.currentLocation = m_pOwningNavigationComponent->GetPosition();
		outObstacle.currentVelocity = m_pOwningNavigationComponent->GetVelocity();
		outObstacle.radius = m_pOwningNavigationComponent->GetCollisionAvoidanceProperties().radius;
		outObstacle.height = m_pOwningNavigationComponent->GetCollisionAvoidanceProperties().height;
		return ETreatType::Obstacle;
	case CEntityAINavigationComponent::SCollisionAvoidanceProperties::EType::None:
		return ETreatType::None;
	}

	CRY_ASSERT(false, "Unhandled CEntityAINavigationComponent::SCollisionAvoidanceProperties::EType value");
	return ETreatType::None;
}

void CCollisionAvoidanceAgent::ApplyComputedVelocity(const Vec2& avoidanceVelocity, float updateTime)
{
	PathFollowResult result;
	result.velocityOut = Vec3(avoidanceVelocity.x, avoidanceVelocity.y, m_pOwningNavigationComponent->GetRequestedVelocity().z);
	m_pOwningNavigationComponent->NewStateComputed(result, this);
}
