/*
*  Copyright (C) 2000 Brian Harris
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2, or (at your option)
*  any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software
*  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
// adds icons to the menus
//

#include "stdafx.h"
#include "../resource.h"
#include "../Utils.h"
#include <afxres.h>

#pragma warning( disable : 4786 ) // C4786 bitches about the std::map template name expanding beyond 255 characters
#include <map>
#include <string>
#include <vector>

#define KMELEON_PLUGIN_EXPORTS
#include "../kmeleon_plugin.h"

#define BMP_MENU_VERSION 3333
#define LEFT_SPACE 16
#define BMP_HEIGHT 16
#define BMP_WIDTH  16


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
WNDPROC KMeleonWndProc;

void DrawCheckMark(HDC pDC,int x,int y,COLORREF color);
void SetOwnerDrawn(HMENU menu, HINSTANCE plugin);
void UnSetOwnerDrawn(HMENU menu);

int Init();
void Create(HWND parent);
void Config(HWND parent);
void Quit();
void DoMenu(HMENU menu, char *param);

pluginFunctions pFunc = {
   Init,
   Create,
   Config,
   Quit,
   DoMenu,
   NULL // no rebar
};

kmeleonPlugin kPlugin = {
   KMEL_PLUGIN_VER,
   "Bitmapped Menus",
   &pFunc
};

/*
# sample config

filename1.bmp {
ID_BLAH1
ID_BLAH2
ID_BLAH3
}
filename2.bmp {
ID_BLARG1
ID_BLARG2
}
*/

char settingsPath[MAX_PATH];

std::vector<HMENU> menus;
int refCount;

HIMAGELIST hImageList;

// this maps command ids to the bitmap/index
typedef std::map<short, int> BmpMapT;
BmpMapT bmpMap;

// maps "ID_BLAH" to ID_BLAH
typedef std::map<std::string, int> DefineMapT;

