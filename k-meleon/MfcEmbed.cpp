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
 *   Conrad Carlen <ccarlen@netscape.com> 
 */

// File Overview....
//
// The typical MFC app, frame creation code + AboutDlg handling
//
// NS_InitEmbedding() is called in InitInstance()
// 
// NS_TermEmbedding() is called in ExitInstance()
// ExitInstance() also takes care of cleaning up of
// multiple browser frame windows on app exit
//
// NS_DoIdleEmbeddingStuff(); is called in the overridden
// OnIdle() method
//
// Code to handle the creation of a new browser window

// Next suggested file to look at : BrowserFrm.cpp

// Local Includes
#include "stdafx.h"
#include "MfcEmbed.h"
#include "BrowserFrm.h"
#include "winEmbedFileLocProvider.h"
#include "ProfileMgr.h"
#include "BrowserImpl.h"
#include "nsIWindowWatcher.h"



#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define MENU_CONFIG_FILE "menus.cfg"
#define ACCEL_CONFIG_FILE "accel.cfg"

BEGIN_MESSAGE_MAP(CMfcEmbedApp, CWinApp)
	//{{AFX_MSG_MAP(CMfcEmbedApp)
	ON_COMMAND(ID_NEW_BROWSER, OnNewBrowser)
	ON_COMMAND(ID_MANAGE_PROFILES, OnManageProfiles)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
  ON_COMMAND(ID_PREFERENCES, OnPreferences)
	// NOTE - the ClassWizard will add and remove mapping macros here.
	//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CMfcEmbedApp::CMfcEmbedApp() :
    m_ProfileMgr(NULL)
{
   mRefCnt = 1; // Start at one - nothing is going to addref this object
}

CMfcEmbedApp theApp;

// Initialize our MFC application and also init
// the Gecko embedding APIs
// Note that we're also init'ng the profile switching
// code here
// Then, create a new BrowserFrame and load our
// default homepage
//
BOOL CMfcEmbedApp::InitInstance()
{
	Enable3dControls();

	// Take a look at 
	// http://www.mozilla.org/projects/xpcom/file_locations.html
	// for more info on File Locations

   winEmbedFileLocProvider *provider = new winEmbedFileLocProvider("KMeleon");
   if(!provider){
      ASSERT(FALSE);
      return FALSE;
   }

   nsresult rv;
   rv = NS_InitEmbedding(nsnull, provider);
   if(NS_FAILED(rv)){
      ASSERT(FALSE);
      return FALSE;
   }

   rv = InitializeWindowCreator();
   if (NS_FAILED(rv)) {
      ASSERT(FALSE);
      return FALSE;
   }

   if(!InitializeProfiles()){
      ASSERT(FALSE);
      NS_TermEmbedding();
      return FALSE;
   }

   plugins.FindAndLoad("kmeleon_*.dll");

   if(!CreateHiddenWindow()){
      ASSERT(FALSE);
      NS_TermEmbedding();
      return FALSE;
   }

   // Parse command line for standard shell commands, DDE, file open
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	// Create the first browser frame window
	CBrowserFrame *pBrowserFrame = CreateNewBrowserFrame();

	//Load the HomePage into the browser view
   if(pBrowserFrame){
      if(!cmdInfo.m_strFileName.IsEmpty())
         pBrowserFrame->m_wndBrowserView.OpenURL(cmdInfo.m_strFileName);
      else
         pBrowserFrame->m_wndBrowserView.LoadHomePage();

      pBrowserFrame->m_setURLBarFocus = true;
   }

   return TRUE;
}

