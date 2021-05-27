// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Gpu/Particles/GpuParticleManager.h"
#include "GpuParticles.h"

CComputeParticlesStage::CComputeParticlesStage(CGraphicsPipeline& graphicsPipeline)
	: CGraphicsPipelineStage(graphicsPipeline)
	, m_oldFrameIdExecute(-1)
	, m_oldFrameIdPreDraw(-1)
	, m_oldFrameIdPostDraw(-1)
{
}

CComputeParticlesStage::~CComputeParticlesStage()
{
}

void CComputeParticlesStage::Init()
{
}

void CComputeParticlesStage::Update()
{
	CRenderView* pRenderView = RenderView();
	int CurrentFrameID = pRenderView->GetFrameId();
	if (CurrentFrameID != m_oldFrameIdExecute)
	{
		gcpRendD3D->GetGpuParticleManager()->RenderThreadUpdate(pRenderView);
		m_oldFrameIdExecute = CurrentFrameID;
	}
}

void CComputeParticlesStage::PreDraw()
{
	CRenderView* pRenderView = RenderView();
	int CurrentFrameID = pRenderView->GetFrameId();
	if (CurrentFrameID != m_oldFrameIdPreDraw)
	{
		gcpRendD3D->GetGpuParticleManager()->RenderThreadPreUpdate(pRenderView);
		m_oldFrameIdPreDraw = CurrentFrameID;
	}
}

void CComputeParticlesStage::PostDraw()
{
	CRenderView* pRenderView = RenderView();
	int CurrentFrameID = pRenderView->GetFrameId();
	if (CurrentFrameID != m_oldFrameIdPostDraw)
	{
		gcpRendD3D->GetGpuParticleManager()->RenderThreadPostUpdate(pRenderView);
		m_oldFrameIdPostDraw = CurrentFrameID;
	}
}