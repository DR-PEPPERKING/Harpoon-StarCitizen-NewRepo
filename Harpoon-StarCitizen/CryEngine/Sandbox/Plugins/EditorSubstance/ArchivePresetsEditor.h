// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once
#include "AssetSystem/AssetEditor.h"
#include <SubstanceCommon.h>

class CAsset;
class QDialogButtonBox;
class QEvent;
class QResizeEvent;
class QScrollableBox;

namespace EditorSubstance
{
class CSubstanceOutputEditorDialog;
class OutputsWidget;

typedef std::map<string, OutputsWidget*>                OutputsWidgetsMap;
typedef std::map<string, std::vector<SSubstanceOutput>> OutputsMap;
typedef std::map<string, Vec2i>                         ResolutionsMap;

class CArchivePresetsEditor : public CAssetEditor
{
	Q_OBJECT
public:
	CArchivePresetsEditor(QWidget* parent = nullptr);
	const string&       GetSubstanceArchive() const             { return m_substanceArchive; }
protected:
	virtual bool        IsDockingSystemEnabled() const override { return false; }
	virtual const char* GetEditorName() const override          { return "Substance Archive Graph Editor"; }

	virtual bool        OnOpenAsset(CAsset* pAsset) override;
	virtual bool        OnSaveAsset(CEditableAsset& editAsset) override;
	virtual void        OnCloseAsset() override;

	virtual bool        eventFilter(QObject*, QEvent*) override;
	virtual void        resizeEvent(QResizeEvent* e) override;
	virtual bool        event(QEvent* e) override;

	void                OnOutputEditorAccepted(const string& graphName);
	void                OnOutputEditorRejected();
	void                OnEditOutputs(const string& graphName);

private:
	OutputsWidgetsMap             m_outputsWidgets;
	QWidget*                      m_pOutputsWidgetHolder;
	OutputsMap                    m_outputs;
	ResolutionsMap                m_resolutions;
	QDialogButtonBox*             m_pButtons;
	string                        m_substanceArchive;
	QWidget*                      m_pModalGuard;
	CSubstanceOutputEditorDialog* m_pOutputsGraphEditor;
	QScrollableBox*               m_pScrollBox;
};

}
