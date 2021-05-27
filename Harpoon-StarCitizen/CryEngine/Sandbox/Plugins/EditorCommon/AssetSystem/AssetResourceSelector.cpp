// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.
#include "StdAfx.h"
#include "AssetResourceSelector.h"

#include "AssetSystem/Asset.h"
#include "AssetSystem/AssetManager.h"
#include "AssetSystem/AssetType.h"
#include "AssetSystem/Browser/AssetTooltip.h"
#include "PreviewToolTip.h"
#include "FileDialogs/EngineFileDialog.h"
#include "PathUtils.h"
#include <IEditor.h>
#include <QFileInfo>

namespace Private_AssetSelector
{

struct SImmediateCallbackResourceSelectorContext : public IResourceSelectionCallback
{
	void SetValue(const char* newValue) override;
	std::function<void(const char* newValue)> callback;
};

void SImmediateCallbackResourceSelectorContext::SetValue(const char* newValue)
{
	if (callback)
	{
		callback(newValue);
	}
}

CAsset* FindAssetForFileAndContext(const SResourceSelectorContext& selectorContext, const char* value)
{
	const CryPathString assetFile(PathUtil::ToUnixPath<CryPathString>(value));
	CAssetManager* const pManager = CAssetManager::GetInstance();
	CAsset* pAsset = pManager->FindAssetForFile(assetFile.c_str());

	if (!pAsset)
	{
		CRY_ASSERT(selectorContext.resourceSelectorEntry && selectorContext.resourceSelectorEntry->IsAssetSelector());
		const SStaticAssetSelectorEntry* selector = static_cast<const SStaticAssetSelectorEntry*>(selectorContext.resourceSelectorEntry);
		const auto& assetTypes = selector->GetAssetTypes();

		if (assetTypes.size() != 1)
			return nullptr;

		const CAssetType* const pType = assetTypes.front();
		if (!pType)
			return nullptr;

		// value may points to source file (e.g. tif), try to find asset based on the asset type.
		pAsset = pManager->FindAssetForFile(PathUtil::ReplaceExtension(assetFile.c_str(), pType->GetFileExtension()));
	}

	return pAsset;
}

dll_string SelectAssetLegacy(const SResourceSelectorContext& selectorContext, const char* previousValue)
{
	CRY_ASSERT(selectorContext.resourceSelectorEntry->IsAssetSelector());
	const SStaticAssetSelectorEntry* selector = static_cast<const SStaticAssetSelectorEntry*>(selectorContext.resourceSelectorEntry);

	CEngineFileDialog::RunParams runParams;
	runParams.initialFile = previousValue;

	for (auto type : selector->GetAssetTypes())
	{
		QString description = QString("%1 (*.%2)").arg(type->GetTypeName()).arg(type->GetFileExtension());
		runParams.extensionFilters << CExtensionFilter(description, type->GetFileExtension());
	}

	runParams.extensionFilters << CExtensionFilter("All Files (*.*)", "*");

	QString filename = CEngineFileDialog::RunGameOpen(runParams, nullptr);

	if (!filename.isEmpty())
		return filename.toStdString().c_str();
	else
		return previousValue;
}

SResourceSelectionResult SelectAsset(const SResourceSelectorContext& selectorContext, const char* previousValue)
{
	SResourceSelectionResult selectionResult;
	const auto pickerState = (EAssetResourcePickerState)GetIEditor()->GetSystem()->GetIConsole()->GetCVar("ed_enableAssetPickers")->GetIVal();
	if (pickerState == EAssetResourcePickerState::Disable)
	{
		selectionResult.selectedResource = SelectAssetLegacy(selectorContext, previousValue);
		selectionResult.selectionAccepted = strcmp(selectionResult.selectedResource.c_str(), previousValue) != 0;
		return selectionResult;
	}

	CRY_ASSERT(selectorContext.resourceSelectorEntry && selectorContext.resourceSelectorEntry->IsAssetSelector());
	const SStaticAssetSelectorEntry* selector = static_cast<const SStaticAssetSelectorEntry*>(selectorContext.resourceSelectorEntry);
	const std::vector<string>& assetTypeNames = selector->GetAssetTypeNames();

	CAssetSelector assetSelector(assetTypeNames);
	bool selectionConfirmed = assetSelector.Execute(selectorContext, assetTypeNames, previousValue);
	selectionResult.selectionAccepted = selectionConfirmed;

	if (selectionConfirmed)
	{
		if (CAsset* pSelectedAsset = assetSelector.GetSelectedAsset())
		{
			selectionResult.selectedResource = pSelectedAsset->GetFile(0).c_str();
			return selectionResult;
		}
	}

	selectionResult.selectedResource = previousValue;
	return selectionResult;
}

void EditAsset(const SResourceSelectorContext& selectorContext, const char* value)
{
	if (value && *value)
	{
		CAsset* pAsset = FindAssetForFileAndContext(selectorContext, value);

		if (pAsset)
			return pAsset->Edit();
	}
}

SResourceValidationResult ValidateAsset(const SResourceSelectorContext& selectorContext, const char* newValue, const char* previousValue)
{

	SResourceValidationResult result{ false, previousValue };

	if (!newValue || !*newValue)
	{
		result.validatedResource = "";
		return result;
	}

	CRY_ASSERT(selectorContext.resourceSelectorEntry->IsAssetSelector());
	const SStaticAssetSelectorEntry* selector = static_cast<const SStaticAssetSelectorEntry*>(selectorContext.resourceSelectorEntry);

	QFileInfo fileInfo(newValue);
	QString assetPath(newValue);

	if (fileInfo.suffix().isEmpty())
	{
		//Try to auto complete it
		if (selector->GetAssetTypes().size() == 1)
		{
			assetPath += ".";
			assetPath += selector->GetAssetTypes()[0] -> GetFileExtension();
		}
		else
		{
			//cannot auto complete, invalid
			return result;
		}
	}

	//If suffix is not empty, does it match allowed types?
	bool bMatch = false;
	const QString suffix = fileInfo.suffix();
	for (auto type : selector->GetAssetTypes())
	{
		if (suffix == type->GetFileExtension())
		{
			bMatch = true;
			break;
		}
	}

	if (bMatch)
	{
		const auto pickerState = (EAssetResourcePickerState)GetIEditor()->GetSystem()->GetIConsole()->GetCVar("ed_enableAssetPickers")->GetIVal();
		if (pickerState == EAssetResourcePickerState::Disable)
		{
			result.isValid = true;
			result.validatedResource = assetPath.toStdString().c_str();
		}
		else
		{
			//Finally check if there is a valid asset for this path
			CAsset* asset = CAssetManager::GetInstance()->FindAssetForFile(assetPath.toStdString().c_str());
			if (asset)
			{
				result.isValid = true;
				result.validatedResource = assetPath.toStdString().c_str();
			}
		}
	}

	return result;
}
}

