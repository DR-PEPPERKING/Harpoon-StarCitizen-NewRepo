// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "LevelModel.h"

#include <ObjectEvent.h>
#include <IObjectManager.h>
#include <Objects/BaseObject.h>
#include <ProxyModels/ItemModelAttribute.h>

#include <QAbstractItemModel>
class CObjectLayer;
class CLevelModelsManager;
struct CLayerChangeEvent;

namespace LevelModelsAttributes
{
extern CItemModelAttribute s_layerNameAttribute;
extern CItemModelAttributeEnumT<ObjectType> s_objectTypeAttribute;
extern CItemModelAttribute s_objectTypeDescAttribute;
extern CItemModelAttribute s_visibleAttribute;
extern CItemModelAttribute s_lockedAttribute;
extern CItemModelAttribute s_defaultMaterialAttribute;
extern CItemModelAttribute s_customMaterialAttribute;
extern CItemModelAttribute s_breakableAttribute;
extern CItemModelAttribute s_smartObjectAttribute;
extern CItemModelAttribute s_flowGraphAttribute;
extern CItemModelAttribute s_geometryAttribute;
extern CItemModelAttribute s_geometryInstancesAttribute;
extern CItemModelAttribute s_lodCountAttribute;
extern CItemModelAttribute s_specAttribute;
extern CItemModelAttribute s_aiGroupIdAttribute;
extern CItemModelAttribute s_LayerColorAttribute;
extern CItemModelAttribute s_linkedToAttribute;
}

//! This class is not meant to be instantiated directly, request an instance from LevelModelsManager
class CLevelLayerModel : public QAbstractItemModel
{
public:

	enum EObjectColumns
	{
		eObjectColumns_LayerColor,
		eObjectColumns_Visible, //Must be kept index 0 to match eLayerColumns_Visible
		eObjectColumns_Frozen,  //Must be kept index 1 to match eLayerColumns_Frozen
		eObjectColumns_VCS,     //Must be kept index 2 to match eLayerColumns_VCS
		eObjectColumns_Name,    //Must be kept index 3 to match eLayerColumns_Name
		eObjectColumns_Layer,
		eObjectColumns_Type,
		eObjectColumns_TypeDesc,
		eObjectColumns_DefaultMaterial,
		eObjectColumns_CustomMaterial,
		eObjectColumns_Breakability,
		eObjectColumns_SmartObject,
		eObjectColumns_FlowGraph,
		eObjectColumns_Geometry,
		eObjectColumns_InstanceCount,
		eObjectColumns_LODCount,
		eObjectColumns_MinSpec,
		eObjectColumns_AI_GroupId,
		eObjectColumns_LinkedTo,
		eObjectColumns_Size
	};

	static CItemModelAttribute* GetAttributeForColumn(EObjectColumns column);
	static QVariant             GetHeaderData(int section, Qt::Orientation orientation, int role);

private:
	CLevelLayerModel(CObjectLayer* pLayer, QObject* parent = nullptr);
	~CLevelLayerModel();
	void Disconnect();

public:

	enum class Roles : int
	{
		InternalPointerRole = CLevelModel::Roles::InternalPointerRole, // = Qt::UserRole, intptr_t (CBaseObject*)
		TypeCheckRole                                                  //Will return ELevelElementType::eLevelElement_Object
	};

	//////////////////////////////////////////////////////////
	// QAbstractItemModel implementation
	virtual int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual int columnCount(const QModelIndex& parent = QModelIndex()) const override
	{
		return eObjectColumns_Size;
	}
	virtual QVariant        data(const QModelIndex& index, int role) const override;
	virtual QVariant        headerData(int section, Qt::Orientation orientation, int role) const override;
	virtual Qt::ItemFlags   flags(const QModelIndex& index) const override;
	virtual QModelIndex     index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	virtual QModelIndex     parent(const QModelIndex& index) const override;
	virtual bool            setData(const QModelIndex& index, const QVariant& value, int role) override;
	virtual Qt::DropActions supportedDragActions() const override;
	virtual Qt::DropActions supportedDropActions() const override;
	virtual QStringList     mimeTypes() const override;
	virtual bool            canDropMimeData(const QMimeData* pData, Qt::DropAction action, int row, int column, const QModelIndex& parent) const override;
	virtual bool            dropMimeData(const QMimeData* pData, Qt::DropAction action, int row, int column, const QModelIndex& parent) override;
	virtual QMimeData*      mimeData(const QModelIndexList& indexes) const override;
	//////////////////////////////////////////////////////////

