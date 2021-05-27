// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Parameter.h"
#include "Object.h"

#include <AK/SoundEngine/Common/AkSoundEngine.h>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
//////////////////////////////////////////////////////////////////////////
void CParameter::Set(IObject* const pIObject, float const value)
{
	auto const pObject = static_cast<CObject const*>(pIObject);
	AK::SoundEngine::SetRTPCValue(m_id, static_cast<AkRtpcValue>(value), pObject->GetId());
}

//////////////////////////////////////////////////////////////////////////
void CParameter::SetGlobally(float const value)
{
	AK::SoundEngine::SetRTPCValue(m_id, static_cast<AkRtpcValue>(value), AK_INVALID_GAME_OBJECT);
}
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio
