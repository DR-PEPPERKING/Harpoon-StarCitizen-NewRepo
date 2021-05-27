// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "SandboxAPI.h"
#include "EntityScript.h"
#include <Objects/IObjectLayer.h>
#include <CryCore/smartptr.h>

//////////////////////////////////////////////////////////////////////////
/*!
   Object Layer.
   All objects are organized in hierarchical layers.
   Every layer can be made hidden/frozen or can be exported or imported back.
 */
//////////////////////////////////////////////////////////////////////////
enum EObjectLayerType
{
	eObjectLayerType_Layer,
	eObjectLayerType_Folder,
	eObjectLayerType_Terrain,
	eObjectLayerType_Size
};

class SANDBOX_API CObjectLayer : public IObjectLayer, public _i_reference_target_t
{
public:
	static CObjectLayer* Create(const char* szName, EObjectLayerType type);

	static void          ForEachLayerOf(const std::vector<CBaseObject*>& objects, std::function<void(CObjectLayer*, const std::vector<CBaseObject*>&)> func);

	//! Set layer name.
	void          SetName(const string& name, bool IsUpdateDepends = false);
	//! Get layer name.
	const string& GetName() const { return m_name; }

	//! Get layer name including its parent names.
	string GetFullName() const;

	//! Get GUID assigned to this layer.
	const CryGUID& GetGUID() const { return m_guid; }

	//////////////////////////////////////////////////////////////////////////
	// Query layer status.
	//////////////////////////////////////////////////////////////////////////
	virtual bool IsVisible(bool isRecursive = true) const override;
	virtual bool IsFrozen(bool isRecursive = true) const override;
	bool         IsExportable() const         { return m_exportable; }
	bool         IsExporLayerPak() const      { return m_exportLayerPak; }
	bool         IsDefaultLoaded() const      { return m_defaultLoaded; }
	bool         HasPhysics() const           { return m_havePhysics; }
	int          GetSpecs() const             { return m_specs; }
	ColorB       GetColor() const;
	bool         IsUsingColorOverride() const { return m_useColorOverride; }
	void         UseColorOverride(bool useColorOverride);

	//////////////////////////////////////////////////////////////////////////
	// Set layer status.
	//////////////////////////////////////////////////////////////////////////
	virtual void SetVisible(bool isVisible, bool isRecursive = false) override;
	virtual void SetFrozen(bool isFrozen, bool isRecursive = false) override;
	void         SetExportable(bool isExportable)         { m_exportable = isExportable; }
	void         SetExportLayerPak(bool isExportLayerPak) { m_exportLayerPak = isExportLayerPak; }
	void         SetDefaultLoaded(bool isDefaultLoaded)   { m_defaultLoaded = isDefaultLoaded; }
	void         SetHavePhysics(bool isHavePhysics)       { m_havePhysics = isHavePhysics; }
	void         SetSpecs(int specs)                      { m_specs = specs; }
	void         SetColor(ColorB color, bool useColorOverride);

	//////////////////////////////////////////////////////////////////////////
	//! Save/Load layer to/from xml node.
	void         SerializeBase(XmlNodeRef& node, bool isLoading);
	virtual void Serialize(XmlNodeRef& node, bool isLoading);

	//! Get number of objects.
	uint GetObjectCount() const;

	//////////////////////////////////////////////////////////////////////////
	// Child layers.
	//////////////////////////////////////////////////////////////////////////
	void          AddChild(CObjectLayer* pLayer, bool isNotify = true);
	void          RemoveChild(CObjectLayer* pLayer, bool isNotify = true);
	void          SetAsRootLayer();
	int           GetChildCount() const override                 { return m_childLayers.size(); }
	CObjectLayer* GetChild(int index) const;
	CObjectLayer* GetParent() const                              { return m_parent; }
	IObjectLayer* GetParentIObjectLayer() const                  { return m_parent; }
	IObjectLayer* GetChildIObjectLayer(int index) const override { return GetChild(index); }
	void          GetDescendants(std::vector<CObjectLayer*>& result) const;
	void          GetAncestors(std::vector<CObjectLayer*>& result) const;

	//! Check if specified layer is direct or indirect parent of this layer.
	bool IsChildOf(const CObjectLayer* pParent) const;
	//////////////////////////////////////////////////////////////////////////

	virtual bool IsFolder() const override { return m_layerType == eObjectLayerType_Folder; }

	virtual void SetModified(bool isModified = true) override;
	bool         IsModified() const { return m_isModified; }

	//////////////////////////////////////////////////////////////////////////
	// User interface support.
	//////////////////////////////////////////////////////////////////////////
	void Expand(bool isExpand);
	bool IsExpanded() const { return m_expanded; }

	// Setup Layer ID for LayerSwith
	void   SetLayerID(uint16 nLayerId) { m_nLayerId = nLayerId; }
	uint16 GetLayerID() const          { return m_nLayerId; }

	//! Returns the filepath of this layer. The path may not exist if the level has not been saved yet.
	string                             GetLayerFilepath() const override;

	EObjectLayerType                   GetLayerType() const      { return m_layerType; }

	virtual const std::vector<string>& GetFiles() const override { return m_files; }

protected:
	friend class CObjectLayerManager;

	// Constructor only valid for rare case where layer delete undo needs to create an empty layer to serialize into
	CObjectLayer();

	CObjectLayer(const char* szName, EObjectLayerType type);

	//! Set layer name.
	void SetNameImpl(const string& name, bool IsUpdateDepends = false);

protected:
	//! GUID of this layer.
	CryGUID m_guid;
	//! Layer Name.
	string  m_name;

	// Layer state.
	//! If true Object of this layer is hidden.
	bool m_hidden;
	//! If true Object of this layer is frozen (not selectable).
	bool m_frozen;
	//! True if objects on this layer must be exported to the game.
	bool m_exportable;
	//! True if layer needs to create its own per layer pak for faster streaming.
	bool m_exportLayerPak;
	//! True if layer needs to be activated on default level loading (map command)
	bool m_defaultLoaded;
	//! True when brushes from layer have physics
	bool m_havePhysics;

	//! True when layer is expanded in GUI. (Should not be exported to game)
	bool m_expanded;

	//! Layer switch on only for specified specs, like PC, etc.
	int m_specs;

	//! True when layer was changed
	bool m_isModified;

	//! Background color in list box
	ColorB m_color;
	bool   m_useColorOverride;

	// List of child layers.
	typedef std::vector<TSmartPtr<CObjectLayer>> ChildLayers;
	ChildLayers m_childLayers;

	//! Pointer to parent layer.
	CObjectLayer* m_parent;

	//! Layer ID for LayerSwith
	uint16           m_nLayerId;

	EObjectLayerType m_layerType;

	//! Attached files. Paths to file are relative to the corresponding level folder.
	std::vector<string> m_files;
};