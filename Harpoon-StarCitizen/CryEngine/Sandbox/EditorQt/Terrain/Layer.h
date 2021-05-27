// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "SandboxAPI.h"
#include <Util/Image.h>
#include <CrySandbox/CrySignal.h>

class CRect;
class CSurfaceType;

//! Single texture layer
class SANDBOX_API CLayer : public CObject
{
public:
	CLayer();
	virtual ~CLayer();

	// Name
	string        GetLayerName() const                { return m_strLayerName; }
	void          SetLayerName(const string& strName) { m_strLayerName = strName; }
	const string& GetMaterialName() const             { return m_materialName; }
	string        GetLayerPath() const;
	void          SetLayerPath(const string& strPath) { m_strLayerPath = strPath; }

	//! Get this Layer GUID.
	CryGUID GetGUID() const { return m_guid; }

	// Slope Angle 0=flat..90=max steep
	float  GetLayerMinSlopeAngle() const             { return m_minSlopeAngle; }
	float  GetLayerMaxSlopeAngle() const             { return m_maxSlopeAngle; }
	float  GetLayerUseRemeshing() const              { return m_fUseRemeshing; }
	float  GetLayerTiling() const                    { return m_fLayerTiling; }
	float  GetLayerSpecularAmount() const            { return m_fSpecularAmount; }
	float  GetLayerSortOrder() const                 { return m_fSortOrder; }
	ColorF GetLayerFilterColor() const               { return m_cLayerFilterColor; }

	void   SetLayerMinSlopeAngle(float min)          { if (m_minSlopeAngle != min) { m_minSlopeAngle = min; InvalidateMask(); } }
	void   SetLayerMaxSlopeAngle(float max)          { if (m_maxSlopeAngle != max) { m_maxSlopeAngle = max; InvalidateMask(); } }
	void   SetLayerUseRemeshing(float fUseRemeshing) { if (m_fUseRemeshing != fUseRemeshing) { m_fUseRemeshing = fUseRemeshing; InvalidateMask(); } }
	void   SetLayerTiling(float fVal)                { if (m_fLayerTiling != fVal) { m_fLayerTiling = fVal; InvalidateMask(); } }
	void   SetLayerSpecularAmount(float fVal)        { if (m_fSpecularAmount != fVal) { m_fSpecularAmount = fVal; InvalidateMask(); } }
	void   SetLayerSortOrder(float fVal)             { if (m_fSortOrder != fVal) { m_fSortOrder = fVal; InvalidateMask(); } }
	void   SetLayerFilterColor(ColorF colVal)        { m_cLayerFilterColor = colVal; InvalidateMask(); }

	// Altitude
	float GetLayerStart() const       { return m_LayerStart; }
	float GetLayerEnd() const         { return m_LayerEnd; }
	void  SetLayerStart(float iStart) { if (m_LayerStart != iStart) { m_LayerStart = iStart; InvalidateMask(); } }
	void  SetLayerEnd(float iEnd)     { if (m_LayerEnd != iEnd) { m_LayerEnd = iEnd; InvalidateMask(); } }

	//! Calculate memory size [in bytes] allocated for this layer
	// Return:
	//   size in bytes
	int GetSize() const;

	//////////////////////////////////////////////////////////////////////////
	// Layer Mask
	//////////////////////////////////////////////////////////////////////////
	unsigned char GetLayerMaskPoint(uint32 x, uint32 y) { return m_layerMask.ValueAt(x, y); }

	// Update current mask for this sector and put it into target mask data.
	// Arguments
	//   mask - Mask image returned from function, may not be not initialized when calling function.
	// Return:
	//   true is mask for this sector exist, false if not.
	bool UpdateMaskForSector(CPoint sector, const CRect& sectorRect, const int resolution, const CPoint vTexOffset,
	                         const CFloatImage& hmap, CByteImage& mask);

	bool        UpdateMask(const CFloatImage& hmap, CByteImage& mask);
	CByteImage& GetMask();
	int         GetMaskResolution() const { return m_maskResolution; }

	void        GenerateWaterLayer16(float* pHeightmapPixels, UINT iHeightmapWidth, UINT iHeightmapHeight, float waterLevel);

	// Texture
	int             GetTextureWidth()             { return m_cTextureDimensions.cx; }
	int             GetTextureHeight()            { return m_cTextureDimensions.cy; }
	CSize           GetTextureDimensions()        { return m_cTextureDimensions; }
	const CImageEx& GetTexture() const            { return m_texture; }
	uint32&         GetTexturePixel(int x, int y) { return m_texture.ValueAt(x, y); }

	string          GetTextureFilename();               // filename without path
	string          GetTextureFilenameWithPath() const; // including path

	void            DrawLayerTexturePreview(LPRECT rcPos, CDC* pDC);
	bool            LoadTexture(string strFileName);
	// used to fill with water color
	void            FillWithColor(COLORREF col, int width, int height);
	bool            LoadTexture(LPCTSTR lpBitmapName, UINT iWidth, UINT iHeight);
	bool            LoadTexture(DWORD* pBitmapData, UINT iWidth, UINT iHeight);
	void            ExportTexture(string strFileName);
	// represents the editor feature
	void            ExportMask(const string& strFileName);
	bool            HasTexture() { return ((GetTextureWidth() != 0) && (m_texture.GetData() != nullptr)); }

