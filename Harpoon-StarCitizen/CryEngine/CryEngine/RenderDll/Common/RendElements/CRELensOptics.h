// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CRELensOptics : public CRenderElement
{
public:
	CRELensOptics();

	virtual void mfExport(struct SShaderSerializeContext& SC)                 {}
	virtual void mfImport(struct SShaderSerializeContext& SC, uint32& offset) {}
};
