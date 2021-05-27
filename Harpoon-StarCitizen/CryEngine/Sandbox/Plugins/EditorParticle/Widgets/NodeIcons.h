// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryParticleSystem/IParticlesPfx2.h>

#include <NodeGraph/ICryGraphEditor.h>
#include <NodeGraph/HeaderIconWidget.h>

namespace CryParticleEditor {

class CParentPinItem;
class CFeaturePinItem;
class CFeatureItem;

typedef std::vector<CFeatureItem*> FeatureItemArray;

enum EIcon : int16
{
	Icon_Enabled,
	Icon_Disabled,
	Icon_NodeSelected,
	Icon_NodeDeactivated,

	Icon_Count
};

typedef CryGraphEditor::CIconArray<Icon_Count> IconMap;

class CEmitterActiveIcon : public CryGraphEditor::CHeaderIconWidget
{
public:
	CEmitterActiveIcon(CryGraphEditor::CNodeWidget& nodeWidget);
	~CEmitterActiveIcon();

	// CryGraphEditor::CHeaderIconWidget
	virtual void OnClicked();
	// ~CryGraphEditor::CHeaderIconWidget

protected:
	void OnNodeSelectionChanged(bool isSelected);
	void OnDeactivatedChanged(bool isDeactivated);

private:
	static IconMap s_iconMap;
};

class CSoloEmitterModeIcon : public CryGraphEditor::CHeaderIconWidget
{
public:
	CSoloEmitterModeIcon(CryGraphEditor::CNodeWidget& nodeWidget);
	~CSoloEmitterModeIcon();

	// CryGraphEditor::CHeaderIconWidget
	virtual void OnClicked();
	// ~CryGraphEditor::CHeaderIconWidget

protected:
	void OnNodeSelectionChanged(bool isSelected);
	void OnVisibilityChanged(bool isVisible);
	void OnDeactivatedChanged(bool isDeactivated);

	void UpdateIcon(bool isSelected, bool isVisible, bool isDeactivated, bool isSoloNode);

private:
	static IconMap s_iconMap;
};

class CEmitterVisibleIcon : public CryGraphEditor::CHeaderIconWidget
{
public:
	CEmitterVisibleIcon(CryGraphEditor::CNodeWidget& nodeWidget);
	~CEmitterVisibleIcon();

	// CryGraphEditor::CHeaderIconWidget
	virtual void OnClicked();
	// ~CryGraphEditor::CHeaderIconWidget

protected:
	void OnNodeSelectionChanged(bool isSelected);
	void OnVisibilityChanged(bool isVisible);
	void OnDeactivatedChanged(bool isDeactivated);

	void UpdateIcon(bool isSelected, bool isVisible, bool isDeactivated);

private:
	static IconMap s_iconMap;
};

}
