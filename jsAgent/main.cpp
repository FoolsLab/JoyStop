#include <Windows.h>
#include <tchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory>

struct InjInfo
{
	TCHAR dllPath[512];
	DWORD pid;
	LPVOID remoteMem;
};

std::pair<std::unique_ptr<unsigned __int8[]>, size_t>
BuildRemoteCode(char* codeStr, void* LibName, void* FuncAddr, void* phMod)
{
	int sz = 0;
	char* p = codeStr;

	while (true)
	{
		if (*p == '\0')
		{
			break;
		}

		if (isxdigit(*p) && isxdigit(*(p + 1)))
		{
			sz += 1;
			p += 2;
			continue;
		}

		if (*p == '%')
		{
			switch (*(p + 1))
			{
			case 's':
				sz += sizeof(LibName);
				break;
			case 'f':
				sz += sizeof(FuncAddr);
				break;
			case 'h':
				sz += sizeof(phMod);
				break;
			default:
				break;
			}
			p += 2;
			continue;
		}

		p++;
	}

	std::unique_ptr<unsigned __int8[]> code(new unsigned __int8[sz]);
	p = codeStr;
	auto pd = code.get();

	while (true)
	{
		if (*p == '\0')
		{
			break;
		}

		if (isxdigit(*p) && isxdigit(*(p + 1)))
		{
			char buf[3] = { *(p + 0), *(p + 1), '\0' };

			*pd = static_cast<unsigned __int8>(strtol(buf, NULL, 16));
			pd++;
			p += 2;
			continue;
		}

		if (*p == '%')
		{
			switch (*(p + 1))
			{
			case 's':
				*(reinterpret_cast<decltype(LibName)*>(pd)) = LibName;
				pd += sizeof(LibName);
				break;
			case 'f':
				*(reinterpret_cast<decltype(FuncAddr)*>(pd)) = FuncAddr;
				pd += sizeof(FuncAddr);
				break;
			case 'h':
				*(reinterpret_cast<decltype(phMod)*>(pd)) = phMod;
				pd += sizeof(phMod);
				break;
			default:
				break;
			}
			p += 2;
			continue;
		}

		p++;
	}

	return { std::move(code), sz };
}

int InjecttionDLL(InjInfo *ii)
{
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, ii->pid);
	if (hProcess == NULL)
	{
		return -1;
	}

	// remote memory:
	// 1. return val of LoadLibraryA (4 or 8 byte)
	// 2. DLL Path (length of path str + 1 byte)
	// 3. remote execution code

	LPVOID remoteMem = (LPVOID)VirtualAllocEx(hProcess, NULL, 512, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (remoteMem == NULL) {
		return -1;
	}

	LPVOID llAddr = (LPVOID)GetProcAddress(GetModuleHandle(_T("kernel32.dll")), "LoadLibraryA");
	if (llAddr == NULL) {
		return -1;
	}

	char remoteCode[] =
#if defined _M_IX86
		"68 %s"		// push s
		"B8 %f"		// mov eax,f
		"FF D0"		// call eax
		"A3 %h"		// dword ptr ds:[h],eax
		"C2 04 00"	// ret 4
#elif defined _M_X64
		"48 83 EC 28"	// sub rsp,28h
		"48 B9 %s"		// mov rcx,s
		"48 B8 %f"		// mov rax,f
		"FF D0"			// call rax
		"48 A3 %h"		// mov qword ptr [h],rax
		"48 83 C4 28"	// add rsp,28h
		"C3"			// ret
#endif
		;

	auto codeDat = BuildRemoteCode(remoteCode, (BYTE*)remoteMem + sizeof(HMODULE), llAddr, remoteMem);

	size_t lenDllPath = strlen(ii->dllPath);

	int n = WriteProcessMemory(hProcess, (BYTE*)remoteMem + sizeof(HMODULE), ii->dllPath, lenDllPath + 1, NULL);
	if (n == 0) {
		return -1;
	}

	n = WriteProcessMemory(hProcess, (BYTE*)remoteMem + sizeof(HMODULE) + lenDllPath + 1, codeDat.first.get(), codeDat.second, NULL);
	if (n == 0) {
		return -1;
	}

	HANDLE thread_id =
		CreateRemoteThread(
			hProcess,
			NULL,
			0,
			(LPTHREAD_START_ROUTINE)((BYTE*)remoteMem + sizeof(HMODULE) + lenDllPath + 1),
			0,
			0,
			NULL
		);
	if (thread_id == NULL) {
		return -1;
	}

	CloseHandle(hProcess);

	ii->remoteMem = remoteMem;
	return 0;
}

int CleanupDLL(InjInfo *ii)
{
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, ii->pid);
	if (hProcess == NULL)
	{
		return -1;
	}

	LPVOID flAddr = (LPVOID)GetProcAddress(GetModuleHandle(_T("kernel32.dll")), "FreeLibrary");
	if (flAddr == NULL) {
		return -1;
	}

	HMODULE hMod;
	ReadProcessMemory(hProcess, ii->remoteMem, &hMod, sizeof(hMod), NULL);

	HANDLE thread_id =
		CreateRemoteThread(
			hProcess,
			NULL,
			0,
			(LPTHREAD_START_ROUTINE)flAddr,
			hMod,
			0,
			NULL
		);
	if (thread_id == NULL) {
		return -1;
	}

	VirtualFree(ii->remoteMem, 0, MEM_DECOMMIT | MEM_RELEASE);

	CloseHandle(hProcess);
	return 0;
}


int WINAPI WinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine,
	int nCmdShow
)
{
	InjInfo ii;
	TCHAR smn[64];

	_stscanf_s(GetCommandLine(), _T("%s %d %s"), ii.dllPath, sizeof(ii.dllPath)/sizeof(TCHAR), &ii.pid, smn, sizeof(smn) / sizeof(TCHAR));

	HANDLE hSm = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, smn);
	if (hSm == NULL)
	{
		MessageBox(NULL, _T("Error"), _T("JoyStop"), MB_OK | MB_ICONERROR);
		return 0;
	}

	if (InjecttionDLL(&ii) != 0)
	{
		MessageBox(NULL, _T("Error"), _T("JoyStop"), MB_OK | MB_ICONERROR);
		return 0;
	}

	WaitForSingleObject(hSm, INFINITE);
	CloseHandle(hSm);

	CleanupDLL(&ii);
	return 0;
}