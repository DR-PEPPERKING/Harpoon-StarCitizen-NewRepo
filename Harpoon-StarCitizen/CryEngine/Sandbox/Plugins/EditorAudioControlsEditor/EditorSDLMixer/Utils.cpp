// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Utils.h"

namespace ACE
{
namespace Impl
{
namespace SDLMixer
{
namespace Utils
{
//////////////////////////////////////////////////////////////////////////
ControlId GetId(EItemType const type, string const& name, string const& path, bool const isLocalized)
{
	string const localized = isLocalized ? "localized" : "";
	string const fullName = GetTypeName(type) + localized + "/" + path + "/" + name;
	return CryAudio::StringToId(fullName.c_str());
}

//////////////////////////////////////////////////////////////////////////
string GetTypeName(EItemType const type)
{
	string name = "";

	switch (type)
	{
	case EItemType::Event:
		{
			name = "event:";
			break;
		}
	case EItemType::Folder:
		{
			name = "folder:";
			break;
		}
	default:
		{
			name = "";
			break;
		}
	}

	return name;
}
} // namespace Utils
} // namespace SDLMixer
} // namespace Impl
} // namespace ACE
