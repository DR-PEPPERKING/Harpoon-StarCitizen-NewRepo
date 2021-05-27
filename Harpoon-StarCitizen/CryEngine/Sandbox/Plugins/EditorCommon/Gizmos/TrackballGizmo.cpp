// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TrackballGizmo.h"

#include "Gizmos/AxisHelper.h"
#include "Preferences/SnappingPreferences.h"
#include "IDisplayViewport.h"

#define HIT_RADIUS (8)

CTrackballGizmo::CTrackballGizmo()
	: m_color(1.0f, 1.0f, 0.0f)
	, m_scale(1.0f)
	, m_rotatedX(0.0f)
	, m_rotatedY(0.0f)
{
}

void CTrackballGizmo::SetPosition(Vec3 pos)
{
	m_position = pos;
}

void CTrackballGizmo::SetColor(Vec3 color)
{
	m_color = color;
}

void CTrackballGizmo::SetScale(float scale)
{
	m_scale = scale;
}

void CTrackballGizmo::Display(SDisplayContext& dc)
{
	if (!GetFlag(EGIZMO_INTERACTING) && GetFlag(EGIZMO_HIGHLIGHTED))
	{
		IDisplayViewport* view = dc.view;

		// this could probably go in a "prepare" step, but for now keep it here
		float scale = view->GetScreenScaleFactor(m_position) * gGizmoPreferences.axisGizmoSize * m_scale;

		dc.SetColor(m_color);
		dc.SetAlpha(0.2f);
		// construct a view aligned matrix
		uint32 curstate = dc.GetState();
		dc.DepthTestOff();
		dc.DrawBall(m_position, scale);
		dc.SetState(curstate);
	}
}

bool CTrackballGizmo::MouseCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int nFlags)
{
	if (GetFlag(EGIZMO_INTERACTING))
	{
		switch (event)
		{
		case eMouseMove:
			{
				Vec3 zDir = view->CameraToWorld(m_position);
				zDir.Normalize();
				Vec3 yDir = view->UpViewDirection();
				Vec3 xDir = yDir ^ zDir;
				yDir = zDir ^ xDir;

				// Don't use the gizmo scale for interaction to keep the precision higher
				const float scale = view->GetScreenScaleFactor(m_position) * gGizmoPreferences.axisGizmoSize;

				Vec3 raySrc, rayDir;
				view->ViewToWorldRay(point, raySrc, rayDir);

				Vec3 offset = raySrc + (((m_position - scale * zDir) - raySrc) * zDir) / (zDir * rayDir) * rayDir - m_position;

				float xradians = (offset * xDir - m_initOffset.x) / scale - m_rotatedX;
				float yradians = (offset * yDir - m_initOffset.y) / scale - m_rotatedY;

				if (gSnappingPreferences.angleSnappingEnabled())
				{
					float xangle = RAD2DEG(xradians);
					xangle = gSnappingPreferences.SnapAngle(xangle);
					xradians = DEG2RAD(xangle);

					float yangle = RAD2DEG(yradians);
					yangle = gSnappingPreferences.SnapAngle(yangle);
					yradians = DEG2RAD(yangle);
				}

				m_rotatedX += xradians;
				m_rotatedY += yradians;

				Quat xrot = Quat::CreateRotationAA(-m_rotatedX, yDir);
				Quat yrot = Quat::CreateRotationAA(m_rotatedY, xDir);
				// arbitrary ordering, apply xrotation first, then yrotation
				const AngleAxis totalRotation(yrot * xrot);

				xrot = Quat::CreateRotationAA(-xradians, yDir);
				yrot = Quat::CreateRotationAA(yradians, xDir);
				const AngleAxis deltaRotation(yrot * xrot);


				signalDragging(view, this, totalRotation, deltaRotation, point, nFlags);
				break;
			}

		case eMouseLUp:
			signalEndDrag(view, this, point, nFlags);
			UnsetFlag(EGIZMO_INTERACTING);
			break;
		}
		return true;
	}
	else
	{
		if (event == eMouseLDown)
		{
			SetFlag(EGIZMO_INTERACTING);
			Vec3 zDir = view->CameraToWorld(m_position);
			zDir.Normalize();
			Vec3 yDir = view->UpViewDirection();
			Vec3 xDir = yDir ^ zDir;
			yDir = zDir ^ xDir;

			const float scale = view->GetScreenScaleFactor(m_position) * gGizmoPreferences.axisGizmoSize * m_scale;

			Vec3 raySrc, rayDir;
			view->ViewToWorldRay(point, raySrc, rayDir);

			Vec3 offset = raySrc + (((m_position - scale * zDir) - raySrc) * zDir) / (zDir * rayDir) * rayDir - m_position;
			// offset is in world space, transform it to the local plane space
			m_initOffset.x = offset * xDir;
			m_initOffset.y = offset * yDir;

			m_rotatedX = 0.0f;
			m_rotatedY = 0.0f;

			signalBeginDrag(view, this, point, nFlags);
			return true;
		}
		else
		{
			return false;
		}
	}
}

void CTrackballGizmo::GetWorldBounds(AABB& bbox)
{
	// this needs to be improved
	bbox.min = Vec3(-1000000, -1000000, -1000000);
	bbox.max = Vec3(1000000, 1000000, 1000000);
}

bool CTrackballGizmo::HitTest(HitContext& hc)
{
	IDisplayViewport* view = hc.view;

	float scale = view->GetScreenScaleFactor(m_position) * gGizmoPreferences.axisGizmoSize * m_scale;

	// Ray in world space.
	hc.view->ViewToWorldRay(hc.point2d, hc.raySrc, hc.rayDir);

	// find nearest point to position, check if distance to radius smaller than threshold
	Vec3 dist = hc.raySrc + ((m_position - hc.raySrc) * hc.rayDir) * hc.rayDir - m_position;

	if (dist.GetLengthSquared() < scale * scale)
	{
		hc.dist = 0;
		return true;
	}

	return false;
}