CBrowserFrame* CMfcEmbedApp::CreateNewBrowserFrame(PRUint32 chromeMask,
												   PRInt32 x, PRInt32 y,
												   PRInt32 cx, PRInt32 cy,
												   PRBool bShowWindow)
{
	// Load the window title from the string resource table
	CString strTitle;
	strTitle.LoadString(IDR_MAINFRAME);

	// Now, create the browser frame
	CBrowserFrame* pFrame = new CBrowserFrame(chromeMask);

   // Restore previous window size is -1
   if (cx==-1 && cy==-1) {
      cx = preferences.posCX;
      cy = preferences.posCY;
      x = CW_USEDEFAULT;
      y = CW_USEDEFAULT;
   }

   // Setup a CRect with the requested window dimensions
	CRect winSize(x, y, cx, cy);

	// Use the Windows default if size is still -1
   if(cx == -1 && cy == -1)
		winSize = CFrameWnd::rectDefault;


   LONG style = WS_OVERLAPPEDWINDOW;
   if (preferences.bMaximized) style |= WS_MAXIMIZE;
   if (!pFrame->Create(NULL, strTitle, style, winSize, NULL, MAKEINTRESOURCE(IDR_MAINFRAME), 0L, NULL))
		return NULL;

   pFrame->SetIcon(LoadIcon(IDR_MAINFRAME), true);
   pFrame->SetIcon(LoadIcon(IDR_MAINFRAME), false);

	// load accelerator resource
   //pFrame->LoadAccelTable(MAKEINTRESOURCE(IDR_MAINFRAME));
   pFrame->m_hAccelTable = accel.GetTable();

	// Show the window...
	if(bShowWindow) {
      if (preferences.bMaximized) pFrame->ShowWindow(SW_MAXIMIZE);
      else pFrame->ShowWindow(SW_SHOW);
		pFrame->UpdateWindow();
	}

	// Add to the list of BrowserFrame windows
	m_FrameWndLst.AddHead(pFrame);
   
   pFrame->m_created = true;

   return pFrame;
}

void CMfcEmbedApp::OnNewBrowser()
{
	CBrowserFrame *pBrowserFrame = CreateNewBrowserFrame();

	//Load the HomePage into the browser view
   if(pBrowserFrame) {
		pBrowserFrame->m_wndBrowserView.LoadHomePage();
      pBrowserFrame->m_setURLBarFocus = true;
   }
}

// This gets called anytime a BrowserFrameWindow is
// closed i.e. by choosing the "close" menu item from
// a window's system menu or by dbl clicking on the
// system menu box
// 
// Sends a WM_QUIT to the hidden window which results
// in ExitInstance() being called and the app is
// properly cleaned up and shutdown
//
void CMfcEmbedApp::RemoveFrameFromList(CBrowserFrame* pFrm, BOOL bCloseAppOnLastFrame/*= TRUE*/)
{
	POSITION pos = m_FrameWndLst.Find(pFrm);
	m_FrameWndLst.RemoveAt(pos);

	// Send a WM_QUIT msg. to the hidden window if we've
	// just closed the last browserframe window and
	// if the bCloseAppOnLastFrame is TRUE. This be FALSE
	// only in the case we're switching profiles
	// Without this the hidden window will stick around
	// i.e. the app will never die even after all the 
	// visible windows are gone.
	if(m_FrameWndLst.GetCount() == 0 && bCloseAppOnLastFrame)
		m_pMainWnd->PostMessage(WM_QUIT);
}

int CMfcEmbedApp::ExitInstance()
{
	// When File/Exit is chosen and if the user
	// has opened multiple browser windows shut all
	// of them down properly before exiting the app
  
   CBrowserFrame* pBrowserFrame = NULL;

	POSITION pos = m_FrameWndLst.GetHeadPosition();
   while( pos != NULL ) {
		pBrowserFrame = (CBrowserFrame *) m_FrameWndLst.GetNext(pos);
		if(pBrowserFrame)	{
			pBrowserFrame->ShowWindow(false);
			pBrowserFrame->DestroyWindow();
		}
	}
	m_FrameWndLst.RemoveAll();

   if (m_pMainWnd)
      m_pMainWnd->DestroyWindow();

   delete m_ProfileMgr;

   theApp.preferences.Save();

   NS_TermEmbedding();

	return 1;
}

BOOL CMfcEmbedApp::OnIdle(LONG lCount)
{
	CWinApp::OnIdle(lCount);

	NS_DoIdleEmbeddingStuff();

	return FALSE;
}

void CMfcEmbedApp::OnPreferences () {
  CPreferencesDlg prefDlg;
  prefDlg.DoModal();
}

void CMfcEmbedApp::OnManageProfiles()
{
  m_ProfileMgr->DoManageProfilesDialog(PR_FALSE);
}

BOOL CMfcEmbedApp::InitializeProfiles() {
   m_ProfileMgr = new CProfileMgr;
   if (!m_ProfileMgr)
      return FALSE;

   nsresult rv;
   NS_WITH_SERVICE(nsIObserverService, observerService, NS_OBSERVERSERVICE_CONTRACTID, &rv);

   observerService->AddObserver(this, NS_LITERAL_STRING("profile-approve-change").get());
   observerService->AddObserver(this, NS_LITERAL_STRING("profile-change-teardown").get());
   observerService->AddObserver(this, NS_LITERAL_STRING("profile-after-change").get());

   m_ProfileMgr->StartUp();

   return TRUE;
}

