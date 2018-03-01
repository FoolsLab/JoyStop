#pragma once

#include <Windows.h>
#include <tchar.h>
#include <Shlwapi.h>
#include "resource.h"

class JoyconIntercepter
{
private:
	class SubPe
	{
		HANDLE hFile;
		TCHAR fPath[512];
	public:
		SubPe(int name, LPCTSTR fn)
		{
			GetTempPath(sizeof(fPath) / sizeof(TCHAR), fPath);
			PathCombine(fPath, fPath, fn);

			HMODULE hm = GetModuleHandle(NULL);
			HRSRC hr = FindResource(hm, MAKEINTRESOURCE(name), _T("SUBPEFILE"));

			LPVOID fileDat = LockResource(LoadResource(hm, hr));

			DWORD szFile = SizeofResource(hm, hr);
			DWORD szw;

			hFile = CreateFile(fPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

			WriteFile(hFile, fileDat, szFile, &szw, NULL);
			FlushFileBuffers(hFile);
			CloseHandle(hFile);
		}
		~SubPe()
		{
			DeleteFile(fPath);
		}
		LPTSTR GetPath()
		{
			return fPath;
		}
	private:

	};
	static SubPe *_agent32, *_agent64, *_dll32, *_dll64;

	static int IsProcess64bit(DWORD pid)
	{
		SYSTEM_INFO si = {};
		GetNativeSystemInfo(&si);

		if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
		{
			return false;
		}

		HANDLE hp = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
		if (hp == NULL)
		{
			return -1;
		}

		BOOL flag64;
		if (IsWow64Process(hp, &flag64) == 0)
		{
			return -1;
		}

		CloseHandle(hp);
		
		if (flag64)
		{
			return false;	// If on Wow64, not 64bit process
		}
		else
		{
			return true;
		}
	}

	DWORD PID;
	HANDLE hSm;
	bool active;
public:
	static int Init()
	{
		_agent32 = new SubPe(IDR_AGENT32, _T("JoyStopAgent32.exe"));
		_agent64 = new SubPe(IDR_AGENT64, _T("JoyStopAgent64.exe"));
		_dll32 = new SubPe(IDR_INJDLL32, _T("JoyStopAgent32.dll"));
		_dll64 = new SubPe(IDR_INJDLL64, _T("JoyStopAgent64.dll"));
		return 0;
	}
	static int Cleanup()
	{
		delete _agent32;
		delete _agent64;
		delete _dll32;
		delete _dll64;
		return 0;
	}
	JoyconIntercepter(DWORD Pid)
	{
		this->hSm = NULL;
		this->active = false;
		this->PID = Pid;
	}
	int InterceptJoycon()
	{
		// Create semaphore to transmit end signal
		TCHAR semaphoreName[64];
		GUID smGuid;

		CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
		CoCreateGuid(&smGuid);
		CoUninitialize();

		wsprintf(semaphoreName, _T("JoyStop{%08x-%04x-%04x-%x%x-%x%x%x%x%x%x}"),
			smGuid.Data1, smGuid.Data2, smGuid.Data3,
			smGuid.Data4[0], smGuid.Data4[1],
			smGuid.Data4[2], smGuid.Data4[3], smGuid.Data4[4], smGuid.Data4[5], smGuid.Data4[6], smGuid.Data4[7]);

		hSm = CreateSemaphore(NULL, 0, 1, semaphoreName);
		if (hSm == NULL)
		{
			return -1;
		}

		LPTSTR agentPath, dllPath;

		if (IsProcess64bit(PID))
		{
			agentPath = _agent64->GetPath();
			dllPath = _dll64->GetPath();
		}
		else
		{
			agentPath = _agent32->GetPath();
			dllPath = _dll32->GetPath();
		}


		TCHAR cmdLine[1024];

		_stprintf_s(cmdLine, _T("%s %d %s"), dllPath, PID, semaphoreName);


		STARTUPINFO			StartupInfo = {};
		PROCESS_INFORMATION	ProcessInfomation = {};

		if (
			CreateProcess(
				agentPath,
				cmdLine,
				NULL,
				NULL,
				NULL,
				CREATE_NO_WINDOW,
				NULL,
				NULL,
				&StartupInfo,
				&ProcessInfomation) == 0
			)
		{
			return -1;
		}

		this->active = true;
		return 0;
	}
	int AcceptJoycon()
	{
		ReleaseSemaphore(hSm, 1, NULL);
		this->active = false;
		return 0;
	}
	DWORD GetPid()
	{
		return PID;
	}
	bool isActive()
	{
		return active;
	}
};