#include <Windows.h>
#include <CommCtrl.h>
#include <tchar.h>
#include <Uxtheme.h>
#include <TlHelp32.h>
#include <Shlwapi.h>
#include <memory>
#include <Psapi.h>
#include <vector>
#include <algorithm>
using namespace std;

#include "resource.h"
#include "icon.h"
#include "inj.h"

#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif


IconManager* iconMngr;
HMENU hMenu;
HWND hWindow;

vector<JoyconIntercepter> JoyStopList;

int InterceptByName(LPTSTR name, LPTSTR path)
{
	HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (hSnapShot == INVALID_HANDLE_VALUE)
	{
		return -1;
	}

	PROCESSENTRY32 process;

	process.dwSize = sizeof(process);

	Process32First(hSnapShot, &process);

	do {
		TCHAR buf[512];
		_tcscpy_s(buf, process.szExeFile);	// TO FIX
		PathStripPath(buf);

		if (_tcscmp(buf, name) != 0)
		{
			continue;
		}

		if (_tcsstr(process.szExeFile, path) == NULL)
		{
			continue;
		}

		JoyStopList.push_back(JoyconIntercepter(process.th32ProcessID));

		JoyStopList.back().InterceptJoycon();
	} while (Process32Next(hSnapShot, &process));
	return 0;
}

int StopInterceptByName(LPTSTR name, LPTSTR path)
{
	for (auto ji = JoyStopList.begin(); ji != JoyStopList.end(); ji++)
	{
		HANDLE hPr = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, ji->GetPid());
		if (hPr == NULL)
		{
			continue;
		}

		TCHAR fn[512];
		GetProcessImageFileName(hPr, fn, sizeof(fn) / sizeof(TCHAR));
		CloseHandle(hPr);

		if (_tcsstr(fn, path) == NULL)
		{
			continue;
		}

		ji->AcceptJoycon();
	}

	
	JoyStopList.erase(
		remove_if(JoyStopList.begin(), JoyStopList.end(),
			[](JoyconIntercepter &ji) ->bool {
				return !ji.isActive();
			}
		),
		JoyStopList.end()
	);
	return 0;
}

void UpdateProcessList(HWND ListBox)
{
	HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (hSnapShot == INVALID_HANDLE_VALUE)
	{
		return;
	}

	ListView_DeleteAllItems(ListBox);

	HIMAGELIST OldLIconList = ListView_GetImageList(ListBox, LVSIL_NORMAL);
	HIMAGELIST OldSIconList = ListView_GetImageList(ListBox, LVSIL_SMALL);

	HIMAGELIST LIconList = ImageList_Create(32, 32, ILC_COLOR4 | ILC_MASK, 8, 1024);
	HIMAGELIST SIconList = ImageList_Create(16, 16, ILC_COLOR4 | ILC_MASK, 8, 1024);

	ListView_SetImageList(ListBox, LIconList, LVSIL_NORMAL);
	ListView_SetImageList(ListBox, SIconList, LVSIL_SMALL);

	ImageList_Destroy(OldLIconList);
	ImageList_Destroy(OldSIconList);

	PROCESSENTRY32 process;

	process.dwSize = sizeof(process);

	DWORD cPid = GetCurrentProcessId();

	Process32First(hSnapShot, &process);

	do {
		if (process.th32ParentProcessID == cPid ||
			process.th32ProcessID == cPid)
		{
			continue;
		}

		HANDLE hPr = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, process.th32ProcessID);
		CloseHandle(hPr);

		if (hPr == NULL)
		{
			continue;
		}

		LVITEM item;
		item.mask = LVIF_TEXT;
		item.pszText = const_cast<LPTSTR>(_T(""));
		item.iItem = 0;
		item.iSubItem = 0;

		int index = ListView_InsertItem(ListBox, &item);

		{
			HICON LIcon, SIcon;

			ExtractIconEx(process.szExeFile, 0, &LIcon, &SIcon, 1);

			ImageList_AddIcon(LIconList, LIcon);
			int ImgIndex = ImageList_AddIcon(SIconList, SIcon);

			LVITEM item;
			item.mask = LVIF_IMAGE;
			item.iImage = ImgIndex;
			item.iItem = index;
			item.iSubItem = 0;

			ListView_SetItem(ListBox, &item);
		}
		{
			size_t len = _tcsclen(process.szExeFile);
			std::unique_ptr<TCHAR>buf(new TCHAR[len + 1]);

			_tcscpy_s(buf.get(), len + 1, process.szExeFile);

			PathStripPath(buf.get());

			LVITEM item;
			item.mask = LVIF_TEXT;
			item.pszText = buf.get();
			item.iItem = index;
			item.iSubItem = 0;

			ListView_SetItem(ListBox, &item);
		}
		{
			TCHAR buf[16];

			_itot_s(process.th32ProcessID, buf, 10);

			LVITEM item;
			item.mask = LVIF_TEXT;
			item.pszText = buf;
			item.iItem = index;
			item.iSubItem = 1;

			ListView_SetItem(ListBox, &item);
		}

	} while (Process32Next(hSnapShot, &process));
}