	CBaseObject*  ObjectFromIndex(const QModelIndex& index) const;
	QModelIndex   IndexFromObject(const CBaseObject* pObject) const;
	int           RowFromObject(const CBaseObject* pObject) const;
	CObjectLayer* GetLayer() { return m_pLayer; }

private:
	void              OnLink(const CBaseObject* pObject);
	void              OnUnLink(const CBaseObject* pObject);

	static bool       Filter(CBaseObject const& obj, void* pLayer);

	CBaseObjectsArray GetLayerObjects() const;

	//! Updates data that was cached as it would be expensive to retrieve every time the view wants to update
	void        Connect();
	void        Rebuild();
	void        Clear();
	void        AddObject(const CBaseObject* pObject);
	void        AddObjects(const std::vector<CBaseObject*>& objects);
	void        RemoveObject(const CBaseObject* pObject);
	void        RemoveObjects(int row, int count = 1);
	void        RemoveObjects(const std::vector<CBaseObject*>& objects);
	void        UpdateCachedDataForObject(const CBaseObject* pObject);

	void        OnObjectEvent(const std::vector<CBaseObject*>& objects, const CObjectEvent&);
	void        OnSelectionChanged();
	void        OnBeforeObjectsAttached(const CBaseObject* pParent, const std::vector<CBaseObject*>& objects);
	void        OnObjectsAttached(const CBaseObject* pParent, const std::vector<CBaseObject*>& objects);
	void        OnBeforeObjectsDetached(const CBaseObject* pParent, const std::vector<CBaseObject*>& objects);
	void        OnObjectsDetached(const CBaseObject* pParent, const std::vector<CBaseObject*>& objects);
	void        OnBeforeObjectsDeleted(const std::vector<CBaseObject*>& objects);
	void        OnObjectsDeleted(const std::vector<CBaseObject*>& objects);

	const char* GetMaterialName(CBaseObject* pObject, bool bUseCustomMaterial) const;
	const char* GetObjectBreakability(CBaseObject* pObject) const;
	void        GetMaterialBreakability(std::set<string>* breakTypes, CMaterial* pMaterial) const;
	const char* GetSmartObject(CBaseObject* pObject) const;
	const char* GetFlowGraphNames(CBaseObject* pObject) const;
	const char* GetGeometryFile(const CBaseObject* pObject) const;
	uint        GetInstancesCount(CBaseObject* pObject) const;
	uint        GETLODNumber(CBaseObject* pObject) const;
	int         GetAIGroupID(CBaseObject* pObject) const;
	void        NotifyUpdateObject(const CBaseObject* pObject, const QVector<int>& updateRoles);

	bool        IsRelatedToTopLevelObject(CBaseObject* pObject) const;

	void        StartBatchProcess();
	void        FinishBatchProcess();

	CObjectLayer*      m_pLayer;
	const CBaseObject* m_pTopLevelNotificationObj;

	// Cached data
	CBaseObjectsArray                m_rootObjects;
	std::map<CBaseObjectPtr, string> m_flowGraphMap;
	std::map<string, int>            m_geometryCountMap;

	// Precached icons: prevents CryIcon ctor on each data() call
	CryIcon m_iconVisibilityTrue;
	CryIcon m_iconVisibilityFalse;
	CryIcon m_iconGeneralLockFalse;
	CryIcon m_iconLink;
	CryIcon m_iconPlaceHolder;
	CryIcon m_iconLevelExplorerLockTrue;

	bool    m_isRuningBatchProcess{ false };
	bool    m_isDisconnected{ false };

	friend class CLevelModelsManager;
};
