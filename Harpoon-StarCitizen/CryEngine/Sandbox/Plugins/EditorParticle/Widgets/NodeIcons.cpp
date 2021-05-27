// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NodeIcons.h"

#include "Undo.h"

#include "Models/ParticleGraphModel.h"
#include "Models/ParticleGraphItems.h"

#include <NodeGraph/NodeWidget.h>

namespace CryParticleEditor {

IconMap CEmitterActiveIcon::s_iconMap;
IconMap CSoloEmitterModeIcon::s_iconMap;
IconMap CEmitterVisibleIcon::s_iconMap;

CEmitterActiveIcon::CEmitterActiveIcon(CryGraphEditor::CNodeWidget& nodeWidget)
	: CHeaderIconWidget(nodeWidget)
{
	static bool isInitialized = false;
	if (isInitialized == false)
	{
		s_iconMap.SetIcon(Icon_Enabled, CryIcon("icons:Graph/Node_active.ico", {
				{ QIcon::Mode::Normal, QColor(25, 255, 255) /*CryGraphEditor::CGlobalStyle::GetSelectionColor()*/ }
		  }), IconMap::HeaderIcon);
		s_iconMap.SetIcon(Icon_Disabled, CryIcon("icons:Graph/Node_active.ico", {
				{ QIcon::Mode::Normal, QColor(94, 94, 94) }
		  }), IconMap::HeaderIcon);
		s_iconMap.SetIcon(Icon_NodeSelected, CryIcon("icons:Graph/Node_active.ico", {
				{ QIcon::Mode::Normal, QColor(255, 255, 255) }
		  }), IconMap::HeaderIcon);
		s_iconMap.SetIcon(Icon_NodeDeactivated, CryIcon("icons:Graph/Node_active.ico", {
				{ QIcon::Mode::Normal, QColor(51, 51, 51) }
		  }), IconMap::HeaderIcon);

		isInitialized = true;
	}

	CNodeItem& node = static_cast<CNodeItem&>(GetViewItem());
	if (!node.IsDeactivated())
		SetDisplayIcon(s_iconMap.GetIcon(Icon_Enabled));
	else
		SetDisplayIcon(s_iconMap.GetIcon(Icon_Disabled));

	GetViewWidget().SignalSelectionChanged.Connect(this, &CEmitterActiveIcon::OnNodeSelectionChanged);
	node.SignalDeactivatedChanged.Connect(this, &CEmitterActiveIcon::OnDeactivatedChanged);
}

CEmitterActiveIcon::~CEmitterActiveIcon()
{
	GetViewWidget().SignalSelectionChanged.DisconnectObject(this);
	CNodeItem& node = static_cast<CNodeItem&>(GetViewItem());
	node.SignalDeactivatedChanged.DisconnectObject(this);
}

void CEmitterActiveIcon::OnClicked()
{
	CNodeItem& node = static_cast<CNodeItem&>(GetViewItem());
	node.SetDeactivated(!node.IsDeactivated());
	if (!node.IsDeactivated())
	{
		if (GetViewWidget().IsSelected())
			SetDisplayIcon(s_iconMap.GetIcon(Icon_NodeSelected));
		else
			SetDisplayIcon(s_iconMap.GetIcon(Icon_Enabled));
	}
	else
	{
		if (GetViewWidget().IsSelected())
			SetDisplayIcon(s_iconMap.GetIcon(Icon_Disabled));
		else
			SetDisplayIcon(s_iconMap.GetIcon(Icon_NodeDeactivated));
	}
}

void CEmitterActiveIcon::OnNodeSelectionChanged(bool isSelected)
{
	CNodeItem& node = static_cast<CNodeItem&>(GetViewItem());
	if (isSelected)
	{
		if (!node.IsDeactivated())
			SetDisplayIcon(s_iconMap.GetIcon(Icon_NodeSelected));
	}
	else
	{
		if (!node.IsDeactivated())
			SetDisplayIcon(s_iconMap.GetIcon(Icon_Enabled));
		else
			SetDisplayIcon(s_iconMap.GetIcon(Icon_NodeDeactivated));
	}
}

void CEmitterActiveIcon::OnDeactivatedChanged(bool isDeactivated)
{
	if (GetViewWidget().IsSelected())
	{
		if (!isDeactivated)
			SetDisplayIcon(s_iconMap.GetIcon(Icon_NodeSelected));
	}
	else
	{
		if (!isDeactivated)
			SetDisplayIcon(s_iconMap.GetIcon(Icon_Enabled));
		else
			SetDisplayIcon(s_iconMap.GetIcon(Icon_NodeDeactivated));
	}
}

CSoloEmitterModeIcon::CSoloEmitterModeIcon(CryGraphEditor::CNodeWidget& nodeWidget)
	: CHeaderIconWidget(nodeWidget)
{
	static bool isInitialized = false;
	if (isInitialized == false)
	{
		s_iconMap.SetIcon(Icon_Enabled, CryIcon("icons:Graph/Node_Single.ico", {
				{ QIcon::Mode::Normal, QColor(255, 100, 150) /*CryGraphEditor::CGlobalStyle::GetSelectionColor()*/ }
		  }), IconMap::HeaderIcon);
		s_iconMap.SetIcon(Icon_Disabled, CryIcon("icons:Graph/Node_Single.ico", {
				{ QIcon::Mode::Normal, QColor(94, 94, 94) }
		  }), IconMap::HeaderIcon);
		s_iconMap.SetIcon(Icon_NodeSelected, CryIcon("icons:Graph/Node_Single.ico", {
				{ QIcon::Mode::Normal, QColor(255, 255, 255) }
		  }), IconMap::HeaderIcon);
		s_iconMap.SetIcon(Icon_NodeDeactivated, CryIcon("icons:Graph/Node_Single.ico", {
				{ QIcon::Mode::Normal, QColor(51, 51, 51) }
		  }), IconMap::HeaderIcon);

		isInitialized = true;
	}

	CNodeItem& node = static_cast<CNodeItem&>(GetViewItem());
	CParticleGraphModel& model = static_cast<CParticleGraphModel&>(node.GetViewModel());
	UpdateIcon(GetViewWidget().IsSelected(), node.IsVisible(), node.IsDeactivated(), model.GetSoloNode() == &node);

	GetViewWidget().SignalSelectionChanged.Connect(this, &CSoloEmitterModeIcon::OnNodeSelectionChanged);
	node.SignalVisibleChanged.Connect(this, &CSoloEmitterModeIcon::OnVisibilityChanged);
	node.SignalDeactivatedChanged.Connect(this, &CSoloEmitterModeIcon::OnDeactivatedChanged);
}

CSoloEmitterModeIcon::~CSoloEmitterModeIcon()
{
	GetViewWidget().SignalSelectionChanged.DisconnectObject(this);
	CNodeItem& node = static_cast<CNodeItem&>(GetViewItem());
	node.SignalVisibleChanged.DisconnectObject(this);
	node.SignalDeactivatedChanged.DisconnectObject(this);
}

void CSoloEmitterModeIcon::OnClicked()
{
	CNodeItem& node = static_cast<CNodeItem&>(GetViewItem());
	CParticleGraphModel& model = static_cast<CParticleGraphModel&>(node.GetViewModel());
	model.ToggleSoloNode(node);

	const bool isSoloNode = model.GetSoloNode() == &node;
	UpdateIcon(GetViewWidget().IsSelected(), node.IsVisible(), node.IsDeactivated(), isSoloNode);
}

void CSoloEmitterModeIcon::OnNodeSelectionChanged(bool isSelected)
{
	CNodeItem& node = static_cast<CNodeItem&>(GetViewItem());
	CParticleGraphModel& model = static_cast<CParticleGraphModel&>(node.GetViewModel());

	const bool isSoloNode = model.GetSoloNode() == &node;
	UpdateIcon(isSelected, node.IsVisible(), node.IsDeactivated(), isSoloNode);
}

void CSoloEmitterModeIcon::OnVisibilityChanged(bool isVisible)
{
	CNodeItem& node = static_cast<CNodeItem&>(GetViewItem());
	CParticleGraphModel& model = static_cast<CParticleGraphModel&>(node.GetViewModel());

	const bool isSoloNode = model.GetSoloNode() == &node;
	UpdateIcon(GetViewWidget().IsSelected(), isVisible, node.IsDeactivated(), isSoloNode);
}

void CSoloEmitterModeIcon::OnDeactivatedChanged(bool isDeactivated)
{
	CNodeItem& node = static_cast<CNodeItem&>(GetViewItem());
	CParticleGraphModel& model = static_cast<CParticleGraphModel&>(node.GetViewModel());

	const bool isSoloNode = model.GetSoloNode() == &node;
	UpdateIcon(GetViewWidget().IsSelected(), node.IsVisible(), isDeactivated, isSoloNode);

}

void CSoloEmitterModeIcon::UpdateIcon(bool isSelected, bool isVisible, bool isDeactivated, bool isSoloNode)
{
	if (GetViewWidget().IsSelected())
	{
		if (isSoloNode)
			SetDisplayIcon(s_iconMap.GetIcon(Icon_NodeSelected));
		else
			SetDisplayIcon(s_iconMap.GetIcon(Icon_Disabled));
	}
	else
	{
		if (isDeactivated)
			SetDisplayIcon(s_iconMap.GetIcon(Icon_NodeDeactivated));
		else if (isSoloNode)
			SetDisplayIcon(s_iconMap.GetIcon(Icon_Enabled));
		else
			SetDisplayIcon(s_iconMap.GetIcon(Icon_Disabled));
	}
}

CEmitterVisibleIcon::CEmitterVisibleIcon(CryGraphEditor::CNodeWidget& nodeWidget)
	: CHeaderIconWidget(nodeWidget)
{
	static bool isInitialized = false;
	if (isInitialized == false)
	{
		s_iconMap.SetIcon(Icon_Enabled, CryIcon("icons:Graph/Node_visible.ico", {
				{ QIcon::Mode::Normal, QColor(255, 100, 150) /*CryGraphEditor::CGlobalStyle::GetSelectionColor()*/ }
		  }), IconMap::HeaderIcon);
		s_iconMap.SetIcon(Icon_Disabled, CryIcon("icons:Graph/Node_visible.ico", {
				{ QIcon::Mode::Normal, QColor(94, 94, 94) }
		  }), IconMap::HeaderIcon);
		s_iconMap.SetIcon(Icon_NodeSelected, CryIcon("icons:Graph/Node_visible.ico", {
				{ QIcon::Mode::Normal, QColor(255, 255, 255) }
		  }), IconMap::HeaderIcon);
		s_iconMap.SetIcon(Icon_NodeDeactivated, CryIcon("icons:Graph/Node_visible.ico", {
				{ QIcon::Mode::Normal, QColor(51, 51, 51) }
		  }), IconMap::HeaderIcon);

		isInitialized = true;
	}

	CNodeItem& node = static_cast<CNodeItem&>(GetViewItem());
	UpdateIcon(GetViewWidget().IsSelected(), node.IsVisible(), node.IsDeactivated());

	GetViewWidget().SignalSelectionChanged.Connect(this, &CEmitterVisibleIcon::OnNodeSelectionChanged);
	node.SignalVisibleChanged.Connect(this, &CEmitterVisibleIcon::OnVisibilityChanged);
	node.SignalDeactivatedChanged.Connect(this, &CEmitterVisibleIcon::OnDeactivatedChanged);
}

CEmitterVisibleIcon::~CEmitterVisibleIcon()
{
	GetViewWidget().SignalSelectionChanged.DisconnectObject(this);
	CNodeItem& node = static_cast<CNodeItem&>(GetViewItem());
	node.SignalVisibleChanged.DisconnectObject(this);
	node.SignalDeactivatedChanged.DisconnectObject(this);
}

void CEmitterVisibleIcon::OnClicked()
{
	CNodeItem& node = static_cast<CNodeItem&>(GetViewItem());
	CParticleGraphModel& model = static_cast<CParticleGraphModel&>(node.GetViewModel());

	if (model.GetSoloNode() == nullptr)
	{
		node.SetVisible(!node.IsVisible());
		UpdateIcon(GetViewWidget().IsSelected(), node.IsVisible(), node.IsDeactivated());
	}
}

void CEmitterVisibleIcon::OnNodeSelectionChanged(bool isSelected)
{
	CNodeItem& node = static_cast<CNodeItem&>(GetViewItem());
	UpdateIcon(isSelected, node.IsVisible(), node.IsDeactivated());
}

void CEmitterVisibleIcon::OnVisibilityChanged(bool isVisible)
{
	CNodeItem& node = static_cast<CNodeItem&>(GetViewItem());
	UpdateIcon(GetViewWidget().IsSelected(), isVisible, node.IsDeactivated());
}

void CEmitterVisibleIcon::OnDeactivatedChanged(bool isDeactivated)
{
	CNodeItem& node = static_cast<CNodeItem&>(GetViewItem());
	UpdateIcon(GetViewWidget().IsSelected(), node.IsVisible(), isDeactivated);
}

void CEmitterVisibleIcon::UpdateIcon(bool isSelected, bool isVisible, bool isDeactivated)
{
	if (isSelected)
	{
		if (isVisible)
			SetDisplayIcon(s_iconMap.GetIcon(Icon_NodeSelected));
		else
			SetDisplayIcon(s_iconMap.GetIcon(Icon_Disabled));
	}
	else
	{
		if (isDeactivated && isVisible)
			SetDisplayIcon(s_iconMap.GetIcon(Icon_NodeDeactivated));
		else if (isVisible)
			SetDisplayIcon(s_iconMap.GetIcon(Icon_Enabled));
		else
			SetDisplayIcon(s_iconMap.GetIcon(Icon_Disabled));
	}
}

}