void ParseConfig(char *buffer) {
   hImageList = ImageList_Create(BMP_WIDTH, BMP_HEIGHT, ILC_MASK | ILC_COLOR8, 32, 256);

	DefineMapT defineMap;
   DefineMapT::iterator defineMapIt;
#define DEFINEMAP_ADD(entry) defineMap[std::string(#entry)] = entry;
#include "../definemap.cpp"

   BOOL currentBitmap = false;
	int index = 0;

	char *p;
	while ((p = strtok(NULL, "\n")) != NULL) {

      // ignore the comments
		if (*p == '#') {
			continue;
		}
		else if (!currentBitmap) {
			char *b = strchr(p, '{');
			if (b) {
				*b = 0;
				TrimWhiteSpace(p);
				p = SkipWhiteSpace(p);

            HBITMAP bitmap;
            // if it's relative to the root (ie: c:\blah or \blah or /blah
				if (*p == '/' || *p == '\\' || *(p+1) == ':') {
					bitmap = (HBITMAP)LoadImage(NULL, p, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
				}
            // else it's relative to the settings directory (just plain blah)
				else {
					char bmpPath[MAX_PATH];
					strcpy(bmpPath, settingsPath);
					strcat(bmpPath, p);
					bitmap = (HBITMAP)LoadImage(NULL, bmpPath, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
				}
            ImageList_AddMasked(hImageList, bitmap, RGB(192, 192, 192));

            DeleteObject(bitmap);

            currentBitmap = true;
			}
		}
		else {
			if ( strchr( p, '}' )) {
            currentBitmap = false;
            index = ImageList_GetImageCount(hImageList);
				continue;
			}

			TrimWhiteSpace(p);
			p = SkipWhiteSpace(p);

			int id;
			defineMapIt = defineMap.find(std::string(p));
			if ( defineMapIt != defineMap.end() )
				id = defineMapIt->second;
			else
				id = 0;
			bmpMap[id] = index;
			index++;
		}
	}
}

int Init() {
   refCount = 0;

   kPlugin.kf->GetPreference(PREF_STRING, "kmeleon.general.settingsDir", settingsPath, "");

   char cfgPath[MAX_PATH];
   strcpy(cfgPath, settingsPath);
   strcat(cfgPath, "menuicons.cfg");

   FILE *cfgFile = fopen(cfgPath, "r");
   if (cfgFile){
      fseek(cfgFile, 0, SEEK_END);
      long cfgFileSize = ftell(cfgFile);
      fseek(cfgFile, 0, SEEK_SET);

      char *cfgFileBuffer = new char[cfgFileSize];
      if (cfgFileBuffer) {
         fread(cfgFileBuffer, sizeof(char), cfgFileSize, cfgFile);

         strtok(cfgFileBuffer, "\n");
         ParseConfig(cfgFileBuffer);

         delete [] cfgFileBuffer;
      }
      fclose(cfgFile);
   }
   return true;
}

void Create(HWND parent){
	KMeleonWndProc = (WNDPROC) GetWindowLong(parent, GWL_WNDPROC);
	SetWindowLong(parent, GWL_WNDPROC, (LONG)WndProc);

   refCount++;
}

void Config(HWND parent){
   char cfgPath[MAX_PATH];
   strcpy(cfgPath, settingsPath);
   strcat(cfgPath, "menuicons.cfg");

   ShellExecute(parent, NULL, "notepad.exe", cfgPath, NULL, SW_SHOW);

   strcpy(cfgPath, settingsPath);
   strcat(cfgPath, "menus.cfg");
   ShellExecute(parent, NULL, "notepad.exe", cfgPath, NULL, SW_SHOW);
}

void Quit(){
   if (hImageList)
      ImageList_Destroy(hImageList);
}

void DoMenu(HMENU menu, char *param){
   // only do this the first time
   if (refCount == 0) {
      menus.push_back(menu);
      if (*param) {
         HINSTANCE plugin = LoadLibrary(param);
         if (plugin) {
            SetOwnerDrawn(menu, plugin);
            FreeLibrary(plugin);
            return;
         }
      }
      SetOwnerDrawn(menu, NULL);
   }
}

typedef int (*DRAWBITMAPPROC)(DRAWITEMSTRUCT *dis);

typedef struct {
   long version; // this is just to differentiate between our ownerdraw menus and someone else's menus
   void *data;
   DRAWBITMAPPROC DrawBitmap;
} MenuDataT;

int DrawBitmap(DRAWITEMSTRUCT *dis) {
	BmpMapT::iterator bmpMapIt;
	bmpMapIt = bmpMap.find(dis->itemID);

	// Load the corresponding bitmap
	if (bmpMapIt != bmpMap.end()){
      int top = (dis->rcItem.bottom - dis->rcItem.top - BMP_HEIGHT) / 2;
      top += dis->rcItem.top;

      if (dis->itemState & ODS_GRAYED)
         ImageList_DrawEx(hImageList, bmpMapIt->second, dis->hDC, dis->rcItem.left+1, top, 0, 0, CLR_NONE, GetSysColor(COLOR_3DFACE), ILD_BLEND  | ILD_TRANSPARENT);

      else if (dis->itemState & ODS_SELECTED)
         ImageList_Draw(hImageList, bmpMapIt->second, dis->hDC, dis->rcItem.left+1, top, ILD_TRANSPARENT);

      else
         ImageList_Draw(hImageList, bmpMapIt->second, dis->hDC, dis->rcItem.left+1, top, ILD_TRANSPARENT);

      return LEFT_SPACE;
	}
   else if (dis->itemState & ODS_CHECKED) {
      if (dis->itemState & ODS_SELECTED)
         DrawCheckMark(dis->hDC, dis->rcItem.left+6, dis->rcItem.top+5, GetSysColor(COLOR_HIGHLIGHTTEXT));
      else
         DrawCheckMark(dis->hDC, dis->rcItem.left+6, dis->rcItem.top+5, GetSysColor(COLOR_MENUTEXT));
   }

   return 0;
}

void DrawCheckMark(HDC pDC,int x,int y,COLORREF color) {
   SetPixel(pDC, x,   y+2, color);
   SetPixel(pDC, x,   y+3, color);
   SetPixel(pDC, x,   y+4, color);

   SetPixel(pDC, x+1, y+3, color);
   SetPixel(pDC, x+1, y+4, color);
   SetPixel(pDC, x+1, y+5, color);

   SetPixel(pDC, x+2, y+4, color);
   SetPixel(pDC, x+2, y+5, color);
   SetPixel(pDC, x+2, y+6, color);

   SetPixel(pDC, x+3, y+3, color);
   SetPixel(pDC, x+3, y+4, color);
   SetPixel(pDC, x+3, y+5, color);

   SetPixel(pDC, x+4, y+2, color);
   SetPixel(pDC, x+4, y+3, color);
   SetPixel(pDC, x+4, y+4, color);

   SetPixel(pDC, x+5, y+1, color);
   SetPixel(pDC, x+5, y+2, color);
   SetPixel(pDC, x+5, y+3, color);

   SetPixel(pDC, x+6, y,   color);
   SetPixel(pDC, x+6, y+1, color);
   SetPixel(pDC, x+6, y+2, color);
}

char *GetMaxTab(HMENU menu){
	MENUITEMINFO mmi;
	mmi.cbSize = sizeof(mmi);

	int maxChars = 0;
   char *maxTab = NULL;
	int state;
	char *string;
	char *tab;

	int count = ::GetMenuItemCount(menu);
	int i;
	for (i=0; i<count; i++) {

		state = ::GetMenuState(menu, i, MF_BYPOSITION);

		if (state & MF_OWNERDRAW){
			mmi.fMask = MIIM_DATA;
			::GetMenuItemInfo(menu, i, true, &mmi);

			MenuDataT *mdt = (MenuDataT *)mmi.dwItemData;
			if (mdt){
				string = (char *)mdt->data;

				tab = strchr(string, '\t');

				if (tab) {
					if (strlen(tab) > maxChars) {
                  maxChars = strlen(tab);
						maxTab = tab+1;
					}
				}
			}
		}
	}
	return maxTab;
}

void DrawMenuItem(DRAWITEMSTRUCT *dis) {

	HMENU menu = (HMENU)dis->hwndItem;

	// make sure itemID is a valid int
	//dis->itemID = LOWORD(dis->itemID);
	MenuDataT *mdt = (MenuDataT *)dis->itemData;
   char *string = (char *)mdt->data;

	BOOL hasBitmap = false;

	// Draw the highlight rectangle
	SetBkMode(dis->hDC, TRANSPARENT);
	if (dis->itemState & ODS_SELECTED) {
		FillRect(dis->hDC, &dis->rcItem, GetSysColorBrush(COLOR_HIGHLIGHT));
		SetTextColor(dis->hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));
	}
	else {
		FillRect(dis->hDC, &dis->rcItem, GetSysColorBrush(COLOR_3DFACE));
	}

   if (mdt->DrawBitmap){
      int space = mdt->DrawBitmap(dis);
      if (space>0) {
         dis->rcItem.left += space;
         hasBitmap = true;
      }
   }

	if (!hasBitmap){
		dis->rcItem.left += LEFT_SPACE;
	}

   dis->rcItem.left += 4;

   RECT rcTab;

	char *tab = strrchr(string, '\t');
	int itemLen, tabLen;
	if (tab) {
		itemLen = tab - string;
      tab++;
		tabLen = strlen(tab);

      char *maxTab = GetMaxTab(menu);

      rcTab.top = dis->rcItem.top;
      rcTab.left = 0;
      if (maxTab){
         DrawText(dis->hDC, maxTab, strlen(maxTab), &rcTab, DT_SINGLELINE | DT_CALCRECT );
      }
      rcTab.left = dis->rcItem.right - rcTab.right - BMP_WIDTH;
      rcTab.right = dis->rcItem.right;
      rcTab.bottom = dis->rcItem.bottom;
	}
	else {
		itemLen = strlen(string);
		tabLen = 0;
	}
	if (dis->itemState & ODS_GRAYED) {
		// setup pen to draw selected, grayed text
		if (dis->itemState & ODS_SELECTED) {
			SetTextColor(dis->hDC, GetSysColor(COLOR_3DFACE));
		}
		// Draw shadow for unselected grayed items
		else {
			dis->rcItem.left += 1;
			dis->rcItem.top += 1;

			SetTextColor(dis->hDC, GetSysColor(COLOR_HIGHLIGHTTEXT));

			DrawText(dis->hDC, string, itemLen, &dis->rcItem, DT_SINGLELINE | DT_VCENTER | DT_NOCLIP );
			if (tab) {
				DrawText(dis->hDC, tab, tabLen, &rcTab, DT_SINGLELINE | DT_VCENTER | DT_NOCLIP );
			}

			dis->rcItem.left -= 1;
			dis->rcItem.top -= 1;
      
			// set pen to draw normal text colour
			SetTextColor(dis->hDC, GetSysColor(COLOR_GRAYTEXT));
		}
	}

	DrawText(dis->hDC, string, itemLen, &dis->rcItem, DT_SINGLELINE | DT_VCENTER | DT_NOCLIP );
	if (tab) {
		DrawText(dis->hDC, tab, tabLen, &rcTab, DT_SINGLELINE | DT_VCENTER | DT_NOCLIP );
	}
}

void MeasureMenuItem(MEASUREITEMSTRUCT *mis, HDC hDC) {
   MenuDataT *menuData = (MenuDataT *)mis->itemData;
   
   NONCLIENTMETRICS ncm = {0};
   ncm.cbSize = sizeof(ncm);
   SystemParametersInfo(SPI_GETNONCLIENTMETRICS,0,(PVOID)&ncm,FALSE);

   HFONT font;
   font = CreateFontIndirect(&ncm.lfMenuFont);
   HFONT oldFont = (HFONT)SelectObject(hDC, font); 

   BOOL bWin98 = FALSE;
   // check for Win98/WinMe (dwMinorVersion = 90 for WinME)
   OSVERSIONINFO info;
   info.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );
   if (GetVersionEx(&info)) {
      if ( (info.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) && (info.dwMinorVersion > 0) ) {
         bWin98 = TRUE;
      }
   }

   char *string = (char *)menuData->data;
   SIZE size;
   int tabWidth = 0;
   char *tab = strrchr(string, '\t');
   if (tab) {
      // Since we are converting string menus to ownerdrawn,
      // Windows98 will automatically adjust the size we return to compensate for the
      // accelerator string, so we shouldn't include it in our measurements
      if (bWin98) {
         int tabWidth = 0;
         GetTextExtentPoint32(hDC, string, tab-string, &size);
         size.cx += 8;
      }
      else {
         size.cx = LOWORD( GetTabbedTextExtent(hDC, string, strlen(string), 0, NULL) );
      }
   }
   else {
      GetTextExtentPoint32(hDC, string, strlen(string), &size);
      size.cx += 28;
   }

   mis->itemWidth = size.cx;
   mis->itemHeight = GetSystemMetrics(SM_CYMENUSIZE);
   if (mis->itemHeight < 18)
      mis->itemHeight = 18;

   SelectObject(hDC, oldFont);
   DeleteObject(font);
}

