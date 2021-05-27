#include "StdAfx.h"
#include "RoomscaleCamera.h"

#include <CryRenderer/IRenderAuxGeom.h>
#include <array>

namespace Cry
{
	namespace DefaultComponents
	{
		namespace VirtualReality
		{
			void CRoomscaleCameraComponent::Register(Schematyc::CEnvRegistrationScope& componentScope)
			{
				// Functions
				{
					auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CRoomscaleCameraComponent::Activate, "{4A595550-254B-4A5F-BFF6-7A08AAC317CE}"_cry_guid, "Activate");
					pFunction->SetDescription("Makes this camera active");
					pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
					componentScope.Register(pFunction);
				}
				{
					auto pFunction = SCHEMATYC_MAKE_ENV_FUNCTION(&CRoomscaleCameraComponent::IsActive, "{4C1647E1-49C0-4D48-A7E0-B71B7EB1DE60}"_cry_guid, "IsActive");
					pFunction->SetDescription("Is this camera active?");
					pFunction->SetFlags(Schematyc::EEnvFunctionFlags::Construction);
					pFunction->BindOutput(0, 'iact', "IsActive");
					componentScope.Register(pFunction);
				}
			}

			CRoomscaleCameraComponent::CRoomscaleCameraComponent()
			{
				m_pCameraManager = gEnv->pSystem->GetIPluginManager()->QueryPlugin<IPlugin_CryDefaultEntities>()->GetICameraManager();
			}

			void CRoomscaleCameraComponent::Initialize()
			{
				m_pCameraManager->AddCamera(this);

				if (m_bActivateOnCreate)
				{
					Activate();
				}
			}

			void CRoomscaleCameraComponent::ProcessEvent(const SEntityEvent& event)
			{
				if (event.event == ENTITY_EVENT_UPDATE)
				{
					const CCamera& systemCamera = gEnv->pSystem->GetViewCamera();

					if (IHmdDevice* pDevice = gEnv->pSystem->GetHmdManager()->GetHmdDevice())
					{
						const auto& worldTranform = m_pEntity->GetWorldTM();
						pDevice->EnableLateCameraInjectionForCurrentFrame(gEnv->pRenderer->GetFrameID(), std::make_pair(Quat(worldTranform), worldTranform.GetTranslation()));

						const HmdTrackingState& state = pDevice->GetLocalTrackingState();
						m_camera.SetMatrix(worldTranform * Matrix34::Create(Vec3(1.f), state.pose.orientation, state.pose.position));
					}
					else
					{
						m_camera.SetMatrix(m_pEntity->GetWorldTM() * Matrix34::Create(Vec3(1.f), IDENTITY, m_pEntity->GetWorldRotation().GetInverted() * Vec3(0, 0, 1.7f)));
					}

					m_camera.SetFrustum(systemCamera.GetViewSurfaceX(), systemCamera.GetViewSurfaceZ(), (75.0_degrees).ToRadians(), m_nearPlane, m_farPlane, systemCamera.GetPixelAspectRatio());

					gEnv->pSystem->SetViewCamera(m_camera);
				}
				else if (event.event == ENTITY_EVENT_START_GAME || event.event == ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED)
				{
					if (m_bActivateOnCreate && !IsActive())
					{
						Activate();
					}
				}
			}

			Cry::Entity::EventFlags CRoomscaleCameraComponent::GetEventMask() const
			{
				Cry::Entity::EventFlags bitFlags = IsActive() ? ENTITY_EVENT_UPDATE : Cry::Entity::EventFlags();
				bitFlags |= ENTITY_EVENT_START_GAME | ENTITY_EVENT_COMPONENT_PROPERTY_CHANGED;

				return bitFlags;
			}

			void CRoomscaleCameraComponent::OnShutDown()
			{
				m_pCameraManager->RemoveCamera(this);
			}

#ifndef RELEASE
			void CRoomscaleCameraComponent::Render(const IEntity& entity, const IEntityComponent& component, SEntityPreviewContext &context) const
			{
				if (context.bSelected)
				{
					Matrix34 slotTransform = GetWorldTransformMatrix();

					// Don't use actual far plane as it's usually huge for cameras
					float distance = 10.f;
					float size = distance * tan((45.0_degrees).ToRadians());

					std::array<Vec3, 4> points =
					{
						{
							Vec3(size, distance, size),
							Vec3(-size, distance, size),
							Vec3(-size, distance, -size),
							Vec3(size, distance, -size)
						}
					};

					gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(slotTransform.GetTranslation(), context.debugDrawInfo.color, slotTransform.TransformPoint(points[0]), context.debugDrawInfo.color);
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(slotTransform.GetTranslation(), context.debugDrawInfo.color, slotTransform.TransformPoint(points[1]), context.debugDrawInfo.color);
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(slotTransform.GetTranslation(), context.debugDrawInfo.color, slotTransform.TransformPoint(points[2]), context.debugDrawInfo.color);
					gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(slotTransform.GetTranslation(), context.debugDrawInfo.color, slotTransform.TransformPoint(points[3]), context.debugDrawInfo.color);

					Vec3 p1 = slotTransform.TransformPoint(points[0]);
					Vec3 p2;
					for (int i = 0; i < points.size(); i++)
					{
						int j = (i + 1) % points.size();
						p2 = slotTransform.TransformPoint(points[j]);
						gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(p1, context.debugDrawInfo.color, p2, context.debugDrawInfo.color);
						p1 = p2;
					}
				}
			}
#endif
		}
	}
}