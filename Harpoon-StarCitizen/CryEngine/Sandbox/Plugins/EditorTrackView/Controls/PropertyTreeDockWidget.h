// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>

#include "TrackViewCoreComponent.h"

#include <CrySerialization/IArchive.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/Enum.h>

class QVBoxLayout;
class QPropertyTreeLegacy;
class CTrackViewSequenceTabWidget;

namespace
{
struct SSelectedKey;
}

class CTrackViewPropertyTreeWidget : public QWidget, public CTrackViewCoreComponent
{
	Q_OBJECT

	struct STrackViewPropertiesRoot
	{
		CTrackViewCore&         core;
		Serialization::SStructs structs;
		yasli::Context          entityContext;
		yasli::Context          sequenceContext;
		yasli::Context          animTimeContext;

		STrackViewPropertiesRoot(CTrackViewCore& _core);

		void Serialize(Serialization::IArchive& ar);

		void SetEntityContext(Serialization::IArchive& ar, const SSelectedKey& key, CTrackViewSequenceTabWidget* pTabWidget);
		void Clear();
		operator Serialization::SStruct();
	};

public:
	CTrackViewPropertyTreeWidget(CTrackViewCore* pTrackViewCore);
	~CTrackViewPropertyTreeWidget() {}

	// CTrackViewCoreComponent
	virtual void        OnTrackViewEditorEvent(ETrackViewEditorEvent event) override;
	virtual const char* GetComponentTitle() const override { return "Property Pane"; }
	// ~CTrackViewCoreComponent

	virtual QSize sizeHint() const { return QSize(80, 100); }

	void          Update();

protected:
	// CTrackViewCoreComponent
	virtual void OnNodeChanged(CTrackViewNode* pNode, ENodeChangeType type) override;
	// ~CTrackViewCoreComponent

private:
	void OnPropertiesBeginUndo();
	void OnPropertiesChanged();
	void OnPropertiesEndUndo(bool undoAccepted);

private:
	bool                                  m_bUndoPush;
	//bool m_bDontUpdateProperties;
	QPropertyTreeLegacy*                        m_pPropertyTree;
	std::vector<STrackViewPropertiesRoot> m_properties;
};
