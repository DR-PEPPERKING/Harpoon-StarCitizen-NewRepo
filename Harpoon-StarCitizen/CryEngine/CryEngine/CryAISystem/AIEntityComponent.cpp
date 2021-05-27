// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AIEntityComponent.h"

CAIEntityComponent::~CAIEntityComponent()
{
	m_pEntity->SetFlags(m_pEntity->GetFlags() & ~ENTITY_FLAG_HAS_AI);
	m_pEntity->SetAIObjectID(0);

	if (gEnv->pAISystem)
	{
		if (m_objectReference.GetAIObject() != nullptr)
		{
			gEnv->pAISystem->GetAIObjectManager()->RemoveObject(GetAIObjectID());
		}
	}
}

void CAIEntityComponent::Initialize()
{
	m_pEntity->SetFlags(m_pEntity->GetFlags() | ENTITY_FLAG_HAS_AI);

	m_objectReference.GetAIObject()->SetPos(m_pEntity->GetWorldPos(), m_pEntity->GetForwardDir());
	m_pEntity->SetAIObjectID(m_objectReference.GetObjectID());
}

void CAIEntityComponent::ProcessEvent(const SEntityEvent& event)
{
	switch (event.event)
	{
		case ENTITY_EVENT_XFORM:
		{
			if (CAIObject* pAIObject = m_objectReference.GetAIObject())
			{
				pAIObject->SetPos(m_pEntity->GetWorldPos(), m_pEntity->GetForwardDir());
			}
		}
		break;
		case ENTITY_EVENT_SET_NAME:
		{
			if (CAIObject* pAIObject = m_objectReference.GetAIObject())
			{
				pAIObject->SetName(m_pEntity->GetName());
			}
		}
		break;
		case ENTITY_EVENT_ACTIVATED:
		{
			if (CAIObject* pAIObject = m_objectReference.GetAIObject())
			{
				if (IAIActorProxy* pProxy = pAIObject->GetProxy())
				{
					pProxy->EnableUpdate(true);
				}
			}
		}
		break;
		case ENTITY_EVENT_DEACTIVATED:
		{
			if (CAIObject* pAIObject = m_objectReference.GetAIObject())
			{
				if (IAIActorProxy* pProxy = pAIObject->GetProxy())
				{
					pProxy->EnableUpdate(false);
				}
			}
		}
		break;
	}

	if (CAIObject* pAIObject = m_objectReference.GetAIObject())
	{
		pAIObject->EntityEvent(event);
	}
}

Cry::Entity::EventFlags CAIEntityComponent::GetEventMask() const
{
	return ENTITY_EVENT_XFORM | ENTITY_EVENT_SET_NAME | ENTITY_EVENT_ACTIVATED | ENTITY_EVENT_DEACTIVATED 
		| ENTITY_EVENT_DONE | ENTITY_EVENT_ATTACH_THIS | ENTITY_EVENT_DETACH_THIS | ENTITY_EVENT_ENABLE_PHYSICS;
}