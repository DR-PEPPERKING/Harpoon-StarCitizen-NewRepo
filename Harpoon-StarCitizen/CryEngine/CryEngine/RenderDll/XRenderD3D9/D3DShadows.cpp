// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   D3DShadows.cpp : shadows support.

   Revision history:
* Created by Honich Andrey

   =============================================================================*/

#include "StdAfx.h"
#include <Cry3DEngine/IRenderNode.h>
#include "../Common/Shadow_Renderer.h"
#include "../Common/ReverseDepth.h"
#include "D3DPostProcess.h"

#include <Cry3DEngine/I3DEngine.h>

#include "GraphicsPipeline/Common/GraphicsPipelineStage.h"

#include "Common/RenderView.h"

#if defined(FEATURE_SVO_GI)
	#include "D3D_SVO.h"
#endif

#if CRY_PLATFORM_DURANGO
	#pragma warning(push)
	#pragma warning(disable : 4273)
BOOL InflateRect(LPRECT lprc, int dx, int dy)
{
	if (!lprc)
		return FALSE;

	lprc->left -= dx;
	lprc->right += dx;
	lprc->top -= dy;
	lprc->bottom += dy;

	return TRUE;
}
	#pragma warning(pop)
#endif

namespace
{
CryCriticalSection g_cDynTexLock;
}

void CD3D9Renderer::EF_PrepareShadowTasksForRenderView(const SRenderingPassInfo& passInfo)
{
	FUNCTION_PROFILER_RENDERER();
	MEMSTAT_CONTEXT(EMemStatContextType::Other, "CD3D9Renderer::EF_PrepareShadowGenRenderList");

	CRenderView* pRenderView = passInfo.GetRenderView();

	auto& arrDynLights = pRenderView->GetLightsArray(eDLT_DynamicLight);
	auto& arrDeferLights = pRenderView->GetLightsArray(eDLT_DeferredLight);

	int NumDynLights = arrDynLights.size();
	int NumDeferLights = arrDeferLights.size();
	if ((NumDynLights + NumDeferLights) <= 0)
		return;

#if defined(ENABLE_PROFILING_CODE)
	m_frameRenderStats[m_nFillThreadID].m_NumShadowPoolFrustums = 0;
	m_frameRenderStats[m_nFillThreadID].m_NumShadowPoolAllocsThisFrame = 0;
	m_frameRenderStats[m_nFillThreadID].m_NumShadowMaskChannels = 0;
#endif

	int nSunID = -1;

	{
		auto itr = arrDynLights.begin();
		for (uint32 nDynLightID = 0; itr != arrDynLights.end(); ++itr, ++nDynLightID)
		{
			EF_PrepareShadowGenForLight(pRenderView, &*itr, nDynLightID);

			if (itr->m_Flags & DLF_SUN)
				nSunID = nDynLightID;
		}
	}

	{
		auto itr = arrDeferLights.begin();
		for (uint32 nDeferLightID = NumDynLights; itr != arrDeferLights.end(); ++itr, ++nDeferLightID)
			EF_PrepareShadowGenForLight(pRenderView, &*itr, nDeferLightID);
	}

	// add custom frustums
	const bool haveSun = nSunID >= 0;
	if (haveSun)
	{
		ShadowMapFrustum** arrCustomFrustums;
		int nFrustumCount;
		gEnv->p3DEngine->GetCustomShadowMapFrustums(arrCustomFrustums, nFrustumCount);
		SRenderLight* pSun = &pRenderView->GetDynamicLight(nSunID);

		for (uint32 i = 0; i < nFrustumCount; ++i)
		{
			ShadowMapFrustum* pCurFrustum = arrCustomFrustums[i];
			IRenderViewPtr pShadowView = std::move(pCurFrustum->pOnePassShadowView);

			CRY_ASSERT(pCurFrustum && !pCurFrustum->bRestrictToRT && pCurFrustum->m_eFrustumType == ShadowMapFrustum::e_PerObject);

			if (PrepareShadowGenForFrustum(pRenderView, reinterpret_cast<CRenderView*>(pShadowView.get()), pCurFrustum, pSun, nSunID))
			{
				ShadowMapFrustumPtr pFrustumForRenderer = pCurFrustum->Clone();

				if (!pShadowView)
				{
					// allocate view if was not prepared by 3dengine
					pShadowView = pCurFrustum->GetNextAvailableShadowsView(pRenderView);
					pShadowView->SwitchUsageMode(IRenderView::eUsageModeWriting);
				}

				pRenderView->AddShadowFrustumToRender(SShadowFrustumToRender(std::move(pFrustumForRenderer), pSun, nSunID, std::move(pShadowView)));
			}

			if (pShadowView)
				pShadowView->SwitchUsageMode(IRenderView::eUsageModeWritingDone);
		}
	}

#if defined(ENABLE_PROFILING_CODE)
	CDeferredShading* pDeferredShading = pRenderView->GetGraphicsPipeline()->GetDeferredShading();
	m_frameRenderStats[m_nFillThreadID].m_NumShadowPoolFrustums += pDeferredShading->m_shadowPoolAlloc.Num();
#endif

	// Prepare frustums for tiled shading
	// Sort shadow frustums into shadow mask slices such that frustums in each slice don't overlap in screen space
	// Add a single slice for sun covering everything
	if (haveSun)
		pRenderView->m_shadows.m_frustumsPerTiledShadingSlice.emplace_back(1, std::make_pair(nullptr, Vec4(0, 0, std::numeric_limits<float>::max(), std::numeric_limits<float>::max())));
	pRenderView->m_shadows.GenerateSortedFrustumsForTiledShadingByScreenspaceOverlap();
	for (int slice = 0; slice < pRenderView->m_shadows.m_frustumsPerTiledShadingSlice.size(); ++slice)
	{
		for (auto& frustumPair : pRenderView->m_shadows.m_frustumsPerTiledShadingSlice[slice])
		{
			if (auto pFrustumToRender = frustumPair.first)
				pFrustumToRender->pLight->m_ShadowMaskIndex = slice;
		}
	}

	// Render All frustums.
	pRenderView->PostWriteShadowViews();
}

