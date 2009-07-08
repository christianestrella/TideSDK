/**
 * Appcelerator Titanium - licensed under the Apache Public License 2
 * see LICENSE in the root folder for details on the license.
 * Copyright (c) 2009 Appcelerator, Inc. All Rights Reserved.
 */
#include "../ui_module.h"
#define _WINSOCKAPI_

using std::vector;
namespace ti
{
	UINT Win32UIBinding::nextItemId = NEXT_ITEM_ID_BEGIN;
	vector<HICON> Win32UIBinding::loadedICOs;
	vector<HBITMAP> Win32UIBinding::loadedBMPs;

	Win32UIBinding::Win32UIBinding(Module *uiModule, Host *host) :
		UIBinding(host),
		evaluator(host),
		menu(0),
		contextMenu(0),
		iconPath("")
	{
	
		if (!Win32UIBinding::IsWindowsXP())
		{
			// Use Activation Context API by pointing it at the WebKit
			// manifest. This should allos us to load our COM object.
			ACTCTX actctx; 
			ZeroMemory(&actctx, sizeof(actctx)); 
			actctx.cbSize = sizeof(actctx); 

			std::string source = host->GetRuntimePath();
			source = FileUtils::Join(source.c_str(), "WebKit.manifest", NULL);
			actctx.lpSource = source.c_str(); // Path to the Side-by-Side Manifest File 
			this->pActCtx = CreateActCtx(&actctx); 
			ActivateActCtx(pActCtx, &this->lpCookie);
		}
		
		INITCOMMONCONTROLSEX InitCtrlEx;

		InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
		InitCtrlEx.dwICC = 0x00004000; //ICC_STANDARD_CLASSES;
		InitCommonControlsEx(&InitCtrlEx);
		
		InitCurl(uiModule);
		addScriptEvaluator(&evaluator);
	}
	
	void Win32UIBinding::InitCurl(Module* uiModule)
	{
		std::string pemPath = FileUtils::Join(uiModule->GetPath().c_str(), "cacert.pem", NULL);
		
		// use _putenv since webkit uses stdlib's getenv which is incompatible with GetEnvironmentVariable
		std::string var = "CURL_CA_BUNDLE_PATH=" + pemPath;
		_putenv(var.c_str());

		curl_register_local_handler(&CurlTiURLHandler);
		curl_register_local_handler(&CurlAppURLHandler);
	}

	Win32UIBinding::~Win32UIBinding()
	{
		if (!Win32UIBinding::IsWindowsXP())
		{
   			DeactivateActCtx(0, this->lpCookie); 
			ReleaseActCtx(this->pActCtx);
		}
	}

	bool Win32UIBinding::IsWindowsXP()
	{
		OSVERSIONINFO osVersion;
		osVersion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		::GetVersionEx(&osVersion);
		return osVersion.dwMajorVersion == 5;
	}

	SharedUserWindow Win32UIBinding::CreateWindow(
		WindowConfig* config,
		SharedUserWindow& parent)
	{
		UserWindow* w = new Win32UserWindow(config, parent);
		return w->GetSharedPtr();
	}

	void Win32UIBinding::ErrorDialog(std::string msg)
	{
		MessageBox(NULL,msg.c_str(),"Application Error",MB_OK|MB_ICONERROR|MB_SYSTEMMODAL);
		UIBinding::ErrorDialog(msg);
	}

	SharedMenu Win32UIBinding::CreateMenu()
	{
		return new Win32Menu();
	}

	SharedMenuItem Win32UIBinding::CreateMenuItem(
		std::string label, SharedKMethod callback, std::string iconURL)
	{
		return new Win32MenuItem(MenuItem::NORMAL, label, callback, iconURL);
	}

	SharedMenuItem Win32UIBinding::CreateSeparatorMenuItem()
	{
		return new Win32MenuItem(MenuItem::SEPARATOR, std::string(), NULL, std::string());
	}

	SharedMenuItem Win32UIBinding::CreateCheckMenuItem(
		std::string label, SharedKMethod callback)
	{
		return new Win32MenuItem(MenuItem::CHECK, label, callback, std::string());
	}

	void Win32UIBinding::SetMenu(SharedMenu newMenu)
	{
		this->menu = newMenu.cast<Win32Menu>();

		// Notify all windows that the app menu has changed.
		std::vector<SharedUserWindow>& windows = this->GetOpenWindows();
		std::vector<SharedUserWindow>::iterator i = windows.begin();
		while (i != windows.end())
		{
			SharedPtr<Win32UserWindow> wuw = (*i++).cast<Win32UserWindow>();
			if (!wuw.isNull())
				wuw->AppMenuChanged();
		}
	}

	void Win32UIBinding::SetContextMenu(SharedMenu newMenu)
	{
		this->contextMenu = newMenu.cast<Win32Menu>();
	}