INT_PTR CALLBACK DlgProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg)
	{
	case WM_INITDIALOG:
		{
			HWND hList = GetDlgItem(hWnd, IDC_PROCESSLIST);

			{
				LVCOLUMN col;
				col.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
				col.fmt = LVCFMT_LEFT;
				col.cx = 180;
				col.pszText = const_cast<LPTSTR>(_T("FileName"));
				ListView_InsertColumn(hList, 0, &col);
			}
			{
				LVCOLUMN col;
				col.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
				col.fmt = LVCFMT_LEFT;
				col.cx = 100;
				col.pszText = const_cast<LPTSTR>(_T("PID"));
				ListView_InsertColumn(hList, 1, &col);
			}

			SetWindowTheme(hList, L"Explorer", NULL);
			ListView_SetExtendedListViewStyle(hList, LVS_EX_FULLROWSELECT);

			UpdateProcessList(hList);


			HWND hListTg = GetDlgItem(hWnd, IDC_TARGETLIST);

			{
				LVCOLUMN col;
				col.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
				col.fmt = LVCFMT_LEFT;
				col.cx = 180;
				col.pszText = const_cast<LPTSTR>(_T("FileName"));
				ListView_InsertColumn(hListTg, 0, &col);
			}
			{
				LVCOLUMN col;
				col.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
				col.fmt = LVCFMT_LEFT;
				col.cx = 100;
				col.pszText = const_cast<LPTSTR>(_T("Path"));
				ListView_InsertColumn(hListTg, 1, &col);
			}

			SetWindowTheme(hListTg, L"Explorer", NULL);
			ListView_SetExtendedListViewStyle(hListTg, LVS_EX_FULLROWSELECT);

			//SetTimer(hWnd, 1, 1000, NULL);
			return TRUE;
		}
	//case WM_TIMER:
	//	UpdateProcessList(GetDlgItem(hWnd, IDC_PROCESSLIST));
	//	break;
	case WM_SHOWWINDOW:
		UpdateProcessList(GetDlgItem(hWnd, IDC_PROCESSLIST));
		break;
	case WM_CLOSE:
		ShowWindow(hWnd, SW_HIDE);
		break;
	case WM_COMMAND:
		{
			switch (LOWORD(wp))
			{
			case ID_MENU_EXIT:
				for (auto ji : JoyStopList)
				{
					ji.AcceptJoycon();
				}
				PostQuitMessage(0);
				break;
			case ID_MENU_SETTING:
				ShowWindow(hWnd, SW_SHOW);
				break;
			case IDC_BUTTONADD:
			{
				HWND hList = GetDlgItem(hWnd, IDC_TARGETLIST);
				TCHAR name[64], path[512];

				GetWindowText(GetDlgItem(hWnd, IDC_PROCESSNAME), name, sizeof(name) / sizeof(TCHAR));

				LVFINDINFO lii;
				lii.flags = LVFI_STRING;
				lii.psz = name;
				if (ListView_FindItem(hList, -1, &lii) != -1)
				{
					break;
				}

				LVITEM item;
				item.mask = LVIF_TEXT;
				item.pszText = name;
				item.iItem = 0;
				item.iSubItem = 0;

				int index = ListView_InsertItem(hList, &item);

				GetWindowText(GetDlgItem(hWnd, IDC_PROCESSPATH), path, sizeof(path) / sizeof(TCHAR));

				item.mask = LVIF_TEXT;
				item.pszText = path;
				item.iItem = index;
				item.iSubItem = 1;

				ListView_SetItem(hList, &item);

				InterceptByName(name, path);
			}
				break;
			case IDC_BUTTONREMOVE:
			{
				HWND hList = GetDlgItem(hWnd, IDC_TARGETLIST);
				int index = ListView_GetNextItem(hList, -1, LVNI_ALL | LVNI_SELECTED);

				TCHAR name[64], path[512];
				ListView_GetItemText(hList, index, 0, name, sizeof(name) / sizeof(TCHAR));
				ListView_GetItemText(hList, index, 1, path, sizeof(path) / sizeof(TCHAR));
				StopInterceptByName(name, path);

				ListView_DeleteItem(hList, index);
			}
				break;
			}
		}
		break;
	case WM_NOTIFY:
		{
			LPNMHDR nmh = reinterpret_cast<LPNMHDR>(lp);

			switch (nmh->idFrom)
			{
			case IDC_PROCESSLIST:
				switch (reinterpret_cast<LPNMLISTVIEW>(nmh)->hdr.code)
				{
				case LVN_ITEMCHANGED:
					{
						HWND hList = GetDlgItem(hWnd, IDC_PROCESSLIST);
						TCHAR buf[64];

						int index = ListView_GetNextItem(hList, -1, LVNI_ALL | LVNI_SELECTED);

						if (index == -1)
						{
							break;
						}

						ListView_GetItemText(hList, index, 0, buf, sizeof(buf) / sizeof(TCHAR));

						SetWindowText(GetDlgItem(hWnd, IDC_PROCESSNAME), buf);
						SetWindowText(GetDlgItem(hWnd, IDC_PROCESSPATH), buf);
					}
					break;
				}
				break;
			}
		}
		break;
	}
	if (iconMngr != NULL)
	{
		iconMngr->Msg(hWnd, msg, wp, lp);
	}
	return FALSE;
}

