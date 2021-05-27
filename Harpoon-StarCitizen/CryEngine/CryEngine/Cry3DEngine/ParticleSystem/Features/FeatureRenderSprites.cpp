// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FeatureCommon.h"
#include "ParticleSystem/ParticleRender.h"
#include <CryMath/RadixSort.h>

namespace pfx2
{

extern TDataType<float> EPDT_Alpha, EPDT_Angle2D;
extern TDataType<uint8> EPDT_Tile;
extern TDataType<UCol>  EPDT_Color;

SERIALIZATION_DECLARE_ENUM(ESortMode,
                           None,
                           BackToFront,
                           FrontToBack,
                           OldToNew,
                           NewToOld
                           )

SERIALIZATION_DECLARE_ENUM(EFacingMode,
                           Screen,
                           Camera,
                           Velocity,
                           Free
                           )

struct SSpritesContext
{
	SSpritesContext(const CParticleComponentRuntime& runtime, const SCameraInfo& camInfo, const SVisEnviron& visEnviron, const SPhysEnviron& physEnviron, CREParticle* pRE, TParticleHeap& memHeap, size_t numParticles)
		: m_runtime(runtime)
		, m_camInfo(camInfo)
		, m_visEnviron(visEnviron)
		, m_physEnviron(physEnviron)
		, m_pRE(pRE)
		, m_particleIds(memHeap, numParticles)
		, m_spriteAlphas(memHeap, numParticles)
		, m_area(0)
		, m_areaDrawn(0)
		, m_areaLimit(0)
		, m_renderFlags(0)
		, m_rangeId(0)
	{
	}

	float AreaToPixels() const
	{
		return sqr(m_camInfo.pCamera->GetAngularResolution());
	}

	const CParticleComponentRuntime& m_runtime;
	const SCameraInfo&               m_camInfo;
	const SVisEnviron&               m_visEnviron;
	const SPhysEnviron&              m_physEnviron;
	AABB                             m_bounds;
	CREParticle*                     m_pRE;
	TParticleIdArray                 m_particleIds;
	TFloatArray                      m_spriteAlphas;
	float                            m_area;
	float                            m_areaDrawn;
	float                            m_areaLimit;
	uint64                           m_renderFlags;
	size_t                           m_rangeId;
};

class CFeatureRenderSprites : public CParticleRenderBase
{
private:
	typedef CParticleRenderBase                     BaseClass;
	typedef SParticleAxes (CFeatureRenderSprites::* AxesFn)(const SSpritesContext& context, TParticleId particleId);

public:
	CRY_PFX2_DECLARE_FEATURE

	CFeatureRenderSprites();

	virtual void AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams) override;
	virtual void ComputeVertices(const CParticleComponentRuntime& runtime, const SCameraInfo& camInfo, CREParticle* pRE, uint64 uRenderFlags, float fMaxPixels) override;
	virtual void Serialize(Serialization::IArchive& ar) override;


private:
	void CullParticles(SSpritesContext& spritesContext);
	void SortSprites(SSpritesContext& spritesContext);
	void WriteToGPUMem(const SSpritesContext& spritesContext);
	template<typename TAxesSampler>
	void WriteToGPUMem(const SSpritesContext& spritesContext, SRenderVertices* pRenderVertices, const TAxesSampler& axesSampler);

	ESortMode   m_sortMode;
	EFacingMode m_facingMode;
	UFloat10    m_aspectRatio;
	UFloat10    m_axisScale;
	Vec2        m_offset;
	UUnitFloat  m_sphericalProjection;
	SFloat      m_cameraOffset;
	bool        m_flipU, m_flipV;
};

CRY_PFX2_IMPLEMENT_FEATURE(CParticleFeature, CFeatureRenderSprites, "Render", "Sprites", colorRender);

//////////////////////////////////////////////////////////////////////////

CFeatureRenderSprites::CFeatureRenderSprites()
	: m_sortMode(ESortMode::None)
	, m_facingMode(EFacingMode::Screen)
	, m_aspectRatio(1.0f)
	, m_axisScale(0.0f)
	, m_offset(ZERO)
	, m_sphericalProjection(0.0f)
	, m_cameraOffset(0.0f)
	, m_flipU(false)
	, m_flipV(false)
{
	m_waterCulling = true;
}

