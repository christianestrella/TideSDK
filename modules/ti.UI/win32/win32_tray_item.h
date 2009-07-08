/**
 * Appcelerator Titanium - licensed under the Apache Public License 2
 * see LICENSE in the root folder for details on the license.
 * Copyright (c) 2009 Appcelerator, Inc. All Rights Reserved.
 */
#ifndef TI_WIN32_TRAY_ITEM_H_
#define TI_WIN32_TRAY_ITEM_H_
#include <windows.h>
#include <shellapi.h>
namespace ti
{

class Win32TrayItem: public TrayItem
{
	public:
	Win32TrayItem(SharedString iconPath, SharedKMethod cb);
	virtual ~Win32TrayItem();

	void SetIcon(SharedString iconPath);
	void SetMenu(SharedMenu menu);
	void SetHint(SharedString hint);
	void Remove();
	void ShowTrayMenu();

	void HandleRightClick();
	void HandleLeftClick();
	UINT GetId();

	static void HandleClickEvent(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	private:
	SharedKMethod callback;
	HMENU oldNativeMenu;
	NOTIFYICONDATA* trayIconData;
	static std::vector<SharedPtr<Win32TrayItem> > trayItems;
};

}

#endif /* TI_WIN32_TRAY_ITEM_H_ */
