// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CompiledRenderObject.h"

#include <../Common/PostProcess/PostEffects.h>

//////////////////////////////////////////////////////////////////////////
#include "GraphicsPipeline/ShadowMap.h"
#include "GraphicsPipeline/SceneGBuffer.h"
#include "GraphicsPipeline/ComputeSkinning.h"

//////////////////////////////////////////////////////////////////////////
CRenderObjectsPools* CPermanentRenderObject::s_pPools;

CRenderObjectsPools* CCompiledRenderObject::s_pPools = 0;
CryCriticalSectionNonRecursive CCompiledRenderObject::m_drawCallInfoLock;

//////////////////////////////////////////////////////////////////////////
CRenderObjectsPools::CRenderObjectsPools()
{
}

//////////////////////////////////////////////////////////////////////////
CRenderObjectsPools::~CRenderObjectsPools()
{
}
//////////////////////////////////////////////////////////////////////////
CConstantBufferPtr CRenderObjectsPools::AllocatePerDrawConstantBuffer()
{
	if (!m_freeConstantBuffers.empty())
	{
		CConstantBufferPtr ptr = std::move(m_freeConstantBuffers.back());
		m_freeConstantBuffers.pop_back();

		CRY_ASSERT(!ptr || (ptr->m_nRefCount == 1), "Someones hold a ref-count, can't reuse ConstantBuffer!");
		CRY_ASSERT(ptr && !ptr->IsNullBuffer(), "Invalid cached pointer");
		return ptr;
	}

	static_assert(
		sizeof(HLSL_PerDrawConstantBuffer_TeVe) >= sizeof(HLSL_PerDrawConstantBuffer_Skin) &&
		sizeof(HLSL_PerDrawConstantBuffer_TeVe) >= sizeof(HLSL_PerDrawConstantBuffer_Base),
		"HLSL_PerDrawConstantBuffer_TerrainVegetation has not the largest constant buffer size."
	);

	auto pCB = gcpRendD3D->m_DevBufMan.CreateConstantBuffer(sizeof(HLSL_PerDrawConstantBuffer_TeVe));
	if (pCB) pCB->SetDebugName("Generic Per-Draw CB");

	return pCB;
}

//////////////////////////////////////////////////////////////////////////
void CRenderObjectsPools::FreePerDrawConstantBuffer(CConstantBufferPtr&& buffer)
{
	if (buffer && !buffer->IsNullBuffer())
	{
		// Constant buffer can still be temporary used CRY_ASSERT(buffer->m_nRefCount == 1, "Attempt to free a buffer that is still used elsewhere");
		m_freeConstantBuffers.emplace_back(std::move(buffer));
	}
}

//////////////////////////////////////////////////////////////////////////
CCompiledRenderObject::~CCompiledRenderObject()
{
	SAFE_RELEASE(m_shaderItem.m_pShader);
	SAFE_RELEASE(m_shaderItem.m_pShaderResources);

	m_pRenderElement = 0;
	m_StencilRef = 0;
	m_perDrawInstances = 1;

	if (m_bOwnPerInstanceCB)
	{
		CRY_ASSERT(m_perDrawCB, "CompiledRenderObject tagged as owning a buffer, but no buffer present");
		if (m_perDrawCB)
		{
			s_pPools->FreePerDrawConstantBuffer(std::move(m_perDrawCB));
		}
	}

	m_perDrawCB.reset();

	m_materialResourceSet.reset();

	for (int i = 0; i < eStage_SCENE_NUM; i++)
	{
		m_pso[i] = DevicePipelineStatesArray();
	}

	m_compilationDirtyFlags[0] = m_compilationDirtyFlags[1] = eObjCompilationOption_All;
}

//////////////////////////////////////////////////////////////////////////
void CCompiledRenderObject::Init(const SShaderItem& shaderItem, CRenderElement* pRE)
{
	SAFE_RELEASE(m_shaderItem.m_pShader);
	SAFE_RELEASE(m_shaderItem.m_pShaderResources);

	m_pRenderElement = pRE;
	m_shaderItem = shaderItem;

	if (m_shaderItem.m_pShader)
		m_shaderItem.m_pShader->AddRef();
	if (m_shaderItem.m_pShaderResources)
		m_shaderItem.m_pShaderResources->AddRef();
}

//////////////////////////////////////////////////////////////////////////
bool CCompiledRenderObject::CheckDynamicInstancing(const SGraphicsPipelinePassContext& RESTRICT_REFERENCE passContext, const CCompiledRenderObject* RESTRICT_POINTER pNextObject) const
{
	if (!m_bDynamicInstancingPossible || !pNextObject->m_bDynamicInstancingPossible)
		return false;

	if (m_pRenderElement != pNextObject->m_pRenderElement)
		return false;

	if (m_materialResourceSet != pNextObject->m_materialResourceSet)
		return false;

	// Check that vegetation bending match
	if (m_pRO->GetBendingData() != pNextObject->m_pRO->GetBendingData())
		return false;

	// Do not instance vegetation across different terrain sectors
	if (m_pRO->m_data.m_pTerrainSectorTextureInfo != pNextObject->m_pRO->m_data.m_pTerrainSectorTextureInfo)
		return false;

	if (m_pso[passContext.stageID][passContext.passID] != pNextObject->m_pso[passContext.stageID][passContext.passID])
		return false;

	return true;
}

