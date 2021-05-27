// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Controls/SplitterCtrl.h"
#include "Controls/PropertyCtrl.h"
#include "Controls/PreviewModelCtrl.h"
#include "BaseLibraryDialog.h"
#include "EntityScriptDialog.h"

struct IEntitySystem;
struct IEntity;

class CEntityPrototypeManager;
class CEntityPrototype;
class CMFCPropertyTree;

/** Dialog which hosts entity prototype library.
 */
class CEntityProtLibDialog : public CBaseLibraryDialog
{
	DECLARE_DYNAMIC(CEntityProtLibDialog)
public:
	CEntityProtLibDialog(CWnd* pParent);
	~CEntityProtLibDialog();

	// Called every frame.
	void         Update();
	virtual UINT GetDialogMenuID();

protected:
	void         InitToolbar(UINT nToolbarResID);
	void         DoDataExchange(CDataExchange* pDX);
	BOOL         OnInitDialog();
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnAddPrototype();
	afx_msg void OnPlay();
	afx_msg void OnUpdatePlay(CCmdUI* pCmdUI);
	afx_msg void OnShowDescription();
	afx_msg void OnDescriptionChange();
	afx_msg void OnAssignToSelection();
	afx_msg void OnSelectAssignedObjects();
	afx_msg void OnNotifyTreeRClick(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnBeginDrag(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnCopy();
	afx_msg void OnPaste();
	afx_msg void OnRemoveLibrary();

	//////////////////////////////////////////////////////////////////////////
	//////////////////////////////////////////////////////////////////////////
	void SelectItem(CBaseLibraryItem* item, bool bForceReload = false);

	//////////////////////////////////////////////////////////////////////////
	void              SpawnEntity(CEntityPrototype* prototype);
	void              ReleaseEntity();
	void              OnUpdateProperties(IVariable* var);
	void              OnReloadEntityScript();
	string            SelectEntityClass();
	CEntityPrototype* GetSelectedPrototype();
	void              OnChangeEntityClass();
	void              UpdatePreview();
	void              OnArchetypeExtensionPropsChanged();

	DECLARE_MESSAGE_MAP()
	bool MarkOrUnmarkPropertyField(IVariable* varArchetype, IVariable* varEntScript);
	void MarkFieldsThatDifferFromScriptDefaults(IVariable* varArchetype, IVariable* varEntScript);
	void MarkFieldsThatDifferFromScriptDefaults(CVarBlock* pArchetypeVarBlock, CVarBlock* pEntScriptVarBlock);
	CSplitterCtrl m_wndHSplitter;
	CSplitterCtrl m_wndVSplitter;
	CSplitterCtrl m_wndScriptPreviewSplitter;
	CSplitterCtrl m_wndSplitter4;
	CSplitterCtrl m_wndPropsHSplitter;

	//CModelPreview
	CEntityScriptDialog m_scriptDialog;
	CPreviewModelCtrl   m_previewCtrl;
	CPropertyCtrl       m_propsCtrl;
	CPropertyCtrl       m_objectPropsCtrl;
	CMFCPropertyTree*   m_pArchetypeExtensionPropsCtrl { nullptr };
	CXTEdit             m_descriptionEditBox;
	CImageList          m_imageList;
	CXTCaption          m_wndCaptionEntityClass;

	//! Selected Prototype.
	IEntity*       m_entity { nullptr };
	IEntitySystem* m_pEntitySystem { nullptr };
	string         m_visualObject;
	string         m_PrototypeMaterial;

	bool           m_bEntityPlaying { false };
	bool           m_bShowDescription { false };

	// Prototype manager.
	CEntityPrototypeManager* m_pEntityManager;
};
