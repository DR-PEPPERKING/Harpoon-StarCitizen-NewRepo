// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QWidget>

class QPropertySplitter : public QWidget
{
	Q_OBJECT
public:
	explicit QPropertySplitter(QWidget* parent = 0)
		: QWidget(parent)
	{
	}
};