//////////////////////////////////////////////////////////////////////////
// allocate and fill buffer
void CCompiledRenderObject::UpdatePerDrawCB(void* pData, size_t size)
{
	if (!m_perDrawCB)
	{
		CRY_ASSERT(!m_bOwnPerInstanceCB, "CompiledRenderObject tagged as owning a buffer, but no buffer present");
		if (!(m_perDrawCB = s_pPools->AllocatePerDrawConstantBuffer()))
		{
			CRY_ASSERT(false, "Warning! Running out of memory for ConstantBuffers!");
			return;
		}
		m_bOwnPerInstanceCB = true;
	}

	CRY_ASSERT(!m_perDrawCB->IsNullBuffer(), "Not allowed to write into the Null resource!");
	m_perDrawCB->UpdateBuffer(pData, size);

}

//////////////////////////////////////////////////////////////////////////
void CCompiledRenderObject::CompilePerDrawCB(CRenderObject* pRenderObject, const CRenderObject::SInstanceInfo& instanceInfo, uint64 objFlags)
{
	const CCompiledRenderObject* pRootCompiled = pRenderObject->m_pCompiledObject;
	if (pRootCompiled && pRootCompiled != this && pRootCompiled->m_perDrawCB)
	{
		CRY_ASSERT(pRenderObject->m_bPermanent);

		// If root object have per instance constant buffer, share ours with root compiled object.
		m_bOwnPerInstanceCB = false;
		m_perDrawCB = pRootCompiled->m_perDrawCB;
		m_rootConstants = pRootCompiled->m_rootConstants;
		return;
	}

	m_bDynamicInstancingPossible = CRendererCVars::CV_r_geominstancing != 0;
	if (!pRenderObject->m_Instances.empty())
		m_bDynamicInstancingPossible = false;

	// Alpha Test
	float dissolve = 0;
	if (objFlags & (FOB_DISSOLVE_OUT | FOB_DISSOLVE))
	{
		dissolve = float(pRenderObject->m_DissolveRef) * (1.0f / 255.0f);
		m_bDynamicInstancingPossible = false;
	}
	float dissolveOut = (objFlags & FOB_DISSOLVE_OUT) ? 1.0f : -1.0f;
	dissolve *= dissolveOut;

	float tessellationPatchIDOffset = alias_cast<float>(m_TessellationPatchIDOffset);

	// silhouette color
	const uint32 params = pRenderObject->m_data.m_nHUDSilhouetteParams;
	const Vec4 silhouetteColor(
		float((params & 0xff000000) >> 24) * (1.0f / 255.0f),
		float((params & 0x00ff0000) >> 16) * (1.0f / 255.0f),
		float((params & 0x0000ff00) >> 8) * (1.0f / 255.0f),
		float((params & 0x000000ff)) * (1.0f / 255.0f));

	Matrix44A objPrevMatr;
	bool bMotionBlurMatrix = CMotionBlur::GetPrevObjToWorldMat(pRenderObject, objFlags, objPrevMatr);
	if (bMotionBlurMatrix)
		m_bDynamicInstancingPossible = false;

	// Common shader per instance data.
	const auto& matrix      = instanceInfo.m_Matrix;
	const auto& bendingData = instanceInfo.m_Bending;

	m_instanceData.matrix = matrix;
	m_instanceData.dissolve = dissolve;
	m_instanceData.tesselationPatchId = tessellationPatchIDOffset;
	m_instanceData.vegetationBendingScale = bendingData.scale;
	m_instanceData.vegetationBendingRadius = bendingData.verticalRadius;

	//////////////////////////////////////////////////////////////////////////
	// wrinkle blending
	bool bHasWrinkleBending = false;
	if (m_shaderItem.m_pShader)
		bHasWrinkleBending = (m_shaderItem.m_pShader->GetFlags2() & EF2_WRINKLE_BLENDING) != 0;

	// NOTE: Hack-fix to match CB/SR data to the shader
	// m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%TEMP_TERRAIN"   , (uint64)0x200000000ULL));
	// m_pSCGFlagLegacyFix.insert(MapNameFlagsItor::value_type("%TEMP_VEGETATION", (uint64)0x400000000ULL));
	const uint64 nMaskGenFX = (*((CShader*)this->m_shaderItem.m_pShader)).m_nMaskGenFX;

	// (%TEMP_TERRAIN || (%TEMP_VEGETATION && %_RT_BLEND_WITH_TERRAIN_COLOR))
	if ((nMaskGenFX & 0x200000000ULL) ||
		(nMaskGenFX & 0x400000000ULL && objFlags & FOB_BLEND_WITH_TERRAIN_COLOR))
	{
		m_bDynamicInstancingPossible = false; //#TODO fix support of dynamic instancing for vegetation

		// NOTE: Get aligned stack-space (pointer and size aligned to manager's alignment requirement)
		CryStackAllocWithSize(HLSL_PerDrawConstantBuffer_TeVe, cb, CDeviceBufferManager::AlignBufferSizeForStreaming);

		cb->CD_WorldMatrix = matrix;
		cb->CD_PrevWorldMatrix = Matrix34(objPrevMatr);

		// [x=VegetationBendingVerticalRadius, y=VegetationBendingScale, z=tessellation patch id offset, w=dissolve]
		cb->CD_CustomData =
			Vec4(
				bendingData.verticalRadius,
				bendingData.scale,
				tessellationPatchIDOffset,
				dissolve
			);

		// Fill terrain texture info if present
		SSectorTextureSet TerrainSectorTextureInfo;
		float RenderObjMaxViewDistance = 0.0f;
		if (SRenderObjData* pOD = pRenderObject->GetObjData())
		{
			if (pOD->m_pTerrainSectorTextureInfo)
				TerrainSectorTextureInfo = *pOD->m_pTerrainSectorTextureInfo;

			RenderObjMaxViewDistance = pOD->m_fMaxViewDistance; // Obj view max distance
		}
		cb->CD_BlendTerrainColInfo[0] = TerrainSectorTextureInfo.fTexOffsetX;
		cb->CD_BlendTerrainColInfo[1] = TerrainSectorTextureInfo.fTexOffsetY;
		cb->CD_BlendTerrainColInfo[2] = TerrainSectorTextureInfo.fTexScale;
		cb->CD_BlendTerrainColInfo[3] = RenderObjMaxViewDistance;

		// Fill terrain layer info if present
		Matrix44f TerrainLayerInfo = Matrix44f(ZERO);
		if (float* pData = (float*)m_pRenderElement->m_CustomData)
		{
			TerrainLayerInfo = *(Matrix44f*)pData;
		}
		cb->CD_TerrainLayerInfo = TerrainLayerInfo;

		cb->CD_CustomData1 = silhouetteColor;
		cb->CD_CustomData2.x = alias_cast<float>(pRenderObject->m_editorSelectionID);

		UpdatePerDrawCB(cb, sizeof(HLSL_PerDrawConstantBuffer_TeVe));
	}
	// (%_RT_SKELETON_SSD || %_RT_SKELETON_SSD_LINEAR || %WRINKLE_BLENDING)
	else if ((objFlags & FOB_SKINNED) || bHasWrinkleBending)
	{
		m_bDynamicInstancingPossible = false;

		// NOTE: Get aligned stack-space (pointer and size aligned to manager's alignment requirement)
		CryStackAllocWithSizeCleared(HLSL_PerDrawConstantBuffer_Skin, cb, CDeviceBufferManager::AlignBufferSizeForStreaming);

		cb->CD_WorldMatrix = matrix;
		cb->CD_PrevWorldMatrix = Matrix34(objPrevMatr);

		// [x=VegetationBendingVerticalRadius, y=VegetationBendingScale, z=tessellation patch id offset, w=dissolve]
		cb->CD_CustomData = Vec4(0, 0, tessellationPatchIDOffset, dissolve);

		if (SRenderObjData* pOD = pRenderObject->GetObjData())
		{
			// Skinning precision offset
			if (pOD->m_pSkinningData)
			{
				cb->CD_SkinningInfo = Vec4(pOD->m_pSkinningData->vecAdditionalOffset, 0.0f);
			}
			// wrinkles mask
			if (pOD->m_pShaderParams)
			{
				SShaderParam::GetValue(ECGP_PI_WrinklesMask0, const_cast<DynArray<SShaderParam>*>(pOD->m_pShaderParams), &cb->CD_WrinklesMask0[0], 0);
				SShaderParam::GetValue(ECGP_PI_WrinklesMask1, const_cast<DynArray<SShaderParam>*>(pOD->m_pShaderParams), &cb->CD_WrinklesMask1[0], 0);
				SShaderParam::GetValue(ECGP_PI_WrinklesMask2, const_cast<DynArray<SShaderParam>*>(pOD->m_pShaderParams), &cb->CD_WrinklesMask2[0], 0);
			}
		}

		if (m_pRenderElement->mfGetType() == eDATA_Mesh)
		{
			// Skinning extra weights
			cb->CD_SkinningInfo[3] = ((CREMeshImpl*)m_pRenderElement)->m_pRenderMesh->m_extraBonesBuffer.IsNullBuffer() ? 0.0f : 1.0f;
		}

		cb->CD_CustomData1 = silhouetteColor;
		cb->CD_CustomData2.x = alias_cast<float>(pRenderObject->m_editorSelectionID);

		UpdatePerDrawCB(cb, sizeof(HLSL_PerDrawConstantBuffer_Skin));
	}
	// default base per instance buffer
	else
	{
		// NOTE: Get aligned stack-space (pointer and size aligned to manager's alignment requirement)
		CryStackAllocWithSize(HLSL_PerDrawConstantBuffer_Base, cb, CDeviceBufferManager::AlignBufferSizeForStreaming);

		cb->CD_WorldMatrix = matrix;
		cb->CD_PrevWorldMatrix = Matrix34(objPrevMatr);

		// [x=VegetationBendingVerticalRadius, y=VegetationBendingScale, z=tessellation patch id offset, w=dissolve]
		cb->CD_CustomData =
			Vec4(
				bendingData.verticalRadius,
				bendingData.scale,
				tessellationPatchIDOffset,
				dissolve
			);

		cb->CD_CustomData1 = silhouetteColor;
		cb->CD_CustomData2.x = alias_cast<float>(pRenderObject->m_editorSelectionID);

		ColorF ambColor = instanceInfo.m_AmbColor;
		uint32 ambColorPacked =
			((static_cast<uint32>(ambColor.r * 255.0f) & 0xFF) << 24)
			| ((static_cast<uint32>(ambColor.g * 255.0f) & 0xFF) << 16)
			| ((static_cast<uint32>(ambColor.b * 255.0f) & 0xFF) << 8)
			| (static_cast<uint32>(ambColor.a * 255.0f) & 0xFF);
		cb->CD_CustomData2.y = alias_cast<float>(ambColorPacked);

		UpdatePerDrawCB(cb, sizeof(HLSL_PerDrawConstantBuffer_Base));
	}
}