SStaticAssetSelectorEntry::SStaticAssetSelectorEntry(const char* typeName)
	: SStaticResourceSelectorEntry(typeName, Private_AssetSelector::SelectAsset, Private_AssetSelector::ValidateAsset, Private_AssetSelector::EditAsset, "", false)
	, assetTypeNames({ typeName })
{
	//Note: we cannot populate the icon easily as CAssetType returns an icon and not an icon path
	isAsset = true;
}

SStaticAssetSelectorEntry::SStaticAssetSelectorEntry(const char* typeName, const std::vector<const char*>& typeNames)
	: SStaticResourceSelectorEntry(typeName, Private_AssetSelector::SelectAsset, Private_AssetSelector::ValidateAsset, Private_AssetSelector::EditAsset, "", false)
{
	for (auto& typeName : typeNames)
	{
		assetTypeNames.push_back(typeName);
	}

	isAsset = true;
}

SStaticAssetSelectorEntry::SStaticAssetSelectorEntry(const char* typeName, const std::vector<string>& typeNames)
	: SStaticResourceSelectorEntry(typeName, Private_AssetSelector::SelectAsset, Private_AssetSelector::ValidateAsset, Private_AssetSelector::EditAsset, "", false)
	, assetTypeNames(typeNames)
{
	isAsset = true;
}

