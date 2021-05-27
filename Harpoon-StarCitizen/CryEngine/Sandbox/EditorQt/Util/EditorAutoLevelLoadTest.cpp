// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EditorAutoLevelLoadTest.h"
#include "IEditorImpl.h"
#include "LogFile.h"

CEditorAutoLevelLoadTest& CEditorAutoLevelLoadTest::Instance()
{
	static CEditorAutoLevelLoadTest levelLoadTest;
	return levelLoadTest;
}

CEditorAutoLevelLoadTest::CEditorAutoLevelLoadTest()
{
	GetIEditorImpl()->RegisterNotifyListener(this);
}

CEditorAutoLevelLoadTest::~CEditorAutoLevelLoadTest()
{
	GetIEditorImpl()->UnregisterNotifyListener(this);
}

void CEditorAutoLevelLoadTest::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnEndSceneOpen:
		CLogFile::WriteLine("[LevelLoadFinished]");
		exit(0);
		break;
	}
}