void CCompiledRenderObject::CompilePerInstanceCB(CRenderObject* pRenderObject, bool bForce)
{
	size_t nSrcInsts = pRenderObject->m_Instances.size();
	if (m_perDrawInstances == nSrcInsts && !bForce)
	{
		return;
	}

	assert(nSrcInsts != 0 || m_bDynamicInstancingPossible);
	m_perDrawInstances = std::max(nSrcInsts, (size_t)1);

	// Allocate temporary aligned array of instances on stack.
	CryStackAllocWithSizeVector(SPerInstanceShaderData, m_perDrawInstances, tempInstanceBuffer, CDeviceBufferManager::AlignBufferSizeForStreaming); //May crash without m_perDrawInstances+1
	if (nSrcInsts == 0)
	{
		// Only 1 instance i in case of the dynamic instancing
		tempInstanceBuffer[0] = m_instanceData;
	}
	else
	{
		for (size_t i = 0; i < m_perDrawInstances; i++)
		{
			SPerInstanceShaderData& inst = tempInstanceBuffer[i];
			inst = m_instanceData;
			if (i < nSrcInsts)
			{
				inst.matrix = pRenderObject->m_Instances[i].m_Matrix;
			}
		}
	}

	// NOTE: The pointer and the size is optimal aligned when sizeof(SPerInstanceShaderData) is optimal aligned
	assert(sizeof(SPerInstanceShaderData) == Align(sizeof(SPerInstanceShaderData), CRY_PLATFORM_ALIGNMENT));
	size_t nSize = m_perDrawInstances * sizeof(SPerInstanceShaderData);

	CConstantBufferPtr pCB = gcpRendD3D.m_DevBufMan.CreateConstantBuffer(nSize);
	if (pCB)
	{
		pCB->UpdateBuffer(tempInstanceBuffer, nSize);
		pCB->SetDebugName("Generic Per-Instance CB");
		m_pInstancingConstBuffer = std::move(pCB);
	}


	
}

