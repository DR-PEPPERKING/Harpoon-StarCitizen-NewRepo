// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include <CryCore/Platform/platform.h>
#include <CrySerialization/yasli/StringList.h>

#include <Dialogs/TreeViewDialog.h>

#include <QEvent>
#include <QObject>
#include <QMenu>
#include <QStandardItemModel>

#include <Serialization/PropertyTreeLegacy/PropertyTreeLegacy.h>
#include <Serialization/PropertyTreeLegacy/PropertyRowImpl.h>
#include <Serialization/PropertyTreeLegacy/IDrawContext.h>
#include <Serialization/PropertyTreeLegacy/PropertyTreeModel.h>
#include <Serialization/QPropertyTreeLegacy/QPropertyTreeLegacy.h>

class QAction;

#define SEARCHABLE_LIST_THRESHOLD 20

namespace Private_MenuCombobox
{

class QPropertyRowComboboxMenu : public QMenu
{
public:
	QPropertyRowComboboxMenu(QWidget* parent)
		: QMenu(parent)
		, m_mouseDownReceived(false)
	{
	}

protected:
	virtual bool event(QEvent* pEvent) override
	{
		switch (pEvent->type())
		{
		case QEvent::MouseButtonPress:
			m_mouseDownReceived = true;
			break;
		case QEvent::MouseButtonRelease:
			if (m_mouseDownReceived)
			{
				return QMenu::event(pEvent);
			}
			else
			{
				return false;
			}
		}

		return QMenu::event(pEvent);
	}

	virtual void leaveEvent(QEvent* e) override
	{
		// fix for Qt 5.6. bug, every mouse move calls leaveEvent, that sets the activeAction to nullptr
		// and on mouseRelease event no action is chosen, so before calling the original implementation
		// store the activeAction and sets it back
		QAction* pAction = activeAction();
		QMenu::leaveEvent(e);
		setActiveAction(pAction);
	}

	bool                m_mouseDownReceived;
};

class QComboBoxHandler : public QObject
{
public:
	QComboBoxHandler(QMenu* pMenu, PropertyTreeLegacy* tree, PropertyRow* comboboxRow, yasli::string& value)
		: QObject(pMenu), tree_(tree), comboboxRow_(comboboxRow), value_(value)
	{
	}

	void actionTriggered(QAction* pAction);

private:
	PropertyRow*        comboboxRow_;
	yasli::string&      value_;
	PropertyTreeLegacy* tree_;
};

int widgetSizeMinHelper(const PropertyTreeLegacy* tree, const PropertyRow* row, RowWidthCache& widthCache)
{
	if (row->userWidgetSize() >= 0)
	{
		return row->userWidgetSize();
	}

	const int width =
		row->userWidgetToContent()
		? widthCache.getOrUpdate(tree, row, tree->_defaultRowHeight())
		: 80;

	return width;
}

bool showPopup(QPropertyTreeLegacy* pTree, Rect rect, PropertyRow* pPropertyRow, const yasli::StringList& stringList, yasli::string& value)
{
	if (stringList.size() > SEARCHABLE_LIST_THRESHOLD)
	{
		CTreeViewDialog dialog(pTree);
		QStandardItemModel model;
		QList<QStandardItem*> rows;

		model.setColumnCount(1);
		model.setHeaderData(0, Qt::Horizontal, pPropertyRow->label());
		rows.reserve(stringList.size());
		int selectedIndex = -1;

		for (const string& item : stringList)
		{
			if (item == value)
				selectedIndex = model.rowCount();

			model.appendRow(new QStandardItem(item.c_str()));
		}

		dialog.Initialize(&model, 0);

		if (-1 != selectedIndex)
			dialog.SetSelectedValue(model.index(selectedIndex, 0), false);

		if (dialog.exec())
		{
			QModelIndex index = dialog.GetSelected();
			if (index.isValid())
			{
				yasli::string newValue(index.data().value<QString>().toUtf8().constData());
				if (value != newValue)
				{
					pTree->model()->rowAboutToBeChanged(pPropertyRow);
					value = newValue;
					pTree->model()->rowChanged(pPropertyRow);
				}
			}
		}
	}
	else
	{
		QPoint menuPoint = pTree->_toScreen(rect.topLeft());
		QPoint menuEndPoint = pTree->_toScreen(Point(rect.right(), rect.top()));
		QPropertyRowComboboxMenu* pMenu = new QPropertyRowComboboxMenu(pTree);
		pMenu->setMinimumWidth(menuEndPoint.x() - menuPoint.x());

		QAction* pSelectedAction = nullptr;
		for (const string& item : stringList)
		{
			QAction* pAction = pMenu->addAction(item.c_str());
			if (item == value)
				pSelectedAction = pAction;
		}

		QComboBoxHandler* pComboboxHandler = new QComboBoxHandler(pMenu, pTree, pPropertyRow, value);
		QWidget::connect(pMenu, &QMenu::triggered, pComboboxHandler, &QComboBoxHandler::actionTriggered);
		pMenu->setActiveAction(pSelectedAction);
		pMenu->popup(menuPoint, pSelectedAction);
	}

	return true;
}

class PropertyRowStaticMenuCombobox : public PropertyRowImpl<yasli::StringListStaticValue>
{
public:
	PropertyRowStaticMenuCombobox() : value_(nullptr) {}

