// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Category.h"

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	#include "Object.h"
	#include <Logger.h>
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

#include <cri_atom_ex.h>

namespace CryAudio
{
namespace Impl
{
namespace Adx2
{
//////////////////////////////////////////////////////////////////////////
void CCategory::Set(IObject* const pIObject, float const value)
{
	SetGlobally(value);

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	auto const pObject = static_cast<CObject const*>(pIObject);
	Cry::Audio::Log(ELogType::Warning, R"(Adx2 - Category "%s" was set to %f on object "%s". Consider setting it globally.)", m_name.c_str(), value, pObject->GetName());
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CCategory::SetGlobally(float const value)
{
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	criAtomExCategory_SetVolumeByName(static_cast<CriChar8 const*>(m_name.c_str()), static_cast<CriFloat32>(value));
	g_categoryValues[m_name] = value;
#else
	criAtomExCategory_SetVolumeByName(static_cast<CriChar8 const*>(m_name.c_str()), static_cast<CriFloat32>(value));
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
}
} // namespace Adx2
} // namespace Impl
} // namespace CryAudio
