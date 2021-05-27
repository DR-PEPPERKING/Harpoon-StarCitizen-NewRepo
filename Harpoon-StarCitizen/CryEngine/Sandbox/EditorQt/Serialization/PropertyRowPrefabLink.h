// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Serialization/PropertyTreeLegacy/IDrawContext.h"
#include "Serialization/PropertyTreeLegacy/PropertyRowField.h"
#include "Serialization/QPropertyTreeLegacy/QPropertyTreeLegacy.h"
#include "Serialization/PropertyTreeLegacy/PropertyTreeModel.h"
#include "Serialization/PropertyTreeLegacy/Imenu.h"
#include "Serialization.h"

//TODO : this file is in editor package, breaking intended package dependencies
#include "Serialization/Decorators/EntityLink.h"

using Serialization::PrefabLink;

class CPrefabObject;

class PropertyRowPrefabLink : public PropertyRowField
{
public:
	~PropertyRowPrefabLink();

	void                        pick(PropertyTreeLegacy* tree);
	void                        select();
	void                        clear();

	bool                        isLeaf() const override   { return true; }
	bool                        isStatic() const override { return false; }

	void                        setValueAndContext(const Serialization::SStruct& ser, Serialization::IArchive& ar) override;
	bool                        assignTo(const Serialization::SStruct& ser) const override;
	bool                        onActivate(const PropertyActivationEvent& e) override;

	virtual bool                usePathEllipsis() const override { return true; }
	string                      valueAsString() const;
	void                        serializeValue(Serialization::IArchive& ar);
	const void*                 searchHandle() const override { return handle_; }
	Serialization::TypeID       typeId() const override       { return type_; }
	const CryGUID& GetGUID()                     { return guid_; }
	CPrefabObject* GetOwner()                    { return owner_; }

	bool           onContextMenu(IMenu& menu, PropertyTreeLegacy* tree) override;

private:
	struct Picker;

	CryGUID                 guid_;
	string                  name_;
	std::unique_ptr<Picker> picker_;
	CryGUID                 owner_guid_;
	CPrefabObject*          owner_;

	Serialization::TypeID   type_;
	const void*             handle_;
};

struct PrefabLinkMenuHandler : PropertyRowMenuHandler
{
public:
	PropertyTreeLegacy*          tree;
	PropertyRowPrefabLink* self;

	PrefabLinkMenuHandler(PropertyTreeLegacy* tree, PropertyRowPrefabLink* container);

	void onMenuPick();
	void onMenuSelect();
	void onMenuDelete();
	void onMenuClone();
	void onMenuExtract();
};
