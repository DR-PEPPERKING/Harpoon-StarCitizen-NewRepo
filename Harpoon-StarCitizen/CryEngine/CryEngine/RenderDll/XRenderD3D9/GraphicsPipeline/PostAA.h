// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/FullscreenPass.h"

class CPostAAStage : public CGraphicsPipelineStage
{
public:
	static const EGraphicsPipelineStage StageID = eStage_PostAA;

	CPostAAStage(CGraphicsPipeline& graphicsPipeline)
		: CGraphicsPipelineStage(graphicsPipeline)
		, m_passSMAAEdgeDetection(&graphicsPipeline)
		, m_passSMAABlendWeights(&graphicsPipeline)
		, m_passSMAANeighborhoodBlending(&graphicsPipeline)
		, m_passTemporalAA(&graphicsPipeline)
		, m_passComposition(&graphicsPipeline)
		, m_passCopySRGB(&graphicsPipeline)
#ifndef _RELEASE
		, m_passAntialiasingDebug(&graphicsPipeline)
#endif
	{
		m_pPrevBackBuffers[CCamera::eEye_Left][0] = m_pPrevBackBuffers[CCamera::eEye_Right][0] = nullptr;
		m_pPrevBackBuffers[CCamera::eEye_Left][1] = m_pPrevBackBuffers[CCamera::eEye_Right][1] = nullptr;
	}

	// Width and height should be the actual dimensions of the texture to be antialiased,
	// which might be different than the provided renderview's output resolution (e.g. VR).
	void CalculateJitterOffsets(int targetWidth, int targetHeight, CRenderView* pTargetRenderView);
	void CalculateJitterOffsets(CRenderView* pRenderView)
	{
		const int32_t w = pRenderView->GetRenderResolution()[0];
		const int32_t h = pRenderView->GetRenderResolution()[1];
		CRY_ASSERT(w > 0 && h > 0);
		if (w > 0 && h > 0)
			CalculateJitterOffsets(w, h, pRenderView);
	}

	void Resize(int renderWidth, int renderHeight) override;
	void Update() override;
	void Init() override;
	void Execute();

private:
	void      ApplySMAA(CTexture*& pCurrRT, CTexture*& pDestRT);
	void      ApplySRGB(CTexture*& pCurrRT, CTexture*& pDestRT);
	void      ApplyTemporalAA(CTexture*& pCurrRT, CTexture*& pMgpuRT, uint32 aaMode);
	void      DoFinalComposition(CTexture*& pCurrRT, CTexture* pDestRT, uint32 aaMode);

	CTexture* GetAARenderTarget(const CRenderView* pRenderView, bool bCurrentFrame) const;

#ifndef _RELEASE
	void      ExecuteDebug(CTexture* pZoomRT, CTexture* pDestRT);
#endif

private:
	_smart_ptr<CTexture> m_pTexAreaSMAA;
	_smart_ptr<CTexture> m_pTexSearchSMAA;
	CTexture*            m_pPrevBackBuffers[CCamera::eEye_eCount][2];
	int                  m_lastFrameID;

	CFullscreenPass      m_passSMAAEdgeDetection;
	CFullscreenPass      m_passSMAABlendWeights;
	CFullscreenPass      m_passSMAANeighborhoodBlending;
	CFullscreenPass      m_passTemporalAA;
	CFullscreenPass      m_passComposition;
	CStretchRectPass     m_passCopySRGB;

	bool                 oldStereoEnabledState{ false };
	int                  oldAAState{ 0 };

#ifndef _RELEASE
	CFullscreenPass      m_passAntialiasingDebug;
#endif
};