void CFeatureRenderSprites::AddToComponent(CParticleComponent* pComponent, SComponentParams* pParams)
{
	BaseClass::AddToComponent(pComponent, pParams);
	pParams->m_renderObjectFlags |= FOB_POINT_SPRITE;
	if (m_facingMode == EFacingMode::Velocity)
		pComponent->AddParticleData(EPVF_Velocity);
	else if (m_facingMode == EFacingMode::Free)
		pComponent->AddParticleData(EPQF_Orientation);
	if (m_facingMode != EFacingMode::Free)
		pParams->m_shaderData.m_sphericalApproximation = m_sphericalProjection;
	else
		pParams->m_shaderData.m_sphericalApproximation = 0.0f;
	pParams->m_scaleParticleSize *= max(+m_aspectRatio, 1.0f);
}

void CFeatureRenderSprites::Serialize(Serialization::IArchive& ar)
{
	BaseClass::Serialize(ar);
	ar(m_sortMode, "SortMode", "Sort Mode");
	ar(m_facingMode, "FacingMode", "Facing Mode");
	ar(m_aspectRatio, "AspectRatio", "Aspect Ratio");
	if (m_facingMode == EFacingMode::Velocity)
		ar(m_axisScale, "AxisScale", "Axis Scale");
	if (m_facingMode != EFacingMode::Free)
		ar(m_sphericalProjection, "SphericalProjection", "Spherical Projection");
	ar(m_sortBias, "SortBias", "Sort Bias");
	ar(m_cameraOffset, "CameraOffset", "Camera Offset");
	ar(m_offset, "Offset", "Offset");
	ar(m_flipU, "FlipU", "Flip U");
	ar(m_flipV, "FlipV", "Flip V");
	ar(m_fillCost, "FillCost", "Fill Cost");
}

void CFeatureRenderSprites::ComputeVertices(const CParticleComponentRuntime& runtime, const SCameraInfo& camInfo, CREParticle* pRE, uint64 uRenderFlags, float fMaxPixels)
{
	const CParticleContainer& container = runtime.GetContainer();
	const CParticleEmitter* pEmitter = runtime.GetEmitter();

	TParticleId numParticles = container.GetNumParticles();
	if (numParticles == 0)
		return;

	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);
	PARTICLE_LIGHT_PROFILER();

	auto& memHeap = GetPSystem()->GetThreadData().memHeap;

	SSpritesContext spritesContext(
		runtime,
		camInfo, pEmitter->GetVisEnv(),
		pEmitter->GetPhysicsEnv(), pRE, memHeap,
		numParticles);
	spritesContext.m_bounds = pEmitter->GetBBox();
	spritesContext.m_renderFlags = uRenderFlags;
	spritesContext.m_areaLimit = fMaxPixels / spritesContext.AreaToPixels();

	CullParticles(spritesContext);

	auto& stats = GetPSystem()->GetThreadData().statsCPU;
	stats.components.rendered ++;

	if (spritesContext.m_particleIds.size() != 0)
	{
		SortSprites(spritesContext);
  		WriteToGPUMem(spritesContext);
		stats.spawners.rendered += runtime.Container(EDD_Spawner).Size();
		stats.particles.rendered += spritesContext.m_particleIds.size();
		stats.particles.culled += runtime.GetContainer().GetNumParticles() - spritesContext.m_particleIds.size();

		stats.pixels.updated += pos_round(spritesContext.m_area * spritesContext.AreaToPixels());
		stats.pixels.rendered += pos_round(spritesContext.m_areaDrawn * spritesContext.AreaToPixels());
		GetPSystem()->GetProfiler().AddEntry(runtime, EPS_RendereredParticles, uint(spritesContext.m_particleIds.size()));
	}
}

