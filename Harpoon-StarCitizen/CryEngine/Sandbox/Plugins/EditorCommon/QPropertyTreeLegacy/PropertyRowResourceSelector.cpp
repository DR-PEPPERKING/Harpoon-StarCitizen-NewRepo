// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "PropertyRowResourceSelector.h"

#include "DragDrop.h"
#include "FileDialogs/EngineFileDialog.h"
#include "PathUtils.h"
#include "Serialization/Decorators/INavigationProvider.h"
#include "Serialization/PropertyTreeLegacy/IMenu.h"
#include "Serialization/PropertyTreeLegacy/PropertyTreeModel.h"
#include "Serialization/QPropertyTreeLegacy/QPropertyTreeLegacy.h"

#include <IEditor.h>
#include <IUndoManager.h>

#include <CrySandbox/ScopedVariableSetter.h>

#include <QLineEdit>

using Serialization::IResourceSelector;

enum Button
{
	BUTTON_PICK,
	BUTTON_EDIT,
	BUTTON_PICK_LEGACY
};

class InplaceWidgetResource : public QObject, public property_tree::InplaceWidget
{
public:
	InplaceWidgetResource(PropertyRowResourceSelector* row, QPropertyTreeLegacy* tree, int width)
		: InplaceWidget(tree)
		, entry_(new QLineEdit(tree))
		, tree_(tree)
		, row_(row)
	{
		entry_->setMaximumWidth(width);
		initialValue_ = row->value();
		entry_->setText(initialValue_);
		entry_->selectAll();
		QObject::connect(entry_.data(), &QLineEdit::editingFinished, this, &InplaceWidgetResource::onEditingFinished);
		connect(entry_.data(), &QLineEdit::textChanged, this, [this, tree]
		{
			QFontMetrics fm(entry_->font());
			int contentWidth = min((int)fm.width(entry_->text()) + 8, tree->width() - entry_->x());
			if (contentWidth > entry_->width())
				entry_->resize(contentWidth, entry_->height());
		});
	}

	~InplaceWidgetResource()
	{
		entry_->hide();
		entry_->setParent(0);
		entry_.take()->deleteLater();
	}

	void  commit()       { onEditingFinished(); }
	void* actualWidget() { return entry_.data(); }

protected:
	void onEditingFinished()
	{
		if (initialValue_ != entry_->text() || row_->multiValue())
		{
			tree()->model()->rowAboutToBeChanged(row_);
			QString text = entry_->text();
			row_->setValue(tree_, text.toUtf8().constData(), row_->searchHandle(), row_->typeId());
			tree()->model()->rowChanged(row_);
		}
		else
			tree_->_cancelWidget();
	}

	QPropertyTreeLegacy*               tree_;
	QScopedPointer<QLineEdit>    entry_;
	PropertyRowResourceSelector* row_;
	QString                      initialValue_;
};

ResourceSelectorMenuHandler::ResourceSelectorMenuHandler(PropertyTreeLegacy* tree, PropertyRowResourceSelector* self)
	: self(self), tree(tree)
{
}

void ResourceSelectorMenuHandler::onMenuClear()
{
	tree->model()->rowAboutToBeChanged(self);
	self->clear();
	tree->model()->rowChanged(self);
}

void ResourceSelectorMenuHandler::onMenuPickResource()
{
	self->pickResource(tree);
}

void ResourceSelectorMenuHandler::onMenuCreateFile()
{
	self->createFile(tree);
}

void ResourceSelectorMenuHandler::onMenuJumpTo()
{
	self->jumpTo(tree);
}

PropertyRowResourceSelector::PropertyRowResourceSelector()
	: provider_(0), id_(0), searchHandle_(0), bActive_(false), selector_(0)
{
}

property_tree::InplaceWidget* PropertyRowResourceSelector::createWidget(PropertyTreeLegacy* tree)
{
	int buttonsWidth = buttonCount() * 16;
	int widgetWidth = max(16, widgetRect(tree).width() - buttonsWidth - 2);

	context_.typeName = type_.c_str();
	QPropertyTreeLegacy* qtree = static_cast<QPropertyTreeLegacy*>(tree);
	context_.parentWidget = qtree;
	if (selector_ && selector_->UsesInputField())
		return new InplaceWidgetResource(this, (QPropertyTreeLegacy*)tree, widgetWidth);
	else
		return nullptr;
}

