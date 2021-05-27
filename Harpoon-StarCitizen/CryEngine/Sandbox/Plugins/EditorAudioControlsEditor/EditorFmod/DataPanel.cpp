// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DataPanel.h"

#include "Common.h"
#include "Impl.h"
#include "FilterProxyModel.h"
#include "TreeView.h"
#include "../Common/ModelUtils.h"

#include <QFilteringPanel.h>
#include <QSearchBox.h>
#include <QtUtil.h>
#include <CryAudioImplFmod/GlobalData.h>

#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QVBoxLayout>

namespace ACE
{
namespace Impl
{
namespace Fmod
{
CryAudio::Impl::Fmod::STriggerInfo g_previewTriggerInfo;
bool g_isPreviewPlaying = false;

//////////////////////////////////////////////////////////////////////////
CDataPanel::CDataPanel(CImpl const& impl, QWidget* const pParent)
	: QWidget(pParent)
	, m_impl(impl)
	, m_pFilterProxyModel(new CFilterProxyModel(this))
	, m_pModel(new CItemModel(impl.GetRootItem(), this))
	, m_pTreeView(new CTreeView(this))
{
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	m_pFilterProxyModel->setSourceModel(m_pModel);
	m_pFilterProxyModel->setFilterKeyColumn(g_nameColumn);

	m_pTreeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_pTreeView->setDragEnabled(true);
	m_pTreeView->setDragDropMode(QAbstractItemView::DragOnly);
	m_pTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pTreeView->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_pTreeView->setTreePosition(g_nameColumn);
	m_pTreeView->setUniformRowHeights(true);
	m_pTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
	m_pTreeView->setModel(m_pFilterProxyModel);
	m_pTreeView->sortByColumn(g_nameColumn, Qt::AscendingOrder);
	m_pTreeView->viewport()->installEventFilter(this);
	m_pTreeView->installEventFilter(this);
	m_pTreeView->header()->setMinimumSectionSize(25);
	m_pTreeView->header()->setSectionResizeMode(static_cast<int>(CItemModel::EColumns::Notification), QHeaderView::ResizeToContents);
	m_pTreeView->TriggerRefreshHeaderColumns();

	m_pFilteringPanel = new QFilteringPanel("ACEFmodDataPanel", m_pFilterProxyModel, this);
	m_pFilteringPanel->SetContent(m_pTreeView);
	m_pFilteringPanel->GetSearchBox()->SetAutoExpandOnSearch(m_pTreeView);

	auto const pMainLayout = new QVBoxLayout(this);
	pMainLayout->setContentsMargins(0, 0, 0, 0);
	pMainLayout->addWidget(m_pFilteringPanel);

	QObject::connect(m_pTreeView, &CTreeView::customContextMenuRequested, this, &CDataPanel::OnContextMenu);
	QObject::connect(m_pTreeView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &CDataPanel::StopEvent);
}

//////////////////////////////////////////////////////////////////////////
CDataPanel::~CDataPanel()
{
	StopEvent();

	if (g_pDataPanel == this)
	{
		g_pDataPanel = nullptr;
	}
}

//////////////////////////////////////////////////////////////////////////
bool CDataPanel::eventFilter(QObject* pObject, QEvent* pEvent)
{
	if (pEvent->type() == QEvent::KeyRelease)
	{
		auto const pKeyEvent = static_cast<QKeyEvent const*>(pEvent);

		if ((pKeyEvent != nullptr) && (pKeyEvent->key() == Qt::Key_Space))
		{
			PlayEvent();
		}
	}
	else if (pEvent->type() == QEvent::MouseButtonDblClick)
	{
		PlayEvent();
	}

	return QWidget::eventFilter(pObject, pEvent);
}

//////////////////////////////////////////////////////////////////////////
void CDataPanel::OnContextMenu(QPoint const& pos)
{
	auto const pContextMenu = new QMenu(this);
	QModelIndexList const selection = m_pTreeView->selectionModel()->selectedRows(g_nameColumn);

	if (!selection.isEmpty())
	{
		if (selection.count() == 1)
		{
			ControlId const itemId = selection[0].data(static_cast<int>(ModelUtils::ERoles::Id)).toInt();
			auto const pItem = static_cast<CItem const*>(m_impl.GetItem(itemId));

			if (pItem != nullptr)
			{
				if (pItem->GetType() == EItemType::Event)
				{
					pContextMenu->addAction(tr("Play Event"), [&]() { PlayEvent(); });
					pContextMenu->addSeparator();
				}

				if ((pItem->GetFlags() & EItemFlags::IsConnected) != EItemFlags::None)
				{
					SControlInfos controlInfos;
					m_impl.SignalGetConnectedSystemControls(pItem->GetId(), controlInfos);

					if (!controlInfos.empty())
					{
						auto const pConnectionsMenu = new QMenu(pContextMenu);

						for (auto const& info : controlInfos)
						{
							pConnectionsMenu->addAction(info.icon, QtUtil::ToQString(info.name), [=]()
										{
											m_impl.SignalSelectConnectedSystemControl(info.id, pItem->GetId());
										});
						}

						pConnectionsMenu->setTitle(tr("Connections (" + ToString(controlInfos.size()) + ")"));
						pContextMenu->addMenu(pConnectionsMenu);
						pContextMenu->addSeparator();
					}
				}

				if (((pItem->GetPakStatus() & EPakStatus::OnDisk) != EPakStatus::None) && !pItem->GetFilePath().IsEmpty())
				{
					pContextMenu->addAction(tr("Show in File Explorer"), [=]()
								{
									QtUtil::OpenInExplorer((PathUtil::GetGameFolder() + "/" + pItem->GetFilePath()).c_str());
								});

					pContextMenu->addSeparator();
				}
			}
		}

		pContextMenu->addAction(tr("Expand Selection"), [=]() { m_pTreeView->ExpandSelection(); });
		pContextMenu->addAction(tr("Collapse Selection"), [=]() { m_pTreeView->CollapseSelection(); });
		pContextMenu->addSeparator();
	}

	pContextMenu->addAction(tr("Expand All"), [=]() { m_pTreeView->expandAll(); });
	pContextMenu->addAction(tr("Collapse All"), [=]() { m_pTreeView->collapseAll(); });

	pContextMenu->exec(QCursor::pos());
}

//////////////////////////////////////////////////////////////////////////
void CDataPanel::PlayEvent()
{
	StopEvent();

	CItem const* const pItem = CItemModel::GetItemFromIndex(m_pTreeView->currentIndex());

	if ((pItem != nullptr) && (pItem->GetType() == EItemType::Event))
	{
		cry_strcpy(g_previewTriggerInfo.name, pItem->GetPathName().c_str());

		gEnv->pAudioSystem->ExecutePreviewTriggerEx(g_previewTriggerInfo);
		g_isPreviewPlaying = true;
	}
}

//////////////////////////////////////////////////////////////////////////
void CDataPanel::StopEvent()
{
	if (g_isPreviewPlaying)
	{
		gEnv->pAudioSystem->StopPreviewTrigger();
		g_isPreviewPlaying = false;
	}
}

//////////////////////////////////////////////////////////////////////////
void CDataPanel::ClearFilters()
{
	m_pFilteringPanel->GetSearchBox()->clear();
	m_pFilteringPanel->Clear();
}

//////////////////////////////////////////////////////////////////////////
void CDataPanel::Reset()
{
	ClearFilters();
	m_pFilterProxyModel->invalidate();
}

//////////////////////////////////////////////////////////////////////////
void CDataPanel::OnBeforeReload()
{
	m_pTreeView->BackupExpanded();
	m_pTreeView->BackupSelection();
	m_pModel->Reset();
}

//////////////////////////////////////////////////////////////////////////
void CDataPanel::OnAfterReload()
{
	m_pModel->Reset();
	m_pTreeView->RestoreExpanded();
	m_pTreeView->RestoreSelection();
}

//////////////////////////////////////////////////////////////////////////
void CDataPanel::OnSelectConnectedItem(ControlId const id)
{
	ClearFilters();
	QModelIndexList const matches = m_pFilterProxyModel->match(m_pFilterProxyModel->index(0, 0, QModelIndex()), static_cast<int>(ModelUtils::ERoles::Id), id, 1, Qt::MatchRecursive);

	if (!matches.isEmpty())
	{
		m_pTreeView->setFocus();
		m_pTreeView->selectionModel()->setCurrentIndex(matches.first(), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
	}
}
} // namespace Fmod
} // namespace Impl
} // namespace ACE
