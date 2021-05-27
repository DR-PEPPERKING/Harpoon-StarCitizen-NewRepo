// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ParticleEffect.h"
#include "ParticleEnviron.h"
#include "ParticleComponentRuntime.h"
#include "ParticleAttributes.h"
#include <CryRenderer/IGpuParticles.h>
#include <Cry3DEngine/GeomRef.h>

namespace pfx2
{

class CParticleEmitter final : public IParticleEmitter, public Cry3DEngineBase
{
public:
	CParticleEmitter(CParticleEffect* pEffect, uint emitterId);
	~CParticleEmitter();

	using TRuntimes = TSmartArray<CParticleComponentRuntime>;

	// IRenderNode
	virtual EERType          GetRenderNodeType() const override { return eERType_ParticleEmitter; }
	virtual const char*      GetEntityClassName() const override;
	virtual const char*      GetName() const override;
	virtual void             SetMatrix(const Matrix34& mat) override;
	virtual Vec3             GetPos(bool bWorldOnly = true) const override;
	virtual void             Render(const struct SRendParams& rParam, const SRenderingPassInfo& passInfo) override;
	virtual IPhysicalEntity* GetPhysics() const override                   { return nullptr; }
	virtual void             SetPhysics(IPhysicalEntity*) override         {}
	virtual void             SetMaterial(IMaterial* pMat) override         {}
	virtual IMaterial*       GetMaterial(Vec3* pHitPos = 0) const override { return nullptr; }
	virtual IMaterial*       GetMaterialOverride() const override          { return nullptr; }
	virtual float            GetMaxViewDist() const override               { return m_maxViewDist; }
	virtual void             Precache() override                           {}
	virtual void             GetMemoryUsage(ICrySizer* pSizer) const override;
	virtual const AABB       GetBBox() const                      override { return m_bounds.IsReset() ? AABB(m_location.t, 0.05f) : m_bounds; }
	virtual void             FillBBox(AABB& aabb) const           override { aabb = GetBBox(); }
	virtual void             SetBBox(const AABB& WSBBox)          override {}
	virtual void             OffsetPosition(const Vec3& delta)    override {}
	virtual bool             IsAllocatedOutsideOf3DEngineDLL()    override { return false; }
	virtual void             SetViewDistRatio(int nViewDistRatio) override;
	virtual void             ReleaseNode(bool bImmediate) override;
	virtual void             SetOwnerEntity(IEntity* pEntity) override     { SetEntity(pEntity, m_entitySlot); }
	virtual IEntity*         GetOwnerEntity() const override               { return m_entityOwner; }
	virtual void             UpdateStreamingPriority(const SUpdateStreamingPriorityContext& streamingContext) override;

	// pfx1 CPArticleEmitter
	virtual int                    GetVersion() const override                        { return 2; }
	virtual bool                   IsAlive() const override                           { return m_alive; }
	virtual bool                   IsActive() const;
	virtual void                   Activate(bool activate) override;
	virtual void                   Kill() override;
	virtual void                   Restart() override;
	virtual void                   SetEffect(const ::IParticleEffect* pEffect) override {}
	virtual const ::IParticleEffect* GetEffect() const override                       { return m_pEffectOriginal; }
	virtual void                   SetEmitGeom(const GeomRef& geom) override;
	virtual void                   SetSpawnParams(const SpawnParams& spawnParams) override;
	virtual void                   GetSpawnParams(SpawnParams& sp) const override;
	using                          IParticleEmitter::GetSpawnParams;
	virtual void                   SetEntity(IEntity* pEntity, int nSlot) override;
	virtual void                   InvalidateCachedEntityData() override;
	virtual uint                   GetAttachedEntityId() override;
	virtual int                    GetAttachedEntitySlot() override                   { return m_entitySlot; }
	virtual void                   SetLocation(const QuatTS& loc) override;
	virtual QuatTS                 GetLocation() const override                       { return m_location; }
	virtual void                   SetTarget(const ParticleTarget& target) override;
	virtual void                   Update() override                                  { UpdateState(); }
	virtual void                   EmitParticle(const EmitParticleData* pData = NULL)  override;

	// pfx2 IParticleEmitter
	virtual IParticleAttributes& GetAttributes()  override { return m_attributeInstance; }
	virtual void                 SetEmitterFeatures(TParticleFeatures& features) override;

