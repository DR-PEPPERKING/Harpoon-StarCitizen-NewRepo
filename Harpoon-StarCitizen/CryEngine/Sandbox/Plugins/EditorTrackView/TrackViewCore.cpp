// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TrackViewPlugin.h"
#include "Nodes/TrackViewSequence.h"

#include "QtUtil.h"
#include <CryMovie/AnimKey_impl.h>
#include "IUndoObject.h"

#include "TrackViewComponentsManager.h"
#include "Controls/SequenceTabWidget.h"
#include "Controls/EditorDialog.h"

#include <QLineEdit>
#include <QByteArray>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>

class QNewSequenceDlg : public CEditorDialog
{
public:
	QNewSequenceDlg()
		: CEditorDialog("New Sequence")
	{
		QVBoxLayout* pDialogLayout = new QVBoxLayout(this);
		QFormLayout* pFormLayout = new QFormLayout();
		pDialogLayout->addLayout(pFormLayout);

		m_pSequenceName = new QLineEdit();
		m_pSequenceName->setMinimumWidth(200);
		QObject::connect(m_pSequenceName, &QLineEdit::textChanged, this, &QNewSequenceDlg::OnTextChanged);
		pFormLayout->addRow(QObject::tr("&Sequence Name:"), m_pSequenceName);

		m_pWarning = new QLabel("Name is already in use!");
		m_pWarning->setHidden(true);
		pFormLayout->addRow("", m_pWarning);

		QDialogButtonBox* pButtonBox = new QDialogButtonBox(this);
		pButtonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
		pDialogLayout->addWidget(pButtonBox);

		m_pOkButton = pButtonBox->button(QDialogButtonBox::Ok);
		m_pOkButton->setEnabled(false);

		QObject::connect(pButtonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
		QObject::connect(pButtonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
	}

	string GetResult() const
	{
		string seqName(m_pSequenceName->text().toLocal8Bit().data());
		return seqName;
	}

protected:
	void OnTextChanged(const QString& text)
	{
		if (!text.isEmpty())
		{
			QByteArray byteArray = text.toLocal8Bit();
			CryStackStringT<char, 256> enteredName(byteArray.data());
			enteredName.MakeLower();

			for (uint32 i = CTrackViewPlugin::GetSequenceManager()->GetCount(); i--; )
			{
				CryStackStringT<char, 256> seqName(CTrackViewPlugin::GetSequenceManager()->GetSequenceByIndex(i)->GetName());
				seqName.MakeLower();

				if (seqName == enteredName)
				{
					m_pWarning->setVisible(true);
					m_pOkButton->setEnabled(false);
					return;
				}
			}

			m_pOkButton->setEnabled(true);
			m_pWarning->setVisible(false);
		}
		else
		{
			m_pOkButton->setEnabled(false);
			m_pWarning->setVisible(false);
		}
	}

	QLineEdit*   m_pSequenceName;
	QLabel*      m_pWarning;
	QPushButton* m_pOkButton;
};

CTrackViewCore::CTrackViewCore()
	: m_currentProperties(0)
	, m_keyMoveMode(eKeyMoveMode_Move)
	, m_keySlideMode(eKeySlideMode_Both)
	, m_bLockProperties(false)
{
	m_pTrackViewComponentsManager = new CTrackViewComponentsManager();

	m_animEntityDefaultTracks.push_back(eAnimParamType_Position);
	m_animEntityDefaultTracks.push_back(eAnimParamType_Rotation);
}

CTrackViewCore::~CTrackViewCore()
{
	delete m_pTrackViewComponentsManager;
}

void CTrackViewCore::Init()
{
	m_pTrackViewComponentsManager->Init(this);

	SetFramerate(SAnimTime::eFrameRate_30fps);
	SetDisplayMode(SAnimTime::EDisplayMode::Time);
	SetSnapMode(eSnapMode_NoSnapping);
	SetKeysMoveMode(eKeyMoveMode_Move);
	SetPlaybackSpeed(1.0f);
}

CTrackViewSequence* CTrackViewCore::GetSequenceByGUID(CryGUID sequenceGUID) const
{
	return CTrackViewPlugin::GetSequenceManager()->GetSequenceByGUID(sequenceGUID);
}

void CTrackViewCore::LockProperties()
{
	m_bLockProperties = true;
}

void CTrackViewCore::UpdateProperties(EPropertyDataSource dataSource)
{
	if (!m_bLockProperties)
	{
		Serialization::SStructs structs;
		GetComponentsManager()->GetTrackViewSequenceTabWidget()->GetPropertiesToUpdate(structs, dataSource);
		SetProperties(structs, dataSource);
	}
}

void CTrackViewCore::UnlockProperties()
{
	m_bLockProperties = false;
}

size_t CTrackViewCore::GetAnimEntityDefaultTrackCount() const
{
	return m_animEntityDefaultTracks.size();
}

CAnimParamType CTrackViewCore::GetAnimEntityDefaultTrackByIndex(size_t index) const
{
	if (index < GetAnimEntityDefaultTrackCount())
	{
		return m_animEntityDefaultTracks[index];
	}

	static CAnimParamType stub;
	return stub;
}

void CTrackViewCore::AddAnimEntityDefaultTrackType(CAnimParamType paramType)
{
	auto it = std::find(m_animEntityDefaultTracks.begin(), m_animEntityDefaultTracks.end(), paramType);
	if (it == m_animEntityDefaultTracks.end())
	{
		m_animEntityDefaultTracks.push_back(paramType);
	}
}

void CTrackViewCore::RemoveAnimEntityDefaultTrackType(CAnimParamType paramType)
{
	auto it = std::find(m_animEntityDefaultTracks.begin(), m_animEntityDefaultTracks.end(), paramType);
	if (it != m_animEntityDefaultTracks.end())
	{
		m_animEntityDefaultTracks.erase(it);
	}
}

bool CTrackViewCore::IsContainsAnimEntityDefaultTrackType(CAnimParamType paramType) const
{
	auto it = std::find(m_animEntityDefaultTracks.begin(), m_animEntityDefaultTracks.end(), paramType);
	return it != m_animEntityDefaultTracks.end();
}

void CTrackViewCore::NewSequence()
{
	QNewSequenceDlg dlg;
	if (dlg.exec())
	{
		if (CTrackViewSequence* pSequence = CTrackViewPlugin::GetSequenceManager()->CreateSequence(dlg.GetResult()))
		{
			OpenSequence(pSequence->GetGUID());
		}
	}
}

void CTrackViewCore::OpenSequence(const CryGUID sequenceGUID)
{
	GetComponentsManager()->GetTrackViewSequenceTabWidget()->OpenSequenceTab(sequenceGUID);
}

void CTrackViewCore::OnPlayPause(bool bState)
{
	CTrackViewPlugin::GetAnimationContext()->PlayPause(bState);
}

bool CTrackViewCore::IsPlaying() const
{
	return CTrackViewPlugin::GetAnimationContext()->IsPlaying();
}

bool CTrackViewCore::IsPaused() const
{
	return CTrackViewPlugin::GetAnimationContext()->IsPaused();
}

bool CTrackViewCore::IsRecording() const
{
	return CTrackViewPlugin::GetAnimationContext()->IsRecording();
}

bool CTrackViewCore::IsLooping() const
{
	return CTrackViewPlugin::GetAnimationContext()->IsLoopMode();
}

void CTrackViewCore::SetPlaybackStart()
{
	if (CTrackViewSequence* pSequence = GetComponentsManager()->GetTrackViewSequenceTabWidget()->GetActiveSequence())
	{
		auto playbackRange = pSequence->GetPlaybackRange();

		CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();
		playbackRange.start = pAnimationContext->GetTime();

		pSequence->SetPlaybackRange(playbackRange);
	}
}

void CTrackViewCore::SetPlaybackEnd()
{
	if (CTrackViewSequence* pSequence = GetComponentsManager()->GetTrackViewSequenceTabWidget()->GetActiveSequence())
	{
		auto playbackRange = pSequence->GetPlaybackRange();

		CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();
		playbackRange.end = pAnimationContext->GetTime();

		pSequence->SetPlaybackRange(playbackRange);
	}
}

void CTrackViewCore::ResetPlaybackRange()
{
	if (CTrackViewSequence* pSequence = GetComponentsManager()->GetTrackViewSequenceTabWidget()->GetActiveSequence())
	{
		TRange<SAnimTime> defaultRange(-1, -1);
		pSequence->SetPlaybackRange(defaultRange);
	}
}

void CTrackViewCore::SetPlaybackRange(TRange<SAnimTime> markers)
{
	if (CTrackViewSequence* pSequence = GetComponentsManager()->GetTrackViewSequenceTabWidget()->GetActiveSequence())
	{
		pSequence->SetPlaybackRange(markers);
	}
}

TRange<SAnimTime> CTrackViewCore::GetPlaybackRange() const
{
	if (CTrackViewSequence* pSequence = GetComponentsManager()->GetTrackViewSequenceTabWidget()->GetActiveSequence())
	{
		return pSequence->GetPlaybackRange();
	}

	return TRange<SAnimTime>(0, 0);
}

void CTrackViewCore::OnRecord(bool bState)
{
	CTrackViewPlugin::GetAnimationContext()->SetRecording(bState);
}

void CTrackViewCore::OnGoToStart()
{
	CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();
	const TRange<SAnimTime> sequenceTime = pAnimationContext->GetTimeRange();
	pAnimationContext->SetTime(sequenceTime.start);
}

void CTrackViewCore::OnGoToEnd()
{
	CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();
	const TRange<SAnimTime> sequenceTime = pAnimationContext->GetTimeRange();
	pAnimationContext->SetTime(sequenceTime.end);
}

void CTrackViewCore::OnStop()
{
	CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();
	pAnimationContext->Stop();
	pAnimationContext->SetTime(SAnimTime(0));
}

void CTrackViewCore::OnLoop(bool bState)
{
	CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();
	pAnimationContext->SetLoopMode(bState);
}

void CTrackViewCore::OnAddSelectedEntities()
{
	if (CTrackViewSequence* pSequence = GetComponentsManager()->GetTrackViewSequenceTabWidget()->GetActiveSequence())
	{
		CTrackViewSequenceNotificationContext context(pSequence);

		CUndo undo("Add selected entities to sequence");

		CTrackViewNode* pSelectedNode = pSequence->GetFirstSelectedNode();
		pSelectedNode = pSelectedNode ? pSelectedNode : pSequence;

		CTrackViewAnimNodeBundle bundle;
		if (pSelectedNode->IsGroupNode())
		{
			CTrackViewAnimNode* pSelectedAnimNode = static_cast<CTrackViewAnimNode*>(pSelectedNode);
			bundle = pSelectedAnimNode->AddSelectedEntities();
		}
		else
		{
			bundle = pSequence->AddSelectedEntities();
		}

		SpawnDefaultTracksForBundle(bundle);
	}
}

void CTrackViewCore::OnAddNode(EAnimNodeType nodeType)
{
	if (CTrackViewSequence* pSequence = GetComponentsManager()->GetTrackViewSequenceTabWidget()->GetActiveSequence())
	{
		CTrackViewSequenceNotificationContext context(pSequence);

		CUndo undo("Create TrackView Director Node");
		const string name = pSequence->GetAvailableNodeNameStartingWith("Director");
		pSequence->CreateSubNode(name, eAnimNodeType_Director);
	}
}

void CTrackViewCore::OnGoToPreviousKey()
{
	CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();
	CTrackViewSequence* pSequence = pAnimationContext->GetSequence();

	if (pSequence)
	{
		SAnimTime time = pAnimationContext->GetTime();
		CTrackViewNode* pNode = pSequence->GetFirstSelectedNode();
		pNode = pNode ? pNode : pSequence;

		if (pNode->SnapTimeToPrevKey(time))
		{
			pAnimationContext->SetTime(time);
		}
	}
}

void CTrackViewCore::OnGoToNextKey()
{
	CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();
	CTrackViewSequence* pSequence = pAnimationContext->GetSequence();

	if (pSequence)
	{
		SAnimTime time = pAnimationContext->GetTime();
		CTrackViewNode* pNode = pSequence->GetFirstSelectedNode();
		pNode = pNode ? pNode : pSequence;

		if (pNode->SnapTimeToNextKey(time))
		{
			pAnimationContext->SetTime(time);
		}
	}
}

void CTrackViewCore::OnMoveKeys()
{
	SetKeysMoveMode(eKeyMoveMode_Move);
}

void CTrackViewCore::OnSlideKeys()
{
	SetKeysMoveMode(eKeyMoveMode_Slide);
}

void CTrackViewCore::OnScaleKeys()
{
	SetKeysMoveMode(eKeyMoveMode_Scale);
}

void CTrackViewCore::SetKeysMoveMode(ETrackViewKeyMoveMode mode)
{
	m_keyMoveMode = mode;
	GetComponentsManager()->BroadcastTrackViewEditorEvent(eTrackViewEditorEvent_OnKeyMoveModeChanged);
}

void CTrackViewCore::SetKeysSlideMode(ETrackViewKeySlideMode slideMode)
{
	m_keySlideMode = slideMode;
	GetComponentsManager()->BroadcastTrackViewEditorEvent(eTrackViewEditorEvent_OnKeyMoveModeChanged);
}

void CTrackViewCore::SetPlaybackSpeed(float newPlaybackSpeed)
{
	m_playbackSpeed = newPlaybackSpeed;
	CAnimationContext* pAnimationContext = CTrackViewPlugin::GetAnimationContext();
	pAnimationContext->SetTimeScale(newPlaybackSpeed);

	GetComponentsManager()->BroadcastTrackViewEditorEvent(eTrackViewEditorEvent_OnPlaybackSpeedChanged);
}

void CTrackViewCore::SetSnapMode(ETrackViewSnapMode newSnapMode)
{
	m_SnapMode = newSnapMode;
	GetComponentsManager()->BroadcastTrackViewEditorEvent(eTrackViewEditorEvent_OnSnapModeChanged);
}

void CTrackViewCore::SetFramerate(SAnimTime::EFrameRate newFramerate)
{
	m_animTimeSettings.fps = newFramerate;
	GetComponentsManager()->BroadcastTrackViewEditorEvent(eTrackViewEditorEvent_OnFramerateChanged);
}

void CTrackViewCore::SetDisplayMode(SAnimTime::EDisplayMode newDisplayMode)
{
	m_animTimeSettings.displayMode = newDisplayMode;
	GetComponentsManager()->BroadcastTrackViewEditorEvent(eTrackViewEditorEvent_OnDisplayModeChanged);
	// TODO: timeline and curveeditor support for different time units
}

void CTrackViewCore::SpawnDefaultTracksForBundle(const CTrackViewAnimNodeBundle& bundle)
{
	QTimer::singleShot(1, [this, bundle]()
	{
		size_t animNodeCount = bundle.GetCount();
		size_t defaultTrackCount = GetAnimEntityDefaultTrackCount();

		for (size_t i = 0; i < animNodeCount; i++)
		{
			CTrackViewAnimNode* pAnimNode = const_cast<CTrackViewAnimNode*>(bundle.GetNode(i));
			for (size_t j = 0; j < defaultTrackCount; j++)
			{
				CAnimParamType paramType = GetAnimEntityDefaultTrackByIndex(j);
				pAnimNode->CreateTrack(paramType);
			}
		}
	});
}

void CTrackViewCore::SetProperties(Serialization::SStructs& structs, EPropertyDataSource source)
{
	m_currentProperties.swap(structs);
	GetComponentsManager()->BroadcastTrackViewEditorEvent(eTrackViewEditorEvent_OnPropertiesChanged);
}

void CTrackViewCore::ClearProperties()
{
	m_currentProperties.clear();
	GetComponentsManager()->BroadcastTrackViewEditorEvent(eTrackViewEditorEvent_OnPropertiesChanged);
}

void CTrackViewCore::OnSyncSelectedTracksToBase()
{
	CTrackViewSequence* pSequence = GetComponentsManager()->GetTrackViewSequenceTabWidget()->GetActiveSequence();
	if (pSequence)
	{
		pSequence->SyncSelectedTracksToBase();
	}
}

void CTrackViewCore::OnSyncSelectedTracksFromBase()
{
	CTrackViewSequence* pSequence = GetComponentsManager()->GetTrackViewSequenceTabWidget()->GetActiveSequence();
	if (pSequence)
	{
		pSequence->SyncSelectedTracksFromBase();
	}
}
