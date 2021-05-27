// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "PickObjectTool.h"
#include "Objects/BaseObject.h"
#include "Viewport.h"
#include <IEditor.h>

IMPLEMENT_DYNAMIC(CPickObjectTool, CEditTool)

CPickObjectTool::CPickObjectTool(IPickObjectCallback* callback, CRuntimeClass* targetClass)
{
	assert(callback != 0);
	m_callback = callback;
	m_targetClass = targetClass;
	m_bMultiPick = false;
}

CPickObjectTool::~CPickObjectTool()
{
}

bool CPickObjectTool::MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags)
{
	if (event == eMouseLDown)
	{
		HitContext hitInfo(view);
		view->HitTest(point, hitInfo);
		CBaseObject* obj = hitInfo.object;
		if (obj)
		{
			if (IsRelevant(obj))
			{
				if (m_callback)
				{
					// Can pick this one.
					m_callback->OnPick(obj);
				}
				if (!m_bMultiPick)
				{
					m_callback = 0;
					GetIEditor()->GetLevelEditorSharedState()->SetEditTool(nullptr);
				}
			}
		}
	}
	else if (event == eMouseMove)
	{
		HitContext hitInfo(view);
		view->HitTest(point, hitInfo);
		CBaseObject* obj = hitInfo.object;
		if (obj)
		{
			if (IsRelevant(obj))
			{
				// Set Cursors.
				view->SetCurrentCursor(STD_CURSOR_HIT, obj->GetName());
			}
			else
			{
				view->SetCurrentCursor(STD_CURSOR_DEFAULT, "");
			}
		}
		else
		{
			view->SetCurrentCursor(STD_CURSOR_DEFAULT, "");
		}
	}
	return true;
}

bool CPickObjectTool::OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	if (nChar == Qt::Key_Escape)
	{
		// Cancel selection.
		GetIEditor()->GetLevelEditorSharedState()->SetEditTool(nullptr);
	}
	return false;
}

bool CPickObjectTool::IsRelevant(CBaseObject* obj)
{
	assert(obj != 0);
	if (obj == NULL)
		return false;
	if (!m_callback)
		return false;

	if (!m_targetClass)
	{
		return m_callback->OnPickFilter(obj);
	}
	else
	{
		if (obj->GetRuntimeClass() == m_targetClass || obj->GetRuntimeClass()->IsDerivedFrom(m_targetClass))
		{
			return m_callback->OnPickFilter(obj);
		}
	}
	return false;
}
