// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "VolumeParameter.h"
#include "Object.h"

namespace CryAudio
{
namespace Impl
{
namespace SDL_mixer
{
//////////////////////////////////////////////////////////////////////////
void CVolumeParameter::Set(IObject* const pIObject, float const value)
{
	auto const pObject = static_cast<CObject*>(pIObject);
	pObject->SetVolume(m_sampleId, crymath::clamp(value, 0.0f, 1.0f));
}

//////////////////////////////////////////////////////////////////////////
void CVolumeParameter::SetGlobally(float const value)
{
	float const parameterValue = crymath::clamp(value, 0.0f, 1.0f);

	for (auto const pObject : g_objects)
	{
		pObject->SetVolume(m_sampleId, parameterValue);
	}
}
} // namespace SDL_mixer
} // namespace Impl
} // namespace CryAudio