void UnSetOwnerDrawn(HMENU menu){
   MENUITEMINFO mmi;
   mmi.cbSize = sizeof(mmi);

   MenuDataT *mdt;
   int state;

   int i;
   int count = ::GetMenuItemCount(menu);
   for (i=0; i<count; i++) {
      state = ::GetMenuState(menu, i, MF_BYPOSITION);
      if (state & MF_POPUP){
         UnSetOwnerDrawn(GetSubMenu(menu, i));
      }
      if (state & MF_OWNERDRAW) {
         mmi.fMask = MIIM_DATA;
         ::GetMenuItemInfo(menu, i, true, &mmi);

         mdt = (MenuDataT *)mmi.dwItemData;
         if (mdt && mdt->version == BMP_MENU_VERSION) {

            // remove our mdt structure from the menu's dwItemData
            MENUITEMINFO mmiNew;
            mmiNew.cbSize = sizeof ( MENUITEMINFO );
            mmi.dwItemData = NULL;
            mmi.fMask = MIIM_DATA;
            SetMenuItemInfo(menu, i, TRUE, &mmi);

            delete [] (char *)mdt->data;
            delete mdt;
         }
      }
   }
}

// this oddly named function converts a menu of type MF_STRING to MF_OWNERDRAW
void StringToOwnerDrawn(HMENU menu, int i, UINT flags, UINT id, DRAWBITMAPPROC DrawProc){
   MENUITEMINFO mmi;
   mmi.cbSize = sizeof(mmi);

   mmi.fMask = MIIM_TYPE;
   mmi.cch = 0;
   mmi.dwTypeData = NULL;
   GetMenuItemInfo(menu, i, true, &mmi);
   mmi.cch ++;
   mmi.dwTypeData = new char[mmi.cch];
   GetMenuItemInfo(menu, i, true, &mmi);

   MenuDataT *mData = new MenuDataT;
   mData->version = BMP_MENU_VERSION;
   mData->data = mmi.dwTypeData;
   mData->DrawBitmap = DrawProc;

   ModifyMenu(menu, i, MF_BYPOSITION | MF_OWNERDRAW | flags, id, (LPCTSTR)(void *)mData);
}

