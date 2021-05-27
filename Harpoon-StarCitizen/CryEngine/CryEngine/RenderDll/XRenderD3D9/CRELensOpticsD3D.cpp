// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "../Common/RendElements/OpticsElement.h"
#include "../Common/RendElements/RootOpticsElement.h"
#include "../Common/RendElements/FlareSoftOcclusionQuery.h"
#include "../Common/RendElements/OpticsPredef.hpp"
#include <Cry3DEngine/I3DEngine.h>

#include "GraphicsPipeline/LensOptics.h"

CRELensOptics::CRELensOptics(void)
{
	mfSetType(eDATA_LensOptics);
	mfUpdateFlags(FCEF_TRANSFORM);
}