	void Win32UIBinding::SetIcon(std::string& iconPath)
	{
		this->iconPath = iconPath;
		printf("new icon path: %s\n", iconPath.c_str());

		// Notify all windows that the app icon has changed
		// TODO this kind of notification should really be placed in UIBinding..
		std::vector<SharedUserWindow>& windows = this->GetOpenWindows();
		std::vector<SharedUserWindow>::iterator i = windows.begin();
		while (i != windows.end())
		{
			SharedPtr<Win32UserWindow> wuw = (*i++).cast<Win32UserWindow>();
			if (!wuw.isNull())
				wuw->AppIconChanged();
		}
	}

	SharedPtr<TrayItem> Win32UIBinding::AddTray(
		SharedString icon_path,
		SharedKMethod cb)
	{
		SharedPtr<TrayItem> trayItem = new Win32TrayItem(icon_path, cb);
		return trayItem;
	}

	long Win32UIBinding::GetIdleTime()
	{
		LASTINPUTINFO lii;
		memset(&lii, 0, sizeof(lii));

		lii.cbSize = sizeof(lii);
		::GetLastInputInfo(&lii);

		DWORD currentTickCount = GetTickCount();
		long idleTicks = currentTickCount - lii.dwTime;

		return (int)idleTicks;
	}

	SharedMenu Win32UIBinding::GetMenu()
	{
		return this->menu;
	}

	SharedMenu Win32UIBinding::GetContextMenu()
	{
		return this->contextMenu;
	}

	std::string& Win32UIBinding::GetIcon()
	{
		return this->iconPath;
	}

	/*static*/
	HBITMAP Win32UIBinding::LoadImageAsBitmap(std::string& path, int sizeX, int sizeY)
	{
		std::string lcpath = path;
		std::transform(lcpath.begin(), lcpath.end(), lcpath.begin(), tolower);
		std::string ext = lcpath.substr(lcpath.size() - 4, 4);
		UINT flags = LR_DEFAULTSIZE | LR_LOADFROMFILE |
			LR_LOADTRANSPARENT | LR_CREATEDIBSECTION;


		HBITMAP h = 0;
		if (ext ==  ".ico") {
			printf("loading bmp: %s as ico\n", path.c_str());
			HICON hicon = (HICON) LoadImageA(
				NULL, path.c_str(), IMAGE_ICON, sizeX, sizeY, LR_LOADFROMFILE);
			h = Win32UIBinding::IconToBitmap(hicon, sizeX, sizeY);
			DestroyIcon(hicon);

		} else if (ext == ".bmp") {
			printf("loading bmp: %s as bmp\n", path.c_str());
			h = (HBITMAP) LoadImageA(
				NULL, path.c_str(), IMAGE_BITMAP, sizeX, sizeY, flags);

		} else if (ext == ".png") {
			printf("loading bmp: %s as png\n", path.c_str());
			h = LoadPNGAsBitmap(path, sizeX, sizeY);
		}

		loadedBMPs.push_back(h);
		return h;
	}

	/*static*/
	HICON Win32UIBinding::LoadImageAsIcon(std::string& path, int sizeX, int sizeY)
	{
		std::string lcpath = path;
		std::transform(lcpath.begin(), lcpath.end(), lcpath.begin(), tolower);
		std::string ext = lcpath.substr(lcpath.size() - 4, 4);
		UINT flags = LR_DEFAULTSIZE | LR_LOADFROMFILE |
			LR_LOADTRANSPARENT | LR_CREATEDIBSECTION;

		HICON h = 0;
		if (ext ==  ".ico") {
			printf("loading ico: %s as ico\n", path.c_str());
			h = (HICON) LoadImageA(
				NULL, path.c_str(), IMAGE_ICON, sizeX, sizeY, LR_LOADFROMFILE);

		} else if (ext == ".bmp") {
			printf("loading ico: %s as bmp\n", path.c_str());
			HBITMAP bitmap = (HBITMAP) LoadImageA(
				NULL, path.c_str(), IMAGE_BITMAP, sizeX, sizeY, flags);
			h = Win32UIBinding::BitmapToIcon(bitmap, sizeX, sizeY);
			DeleteObject(bitmap);

		} else if (ext == ".png") {
			printf("loading ico: %s as png\n", path.c_str());
			HBITMAP bitmap = LoadPNGAsBitmap(path, sizeX, sizeY);
			h = Win32UIBinding::BitmapToIcon(bitmap, sizeX, sizeY);
			DeleteObject(bitmap);
		}

		loadedICOs.push_back(h);
		return (HICON) h;
	}
	
	/*static*/
	HICON Win32UIBinding::BitmapToIcon(HBITMAP bitmap, int sizeX, int sizeY)
	{
		if (!bitmap)
			return 0;

		if (sizeX == 0)
			sizeX = GetSystemMetrics(SM_CYSMICON);
		if (sizeY == 0)
			sizeY = GetSystemMetrics(SM_CYSMICON);

		HBITMAP bitmapMask = CreateCompatibleBitmap(GetDC(NULL), sizeX, sizeY);
		ICONINFO iconInfo = {0};
		iconInfo.fIcon = TRUE;
		iconInfo.hbmMask = bitmapMask;
		iconInfo.hbmColor = bitmap;
		HICON icon = CreateIconIndirect(&iconInfo);
		DeleteObject(bitmapMask);
		
		return icon;
	}

