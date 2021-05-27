// Copyright 2001-2019 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:

   System "Hardware mouse" cursor with reference counter.
   This is needed because Menus / HUD / Profiler / or whatever
   can use the cursor not at the same time be successively
   => We need to know when to enable/disable the cursor.

   -------------------------------------------------------------------------
   History:
   - 18:12:2006   Created by Julien Darré

*************************************************************************/
#ifndef __HARDWAREMOUSE_H__
#define __HARDWAREMOUSE_H__

//-----------------------------------------------------------------------------------------------------

#include <CrySystem/ISystem.h>
#include <CryInput/IHardwareMouse.h>
#include <CryCore/Platform/CryWindows.h>

//-----------------------------------------------------------------------------------------------------

class CHardwareMouse : public IInputEventListener, public IHardwareMouse, public ISystemEventListener
{
public:

	// IInputEventListener
	virtual bool OnInputEvent(const SInputEvent& rInputEvent);
	// ~IInputEventListener

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam);
	// ~ISystemEventListener

	// IHardwareMouse
	virtual void                         Release();
	virtual void                         OnPostInitRenderer();
	virtual void                         OnPostInitInput();
	virtual void                         Event(int iX, int iY, EHARDWAREMOUSEEVENT eHardwareMouseEvent, int wheelDelta = 0);
	virtual void                         AddListener(IHardwareMouseEventListener* pHardwareMouseEventListener);
	virtual void                         RemoveListener(IHardwareMouseEventListener* pHardwareMouseEventListener);
	virtual bool                         SetExclusiveEventListener(IHardwareMouseEventListener* pExclusiveEventListener);
	virtual void                         RemoveExclusiveEventListener(IHardwareMouseEventListener* pExclusiveEventListener);
	virtual IHardwareMouseEventListener* GetCurrentExclusiveEventListener() { return m_pExclusiveEventListener; }
	virtual void                         SetConfinedWnd(CRY_HWND wnd);
	virtual void                         SetGameMode(bool bGameMode);
	virtual void                         IncrementCounter();
	virtual void                         DecrementCounter();
	virtual bool                         IsCursorVisible() const;
	virtual void                         GetHardwareMousePosition(float* pfX, float* pfY);
	virtual void                         SetHardwareMousePosition(float fX, float fY);
	virtual void                         GetHardwareMouseClientPosition(float* pfX, float* pfY);
	virtual void                         SetHardwareMouseClientPosition(float fX, float fY);
	virtual void                         Reset(bool bVisibleByDefault);
	virtual void                         ConfineCursor(bool confine);
	virtual void                         Hide(bool hide);
	virtual bool                         SetCursor(int idc_cursor_id);
	virtual bool                         SetCursor(const char* path);
	virtual void                         UseSystemCursor(bool bUseSystemCursor);
#if CRY_PLATFORM_WINDOWS
	virtual void*                        GetCurrentCursor();
#endif
	virtual ISystemEventListener*        GetSystemEventListener() { return this; }

	void                                 Update();
	void                                 Render();
	// ~IHardwareMouse

	CHardwareMouse(bool bVisibleByDefault);
	~CHardwareMouse();

private:
	void        ShowHardwareMouse(bool bShow);
	static bool IsFullscreen();
	void        DestroyCursor();
	//! evaluate cursor confine state from current reference counting
	void        EvaluateCursorConfinement();
	//! respond to focus-in, focus-out events
	void        HandleFocusEvent(bool bFocus);
	CRY_HWND    GetConfinedWindowHandle() const;

	typedef std::list<IHardwareMouseEventListener*> TListHardwareMouseEventListeners;
	TListHardwareMouseEventListeners m_listHardwareMouseEventListeners;
	IHardwareMouseEventListener*     m_pExclusiveEventListener = nullptr;

	ITexture*  m_pCursorTexture = nullptr;
	int        m_iReferenceCounter;
	float      m_fCursorX;
	float      m_fCursorY;
	float      m_fIncX;
	float      m_fIncY;
	bool       m_bFocus;
	bool       m_bPrevShowState;
	const bool m_allowConfine;
	string     m_curCursorPath;
	bool       m_shouldUseSystemCursor;
	bool       m_usingSystemCursor;
	CRY_HWND   m_confinedWnd;

#if CRY_PLATFORM_WINDOWS
	HCURSOR    m_hCursor;
	int        m_nCurIDCCursorId;
#else
	float      m_fVirtualX;
	float      m_fVirtualY;
#endif

	bool       m_hide;
	bool       m_calledShowHWMouse;
	int        m_debugHardwareMouse = 0;

	static float s_MouseCursorSoftwareOffsetX;
	static float s_MouseCursorSoftwareOffsetY;
	static int   s_MouseControllerEmulation;
};

//-----------------------------------------------------------------------------------------------------

#endif

//-----------------------------------------------------------------------------------------------------
