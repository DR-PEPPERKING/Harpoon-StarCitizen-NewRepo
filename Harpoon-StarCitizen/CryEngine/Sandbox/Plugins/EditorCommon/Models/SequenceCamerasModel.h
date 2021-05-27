// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "EditorCommonAPI.h"

#include <CryMovie/IMovieSystem.h>
#include <QAbstractItemModel>

class EDITOR_COMMON_API CSequenceCamerasModel : public QAbstractItemModel
{
public:
	CSequenceCamerasModel(IAnimSequence& sequence, QObject* pParent = nullptr)
		: m_pSequence(&sequence)
	{
		Load();
	}

	virtual ~CSequenceCamerasModel()
	{}

	// QAbstractItemModel
	virtual int         rowCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual int         columnCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual QVariant    data(const QModelIndex& index, int role) const override;
	virtual QVariant    headerData(int section, Qt::Orientation orientation, int role) const override;
	virtual QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	virtual QModelIndex parent(const QModelIndex& index) const override;
	// ~QAbstractItemModel

private:
	void Load();

	_smart_ptr<IAnimSequence> m_pSequence;
	std::vector<string>       m_cameraNodes;
};