// When the profile switch happens, all open browser windows need to be 
// closed. 
// In order for that not to kill off the app, we just make the MFC app's 
// mainframe be an invisible window which doesn't get closed on profile 
// switches
BOOL CMfcEmbedApp::CreateHiddenWindow()
{
	CFrameWnd *hiddenWnd = new CFrameWnd;
	if(!hiddenWnd)
    return FALSE;

  RECT bounds = { -10010, -10010, -10000, -10000 };
  hiddenWnd->Create(NULL, "main", WS_DISABLED, bounds, NULL, NULL, 0, NULL);
  m_pMainWnd = hiddenWnd;

  return TRUE;
}

nsresult CMfcEmbedApp::InitializePrefs(){
  preferences.Load();

  nsresult rv;
  rv = InitializeCachePrefs();
  NS_ASSERTION(NS_SUCCEEDED(rv), "Could not initialize cache prefs");

  if (!menus.Load(preferences.settingsDir + MENU_CONFIG_FILE)){
    // we used to create the file if it didn't exist
    // but now it should be copied automagically from defaults when the profile is created
    MessageBox(NULL, "Could not find " MENU_CONFIG_FILE, NULL, 0);
  }
  if (!accel.Load(preferences.settingsDir + ACCEL_CONFIG_FILE)){
    MessageBox(NULL, "Could not find " ACCEL_CONFIG_FILE, NULL, 0);
  }

  return rv;
}

nsresult CMfcEmbedApp::InitializeCachePrefs()
{
   const char * const CACHE_DIR_PREF   = "browser.cache.directory";
	
	nsresult rv;
	NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
	if (NS_FAILED(rv)) return rv;
	
	// See if we have a pref to a dir which exists
    nsCOMPtr<nsILocalFile> prefDir;
    rv = prefs->GetFileXPref(CACHE_DIR_PREF, getter_AddRefs(prefDir));
    if (NS_SUCCEEDED(rv)) {
        PRBool isDir;
        rv = prefDir->IsDirectory(&isDir);
        if (NS_SUCCEEDED(rv) && isDir)
            return NS_OK;
    }

    // Set up the new pref
    nsCOMPtr<nsIFile> profileDir;
    rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(profileDir));
    NS_ASSERTION(profileDir, "NS_APP_USER_PROFILE_50_DIR is not defined");
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsILocalFile> cacheDir(do_QueryInterface(profileDir));
    NS_ASSERTION(cacheDir, "Cannot get nsILocalFile from cache dir");

    PRBool exists;
    cacheDir->Append("Cache");
    rv = cacheDir->Exists(&exists);
    if (NS_SUCCEEDED(rv) && !exists)
    rv = cacheDir->Create(nsIFile::DIRECTORY_TYPE, 0775);
    if (NS_FAILED(rv)) return rv;

    return prefs->SetFileXPref(CACHE_DIR_PREF, cacheDir);
}


/* InitializeWindowCreator creates and hands off an object with a callback
   to a window creation function. This will be used by Gecko C++ code
   (never JS) to create new windows when no previous window is handy
   to begin with. This is done in a few exceptional cases, like PSM code.
   Failure to set this callback will only disable the ability to create
   new windows under these circumstances.
*/
nsresult CMfcEmbedApp::InitializeWindowCreator() {
   // give an nsIWindowCreator to the WindowWatcher service
   nsCOMPtr<nsIWindowCreator> windowCreator(NS_STATIC_CAST(nsIWindowCreator *, this));
   if (windowCreator) {
      nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
      if (wwatch) {
         wwatch->SetWindowCreator(windowCreator);
         return NS_OK;
      }
   }
   return NS_ERROR_FAILURE;
}
// ---------------------------------------------------------------------------
//  CMfcEmbedApp : nsISupports
// ---------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS3(CMfcEmbedApp, nsIObserver, nsIWindowCreator, nsISupportsWeakReference);

// ---------------------------------------------------------------------------
//  CMfcEmbedApp : nsIObserver
// ---------------------------------------------------------------------------

// Mainly needed to support "on the fly" profile switching

