// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "WorkQueue.h"

void CWorkQueue::FlushEmpty()
{
	DoNothingOp nothing;
	m_jobQueue.Flush(nothing);
	Empty();
}