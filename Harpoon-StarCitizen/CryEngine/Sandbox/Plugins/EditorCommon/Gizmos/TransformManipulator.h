// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include "AxisHelperExtended.h"
#include "AxisRotateGizmo.h"
#include "AxisScaleGizmo.h"
#include "AxisTranslateGizmo.h"
#include "Gizmo.h"
#include "ITransformManipulator.h"
#include "PlaneScaleGizmo.h"
#include "PlaneTranslateGizmo.h"
#include "TrackballGizmo.h"
#include "UniformScaleGizmo.h"
#include "ViewTranslateGizmo.h"

#include <IEditor.h>


/** Gizmo of Objects animation track.
 */
class EDITOR_COMMON_API CTransformManipulator : public CGizmo, public ITransformManipulator
{
public:
	CTransformManipulator(ITransformManipulatorOwner* owner);
	~CTransformManipulator();

	//////////////////////////////////////////////////////////////////////////
	// Overrides from CGizmo
	//////////////////////////////////////////////////////////////////////////
	virtual void GetWorldBounds(AABB& bbox);
	virtual void Display(SDisplayContext& dc);
	virtual bool HitTest(HitContext& hc);
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	// ITransformManipulator implementation.
	//////////////////////////////////////////////////////////////////////////
	virtual Matrix34 GetTransform() const;
	virtual bool     MouseCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int nFlags);
	virtual void     SetCustomTransform(bool on, const Matrix34& m);

	bool             NeedsSnappingGrid() const override;

	void             BeginMoveDrag(IDisplayViewport* view, CGizmo* gizmo, const Vec3& initialPosition, const CPoint& point, int nFlags);
	void             MoveDragging(IDisplayViewport* view, CGizmo* gizmo, const Vec3& totalMove, const Vec3& deltaMove, const CPoint& point, int nFlags);

	void             BeginRotateDrag(IDisplayViewport* view, CGizmo* gizmo, const CPoint& point, int nFlags);
	void             RotateDragging(IDisplayViewport* view, CGizmo* gizmo, const AngleAxis& totalRotation, const AngleAxis& deltaRotation, const CPoint& point, int nFlags);

	void             BeginScaleDrag(IDisplayViewport* view, CGizmo* gizmo, const CPoint& point, int nFlags);
	void             ScaleDragging(IDisplayViewport* view, CGizmo* gizmo, float scaleTotal, float scaleDelta, const CPoint& point, int nFlags);

	void             EndDrag(IDisplayViewport* view, CGizmo* gizmo, const CPoint& point, int nFlags);

	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	void         SetWorldBounds(const AABB& bbox);

	virtual void SetHighlighted(bool highlighted) override;

	virtual void Invalidate() override { SetFlag(EGIZMO_INVALID); }

private:
	enum EManipulatorMode
	{
		MANIPULATOR_MODE_NONE = -1,

		MOVE_MODE             = 0,
		SCALE_MODE            = 1,
		ROTATE_MODE           = 2,
		SELECT_MODE           = 3,
		ROTATE_CIRCLE_MODE    = 4,

		MAX_MANIPULATOR_MODES = 5
	};

	void SetMatrix(const Matrix34&);
	void SetUpGizmos();
	//! update gizmo position and orientation
	void OnUpdateState();
	void UpdateGizmos();
	void UpdateTransform();
	//! update gizmo colors when for example, setting axis constraints
	void UpdateGizmoColors();
	bool HitTest(HitContext& hc, EManipulatorMode& manipulatorMode);

	void DisplayPivotPoint(SDisplayContext& dc);

	AABB                m_bbox;
	CAxisHelperExtended m_pAxisHelperExtended;

	bool                m_bDragging;
	CPoint              m_cMouseDownPos;
	Vec3                m_initPos;

	Matrix34            m_matrix;
	Matrix33            m_initMatrix;
	Matrix33            m_initMatrixInverse;

	bool                m_bUseCustomTransform;

	bool                m_bSkipConstraintColor;

	//! axis movement gizmos
	CAxisTranslateGizmo m_xMoveAxis;
	CAxisTranslateGizmo m_yMoveAxis;
	CAxisTranslateGizmo m_zMoveAxis;

	//! plane movement gizmos
	CPlaneTranslateGizmo m_xyMovePlane;
	CPlaneTranslateGizmo m_yzMovePlane;
	CPlaneTranslateGizmo m_zxMovePlane;

	//! central circle gizmo, its behavior depends on mode (rotation/move/scale)
	CViewTranslateGizmo m_viewMoveGizmo;

	//! rotation wheel gizmos
	CAxisRotateGizmo m_xWheelGizmo;
	CAxisRotateGizmo m_yWheelGizmo;
	CAxisRotateGizmo m_zWheelGizmo;

	//! Half axis rotation helpers
	CAxisTranslateGizmo m_xRotateAxis;
	CAxisTranslateGizmo m_yRotateAxis;
	CAxisTranslateGizmo m_zRotateAxis;

	//! circle gizmo for view space rotation
	CAxisRotateGizmo m_viewRotationGizmo;
	CTrackballGizmo  m_trackballGizmo;

	//! axis movement gizmos
	CAxisScaleGizmo    m_xScaleAxis;
	CAxisScaleGizmo    m_yScaleAxis;
	CAxisScaleGizmo    m_zScaleAxis;

	CPlaneScaleGizmo   m_xyScalePlane;
	CPlaneScaleGizmo   m_yzScalePlane;
	CPlaneScaleGizmo   m_zxScalePlane;

	CUniformScaleGizmo m_xyzScaleGizmo;

	CGizmo*            m_lastHitGizmo;
	CGizmo*            m_highlightedGizmo;

	// owner of this manipulator object. When the manipulator has been tagged as invalid callbacks of the owner
	//  will be called on next draw to update the manipulator
	ITransformManipulatorOwner* m_owner;
};
