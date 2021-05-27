// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

/*=============================================================================
   CREMeshImpl.cpp : implementation of OcLeaf Render Element.

   Revision history:
* Created by Honitch Andrey

   =============================================================================*/

#include "StdAfx.h"
#include "CREMeshImpl.h"
#include <CryAnimation/ICryAnimation.h>

#include "XRenderD3D9/DriverD3D.h"

void CREMeshImpl::mfGetBBox(AABB& bb) const
{
	const auto& container = *m_pRenderMesh->_GetVertexContainer();
	bb = AABB{ container.m_vBoxMin, container.m_vBoxMax };
}

///////////////////////////////////////////////////////////////////


TRenderChunkArray* CREMeshImpl::mfGetMatInfoList()
{
	return &m_pRenderMesh->m_Chunks;
}

int CREMeshImpl::mfGetMatId()
{
	return m_pChunk->m_nMatID;
}

CRenderChunk* CREMeshImpl::mfGetMatInfo()
{
	return m_pChunk;
}

bool CREMeshImpl::mfUpdate(InputLayoutHandle eVertFormat, EStreamMasks StreamMask, bool bTessellation)
{
	DETAILED_PROFILE_MARKER("CREMeshImpl::mfUpdate");
	FUNCTION_PROFILER_RENDER_FLAT
	IF (m_pRenderMesh == NULL, 0)
		return false;

	const int threadId = gRenDev->GetRenderThreadID();

	// If no updates are pending it counts as having successfully "processed" them
	bool bPendingUpdatesSucceeded = true;

	m_pRenderMesh->m_nFlags &= ~FRM_SKINNEDNEXTDRAW;

	if (m_pRenderMesh->m_Modified[threadId].linked() || bTessellation) // TODO: use the modified list also for tessellated meshes
	{
		m_pRenderMesh->SyncAsyncUpdate(gRenDev->GetRenderThreadID());

		bPendingUpdatesSucceeded = m_pRenderMesh->RT_CheckUpdate(m_pRenderMesh->_GetVertexContainer(), eVertFormat, StreamMask | VSM_MASK, bTessellation);
		if (bPendingUpdatesSucceeded)
		{
			AUTO_LOCK(CRenderMesh::m_sLinkLock);
			m_pRenderMesh->m_Modified[threadId].erase();
		}
	}

	if (!bPendingUpdatesSucceeded)
		return false;

	m_Flags &= ~FCEF_DIRTY;

	return true;
}

void* CREMeshImpl::mfGetPointer(ESrcPointer ePT, int* Stride, EParamType Type, ESrcPointer Dst, EStreamMasks StreamMask)
{
	DETAILED_PROFILE_MARKER("CREMeshImpl::mfGetPointer");
	CRenderMesh* pRM = m_pRenderMesh->_GetVertexContainer();
	byte* pD = NULL;
	IRenderMesh::ThreadAccessLock lock(pRM);

	switch (ePT)
	{
	case eSrcPointer_Vert:
		pD = pRM->GetPosPtr(*Stride, FSL_READ);
		break;
	case eSrcPointer_Tex:
		pD = pRM->GetUVPtr(*Stride, FSL_READ);
		break;
	case eSrcPointer_Normal:
		pD = pRM->GetNormPtr(*Stride, FSL_READ);
		break;
	case eSrcPointer_Tangent:
		pD = pRM->GetTangentPtr(*Stride, FSL_READ);
		break;
	case eSrcPointer_Color:
		pD = pRM->GetColorPtr(*Stride, FSL_READ);
		break;
	default:
		assert(false);
		break;
	}
	if (m_nFirstVertId && pD)
		pD += m_nFirstVertId * (*Stride);
	return pD;
}

//////////////////////////////////////////////////////////////////////////
InputLayoutHandle CREMeshImpl::GetVertexFormat() const
{
	if (m_pRenderMesh)
		return m_pRenderMesh->_GetVertexContainer()->_GetVertexFormat();
	return InputLayoutHandle::Unspecified;
}

//////////////////////////////////////////////////////////////////////////
bool CREMeshImpl::Compile(CRenderObject* pObj, uint64 objFlags, ERenderElementFlags elmFlags, const AABB &localAABB, CRenderView *pRenderView, bool updateInstanceDataOnly)
{
	if (!m_pRenderMesh)
		return false;

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CREMeshImpl::GetGeometryInfo(SGeometryInfo& geomInfo, bool bSupportTessellation)
{
	if (!m_pRenderMesh)
		return false;

	CRenderMesh* pVContainer = m_pRenderMesh->_GetVertexContainer();

	geomInfo.nFirstIndex = m_nFirstIndexId;
	geomInfo.nFirstVertex = m_nFirstVertId;
	geomInfo.nNumVertices = m_nNumVerts;
	geomInfo.nNumIndices = m_nNumIndices;
	geomInfo.nTessellationPatchIDOffset = m_nPatchIDOffset;
	geomInfo.eVertFormat = pVContainer->_GetVertexFormat();
	geomInfo.primitiveType = pVContainer->_GetPrimitiveType();

	// Test if any pending updates have to be processed (and process them)
	if (!mfCheckUpdate(geomInfo.eVertFormat, VSM_MASK, gRenDev->GetRenderFrameID(), bSupportTessellation))
		return false;

	if (!m_pRenderMesh->FillGeometryInfo(geomInfo))
		return false;

	return true;
}

void CREMeshImpl::DrawToCommandList(CRenderObject* pObj, const SGraphicsPipelinePassContext& ctx, CDeviceCommandList* commandList)
{
	//@TODO: implement

	//if (!pRE->mfCheckUpdate(rRP.m_CurVFormat, rRP.m_FlagsStreams_Stream | 0x80000000, nFrameID, bTessEnabled))
	//continue;
}

CREMesh::~CREMesh()
{
	//int nFrameId = gEnv->pRenderer->GetFrameID(false); { char buf[1024]; cry_sprintf(buf,"~CREMesh: %p : frame(%d) \r\n", this, nFrameId); OutputDebugString(buf); }
}