	// CParticleEmitter
	void                      InitSeed();
	void                      DebugRender(const SRenderingPassInfo& passInfo) const;
	void                      CheckUpdated();
	bool                      UpdateState();
	bool                      UpdateParticles();
	void                      SyncUpdateParticles();
	void                      PostUpdate();
	void                      RenderDeferred(const SRenderContext& renderContext);
	CParticleContainer&       GetParentContainer()         { return m_parentContainer; }
	const CParticleContainer& GetParentContainer() const   { return m_parentContainer; }
	const TRuntimes&          GetRuntimes() const          { return m_runtimes; }
	const TRuntimes&          GetRuntimesPreUpdate() const { return m_runtimesPreUpdate; }
	const TRuntimes&          GetRuntimesDeferred() const  { return m_runtimesDeferred; }
	CParticleComponentRuntime*GetRuntimeFor(const CParticleComponent* pComponent) { return m_runtimesFor[pComponent->GetComponentId()]; }
	bool                      HasRuntimes() const          { return !m_runtimesFor.empty(); }
	const CParticleEffect*    GetCEffect() const           { return m_pEffect; }
	CParticleEffect*          GetCEffect()                 { return m_pEffect; }
	void                      Register();
	void                      Unregister();
	void                      Clear();
	void                      UpdateRuntimes();
	void                      UpdateEmitGeomFromEntity();
	void                      Reregister()                 { m_reRegister = true; }
	float                     ComputeMaxViewDist() const;
	const SVisEnviron&        GetVisEnv() const            { return m_visEnviron; }
	const SPhysEnviron&       GetPhysicsEnv() const        { return m_physEnviron; }
	void                      UpdatePhysEnv();
	const SpawnParams&        GetSpawnParams() const       { return m_spawnParams; }
	const GeomRef&            GetEmitterGeometry() const   { return m_emitterGeometry; }
	QuatTS                    GetEmitterGeometryLocation() const;
	const CAttributeInstance& GetAttributeInstance() const { return m_attributeInstance; }
	TParticleFeatures&        GetFeatures()                { return m_emitterFeatures; }
	const ParticleTarget&     GetTarget() const            { return m_target; }
	float                     GetViewDistRatio() const     { return m_viewDistRatio; }
	float                     GetTimeScale() const         { return Cry3DEngineBase::GetCVars()->e_ParticlesDebug & AlphaBit('z') ? 0.0f : m_spawnParams.fTimeScale; }
	float                     GetDeltaTime() const         { return m_time - m_timeUpdated; }
	float                     GetTime() const              { return m_time; }
	float                     GetAge() const               { return m_time - m_timeCreated; }
	STimingParams             GetMaxTimings() const;
	bool                      WasRenderedLastFrame() const { return m_unrendered <= 1 && !IsHidden(); }
	uint32                    GetInitialSeed() const       { return m_initialSeed; }
	uint32                    GetCurrentSeed() const       { return m_currentSeed; }
	uint                      GetEmitterId() const         { return m_emitterId; }
	ColorF                    GetProfilerColor() const     { return m_profilerColor; }
	uint                      GetParticleSpec() const;

	void                      SetUnstable();
	bool                      IsStable() const             { return m_time > m_timeStable && !m_bounds.IsReset(); }
	bool                      IsIndependent() const        { return Unique(); }
	bool                      HasParticles() const;
	bool                      HasBounds() const            { return m_bounds.GetVolume() > 0.0f; }
	void                      AddBounds(const AABB& bb);
	uint                      Debug() const                { return m_debug; }

private:
	void     UpdateBounds();
	void     UpdateFromEntity();
	IEntity* GetEmitGeometryEntity() const;

private:
	_smart_ptr<CParticleEffect> m_pEffect;
	_smart_ptr<CParticleEffect> m_pEffectOriginal;
	SVisEnviron                 m_visEnviron;
	SPhysEnviron                m_physEnviron;
	SpawnParams                 m_spawnParams;
	CAttributeInstance          m_attributeInstance;
	TParticleFeatures           m_emitterFeatures;
	AABB                        m_realBounds;
	AABB                        m_nextBounds;
	AABB                        m_bounds;
	CParticleContainer          m_parentContainer;
	TRuntimes                   m_runtimes;
	TRuntimes                   m_runtimesFor;
	TRuntimes                   m_runtimesPreUpdate;
	TRuntimes                   m_runtimesDeferred;
	uint                        m_environFlags;
	QuatTS                      m_location;
	IEntity*                    m_entityOwner;
	int                         m_entitySlot;
	ParticleTarget              m_target;
	GeomRef                     m_emitterGeometry;
	int                         m_emitterGeometrySlot;
	ColorF                      m_profilerColor;
	float                       m_viewDistRatio;
	float                       m_maxViewDist;
	float                       m_time;
	float                       m_timeCreated;
	float                       m_timeStable;
	float                       m_timeUpdated;
	float                       m_timeDeath;
	int                         m_emitterEditVersion;
	int                         m_effectEditVersion;
	uint                        m_initialSeed;
	uint                        m_currentSeed;
	uint                        m_emitterId;
	bool                        m_registered;
	bool                        m_reRegister;
	bool                        m_active;
	bool                        m_alive;
	uint                        m_unrendered;
	uint                        m_debug;
	stl::PSyncMultiThread       m_lock;
};

}