//////////////////////////////////////////////////////////////////////////
void CCompiledRenderObject::CompilePerDrawExtraResources(CRenderObject* pRenderObject, CRenderView* pRenderView)
{
	if (!m_bHasTessellation && !pRenderObject->m_data.m_pSkinningData) // only needed for skinning and tessellation at the moment
	{
		m_perDrawExtraResources = CSceneRenderPass::GetDefaulDrawExtraResourceSet();
		CRY_ASSERT(m_perDrawExtraResources && m_perDrawExtraResources->IsValid(), "Bad shared default resources");
		return;
	}

	CDeviceResourceSetDesc perInstanceExtraResources(CSceneRenderPass::GetDefaultDrawExtraResourceLayout(), nullptr, nullptr);

	if (SSkinningData* pSkinningData = pRenderObject->m_data.m_pSkinningData)
	{
		CD3D9Renderer::SCharacterInstanceCB* const pCurSkinningData = alias_cast<CD3D9Renderer::SCharacterInstanceCB*>(pSkinningData->pCharInstCB);

		bool bDoComputeDeformation = (pSkinningData->nHWSkinningFlags & eHWS_DC_deformation_Skinning ? true : false);
		if (bDoComputeDeformation)
		{
			CGpuBuffer* pBuffer = gcpRendD3D->GetComputeSkinningStorage()->GetOutputVertices(pSkinningData->pCustomTag);

			if (pBuffer)
			{
				perInstanceExtraResources.SetBuffer(EReservedTextureSlot_ComputeSkinVerts, pBuffer, EDefaultResourceViews::Default, EShaderStage_Vertex);
			}
		}
		else
		{
			perInstanceExtraResources.SetConstantBuffer(eConstantBufferShaderSlot_SkinQuat, pCurSkinningData->boneTransformsBuffer, EShaderStage_Vertex);

			if (pSkinningData->pPreviousSkinningRenderData)
			{
				CD3D9Renderer::SCharacterInstanceCB* const pPrevSkinningData = alias_cast<CD3D9Renderer::SCharacterInstanceCB*>(pSkinningData->pPreviousSkinningRenderData->pCharInstCB);
				perInstanceExtraResources.SetConstantBuffer(eConstantBufferShaderSlot_SkinQuatPrev, pPrevSkinningData->boneTransformsBuffer, EShaderStage_Vertex);
			}

			perInstanceExtraResources.SetBuffer(EReservedTextureSlot_SkinExtraWeights, m_pExtraSkinWeights, EDefaultResourceViews::Default, EShaderStage_Vertex);
		}
	}

	if (m_bHasTessellation && m_pTessellationAdjacencyBuffer)
	{
		perInstanceExtraResources.SetBuffer(EReservedTextureSlot_AdjacencyInfo, m_pTessellationAdjacencyBuffer, EDefaultResourceViews::Default, EShaderStage_Domain);
	}

	m_perDrawExtraResources = GetDeviceObjectFactory().CreateResourceSet(CDeviceResourceSet::EFlags_ForceSetAllState);
	m_perDrawExtraResources->Update(perInstanceExtraResources);
}

