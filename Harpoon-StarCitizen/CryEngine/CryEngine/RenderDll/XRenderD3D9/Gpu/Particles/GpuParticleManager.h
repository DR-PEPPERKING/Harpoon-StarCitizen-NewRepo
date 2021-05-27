// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryRenderer/IGpuParticles.h>
#include "GpuParticleFeatureFactory.h"
#include "Gpu/GpuBitonicSort.h"
#include "GpuParticleComponentRuntime.h"
#include <map>

namespace gpu_pfx2
{

class CGpuInterfaceFactory;

// interface of GPU particle system
class CManager : public IManager
{
public:
	struct SRuntimeRef
	{
		SRuntimeRef() : m_pComponentRuntime(0), m_firstChild(0), m_numChildren(0) {}
		SRuntimeRef(IParticleComponentRuntime* pComponentRuntime)
			: m_pComponentRuntime((CParticleComponentRuntime*)pComponentRuntime)
			, m_firstChild(0)
			, m_numChildren(0)
		{
		}

		CParticleComponentRuntime* m_pComponentRuntime;
		size_t                     m_firstChild;
		size_t                     m_numChildren;
	};

	CManager();

	virtual void BeginFrame() override {}

	virtual IParticleComponentRuntime* CreateParticleContainer(const SComponentParams& params, TConstArray<IParticleFeature*> features) override;

	virtual IParticleFeature* CreateParticleFeature(EGpuFeatureType) override;

	// this needs to be called from the render thread when the renderer
	// releases resources to release references to ComputePSOs
	void ReleaseResources() override;

	virtual void RenderThreadUpdate(CRenderView* pRenderView) override;
	virtual void RenderThreadPreUpdate(CRenderView* pRenderView) override;
	virtual void RenderThreadPostUpdate(CRenderView* pRenderView) override;

	// gets initialized the first time it is called and will allocate buffers
	// (so make sure its only called from the render thread)
	gpu::CBitonicSort* GetBitonicSort(CGraphicsPipeline* pGraphicsPipeline);

private:
	std::vector<_smart_ptr<CParticleComponentRuntime>>& GetWriteRuntimes()
	{
		return m_particleComponentRuntimes[0];
	}
	std::vector<_smart_ptr<CParticleComponentRuntime>>& GetReadRuntimes()
	{
		return m_particleComponentRuntimes[1];
	}

	void Initialize();

	// in here, gpu resources might be freed
	void ProcessResources();

	// this keeps a reference to runtimes so we can control the lifetime and
	// release resources on the render thread if refcount = 1
	std::vector<_smart_ptr<CParticleComponentRuntime>>                    m_particleComponentRuntimes[2];
	std::vector<_smart_ptr<CFeature>>                                     m_particleFeatureGpuInterfaces;
	std::vector<_smart_ptr<CFeature>>                                     m_particleFeatureGpuInterfacesInitialization;

	gpu::CStructuredResource<uint, gpu::BufferFlagsReadWriteReadback>          m_counter;
	gpu::CStructuredResource<uint, gpu::BufferFlagsReadWriteReadback>          m_scratch;
	gpu::CStructuredResource<SReadbackData, gpu::BufferFlagsReadWriteReadback> m_readback;

#if (CRY_RENDERER_DIRECT3D >= 111)
	std::unique_ptr<CClearRegionPass>  m_clearRegionPass;
#endif

	int m_numRuntimesReadback;

	CryCriticalSection                 m_cs;

	std::unique_ptr<gpu::CBitonicSort> m_pBitonicSort;
	CGpuInterfaceFactory               m_gpuInterfaceFactory;
};
}
