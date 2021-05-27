// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <Cry3DEngine/I3DEngine.h>
#include <CryMovie/IMovieSystem.h>
#include <Cry3DEngine/CGF/CryHeaders.h>
#include <CrySystem/Profilers/IStatoscope.h>
#include <CryGame/IGameFramework.h>

#include "D3DPostProcess.h"
#include "D3DStereo.h"
#include "D3DHWShader.h"
#include "../Common/Shaders/RemoteCompiler.h"
#include "../Common/ReverseDepth.h"
#include "../Common/RenderDisplayContext.h"
#ifdef ENABLE_BENCHMARK_SENSOR
#include <IBenchmarkFramework.h>
#include <IBenchmarkRendererSensorManager.h>
#include "BenchmarkCustom/BenchmarkRendererSensor.h"
#endif
#include "../Common/ComputeSkinningStorage.h"
#if defined(FEATURE_SVO_GI)
#include "D3D_SVO.h"
#endif

#include "Gpu/Particles/GpuParticleManager.h"
#include "GraphicsPipeline/Common/GraphicsPipelineStage.h"
#include "GraphicsPipeline/Common/SceneRenderPass.h"
#include "GraphicsPipeline/ComputeSkinning.h"
#include "GraphicsPipeline/SceneGBuffer.h"
#include "GraphicsPipeline/SceneCustom.h"
#include "GraphicsPipeline/SceneDepth.h"
#include "GraphicsPipeline/TiledLightVolumes.h"
#include "GraphicsPipeline/PostAA.h"
#include "Common/RenderView.h"
#include "CompiledRenderObject.h"

#pragma warning(push)
#pragma warning(disable: 4244)

//============================================================================================
// Shaders rendering
//============================================================================================

// Init shaders pipeline
void CD3D9Renderer::EF_Init()
{
	m_bInitialized = true;

	if (CV_r_logTexStreaming && !m_LogFileStr)
	{
		m_LogFileStr = fxopen("Direct3DLogStreaming.txt", "w");
		if (m_LogFileStr)
		{
			iLog->Log("Direct3D texture streaming log file '%s' opened", "Direct3DLogStreaming.txt");
			char time[128];
			char date[128];

			_strtime(time);
			_strdate(date);

			fprintf(m_LogFileStr, "\n==========================================\n");
			fprintf(m_LogFileStr, "Direct3D Textures streaming Log file opened: %s (%s)\n", date, time);
			fprintf(m_LogFileStr, "==========================================\n");
		}
	}

	CHWShader_D3D::mfInit();

	m_CurVertBufferSize = 0;
	m_CurIndexBufferSize = 0;

	// Init compiled objects pool
	{
		m_tempRenderObjects.m_renderObjectsPools.reset(new CRenderObjectsPools);
		// Initialize fast access global pointer
		CCompiledRenderObject::SetStaticPools(m_tempRenderObjects.m_renderObjectsPools.get());
		CPermanentRenderObject::SetStaticPools(m_tempRenderObjects.m_renderObjectsPools.get());
	}

	// Initialize posteffects manager
	if (!m_pPostProcessMgr)
	{
		m_pPostProcessMgr = new CPostEffectsMgr;
		m_pPostProcessMgr->Init();
	}

	if (!m_pComputeSkinningStorage)
		m_pComputeSkinningStorage = new compute_skinning::CStorage;

	// preallocate video memory buffer for particles when using the job system
	m_particleBuffer.Create(CRendererCVars::CV_r_ParticleVerticePoolSize, CRendererCVars::CV_r_ParticleMaxVerticePoolSize);

	if (!m_pGpuParticleManager)
		m_pGpuParticleManager = new gpu_pfx2::CManager;

	if (!m_pWaterSimMgr)
		m_pWaterSimMgr = new CWater;

	CSceneRenderPass::Initialize();

	//SDynTexture::CreateShadowPool();

	m_waterUpdateInfo.m_fLastWaterFOVUpdate    = 0;
	m_waterUpdateInfo.m_LastWaterViewdirUpdate = Vec3(0, 0, 0);
	m_waterUpdateInfo.m_LastWaterUpdirUpdate   = Vec3(0, 0, 0);
	m_waterUpdateInfo.m_LastWaterPosUpdate     = Vec3(0, 0, 0);
	m_waterUpdateInfo.m_fLastWaterUpdate       = 0;
	m_waterUpdateInfo.m_nLastWaterFrameID      = 0;

	m_nCurMinAniso = 4;
	m_nCurMaxAniso = 16;
	m_nMaterialAnisoHighSampler   = CDeviceObjectFactory::GetOrCreateSamplerStateHandle(SSamplerState(FILTER_ANISO16X, false));
	m_nMaterialAnisoLowSampler    = CDeviceObjectFactory::GetOrCreateSamplerStateHandle(SSamplerState(FILTER_ANISO4X, false));
	m_nMaterialAnisoSamplerBorder = CDeviceObjectFactory::GetOrCreateSamplerStateHandle(SSamplerState(FILTER_ANISO16X, eSamplerAddressMode_Border, eSamplerAddressMode_Border, eSamplerAddressMode_Border, 0x0));

#ifdef ENABLE_BENCHMARK_SENSOR
	if (!m_benchmarkRendererSensor)
	{
		m_benchmarkRendererSensor = new BenchmarkRendererSensor(this);
	}
#endif
}

