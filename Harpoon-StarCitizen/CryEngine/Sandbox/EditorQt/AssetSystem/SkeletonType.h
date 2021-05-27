// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <AssetSystem/AssetType.h>

// Skeleton
// Contains the skeleton data and physics proxies used for hit detection and ragdoll simulation which is driven by animations.
class CSkeletonType : public CAssetType
{
public:
	DECLARE_ASSET_TYPE_DESC(CSkeletonType);

	virtual const char*                       GetTypeName() const override       { return "Skeleton"; }
	virtual const char*                       GetUiTypeName() const override     { return QT_TR_NOOP("Skeleton"); }
	virtual const char*                       GetFileExtension() const override  { return "chr"; }
	virtual bool                              IsImported() const override        { return true; }
	virtual bool                              CanBeCopied() const                { return true; }
	virtual bool                              CanBeEdited() const override       { return true; }
	virtual bool                              HasThumbnail() const override      { return false; }
	virtual QColor                            GetThumbnailColor() const override { return QColor(210, 75, 64); }
	virtual std::vector<CItemModelAttribute*> GetDetails() const override;
	virtual QVariant                          GetDetailValue(const CAsset* pAsset, const CItemModelAttribute* pDetail) const override;

	virtual CAssetEditor*                     Edit(CAsset* asset) const override;
	virtual void                              GenerateThumbnail(const CAsset* pAsset) const override;

private:
	virtual CryIcon GetIconInternal() const override;

public:
	//! May be referenced by other asset types, too.
	static CItemModelAttribute s_bonesCountAttribute;
};
