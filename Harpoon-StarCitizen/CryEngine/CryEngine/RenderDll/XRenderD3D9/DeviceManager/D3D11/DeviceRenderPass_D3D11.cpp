// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DeviceResourceSet_D3D11.h"
#include "DeviceRenderPass_D3D11.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CDeviceRenderPass::UpdateImpl(const CDeviceRenderPassDesc& passDesc)
{
	if (!passDesc.GetDeviceRendertargetViews(m_RenderTargetViews, m_RenderTargetCount))
		return false;

	if (!passDesc.GetDeviceDepthstencilView(m_pDepthStencilView))
		return false;

	return true;
}
