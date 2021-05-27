// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Common/GraphicsPipelineStage.h"
#include "Common/FullscreenPass.h"
#include "Common/UtilityPasses.h"

class CSceneDepthStage : public CGraphicsPipelineStage
{
public:
	static const EGraphicsPipelineStage StageID = eStage_SceneDepth;

	CSceneDepthStage(CGraphicsPipeline& graphicsPipeline)
		: CGraphicsPipelineStage(graphicsPipeline)
		, m_LinearizePass(&graphicsPipeline)
	{
		for (auto& pass : m_downsamplePass)
			pass.SetGraphicsPipeline(&graphicsPipeline);
		for (auto& pass : m_readback)
			pass.pass.SetGraphicsPipeline(&graphicsPipeline);
		for (auto& pass : m_DownSamplePasses)
			pass.m_pass.SetGraphicsPipeline(&graphicsPipeline);
		for (auto& pass : m_CopyDepthPasses)
			pass.m_pass.SetGraphicsPipeline(&graphicsPipeline);
	}

	~CSceneDepthStage() override
	{
		ReleaseResources();
	}

	void Init() override;
	void Update() override;
	void Execute();

	// See IRenderer::PinOcclusionBuffer
	float* PinOcclusionBuffer(Matrix44A& camera) threadsafe
	{
		TAutoLock lock(m_lock);
		SResult* pResult;
		if (m_lastPinned < kMaxResults)
		{
			CRY_ASSERT(m_numPinned != 0);
			pResult = m_result + m_lastPinned;
		}
		else if (m_lastResult < kMaxResults)
		{
			CRY_ASSERT(m_numPinned == 0);
			pResult = m_result + m_lastResult;
			m_lastPinned = m_lastResult;
		}
		else
		{
			return nullptr;
		}

		++m_numPinned;
		camera = pResult->camera;
		return pResult->data;
	}

	void UnpinOcclusionBuffer() threadsafe
	{
		TAutoLock lock(m_lock);
		CRY_ASSERT(m_lastPinned < kMaxResults && m_numPinned != 0);
		if (!--m_numPinned)
		{
			m_lastPinned = kMaxResults;
		}
	}

private:
	void ExecuteLinearization();
	void ExecuteDelinearization();
	void ExecuteReadback();

	enum EConfigurationFlags : uint8
	{
		kNone         = 0,
		kMultiRes     = BIT(0),
		kReverseDepth = BIT(1),
		kNativeDepth  = BIT(2),
		kMSAA         = BIT(3),
	};

	struct SReadback
	{
		Matrix44A           camera;
		CFullscreenPass     pass;
		float               zNear;
		float               zFar;
		EConfigurationFlags flags;

		bool                bIssued    : 1;
		bool                bCompleted : 1;
		bool                bReceived  : 1;
	};

	struct SResult
	{
		float     data[CULL_SIZEX * CULL_SIZEY];
		Matrix44A camera;
	};

	using TLock = CryCriticalSectionNonRecursive;
	using TAutoLock = CryAutoCriticalSectionNoRecursive;

	bool      IsReadbackRequired();
	CTexture* GetInputTexture(EConfigurationFlags& flags);
	bool      CreateResources(uint32 sourceWidth, uint32 sourceHeight);
	void      ReleaseResources();
	void      ConfigurePasses(CTexture* pSource, EConfigurationFlags flags);
	void      ExecutePasses(float sourceWidth, float sourceHeight, float sampledWidth, float sampledHeight);
	void      ReadbackLatestData();

	static const uint32       kMaxDownsamplePasses = 4;
	static const uint32       kMaxReadbackPasses = 4;
	static const uint32       kMaxResults = 2;

	CTexture*                 m_pTexLinearDepthReadBack[kMaxReadbackPasses];
	CTexture*                 m_pTexLinearDepthDownSample[kMaxDownsamplePasses];

	const ICVar*              m_pCheckOcclusion = nullptr;       // e_CheckOcclusion
	const ICVar*              m_pStatObjRenderTasks = nullptr;   // e_StatObjBufferRenderTasks
	const ICVar*              m_pCoverageBufferReproj = nullptr; // e_CoverageBufferReproj

	uint32                    m_resourceWidth = 0;
	uint32                    m_resourceHeight = 0;
	int32                     m_sourceTexId = 0;
	EConfigurationFlags       m_resourceFlags;

	uint32                    m_downsamplePassCount = 0;
	uint32                    m_readbackIndex = 0;

	CFullscreenPass           m_downsamplePass[kMaxDownsamplePasses];
	SReadback                 m_readback[kMaxReadbackPasses];

	TLock                     m_lock;
	uint32                    m_lastResult = kMaxResults;
	uint32                    m_lastPinned = kMaxResults;
	uint32                    m_numPinned = 0;
	SResult                   m_result[kMaxResults];

	CDepthDownsamplePass      m_DownSamplePasses[3];
	CDepthDelinearizationPass m_CopyDepthPasses[3];
	CDepthLinearizationPass   m_LinearizePass;
};
