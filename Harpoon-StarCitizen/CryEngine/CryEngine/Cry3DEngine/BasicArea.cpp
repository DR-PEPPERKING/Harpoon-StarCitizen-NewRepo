// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

CBasicArea::~CBasicArea()
{
	delete m_pObjectsTree;
	m_pObjectsTree = NULL;
}
