// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <Controls/EditorDialog.h>

class CAsset;
class QWidget;
class QAbstractItemModel;

//! A dialog that shows a list of assets that use the given assets.
class CAssetReverseDependenciesDialog : public CEditorDialog
{
public:
	CAssetReverseDependenciesDialog(
		const std::vector<CAsset*>& assets, 
		const QString& assetsGroupTitle,
		const QString& dependentAssetsGroupTitle,
		const QString& dependentAssetsInfoText,
		const QString& question = QString(),
		QWidget* pParent = nullptr);
private:
	std::vector<std::unique_ptr<QAbstractItemModel>> m_models;
};