NS_IMETHODIMP CMfcEmbedApp::Observe(nsISupports *aSubject, const PRUnichar *aTopic, const PRUnichar *someData)
{
    nsresult rv = NS_OK;
    
    if (nsCRT::strcmp(aTopic, NS_LITERAL_STRING("profile-approve-change").get()) == 0)
    {
        // Ask the user if they want to
        int result = MessageBox(NULL, "Do you want to close all windows in order to switch the profile?", "Confirm", MB_YESNO | MB_ICONQUESTION);
        if (result != IDYES)
        {
            nsCOMPtr<nsIProfileChangeStatus> status = do_QueryInterface(aSubject);
            NS_ENSURE_TRUE(status, NS_ERROR_FAILURE);
            status->VetoChange();
        }
    }
    else if (nsCRT::strcmp(aTopic, NS_LITERAL_STRING("profile-change-teardown").get()) == 0)
    {
        // Close all open windows. Alternatively, we could just call CBrowserWindow::Stop()
        // on each. Either way, we have to stop all network activity on this phase.
        
	    POSITION pos = m_FrameWndLst.GetHeadPosition();
	    while( pos != NULL )
	    {
		    CBrowserFrame *pBrowserFrame = (CBrowserFrame *) m_FrameWndLst.GetNext(pos);
		    if(pBrowserFrame)
		    {
			    pBrowserFrame->ShowWindow(false);

				// Passing in FALSE below so that we do not
				// kill the main app during a profile switch
				RemoveFrameFromList(pBrowserFrame, FALSE);

				pBrowserFrame->DestroyWindow();
		    }
	    }

        NS_WITH_SERVICE(nsINetDataCacheManager, cacheMgr, NS_NETWORK_CACHE_MANAGER_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv))
          cacheMgr->Clear(nsINetDataCacheManager::MEM_CACHE);
    }
    else if (nsCRT::strcmp(aTopic, NS_LITERAL_STRING("profile-after-change").get()) == 0)
    {
        plugins.UnLoadAll();
        InitializePrefs(); // In case we have just switched to a newly created profile.
        
         // Only make a new browser window on a switch. This also gets
         // called at start up and we already make a window then.
         if (!nsCRT::strcmp(someData, NS_LITERAL_STRING("switch").get()))      
            OnNewBrowser();
    }
    return rv;
}



// ---------------------------------------------------------------------------
//  CMfcEmbedApp : nsIWindowCreator
// ---------------------------------------------------------------------------
NS_IMETHODIMP CMfcEmbedApp::CreateChromeWindow(nsIWebBrowserChrome *parent,
                                               PRUint32 chromeFlags,
                                               nsIWebBrowserChrome **_retval)
{
   // XXX we're ignoring the "parent" parameter
   NS_ENSURE_ARG_POINTER(_retval);
   *_retval = 0;

   CBrowserFrame *pBrowserFrame = CreateNewBrowserFrame(chromeFlags);
   if(pBrowserFrame) {
      *_retval = NS_STATIC_CAST(nsIWebBrowserChrome *, pBrowserFrame->GetBrowserImpl());
      NS_ADDREF(*_retval);
   }
   return NS_OK;
}


#include "version.h"

// AboutDlg Stuff

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

	enum { IDD = IDD_ABOUTBOX };

protected:
  CString m_credits;
  CString m_version;

	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

protected:
	DECLARE_MESSAGE_MAP()
};

#define _QUOTE(blah) #blah

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
  m_credits = _QUOTE(
    K-Meleon - Copyright 2000-2001\r\n
    \r\n
    Uses the Gecko rendering engine by the Mozilla Group\r\n
    Based on MfcEmbed\r\n
    Corner animation stolen from Galeon\r\n
    \r\n
    Contributors:\r\n
    Christophe Thibault <christophe@nullsoft.com>\r\n
    Brian Harris <binaryc@teamreaction.com>\r\n
    Sebastian Spaeth <Sebastian@SSpaeth.de>\r\n
    Chak Nanga <chak@netscape.com>\r\n
    Jeff Doozan <jeff@tcbnetworks.com>\r\n
    \r\n
    Documentation:\r\n
    Lance Hall (aka pyzr) <pyzr@pyzr.com>\r\n
  );
  m_version.Format("Version " VERSION " Build %d Compiled " BUILD_TIME, BUILD_NUMBER);
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX){
	CDialog::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_CREDITS, m_credits);
  DDX_Text(pDX, IDC_EDIT_VERSION, m_version);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()

// Show the AboutDlg
void CMfcEmbedApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}
