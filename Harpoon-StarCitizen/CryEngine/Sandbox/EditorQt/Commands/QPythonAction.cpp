// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>

#include "QPythonAction.h"
#include "Util/BoostPythonHelpers.h"

#include <QAction>
#include <QStatusBar>

#include "Qt/QtMainFrame.h"

QPythonAction::QPythonAction(const QString& actionName, const QString& actionText, QObject* parent, const char* pythonCall)
	: QAction(actionName, parent)
{
	setText(actionText);
	if (pythonCall)
	{
		setProperty("QPythonAction", QVariant(QString(pythonCall)));
	}
	connect(this, &QPythonAction::triggered, this, &QPythonAction::OnTriggered);
}

QPythonAction::QPythonAction(const QString& actionName, const char* pythonCall, QObject* parent)
	: QAction(actionName, parent)
{
	setObjectName(actionName);
	if (pythonCall)
	{
		setProperty("QPythonAction", QVariant(QString(pythonCall)));
	}
	connect(this, &QPythonAction::triggered, this, &QPythonAction::OnTriggered);
}

void QPythonAction::OnTriggered()
{
	string py_command;

	QVariant pythonProp = property("QPythonAction");
	if (pythonProp.isValid())
	{
		QString qstr = pythonProp.toString();
		py_command = qstr.toStdString().c_str();
	}
	else
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "Invalid QCommandAction %s", objectName().toStdString().c_str());
	}

	PyScript::Execute("%s", py_command.c_str());
}