void PropertyRowResourceSelector::setValue(PropertyTreeLegacy* tree, const char* str, const void* handle, const yasli::TypeID& type)
{
	CRY_ASSERT(selector_);

	context_.typeName = type_.c_str();
	QPropertyTreeLegacy* pPropertyTree = static_cast<QPropertyTreeLegacy*>(tree);
	context_.parentWidget = pPropertyTree;
	SResourceValidationResult validatedPath = selector_->ValidateValue(context_, str, value_.c_str());
	value_ = validatedPath.validatedResource.c_str();
	serializer_.setPointer((void*)handle);
	serializer_.setType(type);
}

bool PropertyRowResourceSelector::onMouseDown(PropertyTreeLegacy* tree, Point point, bool& changed)
{
	if (!fieldRect(tree).contains(point))
		return true;

	CRY_ASSERT(selector_);

	context_.typeName = type_.c_str();
	QPropertyTreeLegacy* qtree = static_cast<QPropertyTreeLegacy*>(tree);
	context_.parentWidget = qtree;
	if (!selector_->UsesInputField())
		return true;

	return __super::onMouseDown(tree, point, changed);
}

bool PropertyRowResourceSelector::onDataDragMove(PropertyTreeLegacy* tree)
{
	QPropertyTreeLegacy* qtree = static_cast<QPropertyTreeLegacy*>(tree);
	const QMimeData* const pMimeData = qtree->getMimeData();
	auto ddData = CDragDropData::FromMimeData(pMimeData);
	return ddData->HasFilePaths();
}

bool PropertyRowResourceSelector::onDataDrop(PropertyTreeLegacy* tree)
{
	QPropertyTreeLegacy* qtree = static_cast<QPropertyTreeLegacy*>(tree);
	const QMimeData* const pMimeData = qtree->getMimeData();
	auto ddData = CDragDropData::FromMimeData(pMimeData);

	const QStringList filePaths = ddData->GetFilePaths();
	if (filePaths.isEmpty())
	{
		return false;
	}

	const QString filePath = PathUtil::ToGamePath(filePaths.first());

	// Strip project and game folder path.

	if (value() != filePath || multiValue())
	{
		tree->model()->rowAboutToBeChanged(this);
		setValue(tree, filePath.toUtf8().constData(), searchHandle(), typeId());
		tree->model()->rowChanged(this);
		return true;
	}
	else
	{
		tree->_cancelWidget();
		return true;
	}
}

bool PropertyRowResourceSelector::onShowToolTip()
{
	if (value_.IsEmpty())
		return false;

	CRY_ASSERT(selector_);
	return selector_->ShowTooltip(context_, value_);
}

void PropertyRowResourceSelector::onHideToolTip()
{
	CRY_ASSERT(selector_);
	return selector_->HideTooltip(context_, value_);
}

bool PropertyRowResourceSelector::onActivate(const PropertyActivationEvent& e)
{
	if (PropertyRowField::onActivate(e))
		return true;

	if (onActivateButton(hitButton(e.tree, e.clickPoint), e))
		return true;

	switch (e.reason)
	{
	case PropertyActivationEvent::REASON_DOUBLECLICK:
		{
			const bool canPickResource = !userReadOnly() && provider_ && provider_->CanPickFile(type_.c_str(), id_);
			if (canPickResource)
			{
				pickResource(e.tree);
				return true;
			}
		}
		break;
	case PropertyActivationEvent::REASON_RELEASE:
		{
			const bool canSelectResource = !userReadOnly() && !multiValue() && provider_ && provider_->CanSelect(type_.c_str(), value_.c_str(), id_);
			if (canSelectResource)
			{
				jumpTo(e.tree);
				return true;
			}
		}
		break;
	default:
		break;
	}

	return false;
}