//////////////////////////////////////////////////////////////////////////
#ifdef DO_RENDERSTATS
void CCompiledRenderObject::TrackStats(const SGraphicsPipelinePassContext& RESTRICT_REFERENCE passContext, CRenderObject* pRenderObject) const
{
	AUTO_LOCK_T(CryCriticalSectionNonRecursive, m_drawCallInfoLock);
	CD3D9Renderer* pRenderer = gcpRendD3D;
	if (pRenderObject)
	{
		CRenderElement* pElemBase = pRenderObject->m_pCompiledObject->m_pRenderElement;

		if (pElemBase && pElemBase->mfGetType() == eDATA_Mesh)
		{
			CREMeshImpl* pMesh = (CREMeshImpl*)pElemBase;
			IRenderMesh* pRenderMesh = pMesh ? pMesh->m_pRenderMesh : nullptr;

			if (IRenderNode* pRenderNode = (IRenderNode*)pRenderObject->m_pRenderNode)
			{
				//Add to per node map for r_stats 6
				if (pRenderer->CV_r_stats == 6 || pRenderer->m_pDebugRenderNode || pRenderer->m_bCollectDrawCallsInfoPerNode)
				{
					auto& drawCallsInfoPerNode = *passContext.pDrawCallInfoPerNode;
					auto pItor = drawCallsInfoPerNode.find(pRenderNode);

					if (pItor != drawCallsInfoPerNode.end())
					{
						CRenderer::SDrawCallCountInfo& pInfoDP = pItor->second;
						pInfoDP.Update(pRenderObject, pRenderMesh, passContext.techniqueID);
					}
					else
					{
						CRenderer::SDrawCallCountInfo pInfoDP;
						pInfoDP.Update(pRenderObject, pRenderMesh, passContext.techniqueID);
						drawCallsInfoPerNode.insert(std::make_pair(pRenderNode, pInfoDP));
					}
				}

				//Add to per mesh map for perfHUD / Statoscope
				if (pRenderer->m_bCollectDrawCallsInfo)
				{
					IRenderer::RNDrawcallsMapMesh& drawCallsInfoPerMesh = *passContext.pDrawCallInfoPerMesh;
					auto pItor = drawCallsInfoPerMesh.find(pRenderMesh);

					if (pItor != drawCallsInfoPerMesh.end())
					{
						CRenderer::SDrawCallCountInfo& pInfoDP = pItor->second;
						pInfoDP.Update(pRenderObject, pRenderMesh, passContext.techniqueID);
					}
					else
					{
						CRenderer::SDrawCallCountInfo pInfoDP;
						pInfoDP.Update(pRenderObject, pRenderMesh, passContext.techniqueID);
						drawCallsInfoPerMesh.insert(std::make_pair(pRenderMesh, pInfoDP));
					}
				}
			}
		}
	}
}
#endif