	// represents the editor feature
	// Load a BMP texture into the layer mask.
	bool LoadMask(const string& strFileName);
	void GenerateLayerMask(CByteImage& mask, int width, int height);

	// Release allocated mask
	void ReleaseMask();

	// Release temporary allocated resources
	void ReleaseTempResources();

	// Serialization
	void Serialize(CXmlArchive& xmlAr);

	// Call if mask was Modified.
	void InvalidateMask();
	// Invalidate one sector of the layer mask.
	void InvalidateMaskSector(CPoint sector);

	// Check if layer is valid.
	bool IsValid() { return m_layerMask.IsValid(); }

	// In use
	bool IsInUse()                { return m_bLayerInUse; }
	void SetInUse(bool bNewState) { m_bLayerInUse = bNewState; }

	bool IsAutoGen() const        { return m_bAutoGen; }
	void SetAutoGen(bool bAutoGen);

	//////////////////////////////////////////////////////////////////////////
	// True if layer is currently selected.
	bool          IsSelected() const { return m_bSelected; }
	//! Mark layer as selected or not.
	void          SetSelected(bool bSelected);

	CSurfaceType* GetSurfaceType() const { return m_pSurfaceType; }
	void          SetSurfaceType(CSurfaceType* pSurfaceType);
	int           GetEngineSurfaceTypeId() const;

	void          AssignMaterial(const string& materialName);

	//! Load texture if it was unloaded.
	void PrecacheTexture();
	//! Load mask if it was unloaded.
	void PrecacheMask();

	//////////////////////////////////////////////////////////////////////////
	//! Mark all layer mask sectors as invalid.
	void InvalidateAllSectors();
	void SetAllSectorsValid();

	//! Compress mask.
	void CompressMask();

	//! Export layer block.
	void ExportBlock(const CRect& rect, CXmlArchive& ar);
	//! Import layer block.
	void ImportBlock(CXmlArchive& ar, const CPoint& offset, int nRot = 0);

	//! Allocate mask grid for layer.
	void AllocateMaskGrid();

	void GetMemoryUsage(ICrySizer* pSizer) const;

	//! should always return a valid LayerId
	uint32 GetOrRequestLayerId();

	//! might return 0xffffffff
	uint32 GetCurrentLayerId() const;

	// CHeightmap::m_LayerIdBitmap
	void      SetLayerId(const uint32 dwLayerId);

	CBitmap&  GetTexturePreviewBitmap();
	CImageEx& GetTexturePreviewImage();
	void      Serialize(Serialization::IArchive& ar);

	CCrySignal<void(CLayer*)> signalPropertiesChanged;

	//! Duplicates the current layer.
	CLayer* Duplicate() const;

private:

	enum SectorMaskFlags
	{
		SECTOR_MASK_VALID = 1,
	};

	// Convert the layer from BGR to RGB
	void   BGRToRGB();

	uint8& GetSector(CPoint sector);

	// Autogenerate layer mask.
	// Arguments:
	//   vHTexOffset - offset within hmap
	void AutogenLayerMask(const CRect& rect, const int resolution, const CPoint vHTexOffset, const CFloatImage& hmap, CByteImage& mask);

	// Get native resolution for layer mask (For not autogen levels).
	int GetNativeMaskResolution() const;

	string      m_strLayerName;        // Name (might not be unique)
	string      m_strLayerPath;

	// Layer texture
	CBitmap  m_bmpLayerTexPrev;
	CDC      m_dcLayerTexPrev;
	string   m_strLayerTexPath;
	CSize    m_cTextureDimensions;

	CImageEx m_texture;
	CImageEx m_previewImage;

	ColorF   m_cLayerFilterColor;
	float    m_fUseRemeshing;
	float    m_fLayerTiling;
	float    m_fSpecularAmount;
	float    m_fSortOrder;

	CryGUID  m_guid;

	// Layer parameters
	float  m_LayerStart;
	float  m_LayerEnd;

	float  m_minSlopeAngle;  // min slope 0=flat..90=max steep
	float  m_maxSlopeAngle;  // max slope 0=flat..90=max steep

	string m_materialName;

	// Layer mask data
	string                   m_maskFile;
	CByteImage               m_layerMask;
	CByteImage               m_scaledMask; // Mask used when scaled mask version is needed.
	int                      m_maskResolution;

	bool                     m_bNeedUpdate;

	bool                     m_bCompressedMaskValid;
	CMemoryBlock             m_compressedMask;

	bool                     m_bLayerInUse; // Should this layer be used during terrain generation ?
	bool                     m_bAutoGen;

	bool                     m_bSelected;      // True if layer is selected as current.
	int                      m_iSurfaceTypeId; // -1 if not assigned yet,
	uint32                   m_dwLayerId;      // used in CHeightmap::m_LayerIdBitmap, usually in the range 0..0xff, 0xffffffff means not set

	static UINT              m_iInstanceCount; // Internal instance count, used for unique name assignment

	_smart_ptr<CSurfaceType> m_pSurfaceType;

	//////////////////////////////////////////////////////////////////////////
	// Layer Sectors.
	//////////////////////////////////////////////////////////////////////////
	std::vector<unsigned char> m_maskGrid;  // elements can be 0 or SECTOR_MASK_VALID
	int                        m_numSectors;
};
