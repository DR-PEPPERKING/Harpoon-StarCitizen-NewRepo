// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"
#include "Controls/EditorDialog.h"
#include "ProxyModels/ItemModelAttribute.h"

class CAsset;
class CAssetType;
class CAttributeFilter;

class CFileNameLineEdit;

// #TODO Split this into two classes: open dialog and save dialog.
class EDITOR_COMMON_API CAssetBrowserDialog : public CEditorDialog
{
	Q_OBJECT
public:
	// See IsReadOnlyMode()
	enum class Mode
	{
		OpenSingleAsset,
		OpenMultipleAssets,
		Save,
		Create
	};

	enum class OverwriteMode
	{
		AllowOverwrite = 0,
		NoOverwrite
	};
private:
	class CBrowser;

signals:
	void SelectionChanged(const std::vector<CAsset*>& assets);

public:
	CAssetBrowserDialog(const std::vector<string>& assetTypeNames, Mode mode, QWidget* pParent = nullptr);

	//! Returns the most recently selected asset.
	//! If the dialog has been accepted, the returned value is considered the result of the dialog.
	//! \sa GetSelectedAssets
	CAsset* GetSelectedAsset();

	//! Returns the most recent selection.
	//! If the dialog has been accepted, the returned value is considered the result of the dialog.
	//! \sa GetSelectedAsset
	std::vector<CAsset*>        GetSelectedAssets();

	void                        SelectAsset(const CAsset& asset);
	void                        SelectAsset(const string& path);
	void                        SetOverwriteMode(OverwriteMode mode);
	const string                GetSelectedAssetPath() const { return m_assetPath; }

	static CAsset*              OpenSingleAssetForTypes(const std::vector<string>& assetTypeNames);
	static std::vector<CAsset*> OpenMultipleAssetsForTypes(const std::vector<string>& assetTypeNames);
	static string               SaveSingleAssetForType(const string& assetTypeName, OverwriteMode overwriteMode = OverwriteMode::AllowOverwrite);
	static string               CreateSingleAssetForType(const string& assetTypeName, OverwriteMode overwriteMode);

protected:
	virtual void showEvent(QShowEvent* pEvent) override;

private:
	void OnAccept();
	void OnAcceptSave();
	bool IsReadOnlyMode() const;

private:
	Mode                     m_mode;
	OverwriteMode            m_overwriteMode;
	CBrowser*                m_pBrowser;
	CFileNameLineEdit*       m_pPathEdit;
	AttributeFilterSharedPtr m_pAssetTypeFilter;
	const CAssetType*        m_pAssetType;
	string                   m_assetPath;
};