//////////////////////////////////////////////////////////////////////////
bool CCompiledRenderObject::Compile(EObjectCompilationOptions compilationOptions, const CRenderObject::SInstanceInfo& instanceInfo, uint64 objFlags, ERenderElementFlags elmFlags, const AABB& localAABB, CRenderView* pRenderView)
{
	CRY_PROFILE_FUNCTION(PROFILE_RENDERER);

	const CRenderObject::ERenderPassType passType = pRenderView->GetPassType();

	compilationOptions |= m_compilationDirtyFlags[passType];
	if (compilationOptions == eObjCompilationOption_None)
		return true;
	
	CRenderObject* pRenderObject = m_pRO;
	const bool bMuteWarnings = true;  // @TODO: Remove later

	bool updateInstanceDataOnly = (compilationOptions & eObjCompilationOption_PerDrawDataOnly) == compilationOptions;

	m_bCustomRenderElement = false;

	// Optimization to only update per instance constant buffer and not recompile PSO,
	// Object must be fully compiled already for this flag to have a per instance only effect.
	const bool bInstanceDataUpdateOnly = !m_bIncomplete && updateInstanceDataOnly;

	// Only objects with RenderElements can be compiled
	if (!m_pRenderElement)
	{
		if (!bMuteWarnings) Warning("[CCompiledRenderObject] Compile failed, no render element");
		return true;
	}

	const EDataType reType = m_pRenderElement->mfGetType();

	if (compilationOptions & eObjCompilationOption_PerDrawConstantBuffer && reType != eDATA_Particle)
	{
		// Update AABB by tranforming from local space
		const auto& camera = pRenderView->GetCamera(pRenderView->GetCurrentEye());
		m_aabb = pRenderObject->TransformAABB(objFlags, localAABB, camera.GetPosition());
	}

	const bool bMeshCompatibleRenderElement = reType == eDATA_Mesh || reType == eDATA_Terrain || reType == eDATA_GeomCache || reType == eDATA_ClientPoly;
	if (!bMeshCompatibleRenderElement)
	{
		// Custom render elements cannot be dynamically instanced.
		m_bDynamicInstancingPossible = false;

		// Compile render element specific data.
		m_bCustomRenderElement = true;
		const bool bCompiled = m_pRenderElement->Compile(pRenderObject, objFlags, elmFlags, localAABB, pRenderView, updateInstanceDataOnly);
		m_bIncomplete = !bCompiled;

		return bCompiled;
	}

	CRenderElement::SGeometryInfo geomInfo;

	if (!bInstanceDataUpdateOnly) // first update only: needed for per instance buffers
	{
		const bool bSupportTessellation = (objFlags & FOB_ALLOW_TESSELLATION) && SDeviceObjectHelpers::CheckTessellationSupport(m_shaderItem);
		geomInfo.bonesRemapGUID = (pRenderObject->m_data.m_pSkinningData) ? pRenderObject->m_data.m_pSkinningData->remapGUID : 0;
		if (!m_pRenderElement->GetGeometryInfo(geomInfo, bSupportTessellation))
		{
			if (!bMuteWarnings) Warning("[CCompiledRenderObject] Compile failed, GetGeometryInfo failed");
			m_bIncomplete = true;
			return false;
		}

		CRY_ASSERT(geomInfo.nNumVertexStreams > 0);
		CRY_ASSERT(geomInfo.indexStream.hStream != 0 && geomInfo.indexStream.hStream != ~0u);
		CRY_ASSERT(geomInfo.vertexStreams[0].hStream != 0 && geomInfo.vertexStreams[0].hStream != ~0u);

		m_bHasTessellation = bSupportTessellation;
		m_TessellationPatchIDOffset = geomInfo.nTessellationPatchIDOffset;

		m_pExtraSkinWeights = reinterpret_cast<CGpuBuffer*>(geomInfo.pSkinningExtraBonesBuffer);
		m_pTessellationAdjacencyBuffer = reinterpret_cast<CGpuBuffer*>(geomInfo.pTessellationAdjacencyBuffer);
	}

	if (compilationOptions & eObjCompilationOption_PerDrawConstantBuffer)
	{
		CompilePerDrawCB(pRenderObject, instanceInfo, objFlags);
		m_compilationDirtyFlags[passType] &= ~eObjCompilationOption_PerDrawConstantBuffer;
	}

	if ((!pRenderObject->m_Instances.empty() || m_bDynamicInstancingPossible))
		CompilePerInstanceCB(pRenderObject, m_bDynamicInstancingPossible);

	if (compilationOptions & eObjCompilationOption_PerDrawExtraResources)
	{
		CompilePerDrawExtraResources(pRenderObject, pRenderView);
		m_compilationDirtyFlags[passType] &= ~eObjCompilationOption_PerDrawExtraResources;
	}

	// Data may come in later
	if (!m_perDrawCB || !m_perDrawExtraResources || !m_perDrawExtraResources->IsValid())
	{
		m_bIncomplete = true;
		return false;
	}

	if (bInstanceDataUpdateOnly)
	{
		// Issue the barriers on the core command-list, which executes directly before the Draw()s in multi-threaded jobs
		PrepareForUse(GetDeviceObjectFactory().GetCoreCommandList(), true);
		return true;
	}

	CShaderResources* RESTRICT_POINTER pResources = static_cast<CShaderResources*>(m_shaderItem.m_pShaderResources);
	assert(pResources);

	if (!pResources || !pResources->m_pCompiledResourceSet)
	{
		if (!bMuteWarnings) Warning("[CCompiledRenderObject] Compile failed, invalid resource set");
		m_bIncomplete = true;

		return false;
	}
	m_materialResourceSet = pResources->m_pCompiledResourceSet;

	// Fill stream pointers.
	if (compilationOptions & eObjCompilationOption_InputStreams)
	{
		m_indexStreamSet = GetDeviceObjectFactory().CreateIndexStreamSet(&geomInfo.indexStream);
		m_vertexStreamSet = GetDeviceObjectFactory().CreateVertexStreamSet(geomInfo.nNumVertexStreams, &geomInfo.vertexStreams[0]);

		m_nNumVertexStreams = geomInfo.nNumVertexStreams;
		m_nLastVertexStreamSlot = geomInfo.CalcLastStreamSlot();

		m_drawParams[eDrawParam_General].m_nNumIndices = m_drawParams[eDrawParam_Shadow].m_nNumIndices = geomInfo.nNumIndices;
		m_drawParams[eDrawParam_General].m_nStartIndex = m_drawParams[eDrawParam_Shadow].m_nStartIndex = geomInfo.nFirstIndex;
		m_drawParams[eDrawParam_General].m_nVerticesCount = m_drawParams[eDrawParam_Shadow].m_nVerticesCount = geomInfo.nNumVertices;

		m_compilationDirtyFlags[passType] &= ~eObjCompilationOption_InputStreams;
	}

	if (pRenderObject->m_bPermanent && reType == eDATA_Mesh)
	{
		CREMeshImpl* pRE = ((CREMeshImpl*)m_pRenderElement);
		pRE->m_pRenderMesh->AddShadowPassMergedChunkIndicesAndVertices(pRE->m_pChunk, pRenderObject->m_pCurrMaterial, m_drawParams[eDrawParam_Shadow].m_nVerticesCount, m_drawParams[eDrawParam_Shadow].m_nNumIndices);
	}

	// Stencil ref value
	uint8 stencilRef = 0; // @TODO: get from CRNTmpData::SRNUserData::m_pClipVolume::GetStencilRef
	m_StencilRef = CRenderer::CV_r_VisAreaClipLightsPerPixel ? 0 : (stencilRef | BIT_STENCIL_INSIDE_CLIPVOLUME);
	m_StencilRef |= !(objFlags & FOB_DYNAMIC_OBJECT) ? BIT_STENCIL_RESERVED : 0;
	const bool bAllowTerrainLayerBlending = CRendererCVars::CV_e_TerrainBlendingDebug == 2 ||
	                                        (CRendererCVars::CV_e_TerrainBlendingDebug == 0 && (objFlags & FOB_ALLOW_TERRAIN_LAYER_BLEND));
	const bool bTerrain = pRenderObject->m_data.m_pTerrainSectorTextureInfo != nullptr;
	m_StencilRef |= (bAllowTerrainLayerBlending || bTerrain) ? BIT_STENCIL_ALLOW_TERRAINLAYERBLEND : 0;
	m_StencilRef |= (objFlags & FOB_ALLOW_DECAL_BLEND) ? BIT_STENCIL_ALLOW_DECALBLEND : 0;

	if (m_shaderItem.m_pShader)
	{
		// Helps sort compiling order of the shaders.
		static_cast<CShader*>(m_shaderItem.m_pShader)->UpdateMinVisibleDistance(pRenderObject->m_fDistance);
	}

	// Create Pipeline States
	if (compilationOptions & eObjCompilationOption_PipelineState)
	{
		SGraphicsPipelineStateDescription psoDescription(pRenderObject, objFlags, elmFlags, m_shaderItem, TTYPE_GENERAL, geomInfo.eVertFormat, VSM_NONE /*geomInfo.CalcStreamMask()*/, ERenderPrimitiveType(geomInfo.primitiveType));

		const bool bEnabledInstancing = m_perDrawInstances > 1;
		if (bEnabledInstancing && m_pInstancingConstBuffer)
		{
			//#TODO: Rename HWSR_ENVIRONMENT_CUBEMAP to HWSR_GEOM_INSTANCING
			psoDescription.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_ENVIRONMENT_CUBEMAP];  // Enable flag to use static instancing
		}

		if (pRenderView->IsBillboardGenView())
		{
			psoDescription.objectRuntimeMask |= g_HWSR_MaskBit[HWSR_SPRITE];      // Enable flag to output alpha in G-Buffer shader
		}

		if (!pRenderView->GetGraphicsPipeline()->CreatePipelineStates(m_pso, psoDescription, pResources->m_pipelineStateCache.get()))
		{
			m_bIncomplete = true;

			if (!CRenderer::CV_r_shadersAllowCompilation)
			{
				if (!bMuteWarnings) Warning("[CCompiledRenderObject] Compile failed, PSO creation failed");
				return true;
			}
			else
			{
				return false;  // Shaders might still compile; try recompiling object later
			}
		}

		m_compilationDirtyFlags[passType] &= ~eObjCompilationOption_PipelineState;
	}

	m_bIncomplete = false;

	// Issue the barriers on the core command-list, which executes directly before the Draw()s in multi-threaded jobs
	PrepareForUse(GetDeviceObjectFactory().GetCoreCommandList(), false);
	return true;
}

