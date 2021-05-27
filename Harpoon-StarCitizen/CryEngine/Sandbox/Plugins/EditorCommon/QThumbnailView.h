// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

class CAbstractMenu;
class QButtonGroup;
class QListView;
class QPrecisionSlider;

#include "ProxyModels/ItemModelAttribute.h"
#include <EditorFramework/StateSerializable.h>
#include <CrySandbox/CrySignal.h>
#include "QAdvancedTreeView.h"
#include <QTimer>

//! Extends and specializes a QListView to behave as a thumbnail view (see windows explorer for inspiration)
//! All items have the same size, default is 64x64 pixels. Set ItemSize and Spacing, the rest should work out of the box
//! For convenience use the default Thumbnail attribute to create a specific Thumbnail column
//! Will use s_ThumbnailRole in priority, or Qt::DecorationRole as backup for the icon.
class EDITOR_COMMON_API QThumbnailsView : public QWidget, public IStateSerializable
{
	Q_OBJECT;

public:
	//! Item model with s_ThumbnailIconsRole may return a list of additional icons, where each element of the list should be QIcon or SSubIcon.
	//! SSubIcon allows to assign the position of the icon in thumbnail. All regular QIcon elements will have a TopRight position.
	// \sa QThumbnailsView::s_ThumbnailIconsRole
	// \sa QThumbnailsView::SetModel(QAbstractItemModel* model)
	struct SSubIcon
	{
		enum EPosition
		{
			TopRight,
			TopLeft,
			BottomLeft,
			BottomRight
		};

		QIcon     icon;
		EPosition position;
	};
public:
	//! Pass an internal view to be used as the list view or let it be set as a regular QListView();
	QThumbnailsView(QListView* pInternalView = nullptr, bool showSizeButtons = true, QWidget* parent = nullptr);
	~QThumbnailsView();

	bool         SetItemSize(const QSize& size);
	inline QSize GetItemSize() const { return m_itemSize; }

	void         SetItemSizeBounds(const QSize& min, const QSize& max);
	void         GetItemSizeBounds(QSize& minSizeOut, QSize& maxSizeOut) const { minSizeOut = m_minItemSize; maxSizeOut = m_maxItemSize; }

	void         SetSpacing(int space);
	int          GetSpacing() const;

	void         SetRootIndex(const QModelIndex& index);
	QModelIndex  GetRootIndex() const;

	//! column that will be used to query data to the model
	void SetDataColumn(int column);
	//! Returns false if attribute not found
	bool SetDataAttribute(const CItemModelAttribute* attribute = & Attributes::s_thumbnailAttribute, int attributeRole = Attributes::s_getAttributeRole);

	//! When set, the selection will be restored after model reset, based on persistent model indices and unique id of each index from the original model. Default is true.
	void SetRestoreSelectionAfterReset(bool bRestore) { m_restoreSelection = bRestore; }

	void SetModel(QAbstractItemModel* model);

	//! Helper method for scrolling as the ListView method has non-obvious constraints
	void ScrollToRow(const QModelIndex& indexInRow, QAbstractItemView::ScrollHint scrollHint = QAbstractItemView::EnsureVisible);

	//! Use to configure the internal view if necessary
	QAbstractItemView* GetInternalView();

	void               AppendPreviewSizeActions(CAbstractMenu& menu);

	CCrySignal<void()> signalShowContextMenu;

	//! This is the role used to return thumbnail status:
	//! Return false to signify the thumbnail is loading.
	//! Return QIcon for the actual thumbnail once it is loaded.
	static constexpr int s_ThumbnailRole = Qt::UserRole + 2562;
	//! Return QColor if model provides categorization of thumbnails by color.
	static constexpr int s_ThumbnailColorRole = s_ThumbnailRole + 1;
	//! Return QColor to view the thumbnail on top of the colored background.
	static constexpr int s_ThumbnailBackgroundColorRole = s_ThumbnailColorRole + 1;
	//! Return a list of icons that need to be drawn on top of the thumbnail.
	static constexpr int s_ThumbnailIconsRole = s_ThumbnailBackgroundColorRole + 1;

	virtual QVariantMap GetState() const override;
	virtual void        SetState(const QVariantMap& state) override;

private:

	class Delegate;

	bool eventFilter(QObject* pObject, QEvent* pEvent) override;
	void customEvent(QEvent* pEvent) override;

	void OnModelAboutToBeReset();
	void OnModelReset();
	void OnPreviewSizeButtonClicked(int value);
	void StartUpdateTimer();

	void AddSizingButton(int size, int iconNumber);

	Delegate*                                  m_pDelegate;
	QSize                                      m_minItemSize;
	QSize                                      m_maxItemSize;
	const QSize                                m_defaultSize;
	QSize                                      m_itemSize;
	QTimer                                     m_timer;
	QListView*                                 m_listView;
	QPrecisionSlider*                          m_slider;
	QButtonGroup*                              m_previewSizeButtons;
	const int                                  m_numOfPreviewSizes = 4;
	bool                                       m_timerStarted;
	bool                                       m_restoreSelection;
	std::vector<CAdvancedPersistentModelIndex> m_selectedBackup;
};

Q_DECLARE_METATYPE(QThumbnailsView::SSubIcon);