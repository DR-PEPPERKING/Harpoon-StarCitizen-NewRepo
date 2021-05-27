// Copyright 2015-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FeatureCommon.h"

namespace pfx2
{

//////////////////////////////////////////////////////////////////////////
class CFeatureComment : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_text, "Text", "Text");
	}

private:
	string m_text;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureComment, "General", "Comment", colorGeneral);

//////////////////////////////////////////////////////////////////////////
// CFeatureComponentEnableIf

class CFeatureComponentEnableIf : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

public:
	virtual bool CanMakeRuntime(CParticleEmitter* pEmitter) const override
	{
		return !pEmitter || m_attribute.GetValueAs(pEmitter->GetAttributeInstance(), true);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_attribute, "Attribute", "Attribute");
	}

private:
	CAttributeReference m_attribute;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureComponentEnableIf, "Component", "EnableIf", colorComponent);

//////////////////////////////////////////////////////////////////////////
// CFeatureComponentActivateIf

class CFeatureComponentActivateIf : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

public:
	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->CullSpawners.add(this);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_attribute, "Attribute", "Attribute");
	}

	virtual void CullSpawners(CParticleComponentRuntime& runtime, TVarArray<SSpawnerDesc>& spawners) override
	{
		if (!m_attribute.GetValueAs(runtime.GetEmitter()->GetAttributeInstance(), true))
			spawners.resize(0);
	}

private:
	CAttributeReference m_attribute;
};

CRY_PFX2_LEGACY_FEATURE(CFeatureComponentActivateIf, "Component", "SpawnIf");
CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureComponentActivateIf, "Component", "ActivateIf", colorComponent);

//////////////////////////////////////////////////////////////////////////
// CFeatureComponentActivateRandom

class CFeatureComponentActivateRandom : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

public:
	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override
	{
		pComponent->CullSpawners.add(this);
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		SERIALIZE_VAR(ar, m_probability);
		SERIALIZE_VAR(ar, m_group);
		if (m_group > 0)
			SERIALIZE_VAR(ar, m_selectionStart);
	}

	virtual void CullSpawners(CParticleComponentRuntime& runtime, TVarArray<SSpawnerDesc>& spawners) override
	{
		if (m_probability == 1.0f)
			return;
		uint32 groupKey = runtime.GetEmitter()->GetCurrentSeed() ^ m_group;
		uint newCount = 0;
		for (const auto& spawner : spawners)
		{
			if (m_group > 0)
			{
				// Same random number for all components in group
				SChaosKey spawnerKey(groupKey ^ spawner.m_parentId);
				float select = spawnerKey.RandUNorm();
				if (select < m_selectionStart || select > m_selectionStart + m_probability)
					continue;
			}
			else
			{
				// New random number for each component and spawner
				if (runtime.Chaos().RandUNorm() > m_probability)
					continue;
			}
			spawners[newCount++] = spawner;
		}
		spawners.resize(newCount);
	}

private:

	UUnitFloat                m_probability    = 1.0f;
	TValue<TDefaultMin<uint>> m_group;
	UUnitFloat                m_selectionStart = 0.0f;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureComponentActivateRandom, "Component", "ActivateRandom", colorComponent);

//////////////////////////////////////////////////////////////////////////
// CFeatureComponentEnableByConfig

SERIALIZATION_DECLARE_ENUM(EEnableIfConfig,
	Low      = CONFIG_LOW_SPEC,
	Medium   = CONFIG_MEDIUM_SPEC,
	High     = CONFIG_HIGH_SPEC,
	VeryHigh = CONFIG_VERYHIGH_SPEC
	)

class CFeatureComponentEnableByConfig : public CParticleFeature
{
public:
	CRY_PFX2_DECLARE_FEATURE

public:
	CFeatureComponentEnableByConfig()
		: m_minimumConfig(EEnableIfConfig::Low)
		, m_maximumConfig(EEnableIfConfig::VeryHigh)
		, m_PC(true), m_XBoxOne(true), m_PS4(true) {}

	virtual bool CanMakeRuntime(CParticleEmitter* pEmitter) const override
	{
		CRY_PFX2_PROFILE_DETAIL;

		const uint particleSpec = pEmitter ? pEmitter->GetParticleSpec() : GetPSystem()->GetParticleSpec();
		const bool isPc = particleSpec <= CONFIG_VERYHIGH_SPEC;

		if (isPc)
		{
			return m_PC &&
				(particleSpec >= uint(m_minimumConfig)) &&
				(particleSpec <= uint(m_maximumConfig));
		}
		else if ((particleSpec == CONFIG_DURANGO || particleSpec == CONFIG_DURANGO_X) && m_XBoxOne)
			return true;
		else if ((particleSpec == CONFIG_ORBIS || particleSpec == CONFIG_ORBIS_NEO) && m_PS4)
			return true;

		return false;
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		CParticleFeature::Serialize(ar);
		ar(m_PC, "PC", "PC");
		if (m_PC)
		{
			ar(m_minimumConfig, "Minimum", "Minimum");
			ar(m_maximumConfig, "Maximum", "Maximum");
		}
		ar(m_XBoxOne, "XBoxOne", "Xbox One");
		ar(m_PS4, "PS4", "Playstation 4");
	}

private:
	EEnableIfConfig m_minimumConfig;
	EEnableIfConfig m_maximumConfig;
	bool m_PC, m_XBoxOne, m_PS4;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureComponentEnableByConfig, "Component", "EnableByConfig", colorComponent);

}