int PropertyRowResourceSelector::buttonCount() const
{
	if (!provider_)
	{
		CRY_ASSERT(selector_);

		int btns = 1;

		if (selector_->CanEdit())
			++btns;

		if (selector_->HasLegacyPicker())
			++btns;

		return btns;
	}

	int result = 0;
	if (provider_->CanPickFile(type_.c_str(), id_))
	{
		result = 1;
		if (!multiValue() && value_.empty() && provider_->CanCreate(type_.c_str(), id_))
			result = 2;
	}
	return result;
}

bool PropertyRowResourceSelector::onActivateButton(int button, const PropertyActivationEvent& e)
{
	if (userReadOnly())
		return false;

	switch (button)
	{
	case BUTTON_PICK:
		return pickResource(e.tree);
	case BUTTON_EDIT:
		return editResource(e.tree);
	case BUTTON_PICK_LEGACY:
		{
			context_.useLegacyPicker = true;
			const bool res = pickResource(e.tree);
			context_.useLegacyPicker = false;
			return res;
		}
	default:
		return false;
	}
}

bool PropertyRowResourceSelector::getHoverInfo(PropertyHoverInfo* hover, const Point& cursorPos, const PropertyTreeLegacy* tree) const
{
	if (fieldRect(tree).contains(cursorPos) && provider_ && provider_->CanSelect(type_.c_str(), value_.c_str(), id_) && !provider_->IsSelected(type_.c_str(), value_.c_str(), id_))
		hover->cursor = property_tree::CURSOR_HAND;
	else
		hover->cursor = property_tree::CURSOR_ARROW;
	hover->toolTip = value_;
	return true;
}

void PropertyRowResourceSelector::jumpTo(PropertyTreeLegacy* tree)
{
	if (multiValue())
		return;
	if (provider_)
		provider_->Select(type_.c_str(), value_.c_str(), id_);
}

namespace Private_ResourceSelectorHost
{

class ResourceSelectionCallback : public IResourceSelectionCallback
{
public:
	virtual void SetValue(const char* newValue) override
	{
		if (m_onValueChanged)
		{
			m_onValueChanged(newValue);
		}
	}

	void SetValueChangedCallback(const std::function<void(const char*)>& onValueChanged)
	{
		m_onValueChanged = onValueChanged;
	}

private:
	std::function<void(const char*)> m_onValueChanged;
};

} // namespace Private_ResourceSelectorHost

bool PropertyRowResourceSelector::pickResource(PropertyTreeLegacy* tree)
{
	using namespace Private_ResourceSelectorHost;
	CRY_ASSERT(selector_);

	// With a mouse double click we may encounter an attempt to re-entrance the same instance.
	if (bActive_)
	{
		return false;
	}
	CScopedVariableSetter<bool> state(bActive_, true);

	//Called every time a new resource is selected in the asset selector
	auto setValue = [this, tree](const char* filename)
									{
										value_ = filename;
										//Will cause property tree to apply and revert this row (aka changes will be applied) but will not submit and end undo action
										tree->model()->rowContinuouslyChanged(this);
									};

	ResourceSelectionCallback callback;
	callback.SetValueChangedCallback(setValue);
	context_.callback = &callback;

	context_.typeName = type_.c_str();
	QPropertyTreeLegacy* pPropertyTree = static_cast<QPropertyTreeLegacy*>(tree);
	context_.parentWidget = pPropertyTree;

	const string previousValue = value_;
	//Notify model (and subsequently the tree) that this row is about to change, this usually triggers an undo begin and record on the items being changed
	tree->model()->rowAboutToBeChanged(this);
	SResourceSelectionResult result = selector_->SelectResource(context_, previousValue.c_str());

	if (result.selectionAccepted && previousValue != result.selectedResource.c_str())
	{
		setValue(result.selectedResource.c_str());
		//!Confirm that the row changed and accept the undo (aka place it in the undo history)
		tree->model()->rowChanged(this, true, true);
	}
	else
	{
		//!Rollback this row change and revert the undo (aka call the Undo() function on the undo object)
		tree->model()->rowChanged(this, true, false);
	}

	return true;
}