int IconMenuProc(UINT msg, int cx, int cy)
{
	switch (msg)
	{
	case WM_LBUTTONUP:
		break;
	case WM_RBUTTONUP:
		TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, cx, cy, 0, hWindow, NULL);
		break;
	}
	return 0;
}

int MessageLoop()
{
	MSG msg;
	BOOL bRet;
	while (bRet = GetMessage(&msg, NULL, 0, 0)) {
		if (bRet == -1) {
			break;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return (int)msg.wParam;
}

int WINAPI WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine,
	int nCmdShow
)
{
	JoyconIntercepter::Init();
	InitCommonControls();

	HWND hWnd;
	hWindow = hWnd = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DlgProc);
	if (hWnd == NULL)
	{
		return 0;
	}
	ShowWindow(hWnd, SW_HIDE);

	{
		HMENU hMenuBase = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MENU1));
		hMenu = GetSubMenu(hMenuBase, 0);
	}

	{
		HMENU hManuBase = LoadMenu(hInstance, MAKEINTRESOURCE(IDR_MENU1));
		HMENU hMenu = GetSubMenu(hManuBase, 0);

		iconMngr = new IconManager(hWnd);
		iconMngr->SetIcon(
			static_cast<HICON>(
				LoadImage(hInstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED)
				)
		);
		iconMngr->SetCallBack(IconMenuProc);
	}

	int result = MessageLoop();

	delete iconMngr;
	JoyconIntercepter::Cleanup();
	return result;
}