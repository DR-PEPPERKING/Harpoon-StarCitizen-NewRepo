// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Dialogs/GenericSelectItemDialog.h"

struct ICharacterInstance;

class CSelectAnimationDialog : public CGenericSelectItemDialog
{
	DECLARE_DYNAMIC(CSelectAnimationDialog)
	CSelectAnimationDialog(CWnd* pParent = NULL);
	virtual ~CSelectAnimationDialog() {}

	void    SetCharacterInstance(ICharacterInstance* pCharacterInstance);
	CString GetSelectedItem();

protected:
	virtual BOOL OnInitDialog();

	// Derived Dialogs should override this
	virtual void GetItems(std::vector<SItem>& outItems);

	// Called whenever an item gets selected
	virtual void ItemSelected();

	ICharacterInstance* m_pCharacterInstance;
};