	/*static*/
	HBITMAP Win32UIBinding::IconToBitmap(HICON icon, int sizeX, int sizeY)
	{
		if (!icon)
			return 0;

		if (sizeX == 0)
			sizeX = GetSystemMetrics(SM_CYSMICON);
		if (sizeY == 0)
			sizeY = GetSystemMetrics(SM_CYSMICON);

		HDC hdc = GetDC(NULL);
		HDC hdcmem = CreateCompatibleDC(hdc);
		HBITMAP bitmap = CreateCompatibleBitmap(hdc, sizeX, sizeY);
		HBITMAP holdbitmap = (HBITMAP) SelectObject(hdcmem, bitmap);

		RECT rect = { 0, 0, sizeX, sizeY };
		SetBkColor(hdcmem, RGB(255, 255, 255));
		ExtTextOut(hdcmem, 0, 0, ETO_OPAQUE, &rect, NULL, 0, NULL);
		DrawIconEx(hdcmem, 0, 0, icon, sizeX, sizeY, 0, NULL, DI_NORMAL);

		SelectObject(hdc, holdbitmap);
		DeleteDC(hdcmem);

		return bitmap;
	}

	/*static*/
	HBITMAP Win32UIBinding::LoadPNGAsBitmap(std::string& path, int sizeX, int sizeY)
	{
		if (sizeX == 0)
			sizeX = GetSystemMetrics(SM_CYSMICON);
		if (sizeY == 0)
			sizeY = GetSystemMetrics(SM_CYSMICON);

		cairo_surface_t* insurface = cairo_image_surface_create_from_png(path.c_str());
		cairo_t* pngcr = cairo_create(insurface);
		if (cairo_status(pngcr) != CAIRO_STATUS_SUCCESS) {
			return 0;
		}

		cairo_surface_t* pngsurface = ScaleCairoSurface(insurface, sizeX, sizeY);

		HDC hdc = GetDC(NULL);
		HDC hdcmem = CreateCompatibleDC(hdc);
		HBITMAP bitmap = CreateCompatibleBitmap(hdc, sizeX, sizeY);
		HBITMAP holdbitmap = (HBITMAP) SelectObject(hdcmem, bitmap);

		cairo_surface_t* w32surface = cairo_win32_surface_create(hdcmem);
		cairo_t *cr = cairo_create(w32surface);
		cairo_set_source_surface(cr, pngsurface, 0, 0);
		cairo_rectangle(cr, 0, 0, sizeX, sizeY);
		cairo_fill(cr);

		cairo_destroy(cr);
		cairo_surface_destroy(w32surface);
		cairo_surface_destroy(pngsurface);
		cairo_destroy(pngcr);
		cairo_surface_destroy(insurface);
		
		SelectObject(hdc, holdbitmap);
		DeleteDC(hdcmem);
		return bitmap;
	}

	cairo_surface_t* Win32UIBinding::ScaleCairoSurface(
		cairo_surface_t *oldSurface, int newWidth, int newHeight)
	{
		int oldWidth = cairo_image_surface_get_width(oldSurface);
		int oldHeight = cairo_image_surface_get_height(oldSurface);

		cairo_surface_t *newSurface = cairo_surface_create_similar(
			oldSurface, CAIRO_CONTENT_COLOR_ALPHA, newWidth, newHeight);
		cairo_t *cr = cairo_create(newSurface);
		
		/* Scale *before* setting the source surface (1) */
		cairo_scale(cr, (double) newWidth / oldWidth, (double) newHeight / oldHeight);
		cairo_set_source_surface(cr, oldSurface, 0, 0);
		
		/* To avoid getting the edge pixels blended with 0 alpha, which would 
		 * occur with the default EXTEND_NONE. Use EXTEND_PAD for 1.2 or newer (2) */
		cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REFLECT); 
		
		 /* Replace the destination with the source instead of overlaying */
		cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
		
		/* Do the actual drawing */
		cairo_paint(cr);
		cairo_destroy(cr);
		
		return newSurface;
	 }


	/*static*/
	void Win32UIBinding::ReleaseImage(HANDLE handle)
	{
		vector<HICON>::iterator i = loadedICOs.begin();
		while (i != loadedICOs.end()) {
			if (*i == handle) {
				DestroyIcon(*i);
				return;
			} else {
				i++;
			}
		}

		vector<HBITMAP>::iterator i2 = loadedBMPs.begin();
		while (i2 != loadedBMPs.end()) {
			if (*i2 == handle) {
				::DeleteObject(*i2);
				return;
			} else {
				i2++;
			}
		}

	}

}