void CFeatureRenderSprites::CullParticles(SSpritesContext& spritesContext)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	const CParticleComponentRuntime& runtime = spritesContext.m_runtime;
	const CParticleEmitter& emitter = *runtime.GetEmitter();
	const CCamera& camera = *spritesContext.m_camInfo.pMainCamera;
	const Vec3 cameraPosition = camera.GetPosition();

	// frustum and distance culling
	SFrustumTest frustumTest(camera, spritesContext.m_bounds);
	SFarCulling cullFar(runtime, camera);
	SNearCulling cullNear(runtime, camera, m_facingMode != EFacingMode::Screen);

	// count and cull pixels drawn for near emitters
	const float camAng = camera.GetFov();
	const float screenArea = camAng * camera.GetHorizontalFov();
	const float nearDist = spritesContext.m_bounds.GetDistance(camera.GetPosition());
	const float maxArea = runtime.GetContainer().GetNumParticles()
		* div_min(sqr(runtime.ComponentParams().m_maxParticleSize * 2.0f) * m_aspectRatio, sqr(nearDist), screenArea)
		* m_fillCost;
	const bool cullArea = maxArea > spritesContext.m_areaLimit;
	const bool sumArea = cullArea || maxArea > 1.0f / 256.0f;
	const bool cullDist = cullNear.DoCulling() || cullFar.DoCulling() || sumArea;
	const bool culling = (frustumTest.doTest || cullDist) && !spritesContext.m_camInfo.bShadowPass;
	const bool stretching = m_facingMode == EFacingMode::Velocity && m_axisScale != 0.0f;

	auto& memHeap = GetPSystem()->GetThreadData().memHeap;
	THeapArray<float> areas(memHeap);
	if (sumArea)
		areas.resize(runtime.FullRange().size());

	const CParticleContainer& container = spritesContext.m_runtime.GetContainer();
	IFStream alphas = container.GetIFStream(EPDT_Alpha, 1.0f);
	IFStream sizes = container.GetIFStream(EPDT_Size);
	IVec3Stream positions = container.GetIVec3Stream(EPVF_Position);
	IVec3Stream velocities = container.GetIVec3Stream(EPVF_Velocity);
	auto& particleIds = spritesContext.m_particleIds;
	auto& spriteAlphas = spritesContext.m_spriteAlphas;
	TFloatArray fullSizes(runtime.MemHeap(), container.GetNumParticles());

	uint numParticles = 0;

	// camera culling
	for (auto particleId : runtime.FullRange())
	{
		float alpha = alphas.SafeLoad(particleId);
		const float size = sizes.Load(particleId);
		if (size * alpha <= 0.0f)
			continue;

		float sizeX = size;
		if (stretching)
		{
			const Vec3 velocity = velocities.Load(particleId);
			sizeX = max(size, velocity.GetLengthFast() * m_axisScale);
		}
		sizeX *= m_aspectRatio;
		const float fullSize = max(size, sizeX);

		if (culling)
		{
			const Vec3 position = positions.Load(particleId);

			if (!frustumTest.IsVisible(position, fullSize))
				continue;

			if (cullDist)
			{
				const float invDist = (cameraPosition - position).GetInvLengthFast();
				if (cullNear.DoCulling())
					alpha *= cullNear.DistanceFade(fullSize, invDist);
				if (cullFar.DoCulling())
					alpha *= cullFar.DistanceFade(fullSize, invDist);
				if (sumArea)
				{
					// Compute pixel area, and cull latest particles to enforce pixel limit
					const float area = min(size * sizeX * 4.0f * sqr(invDist), screenArea) * (alpha > 0.0f) * m_fillCost;
					areas[particleId] = area;
				}
			}
		}

		fullSizes[particleId] = fullSize;

		if (alpha > 0.0f)
		{
			particleIds[numParticles++] = particleId;
			spriteAlphas[particleId] = alpha;
		}
	}

	particleIds.resize(numParticles);
	
	if (spritesContext.m_particleIds.size() && !emitter.GetSpawnParams().bIgnoreVisAreas)
	{
		// vis area clipping
		if (IVisArea* pVisArea = spritesContext.m_visEnviron.GetClipVisArea(spritesContext.m_camInfo.pCameraVisArea, spritesContext.m_bounds))
		{
			CRY_PROFILE_SECTION(PROFILE_PARTICLE, "pfx2::CullParticles:VisArea");
			CRY_PFX2_ASSERT(container.HasData(EPDT_Size));
			const Matrix34& viewTM = camera.GetMatrix();
			Vec3 normal = -viewTM.GetColumn0();

			numParticles = 0;
			for (auto particleId : particleIds)
			{
				Sphere sphere;
				const float radius = fullSizes[particleId];
				sphere.center = positions.Load(particleId);
				sphere.radius = radius;
				if (spritesContext.m_visEnviron.ClipVisAreas(pVisArea, sphere, normal))
					spriteAlphas[particleId] *= (sphere.radius - radius) > 0.0f;

				if (spriteAlphas[particleId] > 0.0f)
					particleIds[numParticles++] = particleId;
			}
			particleIds.resize(numParticles);
		}
	}

	// water clipping
	//    PFX2_TODO : Optimize : this routine is *!REALLY!* slow when there are water volumes on the map
	const auto underWater = spritesContext.m_physEnviron.m_tUnderWater;
	if (spritesContext.m_particleIds.size() && underWater == ETrinary::Both)
	{
		CRY_PROFILE_SECTION(PROFILE_PARTICLE, "pfx2::CullParticles:Water");
		CRY_PFX2_ASSERT(container.HasData(EPDT_Size));

		static const float Expand = 1.002f; // fix rcp_fast inaccuracy
		static const Slope<float> AboveSlope { Expand, Expand * 2 }; // fade range is water depth from -1 to 0, increasing
		static const Slope<float> BelowSlope { 0, -Expand }; // fade range is water depth from -1 to 0, decreasing
		Slope<float> waterSlope { 0, 0 };

		const auto& params = runtime.ComponentParams();
		if (params.m_renderStateFlags & OS_TRANSPARENT)
		{
			// Transparent particles always clipped to water volume
			const bool isAfterWater = (spritesContext.m_renderFlags & FOB_AFTER_WATER) != 0;
			const bool cameraUnderWater = spritesContext.m_camInfo.bCameraUnderwater;
			waterSlope = cameraUnderWater != isAfterWater ? AboveSlope : BelowSlope;
		}
		else
		{
			// Opaque particles clipped only if specified
			if (params.m_visibility.m_waterVisibility == EWaterVisibility::AboveWaterOnly)
				waterSlope = AboveSlope;
			else if (params.m_visibility.m_waterVisibility == EWaterVisibility::BelowWaterOnly)
				waterSlope = BelowSlope;
		}

		if (waterSlope.scale != 0)
		{
			Plane waterPlane;
			numParticles = 0;
			for (auto particleId : particleIds)
			{
				const float radius = fullSizes[particleId];
				const Vec3 position = positions.Load(particleId);
				const float waterDist = spritesContext.m_physEnviron.GetWaterPlane(waterPlane, position, radius);
				const float distRel = waterDist * rcp_fast(radius);
				const float waterAlpha = saturate(waterSlope(distRel));
				spriteAlphas[particleId] *= waterAlpha;

				if (waterAlpha > 0.0f)
					particleIds[numParticles++] = particleId;
			}

			particleIds.resize(numParticles);
		}
	}

	if (sumArea)
	{
		for (auto particleId : particleIds)
			spritesContext.m_area += areas[particleId];
		if (cullArea)
			spritesContext.m_areaDrawn = CullArea(spritesContext.m_area, spritesContext.m_areaLimit, particleIds, spriteAlphas, areas);
		else
			spritesContext.m_areaDrawn = spritesContext.m_area;
	}
	else
		spritesContext.m_area = spritesContext.m_areaDrawn = maxArea;
}