	bool onActivate(const PropertyActivationEvent& e)
	{
		if (e.reason == e.REASON_RELEASE)
			return false;
		if (userReadOnly())
			return false;

		property_tree::Rect rect = widgetRect(e.tree);

		if (!rect.contains(e.clickPoint))
			return false;

		QPropertyTreeLegacy* pTree = (QPropertyTreeLegacy*)e.tree;
		return showPopup(pTree, rect, this, stringList_, value_);
	}

	yasli::string valueAsString() const override { return value_.c_str(); }

	bool          assignTo(const Serializer& ser) const override
	{
		*((yasli::StringListStaticValue*)ser.pointer()) = value_.c_str();
		return true;
	}

	void setValueAndContext(const Serializer& ser, yasli::Archive& ar) override
	{
		YASLI_ESCAPE(ser.size() == sizeof(yasli::StringListStaticValue), return );
		const yasli::StringListStaticValue& stringListValue = *((yasli::StringListStaticValue*)(ser.pointer()));
		stringList_.resize(stringListValue.stringList().size());
		for (size_t i = 0; i < stringList_.size(); ++i)
			stringList_[i] = stringListValue.stringList()[i];
		value_ = stringListValue.c_str();
		handle_ = stringListValue.handle();
		type_ = stringListValue.type();
	}

	bool            isLeaf() const override                                           { return true; }
	bool            isStatic() const override                                         { return false; }
	int             widgetSizeMin(const PropertyTreeLegacy* tree) const override      { return widgetSizeMinHelper(tree, this, widthCache_); }
	WidgetPlacement widgetPlacement() const override                                  { return WIDGET_VALUE; }
	const void*     searchHandle() const override                                     { return handle_; }
	yasli::TypeID   typeId() const override                                           { return type_; }
	// Override behavior inherited from PropertyRowField, we always want clicks to trigger the drop-down
	bool            onMouseDown(PropertyTreeLegacy* tree, Point point, bool& changed) { return false; }

	void            redraw(IDrawContext& context) override
	{
		if (multiValue())
			context.drawEntry(context.widgetRect, " ... ", false, true, 0);
		else
			context.drawComboBox(context.widgetRect, valueAsString().c_str(), userReadOnly());
	}

	void serializeValue(yasli::Archive& ar) override
	{
		ar(value_, "value", "Value");
	}

private:
	yasli::StringList     stringList_;
	yasli::string         value_;
	const void*           handle_;
	yasli::TypeID         type_;
	mutable RowWidthCache widthCache_;
};

class PropertyRowMenuCombobox : public PropertyRow
{
public:
	PropertyRowMenuCombobox() : value_(nullptr) {}

	bool onActivate(const PropertyActivationEvent& e)
	{
		if (e.reason == e.REASON_RELEASE)
			return false;
		if (userReadOnly())
			return false;

		property_tree::Rect rect = widgetRect(e.tree);

		if (!rect.contains(e.clickPoint))
			return false;

		QPropertyTreeLegacy* pTree = (QPropertyTreeLegacy*)e.tree;
		return showPopup(pTree, rect, this, stringList_, value_);
	}

	yasli::string valueAsString() const override { return value_.c_str(); }

	bool          assignTo(const Serializer& ser) const override
	{
		*((yasli::StringListValue*)ser.pointer()) = value_.c_str();
		return true;
	}
	void setValueAndContext(const yasli::Serializer& ser, yasli::Archive& ar) override
	{
		YASLI_ESCAPE(ser.size() == sizeof(yasli::StringListValue), return );
		const yasli::StringListValue& stringListValue = *((yasli::StringListValue*)(ser.pointer()));
		stringList_ = stringListValue.stringList();
		value_ = stringListValue.c_str();
		handle_ = stringListValue.handle();
		type_ = stringListValue.type();
	}

	bool            isLeaf() const override                                      { return true; }
	bool            isStatic() const override                                    { return false; }
	int             widgetSizeMin(const PropertyTreeLegacy* tree) const override { return widgetSizeMinHelper(tree, this, widthCache_); }
	WidgetPlacement widgetPlacement() const override                             { return WIDGET_VALUE; }
	const void*     searchHandle() const override                                { return handle_; }
	yasli::TypeID   typeId() const override                                      { return type_; }

	void            redraw(IDrawContext& context) override
	{
		if (multiValue())
			context.drawEntry(context.widgetRect, " ... ", false, true, 0);
		else
			context.drawComboBox(context.widgetRect, valueAsString().c_str(), userReadOnly());
	}

	void serializeValue(yasli::Archive& ar) override
	{
		ar(value_, "value", "Value");
	}

private:
	yasli::StringList     stringList_;
	yasli::string         value_;
	const void*           handle_;
	yasli::TypeID         type_;
	mutable RowWidthCache widthCache_;
};

void QComboBoxHandler::actionTriggered(QAction* pAction)
{
	if (!pAction || !tree_ || !comboboxRow_)
		return;

	yasli::string newValue(pAction->text().toUtf8().constData());
	if (value_ != newValue)
	{
		tree_->model()->rowAboutToBeChanged(comboboxRow_);
		value_ = newValue;
		tree_->model()->rowChanged(comboboxRow_);
	}
}

REGISTER_PROPERTY_ROW(yasli::StringListStaticValue, PropertyRowStaticMenuCombobox)
REGISTER_PROPERTY_ROW(yasli::StringListValue, PropertyRowMenuCombobox)
}