bool PropertyRowResourceSelector::createFile(PropertyTreeLegacy* tree)
{
	if (!provider_)
		return false;

	QString title;
	if (labelUndecorated())
		title = QString("Create file for '") + labelUndecorated() + "'";
	else
		title = "Choose file";

	string originalFilter;
	originalFilter = provider_->GetFileSelectorMaskForType(type_.c_str());

	CEngineFileDialog::RunParams runParams;
	runParams.title = title;
	runParams.initialFile = QString::fromLocal8Bit((defaultPath_.empty() ? value_ : defaultPath_).c_str());
	runParams.extensionFilters = CExtensionFilter::Parse(originalFilter.c_str());
	QString result = CEngineFileDialog::RunGameSave(runParams, static_cast<QPropertyTreeLegacy*>(tree));
	if (!result.isEmpty())
	{
		if (provider_->Create(type_.c_str(), result.toLocal8Bit().data(), id_))
		{
			tree->model()->rowAboutToBeChanged(this);
			value_ = result.toLocal8Bit().data();
			tree->model()->rowChanged(this);
		}
	}
	return true;
}

bool PropertyRowResourceSelector::editResource(PropertyTreeLegacy* tree)
{
	CRY_ASSERT(selector_);
	selector_->EditResource(context_, value_.c_str());
	return true;
}

void PropertyRowResourceSelector::setValueAndContext(const Serialization::SStruct& ser, IArchive& ar)
{
	CRY_ASSERT(GetIEditor() && GetIEditor()->GetResourceSelectorHost());

	IResourceSelector* value = (IResourceSelector*)ser.pointer();
	if (selector_ == nullptr || type_ != value->resourceType)
	{
		type_ = value->resourceType;
		selector_ = GetIEditor()->GetResourceSelectorHost()->GetSelector(type_.c_str());
		CRY_ASSERT(selector_ != nullptr); //A default selector implementation should always be returned, even if no selector is registered
		const char* resourceIconPath = selector_->GetIconPath();
		icon_ = resourceIconPath[0] ? Icon(resourceIconPath) : Icon();
		context_.typeName = type_.c_str();
		context_.resourceSelectorEntry = selector_;
	}
	value_ = value->GetValue();
	id_ = value->GetId();
	searchHandle_ = value->GetHandle();
	wrappedType_ = value->GetType();

	provider_ = ar.context<Serialization::INavigationProvider>();
	if (!provider_ || !provider_->IsRegistered(type_))
	{
		provider_ = 0;
	}

	Serialization::TypeID contextObjectType = selector_->GetContextType();
	if (contextObjectType != Serialization::TypeID())
	{
		context_.contextObject = ar.contextByType(contextObjectType);
		context_.contextObjectType = contextObjectType;
	}

	context_.pCustomParams = value->pCustomParams;

	if (Serialization::SNavigationContext* navigationContext = ar.context<Serialization::SNavigationContext>())
		defaultPath_ = navigationContext->path.c_str();
	else
		defaultPath_.clear();
}

bool PropertyRowResourceSelector::assignTo(const Serialization::SStruct& ser) const
{
	((IResourceSelector*)ser.pointer())->SetValue(value_.c_str());
	return true;
}

void PropertyRowResourceSelector::serializeValue(Serialization::IArchive& ar)
{
	ar(type_, "type");
	ar(value_, "value");
	ar(id_, "index");

	if (ar.isInput() && selector_)
	{
		const char* resourceIconPath = selector_->GetIconPath();
		icon_ = resourceIconPath[0] ? Icon(resourceIconPath) : Icon();
	}
}

Icon PropertyRowResourceSelector::buttonIcon(const PropertyTreeLegacy* tree, int index) const
{
	switch (index)
	{
	case BUTTON_PICK:
		{
			if (provider_ != 0 || icon_.isNull())
			{
				return Icon("icons:General/Folder.ico");
			}
			else
				return icon_;
		}
	case BUTTON_PICK_LEGACY:
		{
			if (provider_ != 0 || icon_.isNull())
			{
				return Icon("icons:General/LegacyResourceSelectorPicker.ico");
			}
			else
				return icon_;
		}
	case BUTTON_EDIT:
		{
			return Icon("icons:General/Edit_Asset.ico");
		}
	default:
		{
			return Icon();
		}
	}
}