bool SStaticAssetSelectorEntry::ShowTooltip(const SResourceSelectorContext& context, const char* value) const
{
	if (value && *value)
	{
		CAsset* asset = CAssetManager::GetInstance()->FindAssetForFile(value);
		if (asset)
		{
			CAssetTooltip::ShowTrackingTooltip(asset, context.parentWidget);
			return true;
		}
	}

	return SStaticResourceSelectorEntry::ShowTooltip(context, value);
}

void SStaticAssetSelectorEntry::HideTooltip(const SResourceSelectorContext& context, const char* value) const
{
	//Will also hide asset tooltip
	QTrackingTooltip::HideTooltip();
}

const std::vector<const CAssetType*>& SStaticAssetSelectorEntry::GetAssetTypes() const
{
	if (!assetTypes.size())
	{
		const_cast<SStaticAssetSelectorEntry*>(this)->assetTypes.reserve(assetTypes.size());
		for (const char* typeName : assetTypeNames)
		{
			auto assetType = GetIEditor()->GetAssetManager()->FindAssetType(typeName);
			CRY_ASSERT(assetType);
			const_cast<SStaticAssetSelectorEntry*>(this)->assetTypes.push_back(assetType);
		}
	}

	return assetTypes;
}

//////////////////////////////////////////////////////////////////////////

SResourceSelectionResult SStaticAssetSelectorEntry::SelectFromAsset(const SResourceSelectorContext& context, const std::vector<string>& types, const char* previousValue)
{
	CAssetSelector wrapper(types);

	bool accepted = wrapper.Execute(context, types, previousValue);
	SResourceSelectionResult result{ accepted, previousValue };

	if (accepted)
	{
		if (CAsset* pSelectedAsset = wrapper.GetSelectedAsset())
		{
			result.selectedResource = pSelectedAsset->GetFile(0).c_str();
		}
	}

	return result;
}

CAssetSelector::CAssetSelector(const std::vector<string>& assetTypeNames) :
	m_dialog(assetTypeNames, CAssetBrowserDialog::Mode::OpenSingleAsset)
{

}

bool CAssetSelector::Execute(const SResourceSelectorContext& context, const std::vector<string>& types, const char* previousValue)
{
	if (previousValue)
	{
		CAssetManager* const pManager = CAssetManager::GetInstance();

		const CAsset* pAsset = pManager->FindAssetForFile(previousValue);

		// previousValue may points to source file (e.g. tif), try to find asset based on the asset type.
		if (!pAsset && types.size() == 1)
		{
			CAssetType* const pType = pManager->FindAssetType(types.front());
			if (pType)
			{
				pAsset = pManager->FindAssetForFile(PathUtil::ReplaceExtension(previousValue, pType->GetFileExtension()));
			}
		}

		if (pAsset)
		{
			m_dialog.SelectAsset(*pAsset);
		}
	}

	QObject::connect(&m_dialog, &CAssetBrowserDialog::SelectionChanged, [&context](const std::vector<CAsset*>& assets)
	{
		CRY_ASSERT(assets.size() <= 1);
		if (!assets.empty() && context.callback)
		{
		  context.callback->SetValue(assets.front()->GetFile(0));
		}
	});

	return m_dialog.Execute();
}

bool CAssetSelector::Execute(std::function<void(const char* newValue)> onValueChangedCallback, const std::vector<string>& types, const char* previousValue)
{
	Private_AssetSelector::SImmediateCallbackResourceSelectorContext callback;
	callback.callback = onValueChangedCallback;

	SResourceSelectorContext ctx;
	ctx.callback = &callback;

	return Execute(ctx, types, previousValue);
}