void CFeatureRenderSprites::SortSprites(SSpritesContext& spritesContext)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	if (spritesContext.m_camInfo.bShadowPass)
		return;
	const uint renderState = spritesContext.m_runtime.ComponentParams().m_renderStateFlags;
	if (m_sortMode == ESortMode::None || !(renderState & OS_ALPHA_BLEND) || GetCVars()->e_ParticlesSortQuality == 0)
		return;

	auto& memHeap = GetPSystem()->GetThreadData().memHeap;
	const uint numSprites = spritesContext.m_particleIds.size();

	THeapArray<uint32> indices(memHeap, numSprites);
	THeapArray<float> keys(memHeap, numSprites);
	auto& particleIds = spritesContext.m_particleIds;

	const CParticleContainer& container = spritesContext.m_runtime.GetContainer();
	const bool byAge = (m_sortMode == ESortMode::NewToOld) || (m_sortMode == ESortMode::OldToNew);
	const bool byDistance = (m_sortMode == ESortMode::FrontToBack) || (m_sortMode == ESortMode::BackToFront);
	const bool invertKey = (m_sortMode == ESortMode::OldToNew) || (m_sortMode == ESortMode::FrontToBack);

	if (byAge)
	{
		if (container.HasData(EPDT_SpawnId))
		{
			auto ids = container.IStream(EPDT_SpawnId);
			for (size_t i = 0; i < numSprites; ++i)
			{
				const TParticleId particleId = particleIds[i];
				keys[i] = (float)ids.Load(particleId);
			}
		}
		else if (!spritesContext.m_runtime.ComponentParams().m_isImmortal
		&& container.HasData(EPDT_SpawnFraction))
		{
			auto fractions = container.IStream(EPDT_SpawnFraction);
			for (size_t i = 0; i < numSprites; ++i)
			{
				const TParticleId particleId = particleIds[i];
				keys[i] = fractions.Load(particleId);
			}
		}
		else
		{
			CRY_PFX2_ASSERT(container.HasData(EPDT_NormalAge));
			CRY_PFX2_ASSERT(container.HasData(EPDT_LifeTime));
			auto ages = container.IStream(EPDT_NormalAge);
			auto lifeTimes = container.IStream(EPDT_LifeTime);

			for (size_t i = 0; i < numSprites; ++i)
			{
				const TParticleId particleId = particleIds[i];
				const float age = ages.Load(particleId) * lifeTimes.Load(particleId);
				keys[i] = age;
			}
		}
	}
	else if (byDistance)
	{
		IVec3Stream positions = container.GetIVec3Stream(EPVF_Position);

		Vec3 camPos = spritesContext.m_camInfo.pCamera->GetPosition();
		for (size_t i = 0; i < numSprites; ++i)
		{
			const TParticleId particleId = particleIds[i];
			const Vec3 pPos = positions.Load(particleId);
			const float dist = rcp_fast(camPos.GetSquaredDistance(pPos));
			keys[i] = dist;
		}
	}
	if (invertKey)
	{
		for (size_t i = 0; i < numSprites; ++i)
			keys[i] = rcp_fast(keys[i]);
	}

	RadixSort(indices.begin(), indices.end(), keys.begin(), keys.end(), memHeap);

	for (size_t i = 0; i < numSprites; ++i)
		indices[i] = particleIds[indices[i]];
	memcpy(particleIds.data(), indices.data(), sizeof(uint32) * indices.size());
}

