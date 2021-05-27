// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common.h"
#include <IParameterConnection.h>
#include <PoolObject.h>
#include <CryAudio/IAudioInterfacesCommonData.h>

namespace CryAudio
{
namespace Impl
{
namespace SDL_mixer
{
class CVolumeParameter final : public IParameterConnection, public CPoolObject<CVolumeParameter, stl::PSyncNone>
{
public:

	CVolumeParameter() = delete;
	CVolumeParameter(CVolumeParameter const&) = delete;
	CVolumeParameter(CVolumeParameter&&) = delete;
	CVolumeParameter& operator=(CVolumeParameter const&) = delete;
	CVolumeParameter& operator=(CVolumeParameter&&) = delete;

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
	explicit CVolumeParameter(SampleId const sampleId, char const* const szName)
		: m_sampleId(sampleId)
		, m_name(szName)
	{}
#else
	explicit CVolumeParameter(SampleId const sampleId)
		: m_sampleId(sampleId)
	{}
#endif  // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE

	virtual ~CVolumeParameter() override = default;

	// IParameterConnection
	virtual void Set(IObject* const pIObject, float const value) override;
	virtual void SetGlobally(float const value) override;
	// ~IParameterConnection

private:

	SampleId const m_sampleId;

#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
	CryFixedStringT<MaxControlNameLength> const m_name;
#endif  // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE
};
} // namespace SDL_mixer
} // namespace Impl
} // namespace CryAudio