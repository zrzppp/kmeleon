/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Chak Nanga <chak@netscape.com> 
 */

// BrowserFrm.h : interface of the CBrowserFrame class
//
/////////////////////////////////////////////////////////////////////////////

#ifndef _IBROWSERFRM_H
#define _IBROWSERFRM_H

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "BrowserView.h"
#include "IBrowserFrameGlue.h"
#include "MostRecentUrls.h"
#include "ToolBarEx.h"
#include "KmeleonConst.h"
#include "ReBarEx.h"

#include "MfcEmbed.h"
extern CMfcEmbedApp theApp;


// A simple UrlBar class...
class CUrlBar : public CComboBoxEx {
public:
   int Create(DWORD style, RECT &rect, CWnd *parentWnd, UINT id){
      int ret = CComboBoxEx::Create(style | CBS_AUTOHSCROLL, rect, parentWnd, id);

      COMBOBOXEXITEM ci;
      ci.mask = CBEIF_IMAGE;
      ci.iItem = -1;
      ci.iImage = 15;
      SetItem(&ci);

      return ret;
   }
	inline GetEnteredURL(CString& url) {
      GetEditCtrl()->GetWindowText(url);
	}
	inline GetSelectedURL(CString& url) {
      GetLBText(GetCurSel(), url);
	}
	inline SetCurrentURL(LPCTSTR pUrl) {
      CComboBoxEx test;
		SetWindowText(pUrl);
	}
	inline AddURLToList(CString& url, bool bAddToMRUList = true) {
      COMBOBOXEXITEM ci;
		ci.mask = CBEIF_TEXT | CBEIF_IMAGE | CBEIF_SELECTEDIMAGE;
      ci.iItem = 0;
      ci.iImage = 15;
      ci.iSelectedImage = 15;
		ci.pszText = (LPTSTR)(LPCTSTR)url;
		InsertItem(&ci);

      if(bAddToMRUList) {
		   m_MRUList.AddURL((LPTSTR)(LPCTSTR)url);
         ResetContent();
         LoadMRUList();
      }
   }
   inline LoadMRUList() {
      for (int i=m_MRUList.GetURLCount()-1;i>=0;i--) {
         CString urlStr(_T(m_MRUList.GetURL(i)));
         AddURLToList(urlStr, false);
      }
   }

protected:
   CMostRecentUrls m_MRUList;
};


class CBrowserFrame : public CFrameWnd
{	
public:
	CBrowserFrame(PRUint32 chromeMask);

protected: 
	DECLARE_DYNAMIC(CBrowserFrame)

public:
   inline CBrowserImpl *GetBrowserImpl() { return m_wndBrowserView.mpBrowserImpl; }

   CToolBarEx      m_wndToolBar;
	CStatusBar      m_wndStatusBar;
	CProgressCtrl   m_wndProgressBar;
	CUrlBar         m_wndUrlBar;
	CReBarEx        m_wndReBar;
   CAnimateCtrl	 m_wndAnimate;

   CImageList      m_toolbarHotImageList;
   CImageList      m_toolbarColdImageList;
   CImageList      m_toolbarDisabledImageList;

   CBitmap         m_bmpBack;

	// The view inside which the embedded browser will
	// be displayed in
	CBrowserView   m_wndBrowserView;

	// This specifies what UI elements this frame will sport
	// w.r.t. toolbar, statusbar, urlbar etc.
	PRUint32 m_chromeMask;

   BOOL m_created; // set after we are created
   BOOL m_setURLBarFocus;

protected:
	//
	// This nested class implements the IBrowserFramGlue interface
	// The Gecko embedding interfaces call on this interface
	// to update the status bars etc.
	//
	class BrowserFrameGlueObj : public IBrowserFrameGlue 
	{
		//
		// NS_DECL_BROWSERFRAMEGLUE below is a macro which expands
		// to the function prototypes of methods in IBrowserFrameGlue
		// Take a look at IBrowserFrameGlue.h for this macro define
		//

		NS_DECL_BROWSERFRAMEGLUE

	} m_xBrowserFrameGlueObj;
	friend class BrowserFrameGlueObj;

public:
	void SetupFrameChrome();

   void LoadBackImage ();
   void SetBackImage ();

   void SaveWindowPos();
   void RestoreWindowPos(PRInt32 *x, PRInt32 *y, PRInt32 *cx, PRInt32 *cy);

   BOOL Create(LPCTSTR lpszClassName,
	   LPCTSTR lpszWindowName,
	   DWORD dwStyle,
	   const RECT& rect,
	   CWnd* pParentWnd,
	   LPCTSTR lpszMenuName,
	   DWORD dwExStyle,
	   CCreateContext* pContext);

   // Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CBrowserFrame)
	public:
	afx_msg void OnClose();
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	virtual BOOL OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo);
	//}}AFX_VIRTUAL

// Implementation
public:
	virtual ~CBrowserFrame();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

// Generated message map functions
protected:
	//{{AFX_MSG(CBrowserFrame)
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSetFocus(CWnd *pOldWnd);
	afx_msg void OnSize(UINT nType, int cx, int cy);
   afx_msg void OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized);
   afx_msg void OnSysColorChange();
	afx_msg void RefreshToolBarItem(WPARAM ItemID, LPARAM unused);
   afx_msg void ToggleToolBar(UINT uID);
   afx_msg void OnCopyData(CWnd* pWnd, COPYDATASTRUCT* pCopyDataStruct);
   afx_msg void OnNewWindow();

	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};


/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif //_IBROWSERFRM_H