string PropertyRowResourceSelector::valueAsString() const
{
	return value_;
}

void PropertyRowResourceSelector::clear()
{
	value_.clear();
}

bool PropertyRowResourceSelector::onContextMenu(IMenu& menu, PropertyTreeLegacy* tree)
{
	using namespace property_tree;
	yasli::SharedPtr<PropertyRow> selfPointer(this);

	ResourceSelectorMenuHandler* handler = new ResourceSelectorMenuHandler(tree, this);
	if (!multiValue() && provider_ && provider_->CanSelect(type_.c_str(), value_.c_str(), id_))
	{
		menu.addAction("Jump to", MENU_DEFAULT, handler, &ResourceSelectorMenuHandler::onMenuJumpTo);
	}
	if (!userReadOnly())
	{
		if (!provider_ || provider_->CanPickFile(type_.c_str(), id_))
			menu.addAction(buttonIcon(tree, 0), "Pick Resource...", "", userReadOnly() ? MENU_DISABLED : 0, handler, &ResourceSelectorMenuHandler::onMenuPickResource);
		if (provider_ && provider_->CanCreate(type_.c_str(), id_))
			menu.addAction(buttonIcon(tree, 1), "Create...", "", 0, handler, &ResourceSelectorMenuHandler::onMenuCreateFile);
		menu.addAction("Clear", userReadOnly() ? MENU_DISABLED : 0, handler, &ResourceSelectorMenuHandler::onMenuClear);
	}
	tree->addMenuHandler(handler);

	PropertyRow::onContextMenu(menu, tree);
	return true;
}

static const char* getFilenameFromPath(const char* path)
{
	const char* lastSep = strrchr(path, '/');
	if (!lastSep)
		return path;
	return lastSep + 1;
}

void PropertyRowResourceSelector::redraw(IDrawContext& context)
{
	int buttonCount = this->buttonCount();
	int offset = 0;
	for (int i = 0; i < buttonCount; ++i)
	{
		Icon icon = buttonIcon(context.tree, i);
		int width = 16;
		offset += width;
		Rect iconRect(context.widgetRect.right() - offset, context.widgetRect.top(), width, context.widgetRect.height());
		context.drawIcon(iconRect, icon, userReadOnly() ? ICON_DISABLED : ICON_NORMAL);
	}

	int iconSpace = offset ? offset + 2 : 0;

	Rect rect = context.widgetRect;
	rect.w -= iconSpace;
	bool pressed = context.pressed || (provider_ ? provider_->IsSelected(type_.c_str(), value_.c_str(), id_) : false);
	bool active = !provider_ || provider_->IsActive(type_.c_str(), value_.c_str(), id_);
	bool modified = provider_ && provider_->IsModified(type_.c_str(), value_.c_str(), id_);
	Icon icon = icon_;
	if (provider_)
		icon = Icon(provider_->GetIcon(type_.c_str(), value_.c_str()));
	bool canSelect = !multiValue() && provider_ && provider_->CanSelect(type_.c_str(), value_.c_str(), id_);

	string text = multiValue() ? "..." : string(modified ? "*" : "") + getFilenameFromPath(valueAsString());
	if (provider_)
	{
		int flags = 0;
		if (userReadOnly())
			flags |= BUTTON_DISABLED;
		if (!canSelect)
			flags |= BUTTON_NO_FRAME;
		if (pressed)
			flags |= BUTTON_PRESSED;
		if (canSelect || !text.empty())
			context.drawButtonWithIcon(icon, rect, text.c_str(), flags, active ? FONT_BOLD : FONT_NORMAL);
	}
	else
		context.drawEntry(context.widgetRect, text.c_str(), true, false, iconSpace);
}

REGISTER_PROPERTY_ROW(IResourceSelector, PropertyRowResourceSelector);
DECLARE_SEGMENT(PropertyRowResourceSelector)
