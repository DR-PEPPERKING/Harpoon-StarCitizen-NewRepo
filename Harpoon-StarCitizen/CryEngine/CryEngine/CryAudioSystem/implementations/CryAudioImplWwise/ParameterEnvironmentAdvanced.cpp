// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ParameterEnvironmentAdvanced.h"
#include "Object.h"

#include <AK/SoundEngine/Common/AkSoundEngine.h>

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
//////////////////////////////////////////////////////////////////////////
void CParameterEnvironmentAdvanced::Set(IObject* const pIObject, float const amount)
{
	auto const pObject = static_cast<CObject*>(pIObject);
	auto const rtpcValue = static_cast<AkRtpcValue>(m_multiplier * amount + m_shift);

	AK::SoundEngine::SetRTPCValue(m_rtpcId, rtpcValue, pObject->GetId());
}
} // namespace Wwise
} // namespace Impl
} // namespace CryAudio
