// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>

class QPropertyGroupBox : public QWidget
{
	Q_OBJECT
public:
	explicit QPropertyGroupBox(QWidget* parent = 0)
		: QWidget(parent)
	{
	}
};

class QPropertyGroupHeader : public QWidget
{
	Q_OBJECT
public:
	explicit QPropertyGroupHeader(QWidget* parent = 0)
		: QWidget(parent)
	{
	}
};
