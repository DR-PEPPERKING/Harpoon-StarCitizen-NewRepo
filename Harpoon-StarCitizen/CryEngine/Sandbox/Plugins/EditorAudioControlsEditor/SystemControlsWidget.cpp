// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SystemControlsWidget.h"

#include "AudioControlsEditorPlugin.h"
#include "SystemSourceModel.h"
#include "SystemLibraryModel.h"
#include "SystemFilterProxyModel.h"
#include "AssetsManager.h"
#include "ImplManager.h"
#include "PropertiesWidget.h"
#include "AssetIcons.h"
#include "AssetUtils.h"
#include "FileImporterUtils.h"
#include "TreeView.h"
#include "Common/IImpl.h"
#include "Common/ModelUtils.h"

#include <QtUtil.h>
#include <CrySystem/File/CryFile.h>
#include <Controls/QuestionDialog.h>
#include <QSearchBox.h>
#include <QFilteringPanel.h>
#include <ProxyModels/MountingProxyModel.h>

#include <QAction>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>

namespace ACE
{
constexpr int g_systemNameColumn = static_cast<int>(CSystemSourceModel::EColumns::Name);

//////////////////////////////////////////////////////////////////////////
void GetAssetsFromIndexesSeparated(
	QModelIndexList const& indexes,
	Libraries& outLibraries,
	Folders& outFolders,
	Controls& outControls)
{
	for (auto const& index : indexes)
	{
		QModelIndex const nameColumnIndex = index.sibling(index.row(), static_cast<int>(CSystemSourceModel::EColumns::Name));
		QVariant const internalPtr = nameColumnIndex.data(static_cast<int>(ModelUtils::ERoles::InternalPointer));

		if (internalPtr.isValid())
		{
			QVariant const type = nameColumnIndex.data(static_cast<int>(ModelUtils::ERoles::SortPriority));

			switch (type.toInt())
			{
			case static_cast<int>(EAssetType::Library):
				{
					outLibraries.push_back(reinterpret_cast<CLibrary*>(internalPtr.value<intptr_t>()));
					break;
				}
			case static_cast<int>(EAssetType::Folder):
				{
					outFolders.push_back(reinterpret_cast<CFolder*>(internalPtr.value<intptr_t>()));
					break;
				}
			default:
				{
					outControls.push_back(reinterpret_cast<CControl*>(internalPtr.value<intptr_t>()));
					break;
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void GetAssetsFromIndexesCombined(QModelIndexList const& indexes, Assets& outAssets)
{
	for (auto const& index : indexes)
	{
		QModelIndex const nameColumnIndex = index.sibling(index.row(), static_cast<int>(CSystemSourceModel::EColumns::Name));
		QVariant const internalPtr = nameColumnIndex.data(static_cast<int>(ModelUtils::ERoles::InternalPointer));

		if (internalPtr.isValid())
		{
			outAssets.push_back(reinterpret_cast<CAsset*>(internalPtr.value<intptr_t>()));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef ConstructTemporaryTrigger(CControl const* const pControl)
{
	XmlNodeRef const node = GetISystem()->CreateXmlNode(CryAudio::g_szTriggerTag);

	if (node.isValid())
	{
		node->setAttr(CryAudio::g_szNameAttribute, pControl->GetName());
		size_t const numConnections = pControl->GetConnectionCount();

		for (size_t i = 0; i < numConnections; ++i)
		{
			IConnection const* const pIConnection = pControl->GetConnectionAt(i);

			if (pIConnection != nullptr)
			{
				AssetUtils::TryConstructTriggerConnectionNode(node, pIConnection, pControl->GetContextId());
			}
		}
	}

	return node;
}

//////////////////////////////////////////////////////////////////////////
CSystemControlsWidget::CSystemControlsWidget(QWidget* const pParent)
	: CEditorWidget(pParent)
	, m_pSystemFilterProxyModel(new CSystemFilterProxyModel(this))
	, m_pSourceModel(new CSystemSourceModel(this))
	, m_pTreeView(new CTreeView(this))
	, m_isReloading(false)
	, m_isCreatedFromMenu(false)
	, m_suppressRenaming(false)
{
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	m_pMountingProxyModel = new CMountingProxyModel(WrapMemberFunction(this, &CSystemControlsWidget::CreateLibraryModelFromIndex), this);
	m_pMountingProxyModel->SetHeaderDataCallbacks(static_cast<int>(CSystemSourceModel::EColumns::Count), &CSystemSourceModel::GetHeaderData, Attributes::s_getAttributeRole);
	m_pMountingProxyModel->SetSourceModel(m_pSourceModel);
	m_pMountingProxyModel->SetDragCallback(&CSystemSourceModel::GetDragDropData);

	m_pSystemFilterProxyModel->setSourceModel(m_pMountingProxyModel);
	m_pSystemFilterProxyModel->setFilterKeyColumn(g_systemNameColumn);

	m_pTreeView->setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
	m_pTreeView->setDragEnabled(true);
	m_pTreeView->setDragDropMode(QAbstractItemView::DragDrop);
	m_pTreeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pTreeView->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_pTreeView->setTreePosition(g_systemNameColumn);
	m_pTreeView->setUniformRowHeights(true);
	m_pTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
	m_pTreeView->setModel(m_pSystemFilterProxyModel);
	m_pTreeView->sortByColumn(g_systemNameColumn, Qt::AscendingOrder);
	m_pTreeView->viewport()->installEventFilter(this);
	m_pTreeView->installEventFilter(this);
	m_pTreeView->header()->setMinimumSectionSize(25);
	m_pTreeView->header()->setSectionResizeMode(static_cast<int>(CSystemSourceModel::EColumns::Notification), QHeaderView::ResizeToContents);
	m_pTreeView->header()->setSectionResizeMode(static_cast<int>(CSystemSourceModel::EColumns::PakStatus), QHeaderView::ResizeToContents);
	m_pTreeView->SetNameColumn(g_systemNameColumn);
	m_pTreeView->TriggerRefreshHeaderColumns();

	m_pFilteringPanel = new QFilteringPanel("ACESystemControlsPanel", m_pSystemFilterProxyModel, this);
	m_pFilteringPanel->SetContent(m_pTreeView);
	m_pFilteringPanel->GetSearchBox()->SetAutoExpandOnSearch(m_pTreeView);

	auto const pMainLayout = new QVBoxLayout(this);
	pMainLayout->setContentsMargins(0, 0, 0, 0);
	InitAddControlWidget(pMainLayout);
	pMainLayout->addWidget(m_pFilteringPanel);

	if (g_pIImpl == nullptr)
	{
		setEnabled(false);
	}

	RegisterAction("general.delete", &CSystemControlsWidget::DeleteSelectedControls);

	QObject::connect(m_pMountingProxyModel, &CMountingProxyModel::rowsInserted, this, &CSystemControlsWidget::SelectNewAsset);
	QObject::connect(m_pTreeView, &CTreeView::customContextMenuRequested, this, &CSystemControlsWidget::OnContextMenu);
	QObject::connect(m_pTreeView->selectionModel(), &QItemSelectionModel::currentChanged, this, &CSystemControlsWidget::StopControlExecution);
	QObject::connect(m_pTreeView->selectionModel(), &QItemSelectionModel::selectionChanged, [&]()
		{
			if (g_pPropertiesWidget != nullptr)
			{
			  g_pPropertiesWidget->OnSetSelectedAssets(GetSelectedAssets(), !m_isReloading);
			}
		});

	g_assetsManager.SignalOnBeforeLibraryRemoved.Connect([this](CLibrary* const pLibrary)
		{
			size_t const libCount = g_assetsManager.GetLibraryCount();

			for (size_t i = 0; i < libCount; ++i)
			{
			  if (g_assetsManager.GetLibrary(i) == pLibrary)
			  {
			    m_libraryModels[i]->DisconnectSignals();
			    m_libraryModels[i]->deleteLater();
			    m_libraryModels.erase(m_libraryModels.begin() + i);
			    break;
				}
			}
		}, reinterpret_cast<uintptr_t>(this));

	g_assetsManager.SignalAssetRenamed.Connect([this](CAsset* const pAsset)
		{
			if (!g_assetsManager.IsLoading() && !m_suppressRenaming)
			{
			  OnRenameSelectedControls(pAsset->GetName());
			  m_pSystemFilterProxyModel->invalidate();
			  m_pTreeView->scrollTo(m_pTreeView->currentIndex());
			}
		}, reinterpret_cast<uintptr_t>(this));

	CAudioControlsEditorPlugin::SignalOnBeforeLoad.Connect([&]()
	    {
	                                                       StopControlExecution();
			}, reinterpret_cast<uintptr_t>(this));

	g_implManager.SignalOnBeforeImplChange.Connect([this]()
		{
			StopControlExecution();
			ClearFilters();
		}, reinterpret_cast<uintptr_t>(this));

	g_implManager.SignalOnAfterImplChange.Connect([this]()
		{
			setEnabled(g_pIImpl != nullptr);
		}, reinterpret_cast<uintptr_t>(this));
}

//////////////////////////////////////////////////////////////////////////
CSystemControlsWidget::~CSystemControlsWidget()
{
	g_assetsManager.SignalOnBeforeLibraryRemoved.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_assetsManager.SignalAssetRenamed.DisconnectById(reinterpret_cast<uintptr_t>(this));
	CAudioControlsEditorPlugin::SignalOnBeforeLoad.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_implManager.SignalOnBeforeImplChange.DisconnectById(reinterpret_cast<uintptr_t>(this));
	g_implManager.SignalOnAfterImplChange.DisconnectById(reinterpret_cast<uintptr_t>(this));

	StopControlExecution();
	DeleteModels();
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::InitAddControlWidget(QVBoxLayout* const pLayout)
{
	QPushButton* const pAddButton = new QPushButton(tr("Add"), this);
	pAddButton->setToolTip(tr("Add new library, folder or control"));

	auto const pAddButtonMenu = new QMenu(pAddButton);
	QObject::connect(pAddButtonMenu, &QMenu::aboutToShow, this, &CSystemControlsWidget::OnUpdateCreateButtons);

	pAddButtonMenu->addAction(GetAssetIcon(EAssetType::Library), tr("Library"), [&]()
		{
			m_isCreatedFromMenu = true;
			g_assetsManager.CreateLibrary(AssetUtils::GenerateUniqueLibraryName("new_library", nullptr));
		});

	pAddButtonMenu->addSeparator();

	m_pCreateParentFolderAction = new QAction(GetAssetIcon(EAssetType::Folder), tr("Parent Folder"), pAddButtonMenu);
	QObject::connect(m_pCreateParentFolderAction, &QAction::triggered, [&]() { CreateParentFolder(); });
	m_pCreateParentFolderAction->setVisible(false);
	pAddButtonMenu->addAction(m_pCreateParentFolderAction);

	m_pCreateFolderAction = new QAction(GetAssetIcon(EAssetType::Folder), tr("Folder"), pAddButtonMenu);
	QObject::connect(m_pCreateFolderAction, &QAction::triggered, [&]() { CreateFolder(GetSelectedAsset()); });
	m_pCreateFolderAction->setVisible(false);
	pAddButtonMenu->addAction(m_pCreateFolderAction);

	m_pCreateTriggerAction = new QAction(GetAssetIcon(EAssetType::Trigger), tr("Trigger"), pAddButtonMenu);
	QObject::connect(m_pCreateTriggerAction, &QAction::triggered, [&]() { CreateControl("new_trigger", EAssetType::Trigger, GetSelectedAsset()); });
	m_pCreateTriggerAction->setVisible(false);
	pAddButtonMenu->addAction(m_pCreateTriggerAction);

	m_pCreateParameterAction = new QAction(GetAssetIcon(EAssetType::Parameter), tr("Parameter"), pAddButtonMenu);
	QObject::connect(m_pCreateParameterAction, &QAction::triggered, [&]() { CreateControl("new_parameter", EAssetType::Parameter, GetSelectedAsset()); });
	m_pCreateParameterAction->setVisible(false);
	pAddButtonMenu->addAction(m_pCreateParameterAction);

	m_pCreateSwitchAction = new QAction(GetAssetIcon(EAssetType::Switch), tr("Switch"), pAddButtonMenu);
	QObject::connect(m_pCreateSwitchAction, &QAction::triggered, [&]() { CreateControl("new_switch", EAssetType::Switch, GetSelectedAsset()); });
	m_pCreateSwitchAction->setVisible(false);
	pAddButtonMenu->addAction(m_pCreateSwitchAction);

	m_pCreateStateAction = new QAction(GetAssetIcon(EAssetType::State), tr("State"), pAddButtonMenu);
	QObject::connect(m_pCreateStateAction, &QAction::triggered, [&]() { CreateControl("new_state", EAssetType::State, GetSelectedAsset()); });
	m_pCreateStateAction->setVisible(false);
	pAddButtonMenu->addAction(m_pCreateStateAction);

	m_pCreateEnvironmentAction = new QAction(GetAssetIcon(EAssetType::Environment), tr("Environment"), pAddButtonMenu);
	QObject::connect(m_pCreateEnvironmentAction, &QAction::triggered, [&]() { CreateControl("new_environment", EAssetType::Environment, GetSelectedAsset()); });
	m_pCreateEnvironmentAction->setVisible(false);
	pAddButtonMenu->addAction(m_pCreateEnvironmentAction);

	m_pCreatePreloadAction = new QAction(GetAssetIcon(EAssetType::Preload), tr("Preload"), pAddButtonMenu);
	QObject::connect(m_pCreatePreloadAction, &QAction::triggered, [&]() { CreateControl("new_preload", EAssetType::Preload, GetSelectedAsset()); });
	m_pCreatePreloadAction->setVisible(false);
	pAddButtonMenu->addAction(m_pCreatePreloadAction);

	m_pCreateSettingAction = new QAction(GetAssetIcon(EAssetType::Setting), tr("Setting"), pAddButtonMenu);
	QObject::connect(m_pCreateSettingAction, &QAction::triggered, [&]() { CreateControl("new_setting", EAssetType::Setting, GetSelectedAsset()); });
	m_pCreateSettingAction->setVisible(false);
	pAddButtonMenu->addAction(m_pCreateSettingAction);

	pAddButton->setMenu(pAddButtonMenu);
	pLayout->addWidget(pAddButton);
}

//////////////////////////////////////////////////////////////////////////
bool CSystemControlsWidget::eventFilter(QObject* pObject, QEvent* pEvent)
{
	switch (pEvent->type())
	{
	case QEvent::KeyRelease:
		{
			if (!m_pTreeView->IsEditing())
			{
				QKeyEvent const* const pKeyEvent = static_cast<QKeyEvent*>(pEvent);

				if (pKeyEvent != nullptr)
				{
					if (pKeyEvent->key() == Qt::Key_Space)
					{
						ExecuteControl();
					}
				}
			}

			break;
		}
	case QEvent::MouseButtonDblClick:
		{
			if (!m_pTreeView->IsEditing())
			{
				ExecuteControl();
			}

			break;
		}
	case QEvent::Drop:
		{
			m_pTreeView->selectionModel()->clearSelection();
			m_pTreeView->selectionModel()->clearCurrentIndex();

			break;
		}
	default:
		{
			break;
		}
	}

	return QWidget::eventFilter(pObject, pEvent);
}

//////////////////////////////////////////////////////////////////////////
Assets CSystemControlsWidget::GetSelectedAssets() const
{
	QModelIndexList const indexes = m_pTreeView->selectionModel()->selectedRows(g_systemNameColumn);
	Assets assets;
	GetAssetsFromIndexesCombined(indexes, assets);
	return assets;
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::Reset()
{
	ClearFilters();
	m_pSystemFilterProxyModel->invalidate();
}

//////////////////////////////////////////////////////////////////////////
CControl* CSystemControlsWidget::CreateControl(string const& name, EAssetType const type, CAsset* const pParent)
{
	CControl* pControl = nullptr;
	m_isCreatedFromMenu = true;

	if (type != EAssetType::State)
	{
		pControl = g_assetsManager.CreateControl(AssetUtils::GenerateUniqueControlName(name, type, nullptr), type, pParent);
	}
	else
	{
		pControl = g_assetsManager.CreateControl(AssetUtils::GenerateUniqueName(name, type, nullptr, pParent), type, pParent);
	}

	return pControl;
}

//////////////////////////////////////////////////////////////////////////
CAsset* CSystemControlsWidget::CreateFolder(CAsset* const pParent)
{
	m_isCreatedFromMenu = true;
	return g_assetsManager.CreateFolder(AssetUtils::GenerateUniqueName("new_folder", EAssetType::Folder, nullptr, pParent), pParent);
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::CreateParentFolder()
{
	QModelIndexList const selection = m_pTreeView->selectionModel()->selectedRows(g_systemNameColumn);
	size_t const numAssets = selection.size();

	Libraries libraries;
	libraries.reserve(numAssets);
	Folders folders;
	folders.reserve(numAssets);
	Controls controls;
	controls.reserve(numAssets);

	GetAssetsFromIndexesSeparated(selection, libraries, folders, controls);

	Assets assetsToMove;
	assetsToMove.reserve(folders.size() + controls.size());

	for (auto const pFolder : folders)
	{
		assetsToMove.push_back(static_cast<CAsset*>(pFolder));
	}

	for (auto const pControl : controls)
	{
		assetsToMove.push_back(static_cast<CAsset*>(pControl));
	}

	auto const pParent = assetsToMove[0]->GetParent();
	auto const pNewFolder = CreateFolder(pParent);
	g_assetsManager.MoveAssets(pNewFolder, assetsToMove);
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::OnContextMenu(QPoint const& pos)
{
	auto const pContextMenu = new QMenu(this);
	auto const pAddMenu = new QMenu(tr("Add"), pContextMenu);
	QModelIndexList const selection = m_pTreeView->selectionModel()->selectedRows(g_systemNameColumn);

	CAsset* pImportAsset = nullptr;

	pContextMenu->addMenu(pAddMenu);
	pContextMenu->addSeparator();

	pAddMenu->addAction(GetAssetIcon(EAssetType::Library), tr("Library"), [&]()
		{
			m_isCreatedFromMenu = true;
			g_assetsManager.CreateLibrary(AssetUtils::GenerateUniqueLibraryName("new_library", nullptr));
		});

	pAddMenu->addSeparator();

	if (!selection.isEmpty())
	{
		size_t const numAssets = selection.size();

		Libraries libraries;
		libraries.reserve(numAssets);
		Folders folders;
		folders.reserve(numAssets);
		Controls controls;
		controls.reserve(numAssets);

		GetAssetsFromIndexesSeparated(selection, libraries, folders, controls);

		bool const isDefaultControlSelected = IsDefaultControlSelected();

		if (!isDefaultControlSelected)
		{
			if (IsParentFolderAllowed())
			{
				pAddMenu->addAction(GetAssetIcon(EAssetType::Folder), tr("Parent Folder"), [&]() { CreateParentFolder(); });
			}

			if (selection.size() == 1)
			{
				QModelIndex const index = m_pTreeView->currentIndex();

				if (index.isValid())
				{
					CAsset* const pAsset = CSystemSourceModel::GetAssetFromIndex(index, g_systemNameColumn);

					if (pAsset != nullptr)
					{
						EAssetType const assetType = pAsset->GetType();

						if ((assetType == EAssetType::Library) || (assetType == EAssetType::Folder))
						{
							CAsset* pParent = nullptr;

							if (assetType == EAssetType::Folder)
							{
								pParent = static_cast<CAsset*>(folders[0]);
							}
							else
							{
								pParent = static_cast<CAsset*>(libraries[0]);
							}

							pAddMenu->addAction(GetAssetIcon(EAssetType::Folder), tr("Folder"), [=]() { CreateFolder(pParent); });

							if ((g_implInfo.flags & EImplInfoFlags::SupportsTriggers) != EImplInfoFlags::None)
							{
								pAddMenu->addAction(GetAssetIcon(EAssetType::Trigger), tr("Trigger"), [=]() { CreateControl("new_trigger", EAssetType::Trigger, pParent); });
							}

							if ((g_implInfo.flags & EImplInfoFlags::SupportsParameters) != EImplInfoFlags::None)
							{
								pAddMenu->addAction(GetAssetIcon(EAssetType::Parameter), tr("Parameter"), [=]() { CreateControl("new_parameter", EAssetType::Parameter, pParent); });
							}

							if ((g_implInfo.flags & EImplInfoFlags::SupportsSwitches) != EImplInfoFlags::None)
							{
								pAddMenu->addAction(GetAssetIcon(EAssetType::Switch), tr("Switch"), [=]() { CreateControl("new_switch", EAssetType::Switch, pParent); });
							}

							if ((g_implInfo.flags & EImplInfoFlags::SupportsEnvironments) != EImplInfoFlags::None)
							{
								pAddMenu->addAction(GetAssetIcon(EAssetType::Environment), tr("Environment"), [=]() { CreateControl("new_environment", EAssetType::Environment, pParent); });
							}

							if ((g_implInfo.flags & EImplInfoFlags::SupportsPreloads) != EImplInfoFlags::None)
							{
								pAddMenu->addAction(GetAssetIcon(EAssetType::Preload), tr("Preload"), [=]() { CreateControl("new_preload", EAssetType::Preload, pParent); });
							}

							if ((g_implInfo.flags & EImplInfoFlags::SupportsSettings) != EImplInfoFlags::None)
							{
								pAddMenu->addAction(GetAssetIcon(EAssetType::Setting), tr("Setting"), [=]() { CreateControl("new_setting", EAssetType::Setting, pParent); });
							}

							if (pParent->GetType() == EAssetType::Library)
							{
								if ((libraries[0]->GetPakStatus() & EPakStatus::OnDisk) != EPakStatus::None)
								{
									pContextMenu->addAction(tr("Show in File Explorer"), [=]()
										{
											QtUtil::OpenInExplorer((PathUtil::GetGameFolder() + "/" + g_assetsManager.GetConfigFolderPath() + pParent->GetName() + ".xml").c_str());
										});

									pContextMenu->addSeparator();
								}
							}

							if ((g_implInfo.flags & EImplInfoFlags::SupportsFileImport) != EImplInfoFlags::None)
							{
								pImportAsset = pAsset;
							}
						}
					}
				}

				if (controls.size() == 1)
				{
					CControl* const pControl = controls[0];
					EAssetType const controlType = pControl->GetType();

					if (controlType == EAssetType::Trigger)
					{
						QAction* const pTriggerAction = new QAction(tr("Execute Trigger"), pContextMenu);
						QObject::connect(pTriggerAction, &QAction::triggered, [&]() { ExecuteControl(); });
						pContextMenu->insertSeparator(pContextMenu->actions().at(0));
						pContextMenu->insertAction(pContextMenu->actions().at(0), pTriggerAction);
					}
					else if (controlType == EAssetType::Switch)
					{
						if ((g_implInfo.flags & EImplInfoFlags::SupportsStates) != EImplInfoFlags::None)
						{
							pAddMenu->addAction(GetAssetIcon(EAssetType::State), tr("State"), [&]() { CreateControl("new_state", EAssetType::State, pControl); });
						}
					}
					else if (controlType == EAssetType::Preload)
					{
						if (!pControl->IsAutoLoad() &&
						    ((pControl->GetContextId() == CryAudio::GlobalContextId) ||
						     (std::find(g_activeUserDefinedContexts.begin(), g_activeUserDefinedContexts.end(), pControl->GetContextId()) != g_activeUserDefinedContexts.end())))
						{
							QAction* const pLoadAction = new QAction(tr("Load Preload Request"), pContextMenu);
							QAction* const pUnloadAction = new QAction(tr("Unload Preload Request"), pContextMenu);
							QObject::connect(pLoadAction, &QAction::triggered, [=]() { gEnv->pAudioSystem->PreloadSingleRequest(CryAudio::StringToId(pControl->GetName()), false); });
							QObject::connect(pUnloadAction, &QAction::triggered, [=]() { gEnv->pAudioSystem->UnloadSingleRequest(CryAudio::StringToId(pControl->GetName())); });
							pContextMenu->insertSeparator(pContextMenu->actions().at(0));
							pContextMenu->insertAction(pContextMenu->actions().at(0), pUnloadAction);
							pContextMenu->insertAction(pContextMenu->actions().at(0), pLoadAction);
						}
					}
					else if (controlType == EAssetType::Setting)
					{
						QAction* const pLoadAction = new QAction(tr("Load Setting"), pContextMenu);
						QAction* const pUnloadAction = new QAction(tr("Unload Setting"), pContextMenu);
						QObject::connect(pLoadAction, &QAction::triggered, [=]() { gEnv->pAudioSystem->LoadSetting(CryAudio::StringToId(pControl->GetName())); });
						QObject::connect(pUnloadAction, &QAction::triggered, [=]() { gEnv->pAudioSystem->UnloadSetting(CryAudio::StringToId(pControl->GetName())); });
						pContextMenu->insertSeparator(pContextMenu->actions().at(0));
						pContextMenu->insertAction(pContextMenu->actions().at(0), pUnloadAction);
						pContextMenu->insertAction(pContextMenu->actions().at(0), pLoadAction);
					}
				}
			}
			else if (controls.size() > 1)
			{
				bool hasOnlyPreloadsInActiveContext = false;

				for (auto const pControl : controls)
				{
					if ((pControl->GetType() == EAssetType::Preload) &&
					    !pControl->IsAutoLoad() &&
					    ((pControl->GetContextId() == CryAudio::GlobalContextId) ||
					     (std::find(g_activeUserDefinedContexts.begin(), g_activeUserDefinedContexts.end(), pControl->GetContextId()) != g_activeUserDefinedContexts.end())))
					{
						hasOnlyPreloadsInActiveContext = true;
					}
					else
					{
						hasOnlyPreloadsInActiveContext = false;
						break;
					}
				}

				if (hasOnlyPreloadsInActiveContext)
				{
					QAction* const pLoadAction = new QAction(tr("Load reload Requests"), pContextMenu);
					QAction* const pUnloadAction = new QAction(tr("Unload Preload Requests"), pContextMenu);
					QObject::connect(pLoadAction, &QAction::triggered, [=]()
						{
							for (auto const pControl : controls)
							{
							  gEnv->pAudioSystem->PreloadSingleRequest(CryAudio::StringToId(pControl->GetName()), false);
							}
						});

					QObject::connect(pUnloadAction, &QAction::triggered, [=]()
						{
							for (auto const pControl : controls)
							{
							  gEnv->pAudioSystem->UnloadSingleRequest(CryAudio::StringToId(pControl->GetName()));
							}
						});

					pContextMenu->insertSeparator(pContextMenu->actions().at(0));
					pContextMenu->insertAction(pContextMenu->actions().at(0), pUnloadAction);
					pContextMenu->insertAction(pContextMenu->actions().at(0), pLoadAction);
				}
			}
		}
		else if ((libraries.size() == 1) && controls.empty() && folders.empty())
		{
			if ((libraries[0]->GetPakStatus() & EPakStatus::OnDisk) != EPakStatus::None)
			{
				CAsset const* const pLibrary = static_cast<CAsset*>(libraries[0]);

				pContextMenu->addAction(tr("Show in File Explorer"), [=]()
					{
						QtUtil::OpenInExplorer((PathUtil::GetGameFolder() + "/" + g_assetsManager.GetConfigFolderPath() + pLibrary->GetName() + ".xml").c_str());
					});

				pContextMenu->addSeparator();
			}
		}

		pContextMenu->addAction(tr("Delete"), [&]() { DeleteSelectedControls(); });

		if (!isDefaultControlSelected)
		{
			pContextMenu->addAction(tr("Rename"), [=]()
				{
					QModelIndex const nameColumnIndex = m_pTreeView->currentIndex().sibling(m_pTreeView->currentIndex().row(), g_systemNameColumn);
					m_pTreeView->edit(nameColumnIndex);
				});

			if (!controls.empty() && libraries.empty() && folders.empty())
			{
				pContextMenu->addAction(tr("Duplicate"), [&]() { DuplicateSelectedControls(); });
			}
		}

		pContextMenu->addSeparator();
		pContextMenu->addAction(tr("Expand Selection"), [=]() { m_pTreeView->ExpandSelection(); });
		pContextMenu->addAction(tr("Collapse Selection"), [=]() { m_pTreeView->CollapseSelection(); });
		pContextMenu->addSeparator();
	}

	pContextMenu->addAction(tr("Expand All"), [=]() { m_pTreeView->expandAll(); });
	pContextMenu->addAction(tr("Collapse All"), [=]() { m_pTreeView->collapseAll(); });

	if (pImportAsset != nullptr)
	{
		pContextMenu->addSeparator();
		pContextMenu->addAction(tr("Import Files"), [=]()
			{
				OpenFileSelector(EImportTargetType::SystemControls, pImportAsset);
			});
	}

	pContextMenu->exec(QCursor::pos());
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::OnRenameSelectedControls(string const& name)
{
	m_suppressRenaming = true;
	QModelIndexList const selection = m_pTreeView->selectionModel()->selectedRows(g_systemNameColumn);

	if (selection.size() > 1)
	{
		for (auto const& index : selection)
		{
			CAsset* const pAsset = CSystemSourceModel::GetAssetFromIndex(index, g_systemNameColumn);

			if (pAsset != nullptr)
			{
				pAsset->SetName(name);
			}
		}
	}

	m_suppressRenaming = false;
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::DuplicateSelectedControls()
{
	QModelIndexList const selection = m_pTreeView->selectionModel()->selectedRows(g_systemNameColumn);
	Assets selectedAssets;

	for (auto const& index : selection)
	{
		if (index.isValid())
		{
			selectedAssets.push_back(CSystemSourceModel::GetAssetFromIndex(index, g_systemNameColumn));
		}
	}

	m_isCreatedFromMenu = true;
	g_assetsManager.DuplicateAssets(selectedAssets);
	m_isCreatedFromMenu = false;
}

//////////////////////////////////////////////////////////////////////////
bool CSystemControlsWidget::DeleteSelectedControls()
{
	QModelIndexList const selection = m_pTreeView->selectionModel()->selectedRows(g_systemNameColumn);
	int const numSelected = selection.length();

	if (numSelected > 0)
	{
		QString text;

		if (numSelected == 1)
		{
			text = tr(R"(Are you sure you want to delete ")") + selection[0].data(Qt::DisplayRole).toString() + R"("?.)";
		}
		else
		{
			text = tr("Are you sure you want to delete the selected controls and folders?");
		}

		auto const messageBox = new CQuestionDialog();
		messageBox->SetupQuestion(g_szEditorName, text);

		if (messageBox->Execute() == QDialogButtonBox::Yes)
		{
			Assets selectedItems;

			for (auto const& index : selection)
			{
				if (index.isValid())
				{
					selectedItems.push_back(CSystemSourceModel::GetAssetFromIndex(index, g_systemNameColumn));
				}
			}

			Assets assetsToDelete;
			AssetUtils::SelectTopLevelAncestors(selectedItems, assetsToDelete);
			AssetNames defaultControlNames;
			AssetNames pakLibraryNames;

			for (auto const pAsset : assetsToDelete)
			{
				if (!pAsset->HasDefaultControlChildren(defaultControlNames))
				{
					if (pAsset->GetType() == EAssetType::Library)
					{
						auto const pLibrary = static_cast<CLibrary* const>(pAsset);

						if ((pLibrary->GetPakStatus() & EPakStatus::OnDisk) != EPakStatus::None)
						{
							g_assetsManager.DeleteAsset(pAsset);
						}
						else if ((pLibrary->GetPakStatus() & EPakStatus::InPak) == EPakStatus::None)
						{
							// Newly created, unsaved library.
							g_assetsManager.DeleteAsset(pAsset);
						}
						else
						{
							pakLibraryNames.emplace_back(pAsset->GetName());
						}
					}
					else
					{
						g_assetsManager.DeleteAsset(pAsset);
					}
				}
			}

			QString notDeletedAssetsText = "";

			if (!pakLibraryNames.empty())
			{
				if (pakLibraryNames.size() > 1)
				{
					notDeletedAssetsText = tr("Libraries in pak file could not get deleted:");
				}
				else
				{
					notDeletedAssetsText = tr("Library in pak file could not get deleted:");
				}

				for (auto const& name : pakLibraryNames)
				{
					notDeletedAssetsText.append("\n\"" + QtUtil::ToQString(name) + "\"");
				}
			}

			if (!defaultControlNames.empty())
			{
				if (!notDeletedAssetsText.isEmpty())
				{
					notDeletedAssetsText.append("\n\n");
				}

				if (defaultControlNames.size() > 1)
				{
					notDeletedAssetsText.append("Default controls could not get deleted:");
				}
				else
				{
					notDeletedAssetsText.append("Default control could not get deleted:");
				}

				for (auto const& name : defaultControlNames)
				{
					notDeletedAssetsText.append("\n\"" + QtUtil::ToQString(name) + "\"");
				}
			}

			if (!notDeletedAssetsText.isEmpty())
			{
				auto const defaultControlsMessageBox = new CQuestionDialog();
				defaultControlsMessageBox->SetupQuestion(g_szEditorName, notDeletedAssetsText, QDialogButtonBox::Ok, QDialogButtonBox::Ok);
				defaultControlsMessageBox->Execute();
			}
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::ExecuteControl()
{
	CAsset const* const pAsset = CSystemSourceModel::GetAssetFromIndex(m_pTreeView->currentIndex(), g_systemNameColumn);

	if ((pAsset != nullptr) && (pAsset->GetType() == EAssetType::Trigger))
	{
		if ((pAsset->GetFlags() & EAssetFlags::IsModified) == EAssetFlags::None)
		{
			CAudioControlsEditorPlugin::ExecuteTrigger(pAsset->GetName());
		}
		else
		{
			auto const pControl = static_cast<CControl const*>(pAsset);
			CAudioControlsEditorPlugin::ExecuteTriggerEx(pControl->GetName(), ConstructTemporaryTrigger(pControl));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::StopControlExecution()
{
	CAudioControlsEditorPlugin::StopTriggerExecution();
}

//////////////////////////////////////////////////////////////////////////
QAbstractItemModel* CSystemControlsWidget::CreateLibraryModelFromIndex(QModelIndex const& sourceIndex)
{
	QAbstractItemModel* model = nullptr;

	if (sourceIndex.model() == m_pSourceModel)
	{
		size_t const numLibraries = m_libraryModels.size();
		auto const row = static_cast<size_t>(sourceIndex.row());

		if (row >= numLibraries)
		{
			m_libraryModels.resize(row + 1);

			for (size_t i = numLibraries; i < row + 1; ++i)
			{
				m_libraryModels[i] = new CSystemLibraryModel(g_assetsManager.GetLibrary(i), this);
			}
		}

		model = m_libraryModels[row];
	}

	return model;
}

//////////////////////////////////////////////////////////////////////////
CAsset* CSystemControlsWidget::GetSelectedAsset() const
{
	CAsset* pAsset = nullptr;
	QModelIndex const index = m_pTreeView->currentIndex();

	if (index.isValid())
	{
		pAsset = CSystemSourceModel::GetAssetFromIndex(index, g_systemNameColumn);
	}

	return pAsset;
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::SelectNewAsset(QModelIndex const& parent, int const row)
{
	if (!g_assetsManager.IsLoading())
	{
		ClearFilters();
		QModelIndex const assetIndex = m_pSystemFilterProxyModel->mapFromSource(m_pMountingProxyModel->index(row, g_systemNameColumn, parent));

		if (m_isCreatedFromMenu)
		{
			m_pTreeView->setCurrentIndex(assetIndex);
			m_pTreeView->edit(assetIndex);
			m_isCreatedFromMenu = false;
		}
		else
		{
			QModelIndex const parentIndex = m_pSystemFilterProxyModel->mapFromSource(parent);
			m_pTreeView->expand(parentIndex);

			if (CSystemSourceModel::GetNumDroppedItems() == 1)
			{
				m_pTreeView->setCurrentIndex(assetIndex);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::OnUpdateCreateButtons()
{
	bool isActionVisible = false;

	if (IsDefaultControlSelected())
	{
		m_pCreateParentFolderAction->setVisible(false);
	}
	else
	{
		m_pCreateParentFolderAction->setVisible(IsParentFolderAllowed());
		QModelIndexList const selection = m_pTreeView->selectionModel()->selectedRows(g_systemNameColumn);

		if (selection.size() == 1)
		{
			QModelIndex const index = m_pTreeView->currentIndex();

			if (index.isValid())
			{
				CAsset const* const pAsset = CSystemSourceModel::GetAssetFromIndex(index, g_systemNameColumn);

				if (pAsset != nullptr)
				{
					EAssetType const assetType = pAsset->GetType();
					bool const isLibraryOrFolder = (assetType == EAssetType::Library) || (assetType == EAssetType::Folder);

					m_pCreateFolderAction->setVisible(isLibraryOrFolder);
					m_pCreateTriggerAction->setVisible(isLibraryOrFolder && ((g_implInfo.flags & EImplInfoFlags::SupportsTriggers) != EImplInfoFlags::None));
					m_pCreateParameterAction->setVisible(isLibraryOrFolder && ((g_implInfo.flags & EImplInfoFlags::SupportsParameters) != EImplInfoFlags::None));
					m_pCreateSwitchAction->setVisible(isLibraryOrFolder && ((g_implInfo.flags & EImplInfoFlags::SupportsSwitches) != EImplInfoFlags::None));
					m_pCreateStateAction->setVisible((assetType == EAssetType::Switch) && ((g_implInfo.flags & EImplInfoFlags::SupportsStates) != EImplInfoFlags::None));
					m_pCreateEnvironmentAction->setVisible(isLibraryOrFolder && ((g_implInfo.flags & EImplInfoFlags::SupportsEnvironments) != EImplInfoFlags::None));
					m_pCreatePreloadAction->setVisible(isLibraryOrFolder && ((g_implInfo.flags & EImplInfoFlags::SupportsPreloads) != EImplInfoFlags::None));
					m_pCreateSettingAction->setVisible(isLibraryOrFolder && ((g_implInfo.flags & EImplInfoFlags::SupportsSettings) != EImplInfoFlags::None));

					isActionVisible = true;
				}
			}
		}
	}

	if (!isActionVisible)
	{
		m_pCreateFolderAction->setVisible(false);
		m_pCreateTriggerAction->setVisible(false);
		m_pCreateParameterAction->setVisible(false);
		m_pCreateSwitchAction->setVisible(false);
		m_pCreateStateAction->setVisible(false);
		m_pCreateEnvironmentAction->setVisible(false);
		m_pCreatePreloadAction->setVisible(false);
		m_pCreateSettingAction->setVisible(false);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CSystemControlsWidget::IsParentFolderAllowed() const
{
	bool isAllowed = false;
	QModelIndexList const selection = m_pTreeView->selectionModel()->selectedRows(g_systemNameColumn);

	if (!selection.isEmpty())
	{
		size_t const numAssets = selection.size();

		Libraries libraries;
		libraries.reserve(numAssets);
		Folders folders;
		folders.reserve(numAssets);
		Controls controls;
		controls.reserve(numAssets);

		GetAssetsFromIndexesSeparated(selection, libraries, folders, controls);

		if (libraries.empty() && (!folders.empty() || !controls.empty()))
		{
			isAllowed = true;
			CAsset const* pParent;

			if (!folders.empty())
			{
				pParent = folders[0]->GetParent();

				for (auto const pFolder : folders)
				{
					if (pFolder->GetParent() != pParent)
					{
						isAllowed = false;
						break;
					}
				}
			}
			else
			{
				pParent = controls[0]->GetParent();
			}

			if (isAllowed)
			{
				for (auto const pControl : controls)
				{
					if ((pControl->GetParent() != pParent) || (pControl->GetType() == EAssetType::State))
					{
						isAllowed = false;
						break;
					}
				}
			}
		}
	}

	return isAllowed;
}

//////////////////////////////////////////////////////////////////////////
bool CSystemControlsWidget::IsDefaultControlSelected() const
{
	bool isDefaultControlSelected = false;
	QModelIndexList const selection = m_pTreeView->selectionModel()->selectedRows(g_systemNameColumn);

	for (auto const& index : selection)
	{
		if (index.isValid())
		{
			CAsset const* const pAsset = CSystemSourceModel::GetAssetFromIndex(index, g_systemNameColumn);

			if ((pAsset != nullptr) && ((pAsset->GetFlags() & EAssetFlags::IsDefaultControl) != EAssetFlags::None))
			{
				isDefaultControlSelected = true;
				break;
			}
		}
	}

	return isDefaultControlSelected;
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::OnBeforeReload()
{
	m_pTreeView->BackupExpanded();
	m_pTreeView->BackupSelection();
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::OnAfterReload()
{
	m_pTreeView->RestoreExpanded();
	m_pTreeView->RestoreSelection();
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::OnFileImporterOpened()
{
	m_pTreeView->setDragDropMode(QAbstractItemView::NoDragDrop);
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::OnFileImporterClosed()
{
	m_pTreeView->setDragDropMode(QAbstractItemView::DragDrop);
}

//////////////////////////////////////////////////////////////////////////
bool CSystemControlsWidget::IsEditing() const
{
	return m_pTreeView->IsEditing();
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::SelectConnectedSystemControl(ControlId const systemControlId, ControlId const implItemId)
{
	ClearFilters();
	QModelIndexList const matches = m_pSystemFilterProxyModel->match(m_pSystemFilterProxyModel->index(0, 0, QModelIndex()), static_cast<int>(ModelUtils::ERoles::Id), systemControlId, 1, Qt::MatchRecursive);

	if (!matches.isEmpty())
	{
		ControlIds selectedConnection {
			implItemId };

		CControl* const pControl = g_assetsManager.FindControlById(systemControlId);

		if (pControl != nullptr)
		{
			pControl->SetSelectedConnections(selectedConnection);
		}

		m_pTreeView->setFocus();
		m_pTreeView->selectionModel()->setCurrentIndex(matches.first(), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
	}
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::ClearFilters()
{
	m_pFilteringPanel->GetSearchBox()->clear();
	m_pFilteringPanel->Clear();
}

//////////////////////////////////////////////////////////////////////////
void CSystemControlsWidget::DeleteModels()
{
	m_pSourceModel->DisconnectSignals();
	m_pSourceModel->deleteLater();

	for (auto const pModel : m_libraryModels)
	{
		pModel->DisconnectSignals();
		pModel->deleteLater();
	}

	m_libraryModels.clear();
}
} // namespace ACE
