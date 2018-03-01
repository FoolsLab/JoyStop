#include <unordered_map>
#include <vector>
#include <string>
using namespace std;

#include <Windows.h>
#include <tchar.h>
#include <mmsystem.h>
#pragma comment(lib,"winmm.lib")
#include <imagehlp.h>
#pragma comment(lib,"imagehlp.lib")

__declspec(dllexport)
MMRESULT JS_joyConfigChanged(
	DWORD dwFlags
)
{
	return joyConfigChanged(dwFlags);
}

__declspec(dllexport)
MMRESULT JS_joyGetDevCapsA(
	UINT uJoyID,
	LPJOYCAPSA pjc,
	UINT cbjc
)
{
	return JOYERR_PARMS;
}

__declspec(dllexport)
MMRESULT JS_joyGetDevCapsW(
	UINT uJoyID,
	LPJOYCAPSW pjc,
	UINT cbjc
)
{
	return JOYERR_PARMS;
}

__declspec(dllexport)
UINT JS_joyGetNumDevs(VOID)
{
	return joyGetNumDevs();
}

__declspec(dllexport)
MMRESULT JS_joyGetPos(
	UINT uJoyID,
	LPJOYINFO pji
)
{
	return JOYERR_PARMS;
}

__declspec(dllexport)
MMRESULT JS_joyGetPosEx(
	UINT uJoyID,
	LPJOYINFOEX pji
)
{
	return JOYERR_PARMS;
}

__declspec(dllexport)
MMRESULT JS_joyGetThreshold(
	UINT uJoyID,
	LPUINT puThreshold
)
{
	return MMSYSERR_INVALPARAM;
}

__declspec(dllexport)
MMRESULT JS_joyReleaseCapture(
	UINT uJoyID
)
{
	return MMSYSERR_INVALPARAM;
}

__declspec(dllexport)
MMRESULT JS_joySetCapture(
	HWND hwnd,
	UINT uJoyID,
	UINT uPeriod,
	BOOL fChanged
)
{
	return JOYERR_PARMS;
}

__declspec(dllexport)
MMRESULT JS_joySetThreshold(
	UINT uJoyID,
	UINT uThreshold
)
{
	return JOYERR_PARMS;
}

unordered_map<string, void*> joyFunc =
{
	{ "joyConfigChanged",	JS_joyConfigChanged },
{ "joyGetDevCapsA",		JS_joyGetDevCapsA },
{ "joyGetDevCapsW",		JS_joyGetDevCapsW },
{ "joyGetNumDevs",		JS_joyGetNumDevs },
{ "joyGetPos",			JS_joyGetPos },
{ "joyGetPosEx",		JS_joyGetPosEx },
{ "joyGetThreshold",	JS_joyGetThreshold },
{ "joyReleaseCapture",	JS_joyReleaseCapture },
{ "joySetCapture",		JS_joySetCapture },
{ "joySetThreshold",	JS_joySetThreshold },
};

vector<pair<PROC*, void*>> joyFuncDefault;

void modifyFuncAddr(PROC* p, void* newaddr)
{
	DWORD flOldProtect;
	VirtualProtect(p, sizeof(p), PAGE_EXECUTE_READWRITE, &flOldProtect);
	*p = static_cast<PROC>(newaddr);
	VirtualProtect(p, sizeof(p), flOldProtect, &flOldProtect);
}

void ModifyJoyconFunc()
{
	HMODULE hMod = GetModuleHandle(NULL);

	ULONG ulSize;
	PIMAGE_IMPORT_DESCRIPTOR pImgDesc = (PIMAGE_IMPORT_DESCRIPTOR)ImageDirectoryEntryToData(hMod, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &ulSize);

	for (size_t i = 0; pImgDesc[i].OriginalFirstThunk != NULL; i++)
	{
		if (strcmp("winmm.dll", reinterpret_cast<char*>(reinterpret_cast<BYTE*>(hMod) + pImgDesc[i].Name)) != 0)
		{
			continue;
		}

		PIMAGE_THUNK_DATA oriThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(reinterpret_cast<BYTE*>(hMod) + pImgDesc[i].OriginalFirstThunk);
		PIMAGE_THUNK_DATA thunk = reinterpret_cast<PIMAGE_THUNK_DATA>(reinterpret_cast<BYTE*>(hMod) + pImgDesc[i].FirstThunk);

		for (size_t j = 0; oriThunk[j].u1.AddressOfData != NULL; j++)
		{
			PIMAGE_IMPORT_BY_NAME impName = reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(reinterpret_cast<BYTE*>(hMod) + oriThunk[j].u1.AddressOfData);
			if (joyFunc.count(impName->Name) == 0)
			{
				continue;
			}

			joyFuncDefault.push_back(
				make_pair(
					reinterpret_cast<PROC*>(&thunk[j].u1.Function),
					reinterpret_cast<void*>(thunk[j].u1.Function)
				)
			);
			modifyFuncAddr(reinterpret_cast<PROC*>(&thunk[j].u1.Function), joyFunc[impName->Name]);		
		}
	}

}

void RestoreJoyconFunc()
{
	for (auto v : joyFuncDefault)
	{
		modifyFuncAddr(v.first, v.second);
	}
	joyFuncDefault.clear();
}
