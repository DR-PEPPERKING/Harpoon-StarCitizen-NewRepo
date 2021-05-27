// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "AnimationContext.h"
#include "TrackViewSequenceManager.h"

#include <CryMovie/AnimKey.h>
#include <CryMovie/AnimTime.h>
#include <CryExtension/CryGUID.h>
#include <QtViewPane.h>

class CTrackViewSequence;
class CTrackViewComponentsManager;

enum ETrackViewSnapMode
{
	eSnapMode_NoSnapping = 1,
	eSnapMode_TimeSnapping,
	eSnapMode_KeySnapping
};

enum ETrackViewKeyMoveMode
{
	eKeyMoveMode_Slide = 1,
	eKeyMoveMode_Scale,
	eKeyMoveMode_Move,
};

enum ETrackViewKeySlideMode
{
	eKeySlideMode_Both = 0,
	eKeySlideMode_Left,
	eKeySlideMode_Right,
};

enum EPropertyDataSource
{
	ePropertyDataSource_None = 0,
	ePropertyDataSource_Sequence,
	ePropertyDataSource_Timeline,
};

class CTrackViewCore
{
public:
	CTrackViewCore();
	~CTrackViewCore();

	void                         Init();

	void                         NewSequence();
	void                         OpenSequence(const CryGUID sequenceGUID);

	void                         OnPlayPause(bool bState);
	void                         OnRecord(bool bState);
	void                         OnGoToStart();
	void                         OnGoToEnd();
	void                         OnStop();
	void                         OnLoop(bool bState);

	bool                         IsPlaying() const;
	bool                         IsPaused() const;
	bool                         IsRecording() const;
	bool                         IsLooping() const;

	void                         SetPlaybackStart();
	void                         SetPlaybackEnd();
	void                         ResetPlaybackRange();
	void                         SetPlaybackRange(TRange<SAnimTime> markers);
	TRange<SAnimTime>            GetPlaybackRange() const;

	void                         OnAddSelectedEntities();
	void                         OnAddNode(EAnimNodeType nodeType);

	void                         OnGoToPreviousKey();
	void                         OnGoToNextKey();
	void                         OnMoveKeys();
	void                         OnSlideKeys();
	void                         OnScaleKeys();

	void                         SetFramerate(SAnimTime::EFrameRate newFramerate);
	void                         SetDisplayMode(SAnimTime::EDisplayMode newDisplayMode);
	void                         SetPlaybackSpeed(float newPlaybackSpeed);
	void                         SetSnapMode(ETrackViewSnapMode newSnapMode);
	void                         SetKeysMoveMode(ETrackViewKeyMoveMode moveMode);
	void                         SetKeysSlideMode(ETrackViewKeySlideMode slideMode);

	void                         ClearProperties();

	void                         OnSyncSelectedTracksToBase();
	void                         OnSyncSelectedTracksFromBase();

	void                         LockProperties();
	void                         UpdateProperties(EPropertyDataSource dataSource = ePropertyDataSource_None);
	void                         UnlockProperties();

	size_t                       GetAnimEntityDefaultTrackCount() const;
	CAnimParamType               GetAnimEntityDefaultTrackByIndex(size_t index) const;
	void                         AddAnimEntityDefaultTrackType(CAnimParamType paramType);
	void                         RemoveAnimEntityDefaultTrackType(CAnimParamType paramType);
	bool                         IsContainsAnimEntityDefaultTrackType(CAnimParamType paramType) const;

	float                        GetCurrentPlaybackSpeed() const                              { return m_playbackSpeed; }
	ETrackViewSnapMode           GetCurrentSnapMode() const                                   { return m_SnapMode; }
	ETrackViewKeyMoveMode        GetCurrentKeyMoveMode() const                                { return m_keyMoveMode; }
	ETrackViewKeySlideMode       GetCurrentKeySlideMode() const                               { return m_keySlideMode; }
	SAnimTime::EFrameRate        GetCurrentFramerate() const                                  { return m_animTimeSettings.fps; }
	SAnimTime::EDisplayMode      GetCurrentDisplayMode() const                                { return m_animTimeSettings.displayMode; }
	void                         GetCurrentProperties(Serialization::SStructs& structs) const { structs = m_currentProperties; }

	CTrackViewSequence*          GetSequenceByGUID(CryGUID sequenceGUID) const;
	CTrackViewComponentsManager* GetComponentsManager() const { return m_pTrackViewComponentsManager; }

	const SAnimTime::Settings*   GetAnimTimeSettings() const  { return &m_animTimeSettings; }

private:
	void                         SpawnDefaultTracksForBundle(const CTrackViewAnimNodeBundle& bundle);
	void                         SetProperties(Serialization::SStructs& structs, EPropertyDataSource source);

	float                        m_playbackSpeed;
	CryGUID                      m_activeSequenceUId;
	ETrackViewSnapMode           m_SnapMode;
	ETrackViewKeyMoveMode        m_keyMoveMode;
	ETrackViewKeySlideMode       m_keySlideMode;
	Serialization::SStructs      m_currentProperties;
	CTrackViewComponentsManager* m_pTrackViewComponentsManager;
	SAnimTime::Settings          m_animTimeSettings;
	std::vector<CAnimParamType>  m_animEntityDefaultTracks;

	bool                         m_bLockProperties;
};
