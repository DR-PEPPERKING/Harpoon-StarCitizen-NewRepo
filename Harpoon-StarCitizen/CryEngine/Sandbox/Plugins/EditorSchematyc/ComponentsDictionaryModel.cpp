// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ComponentsDictionaryModel.h"

#include "ScriptBrowserUtils.h"

#include <CrySchematyc/Reflection/TypeDesc.h>

#include <CrySchematyc/Script/IScriptView.h>
#include <CrySchematyc/Script/IScriptRegistry.h>
#include <CrySchematyc/Script/Elements/IScriptComponentInstance.h>

#include <CrySchematyc/Env/IEnvRegistry.h>
#include <CrySchematyc/Env/IEnvElement.h>
#include <CrySchematyc/Env/Elements/IEnvComponent.h>

namespace CrySchematycEditor {

CComponentDictionaryEntry::CComponentDictionaryEntry()
{
}

CComponentDictionaryEntry::~CComponentDictionaryEntry()
{

}

QVariant CComponentDictionaryEntry::GetColumnValue(int32 columnIndex) const
{
	switch (columnIndex)
	{
	case CComponentsDictionary::Column_Name:
		return QVariant::fromValue(m_name);
	default:
		break;
	}

	return QVariant();
}

QString CComponentDictionaryEntry::GetToolTip(int32 columnIndex) const
{
	return m_description;
}

CComponentsDictionary::CComponentsDictionary(const Schematyc::IScriptElement* pScriptScope)
	: m_pScriptScope(nullptr)
{
	if (pScriptScope)
		Load(pScriptScope);
}

CComponentsDictionary::~CComponentsDictionary()
{

}

void CComponentsDictionary::ResetEntries()
{
	if (m_pScriptScope)
	{
		m_components.reserve(100);

		bool bAttach = false;

		const Schematyc::IScriptComponentInstance* pScriptComponentInstance = Schematyc::DynamicCast<Schematyc::IScriptComponentInstance>(m_pScriptScope);
		if (pScriptComponentInstance)
		{
			Schematyc::IEnvRegistry& registry = gEnv->pSchematyc->GetEnvRegistry();
			const Schematyc::IEnvComponent* pEnvComponent = registry.GetComponent(pScriptComponentInstance->GetTypeGUID());
			if (pEnvComponent)
			{
				if (pEnvComponent->GetDesc().GetComponentFlags().Check(IEntityComponent::EFlags::Socket))
				{
					bAttach = true;
				}
				else
				{
					return;
				}
			}
		}

		Schematyc::IScriptViewPtr pScriptView = gEnv->pSchematyc->CreateScriptView(m_pScriptScope->GetGUID());

		VectorSet<CryGUID> singletonExclusions;
		auto visitScriptComponentInstance = [this, &singletonExclusions](const Schematyc::IScriptComponentInstance& scriptComponentInstance) -> Schematyc::EVisitStatus
		{
			singletonExclusions.insert(scriptComponentInstance.GetTypeGUID());
			return Schematyc::EVisitStatus::Continue;
		};
		pScriptView->VisitScriptComponentInstances(visitScriptComponentInstance, Schematyc::EDomainScope::Derived);

		auto visitEnvComponentFactory = [this, bAttach, &singletonExclusions, &pScriptView](const Schematyc::IEnvComponent& envComponent) -> Schematyc::EVisitStatus
		{
			auto componentFlags = envComponent.GetDesc().GetComponentFlags();
			if (!bAttach || componentFlags.Check(IEntityComponent::EFlags::Attach))
			{
				const CryGUID envComponentGUID = envComponent.GetGUID();
				if (!componentFlags.Check(IEntityComponent::EFlags::Singleton) || (singletonExclusions.find(envComponentGUID) == singletonExclusions.end()))
				{
					Schematyc::CStackString fullName;
					pScriptView->QualifyName(envComponent, fullName);

					CComponentDictionaryEntry entry;
					entry.m_identifier = envComponentGUID;
					entry.m_name = envComponent.GetName();
					entry.m_fullName = fullName.c_str();
					entry.m_description = envComponent.GetDescription();

					m_components.emplace_back(entry);
				}
			}
			return Schematyc::EVisitStatus::Continue;
		};
		pScriptView->VisitEnvComponents(visitEnvComponentFactory);
	}
}

const CAbstractDictionaryEntry* CComponentsDictionary::GetEntry(int32 index) const
{
	if (index < m_components.size())
	{
		return &m_components[index];
	}

	return nullptr;
}

QString CComponentsDictionary::GetColumnName(int32 index) const
{
	switch (index)
	{
	case Column_Name:
		return QString("Name");
	default:
		break;
	}

	return QString();
}

void CComponentsDictionary::Load(const Schematyc::IScriptElement* pScriptScope)
{
	m_pScriptScope = pScriptScope;
	Reset();
	m_pScriptScope = nullptr;
}

}
