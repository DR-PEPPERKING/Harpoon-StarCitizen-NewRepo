// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "EditorCommonAPI.h"
#include "QtViewPane.h"

#include <QAbstractItemModel>

class QAdvancedPropertyTreeLegacy;
class QContainer;
class QObjectTreeWidget;
class QSplitter;
struct SPreferencePage;

class EDITOR_COMMON_API QPreferencePage : public QWidget
{
	Q_OBJECT
public:
	QPreferencePage(SPreferencePage* pPreferencePage, QWidget* pParent = nullptr);
	QPreferencePage(std::vector<SPreferencePage*> preferences, const char* path, QWidget* pParent = nullptr);
	~QPreferencePage();

private:
	void InitUI();
	void OnPreferenceChanged();

	void ConnectPreferences();
	void DisconnectPreferences();

private slots:
	void OnPropertyChanged();
	void OnResetToDefault();

private:
	QAdvancedPropertyTreeLegacy* m_pPropertyTree;
	std::vector<SPreferencePage*> m_preferencePages;
	const string m_path;
};


class EDITOR_COMMON_API QPreferencesDialog : public CDockableWidget
{
public:
	QPreferencesDialog(QWidget* pParent = nullptr);
	~QPreferencesDialog();

	//////////////////////////////////////////////////////////
	// CDockableWidget implementation
	virtual const char* GetPaneTitle() const override { return "Preferences"; }
	virtual QRect       GetPaneRect() override        { return QRect(0, 0, 640, 480); }
	//////////////////////////////////////////////////////////

protected:
	void OnPreferencePageSelected(const char* path);
	void OnPreferencesReset();

private:
	QContainer*        m_pContainer;
	QSplitter*         m_pSplitter;
	QObjectTreeWidget* m_pTreeView;
	string             m_currPath;
};
