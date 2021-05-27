// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

// This is the header file for the general utility dialog for
// overwrite confirmation. The purpose of this dialog, as one might imagine, is to
// get check if the user really wants to overwrite some item, allowing him/her to
// apply the same choices to all the remaining items.
class CGenericOverwriteDialog : public CDialog
{
public:
	CGenericOverwriteDialog(const CString& strTitle, const CString& strText, bool boMultipleFiles = true);
	void DoDataExchange(CDataExchange* pDX);
	BOOL OnInitDialog();
	void OnYes();
	void OnNo();
	void OnCancel();

	// Call THIS method to start this dialog.
	// The return values can be IDYES, IDNO and IDCANCEL
	INT_PTR DoModal();

	bool    IsToAllToggled();

	DECLARE_MESSAGE_MAP();

protected:
	CString m_strTitle;
	CString m_strText;
	bool    m_boToAll;

	bool    m_boMultipleFiles;
	CStatic m_strTextMessage;
	CButton m_oToAll;

	bool    m_boIsModal;
};