class CSpriteFacingModeScreen
{
public:
	CSpriteFacingModeScreen(const CParticleContainer& container, const CCamera& camera)
		: m_sizes(container.GetIFStream(EPDT_Size))
	{
		const Matrix34& viewTM = camera.GetMatrix();
		m_xAxis = viewTM.GetColumn0();
		m_yAxis = -viewTM.GetColumn2();
	}

	ILINE SParticleAxes Sample(TParticleId particleId) const
	{
		SParticleAxes axes;
		const float size = m_sizes.Load(particleId);
		axes.xAxis = m_xAxis * size;
		axes.yAxis = m_yAxis * size;
		return axes;
	}

private:
	const IFStream m_sizes;
	Vec3           m_xAxis;
	Vec3           m_yAxis;
};

class CSpriteFacingModeCamera
{
public:
	CSpriteFacingModeCamera(const CParticleContainer& container, const CCamera& camera)
		: m_positions(container.GetIVec3Stream(EPVF_Position))
		, m_sizes(container.GetIFStream(EPDT_Size))
		, m_cameraPosition(camera.GetPosition())
	{}

	ILINE SParticleAxes Sample(TParticleId particleId) const
	{
		SParticleAxes axes;
		const float size = m_sizes.Load(particleId);
		const Vec3 particlePosition = m_positions.Load(particleId);
		const Vec3 posView = particlePosition - m_cameraPosition;

		axes.xAxis = Vec3(posView.y, -posView.x, 0.0f).GetNormalizedSafe(Vec3(1, 0, 0)) * size;
		axes.yAxis = (posView ^ axes.xAxis).GetNormalized(size);
		return axes;
	}

private:
	const IVec3Stream m_positions;
	const IFStream    m_sizes;
	Vec3              m_cameraPosition;
};