void CCompiledRenderObject::PrepareForUse(CDeviceCommandListRef RESTRICT_REFERENCE commandList, bool bInstanceOnly) const
{
	CDeviceGraphicsCommandInterface* pCommandInterface = commandList.GetGraphicsInterface();

	if (!bInstanceOnly)
	{
		pCommandInterface->PrepareResourcesForUse(EResourceLayoutSlot_PerMaterialRS, m_materialResourceSet.get());
	}

	pCommandInterface->PrepareResourcesForUse(EResourceLayoutSlot_PerDrawExtraRS, m_perDrawExtraResources.get());

	EShaderStage peDrawInlineShaderStages = m_bHasTessellation
	                                        ? EShaderStage_Vertex | EShaderStage_Pixel | EShaderStage_Hull
	                                        : EShaderStage_Vertex | EShaderStage_Pixel;

	pCommandInterface->PrepareInlineConstantBufferForUse(EResourceLayoutSlot_PerDrawCB, m_perDrawCB, eConstantBufferShaderSlot_PerDraw, peDrawInlineShaderStages);

	{
		if (!bInstanceOnly)
		{
			pCommandInterface->PrepareVertexBuffersForUse(m_nNumVertexStreams, m_nLastVertexStreamSlot, m_vertexStreamSet);

			if (m_indexStreamSet == nullptr)
				return;

			pCommandInterface->PrepareIndexBufferForUse(m_indexStreamSet);
		}

		{
			if (m_pInstancingConstBuffer)
			{
				// Render instanced draw calls.
				pCommandInterface->PrepareInlineConstantBufferForUse(EResourceLayoutSlot_PerDrawCB, m_pInstancingConstBuffer, eConstantBufferShaderSlot_PerDraw, EShaderStage_Vertex | EShaderStage_Pixel);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CCompiledRenderObject::DrawToCommandList(const SGraphicsPipelinePassContext& RESTRICT_REFERENCE passContext,
                                              CDeviceCommandList* commandList,
                                              CConstantBuffer* pDynamicInstancingBuffer,
                                              uint32 dynamicInstancingCount) const
{
	//	Alternative: "perDrawInstancingSR != gcpRendD3D->m_DevBufMan.GetNullBufferStructured()->GetDevBuffer();"
	const bool bEnabledInstancing = m_perDrawInstances > 1;

	if (m_bCustomRenderElement)
	{
		m_pRenderElement->DrawToCommandList(m_pRO, passContext, commandList);
		return;
	}

	CDeviceGraphicsCommandInterface& RESTRICT_REFERENCE commandInterface = *commandList->GetGraphicsInterface();

	const bool bIncompleteResourceSets =
		!m_materialResourceSet || !m_materialResourceSet->IsValid() ||
		!m_perDrawExtraResources || !m_perDrawExtraResources->IsValid();

#if defined(ENABLE_PROFILING_CODE)
	if (m_bIncomplete || bIncompleteResourceSets)
	{
		CryInterlockedIncrement(&SRenderStatistics::Write().m_nIncompleteCompiledObjects);
	}
#endif

	const CDeviceGraphicsPSO* pPso = m_pso[passContext.stageID][passContext.passID].get();
	if (!pPso || !pPso->IsValid() || m_bIncomplete || bIncompleteResourceSets)
		return;

	// Set states
	commandInterface.SetPipelineState(pPso);
	commandInterface.SetStencilRef(m_StencilRef);
	commandInterface.SetResources(EResourceLayoutSlot_PerMaterialRS, m_materialResourceSet.get());
	commandInterface.SetResources(EResourceLayoutSlot_PerDrawExtraRS, m_perDrawExtraResources.get());

	auto perDrawCB = bEnabledInstancing ? pDynamicInstancingBuffer : m_perDrawCB.get();

	const EShaderStage perDrawInlineShaderStages = m_bHasTessellation
	                                               ? EShaderStage_Vertex | EShaderStage_Pixel | EShaderStage_Hull
	                                               : EShaderStage_Vertex | EShaderStage_Pixel;

	{
		const SDrawParams& drawParams = m_drawParams[passContext.stageID != eStage_ShadowMap ? eDrawParam_General : eDrawParam_Shadow];

#if defined(ENABLE_PROFILING_CODE)
		if (m_vertexStreamSet)
		{
			if (!!m_vertexStreamSet[VSF_HWSKIN_INFO])
			{
				CryInterlockedIncrement(&SRenderStatistics::Write().m_NumRendSkinnedObjects);
			}
		}

	#ifdef DO_RENDERSTATS
		CD3D9Renderer* pRenderer = gcpRendD3D;

		if (pRenderer->CV_r_stats == 6 ||
		    pRenderer->m_pDebugRenderNode ||
		    pRenderer->m_bCollectDrawCallsInfoPerNode ||
		    pRenderer->m_bCollectDrawCallsInfo)
		{
			TrackStats(passContext, m_pRO);
		}
	#endif					
#endif

		commandInterface.SetVertexBuffers(m_nNumVertexStreams, m_nLastVertexStreamSlot, m_vertexStreamSet);

		if (m_indexStreamSet == nullptr)
		{
			if (CRenderer::CV_r_NoDraw != 3)
			{
				commandInterface.SetInlineConstantBuffer(EResourceLayoutSlot_PerDrawCB, perDrawCB, eConstantBufferShaderSlot_PerDraw, perDrawInlineShaderStages);
				commandInterface.Draw(drawParams.m_nVerticesCount, dynamicInstancingCount, 0, 0);
			}

			return;
		}

		commandInterface.SetIndexBuffer(m_indexStreamSet);

		if (CRenderer::CV_r_NoDraw != 3)
		{
			if (bEnabledInstancing && m_pInstancingConstBuffer)
			{
				perDrawCB = m_pInstancingConstBuffer;
				dynamicInstancingCount = m_perDrawInstances;
			}

			commandInterface.SetInlineConstantBuffer(EResourceLayoutSlot_PerDrawCB, perDrawCB, eConstantBufferShaderSlot_PerDraw, perDrawInlineShaderStages);
			commandInterface.DrawIndexed(drawParams.m_nNumIndices, dynamicInstancingCount, drawParams.m_nStartIndex, 0, 0);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CPermanentRenderObject::~CPermanentRenderObject()
{
	// Free compiled objects
	for (int i = 0, num = m_permanentRenderItems[eRenderPass_Shadows].size(); i < num; i++)
	{
		const SPermanentRendItem& pri = m_permanentRenderItems[eRenderPass_Shadows][i];
		if (pri.m_pCompiledObject && !pri.m_pCompiledObject->m_bSharedWithShadow) // Shared object will be released in the general items
		{
			CCompiledRenderObject::FreeToPool(pri.m_pCompiledObject);
		}
	}
	for (int i = 0, num = m_permanentRenderItems[eRenderPass_General].size(); i < num; i++)
	{
		const SPermanentRendItem& pri = m_permanentRenderItems[eRenderPass_General][i];
		if (pri.m_pCompiledObject)
		{
			CCompiledRenderObject::FreeToPool(pri.m_pCompiledObject);
		}
	}

	if (m_pNextPermanent)
	{
		FreeToPool(static_cast<CPermanentRenderObject*>(m_pNextPermanent));
		m_pNextPermanent = nullptr;
	}

	m_bPermanent = false; 
}

CCompiledRenderObject* CCompiledRenderObject::AllocateFromPool()
{
	CCompiledRenderObject* pObject = s_pPools->m_compiledObjectsPool.New();
	return pObject;
}

void CCompiledRenderObject::FreeToPool(CCompiledRenderObject* ptr)
{
	s_pPools->m_compiledObjectsPool.Delete(ptr);
}

CPermanentRenderObject* CPermanentRenderObject::AllocateFromPool()
{
	CPermanentRenderObject* pObject = s_pPools->m_permanentRenderObjectsPool.New();
	return pObject;
}

void CPermanentRenderObject::FreeToPool(CPermanentRenderObject* pObj)
{
	s_pPools->m_permanentRenderObjectsPool.Delete(static_cast<CPermanentRenderObject*>(pObj));
}
