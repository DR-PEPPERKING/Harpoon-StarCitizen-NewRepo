// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IParameterConnection.h>
#include <PoolObject.h>
#include <CryAudio/IAudioInterfacesCommonData.h>

#include <cri_atom_ex.h>

namespace CryAudio
{
namespace Impl
{
namespace Adx2
{
class CAisacControl final : public IParameterConnection, public CPoolObject<CAisacControl, stl::PSyncNone>
{
public:

	CAisacControl() = delete;
	CAisacControl(CAisacControl const&) = delete;
	CAisacControl(CAisacControl&&) = delete;
	CAisacControl& operator=(CAisacControl const&) = delete;
	CAisacControl& operator=(CAisacControl&&) = delete;

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	explicit CAisacControl(CriAtomExAisacControlId const id, char const* const szName)
		: m_id(id)
		, m_name(szName)
	{}
#else
	explicit CAisacControl(CriAtomExAisacControlId const id)
		: m_id(id)
	{}
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

	virtual ~CAisacControl() override = default;

	// IParameterConnection
	virtual void Set(IObject* const pIObject, float const value) override;
	virtual void SetGlobally(float const value) override;
	// ~IParameterConnection

private:

	CriAtomExAisacControlId const m_id;

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	CryFixedStringT<MaxControlNameLength> const m_name;
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
};
} // namespace Adx2
} // namespace Impl
} // namespace CryAudio
