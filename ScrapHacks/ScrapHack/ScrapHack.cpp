// ScrapHack.cpp: Definiert die exportierten Funktionen für die DLL-Anwendung.
//

#include "stdafx.h"
#include <string>
#include <vector>
#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>
#include "Scrapland.h"
#define DLL_EXPORT extern "C" __declspec(dllexport)

using namespace std;

bool initialized = false;


string GetLastErrorAsString() {
	DWORD errorMessageID = GetLastError();
	if (errorMessageID == 0) return "No error";
	LPSTR messageBuffer = NULL;
	size_t m_size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
	string message(messageBuffer, m_size);
	LocalFree(messageBuffer);
	if (!message.empty() && message[message.length() - 1] == '\n') {
		message.erase(message.length() - 1);
	}
	return message;
}
void SetupStreams() {
	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);
	ios::sync_with_stdio();
	std::wcout.clear();
	std::cout.clear();
	std::wcerr.clear();
	std::cerr.clear();
	std::wcin.clear();
	std::cin.clear();
}
void SetupConsole() {
	if (!AllocConsole()) {
		FreeConsole();
		AllocConsole();
	}
	SetupStreams();
}

void SetupConsole(const char* title) {
	SetupConsole();
	SetConsoleTitleA(title);
}
void FreeConsole(bool wait) {
	if (wait) {
		cout << "[?] Press Enter to Exit";
		cin.ignore();
	}
	FreeConsole();
}

bool in_foreground = false;
BOOL CALLBACK EnumWindowsProcMy(HWND hwnd, LPARAM lParam)
{
	DWORD lpdwProcessId;
	GetWindowThreadProcessId(hwnd, &lpdwProcessId);
	if (lpdwProcessId == lParam)
	{
		in_foreground = (hwnd == GetForegroundWindow()) || (hwnd == GetActiveWindow());
		return FALSE;
	}
	return TRUE;
}

bool key_down(int keycode,int delay=100) {
	in_foreground = false;
	EnumWindows(EnumWindowsProcMy, GetCurrentProcessId());
	if (in_foreground) {
		if (GetAsyncKeyState(keycode)) {
			while (GetAsyncKeyState(keycode)) {
				Sleep(delay);
			}
			return true;
		}
	}
	return false;
}

DLL_EXPORT DWORD DllInit() {
	if (!initialized) {
		SetupConsole();
		initialized = true;
		char mfn[1024];
		GetModuleFileName(0, mfn, 1024);
		string s("[+] DLL Loaded in ");
		s += mfn;
		cout << s << endl;
		scrap_log(0x00ff00, (s + "\n").c_str());
		while (true) {
			if (key_down(VK_F12)) {
				scrap_log(0x00ff00, "BOOP!\n");
			}
		}
		return (DWORD)1;
	}
	return (DWORD)0;
}