class CSpriteFacingModeVelocity
{
public:
	CSpriteFacingModeVelocity(const CParticleContainer& container, const CCamera& camera, float axisScale)
		: m_positions(container.GetIVec3Stream(EPVF_Position))
		, m_velocities(container.GetIVec3Stream(EPVF_Velocity))
		, m_sizes(container.GetIFStream(EPDT_Size))
		, m_cameraPosition(camera.GetPosition())
		, m_cameraXAxis(camera.GetMatrix().GetColumn0())
		, m_axisScale(axisScale)
	{}

	ILINE SParticleAxes Sample(TParticleId particleId) const
	{
		SParticleAxes axes;
		const Vec3 particlePosition = m_positions.Load(particleId);
		const Vec3 particleVelocity = m_velocities.Load(particleId);
		const float size = m_sizes.Load(particleId);

		const Vec3 moveDirection = particleVelocity.GetNormalizedSafe(m_cameraXAxis);
		const Vec3 normal = (particlePosition - m_cameraPosition).GetNormalized();
		const float axisSize = max(size, particleVelocity.GetLength() * m_axisScale);
		axes.xAxis = moveDirection * axisSize;
		axes.yAxis = (normal ^ moveDirection) * size;

		return axes;
	}

private:
	const IVec3Stream m_positions;
	const IVec3Stream m_velocities;
	const IFStream    m_sizes;
	Vec3              m_cameraPosition;
	Vec3              m_cameraXAxis;
	float             m_axisScale;
};

class CSpriteFacingModeFree
{
public:
	CSpriteFacingModeFree(const CParticleContainer& container)
		: m_orientations(container.GetIQuatStream(EPQF_Orientation))
		, m_sizes(container.GetIFStream(EPDT_Size))
	{}

	ILINE SParticleAxes Sample(TParticleId particleId) const
	{
		SParticleAxes axes;
		const Quat orientation = m_orientations.Load(particleId);
		const float size = m_sizes.Load(particleId);
		axes.xAxis = orientation.GetColumn0() * size;
		axes.yAxis = -orientation.GetColumn1() * size;
		return axes;
	}

private:
	const IQuatStream m_orientations;
	const IFStream    m_sizes;
};

void CFeatureRenderSprites::WriteToGPUMem(const SSpritesContext& spritesContext)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	const CParticleContainer& container = spritesContext.m_runtime.GetContainer();
	const CCamera& camera = *spritesContext.m_camInfo.pCamera;
	const uint numSprites = spritesContext.m_particleIds.size();

	SRenderVertices *pRenderVertices = spritesContext.m_pRE->AllocPullVertices(numSprites);
	pRenderVertices->fPixels = spritesContext.m_area * spritesContext.AreaToPixels();

	switch (m_facingMode)
	{
	case EFacingMode::Screen:
		WriteToGPUMem(spritesContext, pRenderVertices, CSpriteFacingModeScreen(container, camera));
		break;
	case EFacingMode::Camera:
		WriteToGPUMem(spritesContext, pRenderVertices, CSpriteFacingModeCamera(container, camera));
		break;
	case EFacingMode::Velocity:
		WriteToGPUMem(spritesContext, pRenderVertices, CSpriteFacingModeVelocity(container, camera, m_axisScale));
		break;
	case EFacingMode::Free:
		WriteToGPUMem(spritesContext, pRenderVertices, CSpriteFacingModeFree(container));
		break;
	}
}