bool CD3D9Renderer::EF_PrepareShadowGenForLight(CRenderView* pRenderView, SRenderLight* pLight, int nLightID)
{
	assert((unsigned int) nLightID < (MAX_REND_LIGHTS + MAX_DEFERRED_LIGHTS));
	if ((unsigned int) nLightID >= (MAX_REND_LIGHTS + MAX_DEFERRED_LIGHTS))
	{
		//Warning("CRenderer::EF_PrepareShadowGenForLight: Too many light sources used ..."); // redundant
		return false;
	}

	if (!(pLight->m_Flags & DLF_CASTSHADOW_MAPS))
		return false;

	ShadowMapFrustum** ppSMFrustumList = pLight->m_pShadowMapFrustums;
	//assert( ppSMFrustumList != 0); fail with differed lights
	if (!ppSMFrustumList)
		return false;

	for (int nCaster = 0; *ppSMFrustumList && nCaster != MAX_GSM_LODS_NUM; ++ppSMFrustumList, ++nCaster)
	{
		ShadowMapFrustum* pCurFrustum = (*ppSMFrustumList);
		IRenderViewPtr pShadowView = std::move(pCurFrustum->pOnePassShadowView);

		CRY_ASSERT(pCurFrustum && !pCurFrustum->bRestrictToRT);

		if (PrepareShadowGenForFrustum(pRenderView, reinterpret_cast<CRenderView*>(pShadowView.get()), pCurFrustum, pLight, nLightID))
		{
			ShadowMapFrustumPtr pFrustumForRenderer = pCurFrustum->Clone();

			if (!pShadowView)
			{
				// allocate view if was not prepared by 3dengine
				pShadowView = pCurFrustum->GetNextAvailableShadowsView(pRenderView);
				pShadowView->SwitchUsageMode(IRenderView::eUsageModeWriting);
			}

			pRenderView->AddShadowFrustumToRender(SShadowFrustumToRender(std::move(pFrustumForRenderer), pLight, nLightID, std::move(pShadowView)));
		}
		
		if (pShadowView)
			pShadowView->SwitchUsageMode(IRenderView::eUsageModeWritingDone);
	}

	return true;
}

