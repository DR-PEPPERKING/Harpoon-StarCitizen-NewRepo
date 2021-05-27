// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "EditorCommonAPI.h"
#include "Gizmo.h"
#include <CrySandbox/CrySignal.h>

//////////////////////////////////////////////////////////////////////////
// Axis Gizmo.
//
// Interacts with an axis by constrained movement along that axis
//////////////////////////////////////////////////////////////////////////
class EDITOR_COMMON_API CAxisTranslateGizmo : public CGizmo
{
public:
	CAxisTranslateGizmo(bool bDrawArrowTip = true);

	//! set position - should be world space
	void         SetPosition(Vec3 pos);
	//! set direction - should be world space
	void         SetDirection(Vec3 dir);
	//! set rgb color of the gizmo
	void         SetColor(Vec3 color);
	//! set offset from central position as factor of the gizmo length
	void         SetOffset(float offset);
	//! set unique scale of the gizmo
	void         SetScale(float scale);

	virtual void Display(SDisplayContext& dc) override;

	virtual bool MouseCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int nFlags) override;

	virtual void GetWorldBounds(AABB& bbox) override;

	virtual bool HitTest(HitContext& hc) override;

	// emitted when user starts dragging the gizmo
	CCrySignal<void (IDisplayViewport* view, CGizmo* gizmo, const Vec3& initialPosition, const CPoint& point, int nFlags)> signalBeginDrag;

	// emitted while dragging.
	CCrySignal<void (IDisplayViewport* view, CGizmo* gizmo, const Vec3& totalOffset, const Vec3& deltaOffset, const CPoint& point, int nFlags)> signalDragging;

	// emitted when finished dragging
	CCrySignal<void (IDisplayViewport* view, CGizmo* gizmo, const CPoint& point, int nFlags)> signalEndDrag;

private:
	void DrawArrow(SDisplayContext& dc, Vec3 position, Vec3 direction);

	// position and direction in world space
	Vec3   m_position;
	Vec3   m_direction;
	Vec3   m_color;

	//! custom scale of widget - final size is calculated from z-distance of widget to camera and global parameters
	float m_scale;
	//! offset from position along the axis (as widget length factor but will also be scaled before use)
	float m_offset;

	float m_lastScale;

	//! data kept during interaction
	Vec3 m_initPosition;
	Vec3 m_initDirection;
	Vec3 m_initOffset;

	Vec3 m_interactionOffset;

	bool m_bDrawArrowTip;
};