template<typename TAxesSampler>
void CFeatureRenderSprites::WriteToGPUMem(const SSpritesContext& spritesContext, SRenderVertices* pRenderVertices, const TAxesSampler& axesSampler)
{
	const SComponentParams& params = spritesContext.m_runtime.ComponentParams();
	const CParticleContainer& container = spritesContext.m_runtime.GetContainer();
	const IVec3Stream positions = container.GetIVec3Stream(EPVF_Position);
	const IFStream ages = container.GetIFStream(EPDT_NormalAge);
	const IFStream lifetimes = container.GetIFStream(EPDT_LifeTime);
	const IFStream angles = container.GetIFStream(EPDT_Angle2D, 0.0f);
	const IColorStream colors = container.GetIColorStream(EPDT_Color);
	const TIStream<uint8> tiles = container.IStream(EPDT_Tile);
	const Vec3 camPos = spritesContext.m_camInfo.pCamera->GetPosition();
	const Vec3 emitterPosition = spritesContext.m_runtime.GetEmitter()->GetPos();
	const Vec3 cameraOffset = (emitterPosition - camPos).GetNormalized(m_cameraOffset);
	const auto& particleIds = spritesContext.m_particleIds;
	const auto& spriteAlphas = spritesContext.m_spriteAlphas;
	const uint numSprites = spritesContext.m_particleIds.size();
	const float flipU = m_flipU ? -1.0f : 1.0f;
	const float flipV = m_flipV ? -1.0f : 1.0f;

	const bool hasAngles2D = container.HasData(EPDT_Angle2D);
	const bool hasColors = container.HasData(EPDT_Color);
	const bool hasTiling = container.HasData(EPDT_Tile);
	const bool hasAnimation = params.m_textureAnimation.IsAnimating();
	const bool hasAbsFrameRate = params.m_textureAnimation.HasAbsoluteFrameRate();
	const bool hasOffset = (m_offset != Vec2(ZERO)) || (m_cameraOffset != 0.0f);

	const uint spritesPerChunk = vertexChunckSize / sizeof(SParticleAxes);
	const uint numChunks = ((numSprites + spritesPerChunk - 1) / spritesPerChunk);
	CWriteCombinedBuffer<Vec3, vertexBufferSize, spritesPerChunk * sizeof(Vec3)> wcPositions(pRenderVertices->aPositions);
	CWriteCombinedBuffer<SParticleAxes, vertexBufferSize, spritesPerChunk * sizeof(SParticleAxes)> wcAxes(pRenderVertices->aAxes);
	CWriteCombinedBuffer<SParticleColorST, vertexBufferSize, spritesPerChunk * sizeof(SParticleColorST)> wcColorSTs(pRenderVertices->aColorSTs);

	SParticleColorST colorSTDefault;
	colorSTDefault.st.dcolor = 0;
	colorSTDefault.st.z = uint8(params.m_shaderData.m_firstTile);
	colorSTDefault.color.dcolor = ~0;

	for (uint chunk = 0, spriteIdx = 0; chunk < numChunks; ++chunk)
	{
		const uint chunkSprites = min(spritesPerChunk, numSprites - chunk * spritesPerChunk);
		if (!wcPositions.CheckAvailable(chunkSprites) || !wcAxes.CheckAvailable(chunkSprites) || !wcColorSTs.CheckAvailable(chunkSprites))
			break;

		for (uint sprite = 0; sprite < chunkSprites; ++sprite, ++spriteIdx)
		{
			const TParticleId particleId = particleIds[spriteIdx];

			SParticleAxes axes = axesSampler.Sample(particleId);
			if (hasAngles2D)
			{
				const float angle = angles.Load(particleId);
				RotateAxes(&axes.xAxis, &axes.yAxis, angle);
			}
			axes.xAxis *= m_aspectRatio * flipU;
			axes.yAxis *= flipV;

			wcAxes.Array().push_back(axes);

			Vec3 position = positions.Load(particleId);
			if (hasOffset)
				position += axes.xAxis * m_offset.x - axes.yAxis * m_offset.y + cameraOffset;
			wcPositions.Array().push_back(position);

			SParticleColorST colorST = colorSTDefault;

			if (hasTiling)
				colorST.st.z += tiles.Load(particleId);

			if (hasAnimation)
			{
				float age = ages.Load(particleId);
				float animPos = hasAbsFrameRate ?
					params.m_textureAnimation.GetAnimPosAbsolute(age * lifetimes.Load(particleId))
					: params.m_textureAnimation.GetAnimPosRelative(age);

				colorST.st.z += int(animPos);
				colorST.st.w = FloatToUFrac8Saturate(animPos - int(animPos));
			}

			if (hasColors)
				colorST.color = colors.Load(particleId);
			colorST.color.a = FloatToUFrac8Saturate(spriteAlphas[particleId]);

 			wcColorSTs.Array().push_back(colorST);
		}
	}
}

}
