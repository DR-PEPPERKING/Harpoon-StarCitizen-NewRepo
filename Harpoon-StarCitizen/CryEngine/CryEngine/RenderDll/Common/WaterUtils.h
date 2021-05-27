// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

class CWaterSim;

// todo. refactor me - should be more general

class CWater
{
public:

	CWater() : m_pWaterSim(0)
	{
	}

	~CWater()
	{
		Release();
	}

	// Create/Initialize simulation
	void Create(float fA, float fWind, float fWorldSizeX, float fWorldSizeY);
	void Release();
	void SaveToDisk(const char* pszFileName);

	// Update water simulation
	void Update(int nFrameID, float fTime, bool bOnlyHeight = false, void* pRawPtr = NULL);

	// Get water simulation data
	Vec3      GetPositionAt(int x, int y) const;
	float     GetHeightAt(int x, int y) const;
	Vec4*     GetDisplaceGrid() const;

	const int GetGridSize() const;

	void      GetMemoryUsage(ICrySizer* pSizer) const;

	bool      NeedInit() const { return m_pWaterSim == NULL; }
private:

	CWaterSim* m_pWaterSim;
};

static CWater* WaterSimMgr()
{
	return gRenDev->m_pWaterSimMgr;
}
