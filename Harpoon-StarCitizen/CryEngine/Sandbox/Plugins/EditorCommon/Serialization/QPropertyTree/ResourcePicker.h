// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include "IPropertyTreeWidget.h"

#include <IResourceSelectorHost.h>
#include <QWidget>

struct SStaticResourceSelectorEntry;

class QHBoxLayout;
class QLineEdit;
class CAssetDragDropLineEdit;

namespace Serialization
{
struct IResourceSelector;
}

class CResourcePicker : public QWidget, public IPropertyTreeWidget
{
	Q_OBJECT
	Q_INTERFACES(IPropertyTreeWidget)
public:
	CResourcePicker();

	virtual void SetValue(void* pValue, const yasli::TypeID& type, const yasli::Archive& ar) final;
	virtual void GetValue(void* pValue, const yasli::TypeID& type) const final;
	virtual void Serialize(Serialization::IArchive& ar) final;
	virtual bool SupportsMultiEdit() const final { return true; }
	virtual void SetMultiEditValue() final;

	void         OnValueContinuousChange(const char* newValue);

protected:
	bool event(QEvent* pEvent) override;

private:
	void Init(Serialization::IResourceSelector* pSelector, const yasli::Archive& ar);
	QWidget* AddButton(const char* szIconPath, const char* szToolTip, void (CResourcePicker::* pCallback)());

	void OnValueChanged(const char* newValue);
	void OnEdit();
	void OnPick();
	void OnPickLegacy();

private:
	SResourceSelectorContext            m_context;
	QString                             m_previousValue;
	const SStaticResourceSelectorEntry* m_pSelector;
	string                              m_type;
	QHBoxLayout*                        m_pLayout;
	CAssetDragDropLineEdit*             m_pLineEdit;
	QWidget*                            m_pLegacyPickerButton;
	QWidget*                            m_pEditButton;
};
