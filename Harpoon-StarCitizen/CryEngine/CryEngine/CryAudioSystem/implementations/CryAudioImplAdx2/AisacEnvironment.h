// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IEnvironmentConnection.h>
#include <PoolObject.h>
#include <CryAudio/IAudioInterfacesCommonData.h>

#include <cri_atom_ex.h>

namespace CryAudio
{
namespace Impl
{
namespace Adx2
{
class CAisacEnvironment final : public IEnvironmentConnection, public CPoolObject<CAisacEnvironment, stl::PSyncNone>
{
public:

	CAisacEnvironment() = delete;
	CAisacEnvironment(CAisacEnvironment const&) = delete;
	CAisacEnvironment(CAisacEnvironment&&) = delete;
	CAisacEnvironment& operator=(CAisacEnvironment const&) = delete;
	CAisacEnvironment& operator=(CAisacEnvironment&&) = delete;

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	explicit CAisacEnvironment(CriAtomExAisacControlId const id, char const* const szName)
		: m_id(id)
		, m_name(szName)
	{}
#else
	explicit CAisacEnvironment(
		CriAtomExAisacControlId const id)
		: m_id(id)
	{}
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

	virtual ~CAisacEnvironment() override = default;

	// IEnvironmentConnection
	virtual void Set(IObject* const pIObject, float const amount) override;
	// ~IEnvironmentConnection

private:

	CriAtomExAisacControlId const m_id;

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	CryFixedStringT<MaxControlNameLength> const m_name;
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
};
} // namespace Adx2
} // namespace Impl
} // namespace CryAudio
