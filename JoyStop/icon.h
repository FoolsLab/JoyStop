#pragma once

#include <Windows.h>
#include "resource.h"

class IconManager
{
private:
	const static int wm_tasktray = WM_APP + 0x1000;
	const static int uid_tasktray = 1;

	HWND hWnd;

	using CallBackFunc = int(*)(UINT msg, int cursorX, int cursorY);
	CallBackFunc actProc;

	GUID guid;

public:
	void SetIcon(HICON hIcon)
	{
		NOTIFYICONDATA nid;

		nid.cbSize = sizeof(nid);
		nid.uID = uid_tasktray;
		nid.hWnd = hWnd;
		nid.uFlags = (NIF_ICON | NIF_MESSAGE | NIF_GUID);
		nid.hIcon = hIcon;
		nid.uCallbackMessage = wm_tasktray;
		nid.guidItem = guid;

		Shell_NotifyIcon(NIM_MODIFY, &nid);
	}
	void SetCallBack(CallBackFunc proc)
	{
		actProc = proc;
	}

	IconManager(HWND hWindow)
	{
		CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
		CoCreateGuid(&guid);
		CoUninitialize();

		this->hWnd = hWindow;

		NOTIFYICONDATA nid;

		nid.cbSize = sizeof(nid);
		nid.uID = uid_tasktray;
		nid.hWnd = hWnd;
		nid.uFlags = (NIF_ICON | NIF_MESSAGE | NIF_GUID);
		nid.hIcon = static_cast<HICON>( LoadImage(0, MAKEINTRESOURCE(IDI_APPLICATION), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED) );
		nid.uCallbackMessage = wm_tasktray;
		nid.guidItem = guid;
		
		Shell_NotifyIcon(NIM_ADD, &nid);
	}
	~IconManager()
	{
		NOTIFYICONDATA nid;

		nid.cbSize = sizeof(nid);
		nid.hWnd = hWnd;
		nid.uID = uid_tasktray;
		nid.guidItem = guid;
		nid.uFlags = (NIF_GUID);

		Shell_NotifyIcon(NIM_DELETE, &nid);
	}

	int Msg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (actProc == NULL)
		{
			return 0;
		}
		switch (msg)
		{
		case wm_tasktray:
			NOTIFYICONIDENTIFIER nii;
			nii.cbSize = sizeof(nii);
			nii.hWnd = hWnd;
			nii.uID = uid_tasktray;
			nii.guidItem = guid;

			RECT iconRect;
			Shell_NotifyIconGetRect(&nii, &iconRect);
			
			POINT point;
			GetCursorPos(&point);

			if (
				iconRect.left < point.x &&
				iconRect.right > point.x &&
				iconRect.top < point.y &&
				iconRect.bottom > point.y)
			{
				actProc(static_cast<UINT>(lParam), point.x, point.y);
			}
			break;
		}
		return 0;
	}

private:

};
