// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <QToolBar>
#include "TrackViewCoreComponent.h"

class QActionGroup;

class CTrackViewKeysToolbar : public QToolBar, public CTrackViewCoreComponent
{
	Q_OBJECT

	//TODO: avoid friend class
	friend class CTrackViewWindow; 
	//~TODO

public:
	CTrackViewKeysToolbar(CTrackViewCore* pTrackViewCore);
	~CTrackViewKeysToolbar() {}

protected:
	virtual void        OnTrackViewEditorEvent(ETrackViewEditorEvent event) override;
	virtual const char* GetComponentTitle() const override { return "Keys Toolbar"; }

private:
	void                SetupSlideAction(QAction* pAction);

private slots:
	void OnGoToPreviousKey();
	void OnGoToNextKey();
	void OnMoveKeys();
	void OnSlideKeys();
	void OnScaleKeys();
	void OnSnapModeChanged();
	void OnSyncSelectedTracksToBase();
	void OnSyncSelectedTracksFromBase();

private:
	QActionGroup* m_pSnappingGroup;
	QActionGroup* m_pMoveModeGroup;
};