void CD3D9Renderer::PrepareShadowPool(CRenderView* pRenderView) const
{
	CDeferredShading* pDeferredShading = pRenderView->GetGraphicsPipeline()->GetDeferredShading();

	const auto nRequestedPoolSize = CRendererCVars::CV_e_ShadowsPoolSize;
	auto& shadowPoolSize = pDeferredShading->m_nShadowPoolSize;
	auto& blockPack = pDeferredShading->m_blockPack;
	auto& shadowPoolAlloc = pDeferredShading->m_shadowPoolAlloc;

	RenderLightsList& arrLights = pRenderView->GetLightsArray(eDLT_DeferredLight);

	if (shadowPoolSize != nRequestedPoolSize)
	{
		blockPack.UpdateSize(nRequestedPoolSize >> TEX_POOL_BLOCKLOGSIZE, nRequestedPoolSize >> TEX_POOL_BLOCKLOGSIZE);
		shadowPoolSize = nRequestedPoolSize;

		// clear pool and reset allocations
		blockPack.Clear();
		shadowPoolAlloc.SetUse(0);
	}

	bool forceClearPool = CRenderer::CV_r_ShadowPoolMaxFrames == 0;

	// TODO: Sort lights so allocated ones are rendered first to reduce impact of a thrash

	// Clear if forced
	if (forceClearPool || arrLights.empty())
	{
		blockPack.Clear();
		shadowPoolAlloc.SetUse(0);
	}
	else
	{
		// Clear out orphaned slots
		for (SShadowAllocData& pAlloc : shadowPoolAlloc)
		{
			if (pAlloc.isFree())
				continue;

			bool found = false;
			for (const auto& light : arrLights)
			{
				if (pAlloc.m_lightID == light.m_nEntityId)
				{
					found = true;
					break;
				}
			}

			if (!found)
			{
				blockPack.RemoveBlock(pAlloc.m_blockID);
				pAlloc.Clear();
			}
		}

	}
}

