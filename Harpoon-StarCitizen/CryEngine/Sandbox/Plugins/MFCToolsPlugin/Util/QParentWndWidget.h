// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "MFCToolsDefines.h"
#include <QWidget>

// QParentWndWidget can be used to show Qt popup windows/dialogs on top on
// Win32/MFC windows.
//
// Example of usage:
//   QParentWndWidget parent(parentHwnd);
//
//   QDialog dialog(parent);
//   dialog.exec(...);
class MFC_TOOLS_PLUGIN_API QParentWndWidget : public QWidget
{
	Q_OBJECT
public:
	QParentWndWidget(HWND parent);

	void show();
	void hide();
	void center();

	HWND parentWindow() const { return m_parent; }

protected:
	void childEvent(QChildEvent* e) override;
	void focusInEvent(QFocusEvent* ev) override;
	bool focusNextPrevChild(bool next) override;

	bool eventFilter(QObject* o, QEvent* e) override;
	bool nativeEvent(const QByteArray&, void* message, long*);

private:
	HWND m_parent;
	HWND m_parentToCenterOn;
	HWND m_modalityRoot;

	HWND m_previousFocus;

	bool m_parentWasDisabled;
};