void SetOwnerDrawn(HMENU menu, HINSTANCE plugin){
   DRAWBITMAPPROC DrawProc = NULL;
   if (plugin)
      DrawProc = (DRAWBITMAPPROC)GetProcAddress(plugin, "DrawBitmap");
   if (!DrawProc)
      DrawProc = DrawBitmap;

   int state;

   int i;
   int count = GetMenuItemCount(menu);
   for (i=0; i<count; i++){
      state = GetMenuState(menu, i, MF_BYPOSITION);
      if (state & MF_POPUP) {
         HMENU subMenu = GetSubMenu(menu, i);
         SetOwnerDrawn(subMenu, plugin);
         if (plugin) {
            StringToOwnerDrawn(menu, i, MF_POPUP, (UINT)subMenu, DrawProc);
         }
      }
      else if (state == 0) {
         StringToOwnerDrawn(menu, i, 0, GetMenuItemID(menu, i), DrawProc);
      }
   }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam){

   if (message == WM_MEASUREITEM) {
      MEASUREITEMSTRUCT *mis = (MEASUREITEMSTRUCT *)lParam;
      MenuDataT * mdt = (MenuDataT *)mis->itemData;
      if (mis->CtlType == ODT_MENU && mdt->version == BMP_MENU_VERSION) {
         HDC hDC = GetWindowDC(hWnd);
         MeasureMenuItem(mis, hDC);
         ReleaseDC(hWnd, hDC);
         return true;
      }
   }
   else if (message == WM_DRAWITEM){
      DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lParam;
      MenuDataT * mdt = (MenuDataT *)dis->itemData;
      if (dis->CtlType == ODT_MENU && mdt->version == BMP_MENU_VERSION){
         DrawMenuItem(dis);
         return true;
      }
   }
   else if (message == WM_DESTROY){
      refCount--;
      if (refCount == 0){
         while(menus.size()){
            UnSetOwnerDrawn(menus.front());
            menus.erase(menus.begin());
         }
      }
   }
   return CallWindowProc(KMeleonWndProc, hWnd, message, wParam, lParam);
}

// so it doesn't munge the function name
extern "C" {

KMELEON_PLUGIN kmeleonPlugin *GetKmeleonPlugin() {
  return &kPlugin;
}

}