bool CD3D9Renderer::PrepareShadowGenForFrustum(CRenderView* pRenderView, CRenderView* pShadowView, ShadowMapFrustum* pCurFrustum, const SRenderLight* pLight, int nLightID)
{
	const auto frameID = pRenderView->GetFrameId();
	const auto nSides = pCurFrustum->GetNumSides();

	PROFILE_FRAME(PrepareShadowGenForFrustum);

	//validate shadow frustum
	CRY_ASSERT(pCurFrustum);
	CRY_ASSERT(!pCurFrustum->bRestrictToRT);
	CRY_ASSERT(pLight);

	if (!pCurFrustum)
		return false;
	if (pCurFrustum->m_eFrustumType == ShadowMapFrustum::e_Nearest)
		return true;
	if (pCurFrustum->IsCached() && pCurFrustum->nTexSize == 0)
		return false;

	// Static shadow map might not have any active casters, so don't reset nSideSampleMask every frame
	if (!pCurFrustum->IsCached())
	{
		if (pCurFrustum->m_eFrustumType == ShadowMapFrustum::e_GsmDynamicDistance)
			pCurFrustum->RequestSamples((1U << pCurFrustum->GetNumSides()) - 1U);
		else
		{
			for (int nSide = 0; nSide < pCurFrustum->GetNumSides(); ++nSide)
			{
				if (pCurFrustum->ShouldUpdate(nSide))
				{
					if (pShadowView && pShadowView->HasAddedItems(ERenderListID(nSide)))
					{
						pCurFrustum->RequestSample(nSide);
					}
#if 0
					// It is empty now, remove the sample-and the update request
					else
					{
						pCurFrustum->ClearUpdates(BIT(nSide));   // TODO: allow update-masking for update-rate frusta
						pCurFrustum->ClearSampleFromSide(nSide); // TODO: find out why sampling can not be deactivated for empty frusta/shadowmaps
					}
#endif
				}
#if 0
				// It was empty the last time it was updated, remove the sample-request
				else if (!pCurFrustum->nSideDrawnWithN[nSide])
				{
					pCurFrustum->ClearSampleFromSide(nSide);     // TODO: find out why sampling can not be deactivated for empty frusta/shadowmaps
				}
#endif
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	//recalculate LOF rendering params
	//////////////////////////////////////////////////////////////////////////

	//calc texture resolution and frustum resolution
	pCurFrustum->nTexSize = max(pCurFrustum->nTexSize, 32);

	pCurFrustum->nTextureWidth = pCurFrustum->nTexSize;
	pCurFrustum->nTextureHeight = pCurFrustum->nTexSize;
	pCurFrustum->clearValue = pCurFrustum->clearValue;
	pCurFrustum->nShadowMapSize = pCurFrustum->nTexSize;

	//////////////////////////////////////////////////////////////////////////
	//Select shadow buffers format
	//////////////////////////////////////////////////////////////////////////
	const ETEX_Type eTT = eTT_2D;
	ETEX_Format eTF = eTF_D24S8;  //fallback formats
	if (pCurFrustum->IsCached())
		eTF = CV_r_ShadowsCacheFormat == 0 ? eTF_D32F : eTF_D16;
	else if (CV_r_shadowtexformat == 0)
		eTF = eTF_D32F;
	else if (CV_r_shadowtexformat == 1)
		eTF = eTF_D16;

	//////////////////////////////////////////////////////////////////////////
	//assign requested texture format
	pCurFrustum->m_eReqTF = eTF;
	pCurFrustum->m_eReqTT = eTT;

	//assign owners
	pCurFrustum->pFrustumOwner = pCurFrustum;

	//////////////////////////////////////////////////////////////////////////

	{
		// Group early outs
		if ((pCurFrustum->m_eFrustumType == ShadowMapFrustum::e_GsmDynamic) & !pCurFrustum->ShouldProcess())
			return false;
		if ((pCurFrustum->m_eFrustumType == ShadowMapFrustum::e_GsmDynamic) & !pCurFrustum->ShouldUpdate())
			return true;
	}

	CCamera tmpCamera;
	for (int nS = 0; nS < nSides; nS++)
	{
		// Symmetric to RT-code, see: CShadowMapStage::PrepareShadowPasses
		if ((pCurFrustum->m_eFrustumType == ShadowMapFrustum::e_GsmDynamic) & !pCurFrustum->ShouldUpdate(nS))
			continue;

		//////////////////////////////////////////////////////////////////////////
		// Calc frustum CCamera for current frustum
		//////////////////////////////////////////////////////////////////////////
		if (!pCurFrustum->bOmniDirectionalShadow)
		{
			if (pCurFrustum->m_Flags & DLF_PROJECT)
			{
				SRenderLight instLight = *pLight;

				// Frustum angle is clamped to prevent projection matrix problems.
				// We clamp here because area lights and non-shadow casting lights can cast 180 degree light.
				CShadowUtils::GetCubemapFrustumForLight(&instLight, 0, min(2 * pLight->m_fLightFrustumAngle, 175.0f), &(pCurFrustum->mLightProjMatrix), &(pCurFrustum->mLightViewMatrix), false);
				//TF enable linear shadow space and disable this back faces for projectors
				//gRenDev->m_renderThreadInfo.m_PersFlags |= RBPF_MIRRORCULL;
			}
			else
			{
				if (pCurFrustum->m_eFrustumType == ShadowMapFrustum::e_PerObject)
				{
					AABB lsBounds = CShadowUtils::GetShadowMatrixForCasterBox(pCurFrustum->mLightProjMatrix, pCurFrustum->mLightViewMatrix, pCurFrustum, 20.f);

					// normalize filter filter kernel size to dimensions of light space bounding box
					pCurFrustum->fWidthS *= pCurFrustum->nTextureWidth / (lsBounds.max.x - lsBounds.min.x);
					pCurFrustum->fWidthT *= pCurFrustum->nTextureHeight / (lsBounds.max.y - lsBounds.min.y);
				}
				else if (pCurFrustum->m_eFrustumType != ShadowMapFrustum::e_HeightMapAO)
				{
					Matrix44 ident(IDENTITY);
					CShadowUtils::GetShadowMatrixOrtho(pCurFrustum->mLightProjMatrix, pCurFrustum->mLightViewMatrix, ident, pCurFrustum, false);
				}
			}

			//Pre-multiply matrices
			Matrix44 mViewProj = Matrix44(pCurFrustum->mLightViewMatrix) * Matrix44(pCurFrustum->mLightProjMatrix);
			pCurFrustum->mLightViewMatrix = mViewProj;
			pCurFrustum->mLightProjMatrix.SetIdentity();
		}

		// Mark this side of frustum needs rendering.
		pCurFrustum->MarkSideAsRendered(nS, frameID, pShadowView ? pShadowView->NumAddedItems(ERenderListID(nS)) : 0);
	}//nSides

	return true;
}

// setup projection texgen
CShadowUtils::SShadowsSetupInfo CD3D9Renderer::ConfigShadowTexgen(CRenderView* pRenderView, const ShadowMapFrustum* pFr, int nFrustNum, bool bScreenToLocalBasis)
{
	Matrix44A shadowMat, mTexScaleBiasMat, mLightView, mLightProj, mLightViewProj;
	CShadowUtils::SShadowsSetupInfo Data;

	//check for successful PrepareDepthMap
	if (!pFr->pDepthTex && !pFr->bUseShadowsPool)
	{
		return CShadowUtils::SShadowsSetupInfo();
	}

	//calc view inversion matrix
	//Matrix44 *pCurrViewMatrix = gRenDev->m_renderThreadInfo[m_RP.m_nProcessThreadID].m_matView->GetTop();
	//Matrix44 mInvertedView = pCurrViewMatrix->GetInverted();

	//shadow frustum adjustment matrix
	//half texel offset
	//TOFIX: should take in account real shadow atlas resolution
	float fOffsetX = 0.5f;
	float fOffsetY = 0.5f;

	const Matrix44A mClipToTexSpace = Matrix44(0.5f, 0.0f, 0.0f, 0.0f,
	                                           0.0f, -0.5f, 0.0f, 0.0f,
	                                           0.0f, 0.0f, 1.0f, 0.0f,
	                                           fOffsetX, fOffsetY, 0.0f, 1.0f);

	mTexScaleBiasMat = mClipToTexSpace;

	//FIX:remove temporarily matrices
	if ((pFr->bOmniDirectionalShadow || pFr->bUseShadowsPool) && nFrustNum > -1)
	{

		if (!pFr->bOmniDirectionalShadow)
		{
			mLightView = pFr->mLightViewMatrix;
			mLightProj.SetIdentity();
		}
		else
		{
			CShadowUtils::GetCubemapFrustum(FTYP_SHADOWOMNIPROJECTION, pFr, nFrustNum, &mLightProj, &mLightView);
		}

		float arrOffs[2];
		float arrScale[2];
		pFr->GetTexOffset(nFrustNum, arrOffs, arrScale);

		//calculate crop matrix for  frustum
		//TD: investigate proper half-texel offset with mCropView
		Matrix44 mCropView(arrScale[0], 0.0f, 0.0f, 0.0f,
		                   0.0f, arrScale[1], 0.0f, 0.0f,
		                   0.0f, 0.0f, 1.0f, 0.0f,
		                   arrOffs[0], arrOffs[1], 0.0f, 1.0f);

		// multiply the projection matrix with it
		mTexScaleBiasMat = mTexScaleBiasMat * mCropView;
	}
	else
	{
		mLightView = pFr->mLightViewMatrix;

		if (pFr->m_eFrustumType == ShadowMapFrustum::e_GsmDynamicDistance)
		{
			Matrix44 mCropView(IDENTITY);
			mCropView.m00 = pFr->shadowCascade.GetDim().x / pFr->pDepthTex->GetWidth();
			mCropView.m11 = pFr->shadowCascade.GetDim().y / pFr->pDepthTex->GetHeight();
			mCropView.m30 = pFr->shadowCascade.Min.x / pFr->pDepthTex->GetWidth();
			mCropView.m31 = pFr->shadowCascade.Min.y / pFr->pDepthTex->GetHeight();

			mTexScaleBiasMat = mTexScaleBiasMat * mCropView;
		}

		//temporarily disabled since mLightProjMatrix contains pre-multiplied matrix already
		mLightProj.SetIdentity(); //pFr->mLightProjMatrix;
		//mLightProj = pFr->mLightProjMatrix;
	}

	//currently mLightProj has already pre-multiplied LightProj matrix
	mLightViewProj = mLightView * mLightProj;

	shadowMat = /*mInvertedView * */ mLightViewProj * mTexScaleBiasMat;

	//set shadow matrix
	Data.ShadowMat = shadowMat.GetTransposed(); //TOFIX: construct projection and view matrices with Column Convention

#if defined(VOLUMETRIC_FOG_SHADOWS)
	//use cur TexGen for homogeneous position reconstruction
	if (bScreenToLocalBasis && CRenderer::CV_r_FogShadowsMode == 1)
	{
		Vec4r vWBasisX, vWBasisY, vWBasisZ, vCamPos;
		const SRenderViewport& viewport = pRenderView->GetGraphicsPipeline()->GetCurrentRenderView()->GetViewport();

		CCamera Cam = pRenderView->GetCamera(CCamera::eEye_Left);
		if (pFr->m_eFrustumType == ShadowMapFrustum::e_Nearest && m_drawNearFov > 1.0f && m_drawNearFov < 179.0f)
			Cam.SetFrustum(Cam.GetViewSurfaceX(), Cam.GetViewSurfaceZ(), DEG2RAD(m_drawNearFov), Cam.GetNearPlane(), Cam.GetFarPlane(), Cam.GetPixelAspectRatio());

		CShadowUtils::ProjectScreenToWorldExpansionBasis(Data.ShadowMat, Cam, pRenderView->m_vProjMatrixSubPixoffset,
		                                                 static_cast<float>(viewport.width), static_cast<float>(viewport.height), vWBasisX, vWBasisY, vWBasisZ, vCamPos, true);

	#pragma warning(push)
	#pragma warning(disable: 4244)
		const auto mScreenToShadow = Matrix44(vWBasisX.x, vWBasisX.y, vWBasisX.z, vWBasisX.w,
		                                      vWBasisY.x, vWBasisY.y, vWBasisY.z, vWBasisY.w,
		                                      vWBasisZ.x, vWBasisZ.y, vWBasisZ.z, vWBasisZ.w,
		                                      vCamPos.x, vCamPos.y, vCamPos.z, vCamPos.w);
	#pragma warning(pop)

		float fScreenScale = (CV_r_FogShadows == 2) ? 4.0f : 2.0f;

		Matrix44A mLocalScale;
		mLocalScale.SetIdentity();
		mLocalScale.m00 = fScreenScale;
		mLocalScale.m11 = fScreenScale;

		Data.ShadowMat = mScreenToShadow.GetTransposed() * mLocalScale;
	}
#endif

	Data.RecpFarDist = 1.f / (pFr->fFarDist);

	return Data;
}