// Shutdown shaders pipeline
void CD3D9Renderer::EF_Exit(bool bFastShutdown)
{
	// Reset states
	GetDeviceObjectFactory().GetCoreCommandList().Reset();

#ifdef ENABLE_BENCHMARK_SENSOR
	SAFE_DELETE(m_benchmarkRendererSensor);
#endif

	//if (m_pStereoRenderer)
	//	m_pStereoRenderer->ReleaseResources();

	SAFE_DELETE(m_pWaterSimMgr);
	SAFE_DELETE(m_pPostProcessMgr);
	SAFE_DELETE(m_pComputeSkinningStorage);

	m_tempRenderObjects.m_renderObjectsPools.reset();

	if (!bFastShutdown)
		CHWShader_D3D::ShutDown();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

struct SPreprocess
{
	int               m_nPreprocess;
	int               m_Num;
	CRenderObject*    m_pObject;
	int               m_nTech;
	CShader*          m_Shader;
	CShaderResources* m_pRes;
	CRenderElement*   m_RE;
};

struct Compare2
{
	bool operator()(const SPreprocess& a, const SPreprocess& b) const
	{
		return a.m_nPreprocess < b.m_nPreprocess;
	}
};

// Current scene preprocess operations (Rendering to RT, screen effects initializing, ...)
int CD3D9Renderer::EF_Preprocess(SRendItem* ri, uint32 nums, uint32 nume, const SRenderingPassInfo& passInfo) PREFAST_SUPPRESS_WARNING(6262)
{
	uint32 i, j;
	CShader* Shader;
	CShaderResources* Res;
	CRenderObject* pObject;
	int nTech;

	SPreprocess Procs[512];
	uint32 nProcs = 0;

	CTimeValue time0 = iTimer->GetAsyncTime();

	if (m_LogFile)
		gRenDev->Logv("*** Start preprocess frame ***\n");

	int nReturn  = 0;

	for (i = nums; i < nume; i++)
	{
		if (nProcs >= 512)
			break;

		SShaderItem shaderItem;
		SRendItem::ExtractShaderItem(ri[i].SortVal, ri[i].nBatchFlags, shaderItem);
		nTech = shaderItem.m_nTechnique;
		Shader = static_cast<CShader*>(shaderItem.m_pShader);
		Res = static_cast<CShaderResources*>(shaderItem.m_pShaderResources);

		pObject = ri[i].pCompiledObject->m_pRO;
		if (!(ri[i].nBatchFlags & FSPR_MASK))
			break;
		nReturn++;
		if (nTech < 0)
			nTech = 0;
		if (nTech < (int)Shader->m_HWTechniques.Num())
		{
			for (j = SPRID_FIRST; j < 32; j++)
			{
				uint32 nMask = 1 << j;
				if (nMask >= FSPR_MAX || nMask > (ri[i].nBatchFlags & FSPR_MASK))
					break;
				if (nMask & ri[i].nBatchFlags)
				{
					Procs[nProcs].m_nPreprocess = j;
					Procs[nProcs].m_Num         = i;
					Procs[nProcs].m_Shader      = Shader;
					Procs[nProcs].m_pRes        = Res;
					Procs[nProcs].m_RE          = ri[i].pElem;
					Procs[nProcs].m_pObject     = pObject;
					Procs[nProcs].m_nTech       = nTech;
					nProcs++;
				}
			}
		}
	}
	if (!nProcs)
		return 0;
	std::sort(&Procs[0], &Procs[nProcs], Compare2());

	bool bRes = true;
	for (i = 0; i < nProcs; i++)
	{
		SPreprocess* pr = &Procs[i];
		if (!pr->m_Shader)
			continue;
		switch (pr->m_nPreprocess)
		{
		case SPRID_SCANTEX:
		case SPRID_SCANTEXWATER:
			{
				bool bTryPreprocess = false;
				CRenderObject* pObj = pr->m_pObject;
				int nT              = pr->m_nTech;
				if (nT < 0)
					nT = 0;
				SShaderTechnique* pTech = pr->m_Shader->m_HWTechniques[nT];
				CShaderResources* pRes  = pr->m_pRes;
				for (j = 0; j < pTech->m_RTargets.Num(); j++)
				{
					SHRenderTarget* pTarg = pTech->m_RTargets[j];
					if (pTarg->m_eOrder == eRO_PreProcess)
					{
						bTryPreprocess = true;
						bRes &= FX_DrawToRenderTarget(pr->m_Shader, pRes, pObj, pTech, pTarg, pr->m_nPreprocess, pr->m_RE, passInfo);
					}
				}
				if (pRes)
				{
					// TODO: Instead of rendering to another target again, consider copying instead!
					for (j = 0; j < pRes->m_RTargets.Num(); j++)
					{
						SHRenderTarget* pTarg = pRes->m_RTargets[j];
						if (pTarg->m_eOrder == eRO_PreProcess)
						{
							bTryPreprocess = true;
							bRes &= FX_DrawToRenderTarget(pr->m_Shader, pRes, pObj, pTech, pTarg, pr->m_nPreprocess, pr->m_RE, passInfo);
						}
					}
				}

				// NOTE: try water reflection pre-process again because it wouldn't be executed when the shader uses only Texture and SamplerState instead of Sampler.
				const uint64 ENVIRONMENT_MAP_MASK = 0x4; // this is defined in Water.ext as %ENVIRONMENT_MAP.
				if (!bTryPreprocess
					&& pr->m_RE->mfGetType() == eDATA_WaterOcean
					&& ((pr->m_Shader->m_nMaskGenFX & ENVIRONMENT_MAP_MASK) == 0))
				{
					CREWaterOcean* pOcean = static_cast<CREWaterOcean*>(pr->m_RE);
					SHRenderTarget* pTarg = pOcean->GetReflectionRenderTarget();
					bRes &= FX_DrawToRenderTarget(pr->m_Shader, pRes, pObj, pTech, pTarg, pr->m_nPreprocess, pr->m_RE, passInfo);

#if !defined(RELEASE) && defined(_DEBUG)
					static string ENVIRONMENT_MAP_NAME("%ENVIRONMENT_MAP");
					auto* pShaderGenBase = pr->m_Shader->GetGenerationParams();
					bool findParameter = false;
					if(pShaderGenBase)
					{
						for(unsigned nBaseBit(0); nBaseBit < pShaderGenBase->m_BitMask.size(); ++nBaseBit)
						{
							SShaderGenBit* pBaseBit = pShaderGenBase->m_BitMask[nBaseBit];

							if(!pBaseBit->m_ParamName.empty())
							{
								if(ENVIRONMENT_MAP_NAME == pBaseBit->m_ParamName)
								{
									if (ENVIRONMENT_MAP_MASK == pBaseBit->m_Mask)
									{
										findParameter = true;
										break;
									}
								}
							}
						}
					}
					CRY_ASSERT(findParameter);
#endif
				}
			}
			break;

		case SPRID_CUSTOMTEXTURE:
			{
				CRenderObject* pObj = pr->m_pObject;
				int nT              = pr->m_nTech;
				if (nT < 0)
					nT = 0;
				SShaderTechnique* pTech = pr->m_Shader->m_HWTechniques[nT];
				CShaderResources* pRes  = pr->m_pRes;
				for (j = 0; j < pRes->m_RTargets.Num(); j++)
				{
					SHRenderTarget* pTarg = pRes->m_RTargets[j];
					if (pTarg->m_eOrder == eRO_PreProcess)
						bRes &= FX_DrawToRenderTarget(pr->m_Shader, pRes, pObj, pTech, pTarg, pr->m_nPreprocess, pr->m_RE, passInfo);
				}
			}
			break;

		default:
			assert(0);
		}
	}

	if (m_LogFile)
		gRenDev->Logv("*** End preprocess frame ***\n");

	m_frameRenderStats[m_nFillThreadID].m_fPreprocessTime += iTimer->GetAsyncTime().GetDifferenceInSeconds(time0);

	return nReturn;
}

void CD3D9Renderer::EF_EndEf2D(const bool bSort)
{
}

//==============================================================================================
float* CD3D9Renderer::PinOcclusionBuffer(Matrix44A& camera, const SGraphicsPipelineKey& graphicsPipelineKey)
{
	std::shared_ptr<CGraphicsPipeline> pGP = FindGraphicsPipeline(graphicsPipelineKey);
	if (pGP)
	{
		if (auto pSceneDepthStage = pGP->GetStage<CSceneDepthStage>())
		{
			return pSceneDepthStage->PinOcclusionBuffer(camera);
		}
	}

	return nullptr;
}

void CD3D9Renderer::UnpinOcclusionBuffer(const SGraphicsPipelineKey& graphicsPipelineKey)
{
	std::shared_ptr<CGraphicsPipeline> pGP = FindGraphicsPipeline(graphicsPipelineKey);
	if (pGP)
	{
		if (auto pSceneDepthStage = pGP->GetStage<CSceneDepthStage>())
		{
			pSceneDepthStage->UnpinOcclusionBuffer();
		}
	}
}

void CD3D9Renderer::RT_UpdateSkinningConstantBuffers(CRenderView* pRenderView)
{
	PROFILE_FRAME(RT_UpdateSkinningConstantBuffers);
	unsigned poolId = pRenderView->GetSkinningDataPools().poolIndex % CRY_ARRAY_COUNT(m_SkinningDataPool);
	for (util::list<SCharacterInstanceCB>* iter = m_CharCBActiveList[poolId].next; iter != &m_CharCBActiveList[poolId]; iter = iter->next)
	{
		SCharacterInstanceCB* cb = iter->item<&SCharacterInstanceCB::list>();
		if (cb->updated)
			continue;
		SSkinningData* pSkinningData = cb->m_pSD;

		// make sure all sync jobs filling the buffers have finished
		if (pSkinningData->pAsyncJobs)
		{
			PROFILE_FRAME(FX_UpdateCharCBs_ASYNC_WAIT);
			gEnv->pJobManager->WaitForJob(*pSkinningData->pAsyncJobs);
		}

		// NOTE: The pointers and the size is 16 byte aligned
		size_t boneQuatsSSize   = Align(pSkinningData->nNumBones        * sizeof(DualQuat                       ), CRY_PLATFORM_ALIGNMENT);
		size_t activeMorphsSize = Align(pSkinningData->nNumActiveMorphs * sizeof(compute_skinning::SActiveMorphs), CRY_PLATFORM_ALIGNMENT);

		cb->boneTransformsBuffer->UpdateBuffer(pSkinningData->pBoneQuatsS, boneQuatsSSize);
		if (pSkinningData->nNumActiveMorphs)
			cb->activeMorphsBuffer.UpdateBufferContent(pSkinningData->pActiveMorphs, activeMorphsSize);

		cb->updated = true;
	}

	// free a buffer each frame if we have an over-comittment of more than 75% compared
	// to our last 2 frames of rendering
	{
		int committed      = CryInterlockedCompareExchange((LONG*)&m_CharCBAllocated, 0, 0);
		int totalRequested = m_CharCBFrameRequired[poolId] + m_CharCBFrameRequired[(poolId - 1) % 3];
		WriteLock _lock(m_lockCharCB);
		if (totalRequested * 4 > committed * 3 && m_CharCBFreeList.empty() == false)
		{
			delete m_CharCBFreeList.prev->item<&SCharacterInstanceCB::list>();
			CryInterlockedDecrement(&m_CharCBAllocated);
		}
	}
}

void* CD3D9Renderer::FX_AllocateCharInstCB(SSkinningData* pSkinningData, uint32 frameId)
{
	PROFILE_FRAME(FX_AllocateCharInstCB);
	SCharacterInstanceCB* cb = NULL;
	{
		WriteLock _lock(m_lockCharCB);
		if (m_CharCBFreeList.empty() == false)
		{
			cb = m_CharCBFreeList.next->item<&SCharacterInstanceCB::list>();
			cb->list.erase();
		}
	}
	if (cb == NULL)
	{
		cb = new SCharacterInstanceCB();
		cb->boneTransformsBuffer = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(768 * sizeof(DualQuat), true, true);
		cb->activeMorphsBuffer.Create(768, sizeof(compute_skinning::SActiveMorphs), DXGI_FORMAT_UNKNOWN, CDeviceObjectFactory::USAGE_CPU_WRITE | CDeviceObjectFactory::USAGE_STRUCTURED | CDeviceObjectFactory::BIND_SHADER_RESOURCE, NULL);
		CryInterlockedIncrement(&m_CharCBAllocated);
	}
	cb->updated = false;
	cb->m_pSD   = pSkinningData;
	{
		WriteLock _lock(m_lockCharCB);
		cb->list.relink_tail(&m_CharCBActiveList[frameId % 3]);
	}
	CryInterlockedIncrement(&m_CharCBFrameRequired[frameId % 3]);
	return cb;
}

void CD3D9Renderer::FX_ClearCharInstCB(uint32 frameId)
{
	PROFILE_FRAME(FX_ClearCharInstCB);
	uint32 poolId = frameId % 3;
	WriteLock _lock(m_lockCharCB);
	m_CharCBFrameRequired[poolId] = 0;
	m_CharCBFreeList.splice_tail(&m_CharCBActiveList[poolId]);
}

void CD3D9Renderer::RT_PreRenderScene(CRenderView* pRenderView)
{
	std::shared_ptr<CGraphicsPipeline> pActiveGraphicsPipeline = pRenderView ? pRenderView->GetGraphicsPipeline() : nullptr;
	const uint32 shaderRenderingFlags = pActiveGraphicsPipeline ? pActiveGraphicsPipeline->GetRenderFlags() : 0U;

	if (pActiveGraphicsPipeline)
		pActiveGraphicsPipeline->SetCurrentRenderView(pRenderView);

	//if (pRenderView->GetCurrentEye() != CCamera::eEye_Right)
	//{
	//	m_pGpuParticleManager->RenderThreadPreUpdate();
	//}

	const bool bRecurse = pRenderView ? pRenderView->IsRecursive() : false;
	const bool bAllowPostProcess = pActiveGraphicsPipeline ? pActiveGraphicsPipeline->IsPostProcessingEnabled() : false;
	const bool bAllowPostAA = bAllowPostProcess
	                          && (GetWireframeMode() == R_SOLID_MODE)
	                          && (CRenderer::CV_r_DeferredShadingDebugGBuffer == 0);

	// Update the character CBs (only active on D3D11 style platforms)
	// Needs to be done before objects are compiled
	if (pRenderView)
		RT_UpdateSkinningConstantBuffers(pRenderView);

	CRenderMesh::RT_PerFrameTick();
	CMotionBlur::InsertNewElements();
	CRenderMesh::UpdateModified();

	// Calculate AA jitter
	if (pActiveGraphicsPipeline && bAllowPostAA)
	{
		if (GetS3DRend().IsStereoEnabled())
		{
			const auto* eye = GetS3DRend().GetEyeDisplayContext(CCamera::eEye_Left).first;
			if (eye)
			{
				auto* pStage = pActiveGraphicsPipeline->GetStage<CPostAAStage>();
				pStage->CalculateJitterOffsets(eye->GetCurrentBackBuffer()->GetWidth(), eye->GetCurrentBackBuffer()->GetHeight(), pRenderView);
			}
		}
		else
		{
			const auto& ro = GetActiveDisplayContext()->GetRenderOutput();
			if (ro)
			{
				auto* pStage = pActiveGraphicsPipeline->GetStage<CPostAAStage>();
				pStage->CalculateJitterOffsets(ro->GetOutputResolution().x, ro->GetOutputResolution().y, pRenderView);
			}
		}
	}

	{
		PROFILE_FRAME(CompileModifiedShaderItems);
		for (auto pShaderResources : CShader::s_ShaderResources_known)
		{
			// TODO: Check why s_ShaderResources_known can contain null pointers
			if (pShaderResources && pShaderResources->HasChanges())
			{
				// NOTE: unconditionally clear dirty flag here, as there is no point in trying to update the resource set again
				// in case of failure. any change to the resources will set the dirty flag again.
				pShaderResources->RT_UpdateResourceSet();
			}
		}
	}

	if (pActiveGraphicsPipeline)
	{
		static int lightVolumeOldFrameID = -1;
		const int newFrameID = pRenderView->GetFrameId();

		// Update light volumes info
		const bool updateLightVolumes =
			lightVolumeOldFrameID != newFrameID &&
			!bRecurse &&
			(shaderRenderingFlags & SHDF_ALLOWPOSTPROCESS) != 0;
		if (updateLightVolumes)
		{
			CTiledLightVolumesStage* pLightVolumeStage = pActiveGraphicsPipeline->GetStage<CTiledLightVolumesStage>();
			if (pLightVolumeStage)
			{
				pLightVolumeStage->GetLightVolumeBuffer().UpdateContent();
			}

			lightVolumeOldFrameID = newFrameID;
		}

		pActiveGraphicsPipeline->m_nStencilMaskRef = STENCIL_VALUE_OUTDOORS + 1;
	}

	m_maskRenderPhaseLog[m_SceneRecurseCount - 1] |= eRP_PreRenderScene;
}

void CD3D9Renderer::RT_PostRenderScene(CRenderView* pRenderView)
{
	{
		PROFILE_FRAME(ShadowViewsEndFrame);
		for (auto& fr : pRenderView->m_shadows.m_renderFrustums)
		{
			auto pShadowView = reinterpret_cast<CRenderView*>(fr.pShadowsView.get());
			pShadowView->SwitchUsageMode(IRenderView::eUsageModeReadingDone);
		}
	}

	const bool bRecurse = pRenderView->IsRecursive();
	if (!bRecurse)
	{
		gRenDev->GetIRenderAuxGeom()->Submit();
	}

	m_maskRenderPhaseLog[m_SceneRecurseCount - 1] |= eRP_PostRenderScene;
}

// Render thread only scene rendering
void CD3D9Renderer::RT_RenderScene(CRenderView* pRenderView)
{
	FUNCTION_PROFILER_RENDERER();
	PROFILE_LABEL_SCOPE_DYNAMIC((pRenderView->IsRecursive() ? "SCENE_REC" : "SCENE"), "SCENE");

	gcpRendD3D->SetCurDownscaleFactor(gcpRendD3D->m_CurViewportScale);

	{
		PROFILE_FRAME(WaitForRenderView);
		pRenderView->SwitchUsageMode(CRenderView::eUsageModeReading);
	}

	const CTimeValue Time = iTimer->GetAsyncTime();

	// Only Billboard rendering doesn't use CRenderOutput
	if (!pRenderView->GetRenderOutput() && !pRenderView->IsBillboardGenView())
	{
		pRenderView->AssignRenderOutput(GetActiveDisplayContext()->GetRenderOutput());
		pRenderView->GetRenderOutput()->BeginRendering(pRenderView);
	}

	std::shared_ptr<CGraphicsPipeline> pActiveGraphicsPipeline = pRenderView->GetGraphicsPipeline();
	uint32 shaderRenderingFlags = pActiveGraphicsPipeline->GetRenderFlags();

	CFlashTextureSourceSharedRT::SetupSharedRenderTargetRT();

	if (!pRenderView->IsRecursive())
	{
		D3D11_VIEWPORT viewport = RenderViewportToD3D11Viewport(pRenderView->GetViewport());
		pActiveGraphicsPipeline->GetVrProjectionManager()->Configure(viewport, pRenderView->GetCurrentEye() == CCamera::eEye_Right);
	}

	int nSaveDrawNear     = CV_r_nodrawnear;
	int nSaveDrawCaustics = CV_r_watercaustics;
	if (shaderRenderingFlags & SHDF_NO_DRAWCAUSTICS)
		CV_r_watercaustics = 0;
	if (shaderRenderingFlags & SHDF_NO_DRAWNEAR)
		CV_r_nodrawnear = 1;

	m_bDeferredDecals = false;

	m_vSceneLuminanceInfo = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
	m_fAdaptedSceneScale  = m_fAdaptedSceneScaleLBuffer = m_fScotopicSceneScale = 1.0f;

	// This scope is the only one allowed to utilize the graphics pipeline
	{
		CRY_ASSERT(shaderRenderingFlags & SHDF_ALLOWHDR);

		{
			PROFILE_FRAME(WaitForParticleRendItems);
			SyncComputeVerticesJobs();
			UnLockParticleVideoMemory(GetRenderFrameID());
		}

		pActiveGraphicsPipeline->Update(EShaderRenderingFlags(shaderRenderingFlags));

		// Creating CompiledRenderObjects should happen after Update() call of the GraphicsPipeline, as it requires access to initialized Render Targets
		// If some pipeline stage manages/retires resources used in compiled objects, they should also be handled in Update()
		pRenderView->CompileModifiedRenderObjects();

		// Sort transparent lists that might have refractive items that will require resolve passes.
		// This is done after the CompileModifiedRenderObjects we need to project render items AABB.
		pRenderView->StartOptimizeTransparentRenderItemsResolvesJob();

		pActiveGraphicsPipeline->Execute();

		//////////////////////////////////////////////////////////////////////////
		// Normally it does this:
		//  [HDR,  renderResolution] CRenderingResources::s_ptexHDRTarget ->
		//  [HDR,  outputResolution] CRenderOutput::m_pColorTarget ->
		//  [HDR, displayResolution] CRenderingDisplayContext::m_pColorTarget ->
		//  [LDR, displayResolution] BackBuffer
		//
		// Depending on the setting any of the targets can substitute an adjacent buffer, say:
		//  [HDR,  renderResolution] BackBuffer
		//
		// This would happen if the back-buffer is HDR and matches the rendering resolution
		ResolveSupersampledRendering(pActiveGraphicsPipeline);
		ResolveSubsampledOutput(pActiveGraphicsPipeline);
		ResolveHighDynamicRangeDisplay(pActiveGraphicsPipeline);
		// Everything after this location will render directly into the display buffer/resolution
		//////////////////////////////////////////////////////////////////////////

		pActiveGraphicsPipeline->ClearState();
	}

	////////////////////////////////////////////////

	CV_r_nodrawnear            = nSaveDrawNear;
	CV_r_watercaustics         = nSaveDrawCaustics;

	{
		PROFILE_FRAME(RenderViewEndFrame);
		pRenderView->SwitchUsageMode(CRenderView::eUsageModeReadingDone);
	}

	SRenderStatistics::Write().m_fRenderTime += iTimer->GetAsyncTime().GetDifferenceInSeconds(Time);

	if (CRendererCVars::CV_r_FlushToGPU >= 1)
		GetDeviceObjectFactory().FlushToGPU();

	m_maskRenderPhaseLog[m_SceneRecurseCount - 1] |= eRP_RenderScene;
}

//======================================================================================================
// Process all render item lists (can be called recursively)
void CD3D9Renderer::SubmitRenderViewForRendering(int nFlags, const SRenderingPassInfo& passInfo)
{
	ASSERT_IS_MAIN_THREAD(m_pRT)
	_smart_ptr<CRenderView> pRenderView = passInfo.GetRenderView();

	// Writing to this render view from this thread should be done.
	pRenderView->SwitchUsageMode(CRenderView::eUsageModeWritingDone);

	if (passInfo.IsGeneralPass())
	{
		if ((nFlags & SHDF_ALLOWPOSTPROCESS))
		{
			// Perform pre-process operations for the current frame
			auto& postProcessRenderItems = pRenderView->GetRenderItems(EFSLIST_PREPROCESS);
			if (!postProcessRenderItems.empty() && pRenderView->GetBatchFlags(EFSLIST_PREPROCESS) & FSPR_MASK)
			{
				const int size = static_cast<int>(postProcessRenderItems.size());

				// Sort render items as we need them
				SRendItem::mfSortPreprocess(&postProcessRenderItems[0], size);
				EF_Preprocess(&postProcessRenderItems[0], 0, size, passInfo);
			}
		}
	}

	// Setup stereo rendering context
	const bool performStereoRendering = m_pStereoRenderer->IsStereoEnabled() && !passInfo.IsRecursivePass();
	SStereoRenderContext stereoContext = {};
	if (performStereoRendering)
		stereoContext = m_pStereoRenderer->PrepareStereoRenderingContext(nFlags, passInfo);

	ExecuteRenderThreadCommand([=]()
	{
		CTimeValue timeRenderSceneBegin = iTimer->GetAsyncTime();

		// when we are in video mode, don't execute the command
		if (m_pRT->IsRenderThread() && m_pRT->m_eVideoThreadMode == SRenderThread::eVTM_Disabled)
		{
			RT_PreRenderScene(pRenderView);

			if (performStereoRendering)
			{
				m_pStereoRenderer->RenderScene(stereoContext);
			}
			else
			{
				pRenderView->SetCurrentEye(CCamera::eEye_Left);
				pRenderView->SetZoomFactor(passInfo.GetZoomFactor());
				RT_RenderScene(pRenderView);
			}

			RT_PostRenderScene(pRenderView);
		}
		else
		{
			// cleanup when showing loading render screen
			if (!pRenderView->IsRecursive())
			{
				////////////////////////////////////////////////
				// to non-thread safe remaing work for *::Render functions
				CRenderMesh::RT_PerFrameTick();
				CMotionBlur::InsertNewElements();
			}
		}

		SRenderStatistics::Write().m_Summary.sceneTime += iTimer->GetAsyncTime().GetDifferenceInSeconds(timeRenderSceneBegin);
	}, ERenderCommandFlags::None);
}

// Process all render item lists
void CD3D9Renderer::EF_EndEf3D(const int nPrecacheUpdateIdSlow, const int nPrecacheUpdateIdFast, const SRenderingPassInfo& passInfo, const int nRenderFlags)
{
	ASSERT_IS_MAIN_THREAD(m_pRT)
	m_beginFrameCount--;

	if (m_beginFrameCount < 0)
	{
		iLog->Log("Error: CRenderer::EF_EndEf3D without CRenderer::EF_StartEf");
		return;
	}

	m_streamZonesRoundId[0] = max(m_streamZonesRoundId[0], nPrecacheUpdateIdFast);
	m_streamZonesRoundId[1] = max(m_streamZonesRoundId[1], nPrecacheUpdateIdSlow);

	m_p3DEngineCommon[passInfo.ThreadID()].Update(passInfo);

	if (CV_r_NoDraw == 4)
	{
		return;
	}

	const bool bSecondaryViewport = (nRenderFlags & SHDF_SECONDARY_VIEWPORT) != 0;
	if (bSecondaryViewport)
	{
		CRenderView* pRenderView = passInfo.GetRenderView();
		CRY_ASSERT(pRenderView);

		const CCamera& cam = passInfo.GetCamera();
		pRenderView->SetCameras(&cam, 1);
		pRenderView->SetPreviousFrameCameras(&cam, 1);
	}

	int nAsyncShaders = CV_r_shadersasynccompiling;
	if (nRenderFlags & SHDF_NOASYNC)
		CV_r_shadersasynccompiling = 0;

	if (GetS3DRend().IsStereoEnabled())
	{
		GetS3DRend().ProcessScene(nRenderFlags, passInfo);
	}
	else
	{
		RenderFrame(nRenderFlags, passInfo);
	}

	CV_r_shadersasynccompiling = nAsyncShaders;
}

//////////////////////////////////////////////////////////////////////////
void CD3D9Renderer::RenderFrame(int nSceneRenderingFlags, const SRenderingPassInfo& passInfo)
{
	ASSERT_IS_MAIN_THREAD(m_pRT)

	if (!passInfo.IsRecursivePass() && (passInfo.GetRenderView()->GetCurrentEye() != CCamera::eEye_Right) && !CV_r_measureoverdraw)
	{
		bool bAllowDeferred = (nSceneRenderingFlags & SHDF_ZPASS) != 0;
		if (bAllowDeferred)
		{
			gRenDev->m_cEF.mfRefreshSystemShader("DeferredShading", CShaderMan::s_shDeferredShading);
		}
	}

	// Update per-frame params
	const bool bSecondaryViewport = (nSceneRenderingFlags & SHDF_SECONDARY_VIEWPORT) != 0;
	ReadPerFrameShaderConstants(passInfo, bSecondaryViewport);

	{
		CTimeValue time0 = iTimer->GetAsyncTime();
#ifndef _RELEASE
		if (CV_r_excludeshader->GetString()[0] != '0')
		{
			char nm[256];
			cry_strcpy(nm, CV_r_excludeshader->GetString());
			strlwr(nm);
			//m_RP.m_sExcludeShader = nm;
		}
		else
		{
			//m_RP.m_sExcludeShader = "";
		}
#endif

		SubmitRenderViewForRendering(nSceneRenderingFlags, passInfo);

		SRenderStatistics::Write().m_fSceneTimeMT += gEnv->pTimer->GetAsyncTime().GetDifferenceInSeconds(time0);
	}
}

bool CD3D9Renderer::StoreGBufferToAtlas(const RectI& rcDst, int nSrcWidth, int nSrcHeight, int nDstWidth, int nDstHeight,
                                        ITexture* pDataD, ITexture* pDataN, CGraphicsPipeline* pGraphicsPipeline)
{
	bool bRes = true;

	const CGraphicsPipelineResources& pipelineResources = pGraphicsPipeline->GetPipelineResources();

	CTexture* pGBuffD = pipelineResources.m_pTexSceneDiffuse;
	CTexture* pGBuffN = pipelineResources.m_pTexSceneNormalsMap;

	CTexture* pDstD = (CTexture*)pDataD;
	CTexture* pDstN = (CTexture*)pDataN;

	RECT SrcBox, DstBox;
	SrcBox.left = 0;
	SrcBox.top = 0;
	SrcBox.right = nSrcWidth;
	SrcBox.bottom = nSrcHeight;
	DstBox.left = rcDst.x;
	DstBox.top = rcDst.y;
	DstBox.right = rcDst.x + nDstWidth;
	DstBox.bottom = rcDst.y + nDstHeight;

	CStretchRegionPass(pGraphicsPipeline).Execute(pGBuffD, pDstD, &SrcBox, &DstBox);
	CStretchRegionPass(pGraphicsPipeline).Execute(pGBuffN, pDstN, &SrcBox, &DstBox);

	return bRes;
}

void CD3D9Renderer::EnablePipelineProfiler(bool bEnable)
{
#if defined(ENABLE_SIMPLE_GPU_TIMERS)
	m_pPipelineProfiler->SetEnabled(bEnable);
#endif
}

void CD3D9Renderer::LogShaderImportMiss(const CShader* pShader)
{
#if defined(SHADERS_SERIALIZING)
	stack_string requestLineStr, shaderList;

	if (!CRenderer::CV_r_shaderssubmitrequestline || !CRenderer::CV_r_shadersremotecompiler)
		return;

	gRenDev->m_cEF.CreateShaderExportRequestLine(pShader, requestLineStr);

#if CRY_PLATFORM_DURANGO
	shaderList = "ShaderList_Durango.txt";
#elif CRY_PLATFORM_ORBIS
	shaderList = "ShaderList_Orbis.txt";
#else
	shaderList = "ShaderList_PC.txt";
#endif

#ifdef SHADER_ASYNC_COMPILATION
	if (CRenderer::CV_r_shadersasynccompiling)
	{
		// Lazy init?
		if (!SShaderAsyncInfo::PendingList().m_Next)
		{
			SShaderAsyncInfo::PendingList().m_Next  = &SShaderAsyncInfo::PendingList();
			SShaderAsyncInfo::PendingList().m_Prev  = &SShaderAsyncInfo::PendingList();
			SShaderAsyncInfo::PendingListT().m_Next = &SShaderAsyncInfo::PendingListT();
			SShaderAsyncInfo::PendingListT().m_Prev = &SShaderAsyncInfo::PendingListT();
		}

		SShaderAsyncInfo* pAsyncRequest = new SShaderAsyncInfo;

		if (pAsyncRequest)
		{
			pAsyncRequest->m_RequestLine         = requestLineStr.c_str();
			pAsyncRequest->m_shaderList          = shaderList.c_str();
			pAsyncRequest->m_Text                = "";
			pAsyncRequest->m_bDeleteAfterRequest = true;
			CAsyncShaderTask::InsertPendingShader(pAsyncRequest);
		}
	}
	else
#endif
	{
		NRemoteCompiler::CShaderSrv::Instance().RequestLine(shaderList.c_str(), requestLineStr.c_str());
	}
#endif
}

///////////////////////////////////////////////////////////////////////////////
void CD3D9Renderer::WaitForParticleBuffer(int frameId)
{
	CRY_PROFILE_FUNCTION(PROFILE_PARTICLE);

	gcpRendD3D.GetParticleBufferSet().WaitForFence(frameId);
}
//========================================================================================================

#pragma warning(pop)
