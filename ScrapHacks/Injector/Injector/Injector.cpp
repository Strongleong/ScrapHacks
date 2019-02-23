#include "stdafx.h"
#include <iostream>
#include <windows.h>
#include <TlHelp32.h>
#include <string>
#include <fstream>
#include <algorithm>
#include <vector>
#include <string.h>
using namespace std;

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

string fromhex(string input) {
	transform(input.begin(), input.end(), input.begin(), ::toupper);
	string hc = "0123456789ABCDEF";
	string o = "";
	int n = 0;
	int v = 0;
	for (unsigned char c : input) {
		if (hc.find(c) != size_t(-1)) {
			if ((n++) % 2 == 0) {
				v = hc.find(c) << 4;
			}
			else {
				o += char(v + hc.find(c));
			}
		}
		else {
			cout << "Invalid Character in hex string" << endl;
			return "";
		}
	}
	return o;

}

vector<string> split(string str, char sep) {
	vector<string> ret;
	string part;
	for (auto n : str) {
		if (n == sep) {
			ret.push_back(part);
			part.clear();
		}
		else {
			part = part + n;
		}
	}
	if (part != "") ret.push_back(part);
	return ret;
}

bool fexists(const char* filename) {
	ifstream ifile(filename);
	bool ret = ifile.good();
	ifile.close();
	return ret;
}

bool HasModule(int PID, const char* modname) {
	HANDLE hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, PID);
	MODULEENTRY32 me;
	me.dwSize = sizeof(MODULEENTRY32);
	if (hModuleSnap == INVALID_HANDLE_VALUE) {
		return false;
	}
	if (!Module32First(hModuleSnap, &me)) {
		CloseHandle(hModuleSnap);
		cout << "Error reading Module Snapshot" << endl;
	}
	else {
		do {
			if (strstr((const char*)me.szModule, modname) != NULL) return true;
		} while (Module32Next(hModuleSnap, &me));
	}
	return false;
}

void PrintHelp(char* myname) {
	cout << "Usage: " << endl;
	cout << endl;
	cout << myname << " code:file|code:hex [filename|hex_shellcode] [PID|Process Name|.]" << endl;
	cout << "\tcode:file: Read Shellcode from File and Inject" << endl;
	cout << "\tcode:hex: Read Hex Encoded Shellcode from String and Inject" << endl;
	cout << endl;
	cout << myname << " dll [DLL Filename]:[Function Name] [PID|Process Name|.]" << endl;
	cout << "\tdll: Inject DLL" << endl;
	cout << endl;
	cout << "Using . as the Target Process causes the Injector to inject into itself" << endl;
	cout << endl;
	return;
}

DWORD ProcNameToPID(char* szPname) {
	DWORD PID = -1;
	cout << "[*] Getting PID from Process Name '" << szPname << "'" << endl;

	PROCESSENTRY32 pe;
	pe.dwSize = sizeof(PROCESSENTRY32);
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPALL, 0);
	if (!Process32First(hSnap, &pe)) {
		CloseHandle(hSnap);
		cout << "[!] Error reading Process Snapshot: " << GetLastErrorAsString() << endl;
		return 0;
	}

	do {
		if (strstr((const char*)pe.szExeFile, szPname) != NULL) {
			cout << "[+] Got PID: " << pe.th32ProcessID << endl;
			PID = pe.th32ProcessID;
			break;
		}
	} while (Process32Next(hSnap, &pe));
	CloseHandle(hSnap);
	return PID;
}


bool adjustPrivs(HANDLE hProc) {
	HANDLE hToken;
	LUID luid;
	TOKEN_PRIVILEGES tkprivs;
	if (!OpenProcessToken(hProc, (TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY), &hToken)) {
		cout << "[!] Could not Open Process Token: " << GetLastErrorAsString() << endl;
		return 0;
	}
	if (!LookupPrivilegeValue(0, SE_DEBUG_NAME, &luid)) {
		CloseHandle(hToken);
		cout << "[!] Error Looking up Privilege Value: " << GetLastErrorAsString() << endl;
		return 0;
	}
	tkprivs.PrivilegeCount = 1;
	tkprivs.Privileges[0].Luid = luid;
	tkprivs.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	bool bRet = AdjustTokenPrivileges(hToken, 0, &tkprivs, sizeof(tkprivs), NULL, NULL);
	CloseHandle(hToken);
	if (!bRet) {
		cout << "[!] Could Not Adjust Privileges: " << GetLastErrorAsString() << endl;
	}
	return bRet;
}

void InjectCode(HANDLE hProc, DWORD PID, unsigned const char* sc, unsigned int sclen, bool do_resume = false) {
	cout << "[*] Allocating " << sclen << " Bytes of Memory" << endl;
	LPVOID mem = VirtualAllocEx(hProc, 0, sclen, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (mem == NULL) {
		cout << "[!] Could not Allocate Memory: " << GetLastErrorAsString() << endl;
		return;
	}
	cout << "[+] Allocated " << sclen << " Bytes at " << mem << endl;
	cout << "[*] Injecting Code" << endl;
	WriteProcessMemory(hProc, mem, sc, sclen, 0);
	cout << "[*] Starting Remote Thread" << endl;
	//HANDLE hThread=
	CreateRemoteThread(hProc, 0, 0, (LPTHREAD_START_ROUTINE)mem, 0, 0, 0);
}

void InjectDll(HANDLE hProc, DWORD PID, const char* dll_name, const char* func_name, bool do_resume = false) {
	HANDLE hRemThread;
	char dll_full_path[MAX_PATH];
	GetFullPathNameA(dll_name, MAX_PATH, dll_full_path, 0);
	cout << "[*] Injecting Dll " << dll_name << endl;
	if (HasModule(PID, dll_name)) {
		cout << "[*] Dll already Loaded" << endl;
	}
	else {
		if (!fexists(dll_full_path)) {
			cout << "[!] Dll file not found!" << endl;
			return;
		}
		HINSTANCE  hK32 = LoadLibraryA("kernel32");
		cout << "[*] Getting Address of LoadLibrary" << endl;
		LPVOID LoadLibrary_Address = (LPVOID)GetProcAddress(hK32, "LoadLibraryA");
		FreeLibrary(hK32);
		cout << "[+] LoadLibrary is at " << LoadLibrary_Address << endl;
		cout << "[*] Allocating " << strlen(dll_full_path) << " Bytes of Memory" << endl;
		LPVOID mem = VirtualAllocEx(hProc, NULL, strlen(dll_full_path), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
		if (mem == NULL) {
			cout << "[!] Could not Allocate Memory: " << GetLastErrorAsString() << endl;
			return;
		}
		cout << "[*] Writing Dll Name to Process Memory at " << mem << endl;
		WriteProcessMemory(hProc, mem, dll_full_path, strlen(dll_full_path), 0);
		cout << "[*] Creating Thread to Load DLL" << endl;
		if (do_resume) {
			hRemThread = CreateRemoteThread(hProc, 0, 0, (LPTHREAD_START_ROUTINE)LoadLibrary_Address, mem, CREATE_SUSPENDED, 0);
			ResumeThread(hRemThread);
		}
		else {
			hRemThread = CreateRemoteThread(hProc, 0, 0, (LPTHREAD_START_ROUTINE)LoadLibrary_Address, mem, 0, 0);
		}
		cout << "[*] Waiting for DLL to load" << endl;
		WaitForSingleObject(hRemThread, INFINITE);
		CloseHandle(hRemThread);
	}
	return;
}
int main(int argc, char* argv[]) {
	string prog;
	HANDLE hProc = INVALID_HANDLE_VALUE;
	STARTUPINFOA startupinfo;
	PROCESS_INFORMATION processinfo;
	SetConsoleTitleA("Injector");
	unsigned int PID;
	bool do_resume = false;
	if (argc<2) {
		PrintHelp(argv[0]);
		return -1;
	}
	unsigned const char* sc = (unsigned const char*)"";
	size_t sclen = 0;
	string filecont;
	vector<string> mode = split(argv[1], ':');
	if (mode.size()>1) {
		if (mode[0] == "code") {
			if (mode[1] == "file") {
				ifstream scfile;
				if (!fexists(argv[2])) {
					cout << "[!] File not found!" << endl;
					return -1;
				}
				cout << "[*] Reading Shellcode from " << argv[2] << endl;
				scfile.open(argv[2]);
				scfile.seekg(0, ios::end);
				sclen = (size_t)scfile.tellg();
				scfile.seekg(0, ios::beg);
				filecont.resize(sclen);
				scfile.read(&filecont[0], sclen);
				scfile.close();
				cout << "[*] Read " << sclen << " Bytes" << endl;
				sc = (unsigned char*)filecont.c_str();
			}
			else if (mode[1] == "hex") {
				string scs = fromhex(argv[2]);
				sc = (unsigned const char*)scs.c_str();
				sclen = scs.length();
			}
			else {
				cout << "[!] Invalid Mode" << endl;
				return -1;
			}
		}
	}
	if (string(argv[3]) == ".") {
		PID = GetCurrentProcessId();
		cout << "[*] Injecting into self (PID: " << PID << ")" << endl;
	}
	else {
		PID = atoi(argv[3]);
		if (!PID) {
			PID = ProcNameToPID(argv[3]);
			if (PID == (DWORD)-1) {
				if (fexists(argv[3])) {
					string cmdline;
					for (int an = 4; an<argc; an++) {
						if (cmdline.size()) {
							cmdline = cmdline + " " + argv[an];
						}
						else {
							cmdline = argv[an];
						}
					}
					prog = string(argv[3]);
					cout << "[*] Starting new Process '" << prog << "' with commandline '" << cmdline << "'" << endl;
					prog += " ";
					prog += cmdline;
					ZeroMemory(&startupinfo, sizeof(startupinfo));
					startupinfo.cb = sizeof(startupinfo);
					ZeroMemory(&processinfo, sizeof(processinfo));
					if (!CreateProcessA(NULL, (char*)prog.c_str(), NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &startupinfo, &processinfo))
					{
						cout << "[!] Could not create process: " << GetLastErrorAsString() << endl;
						return -1;
					}
					PID = processinfo.dwProcessId;
					hProc = processinfo.hProcess;
					do_resume = true;
					cout << "[*] Created Suspended Process with PID " << PID << endl;
				}
				else {
					cout << "[!] Executable not found" << endl;
				}
			}
		}
	}
	if (PID == (DWORD)-1) {
		cout << "[!] Process not Found!" << endl;
		return -1;
	}
	if (hProc == INVALID_HANDLE_VALUE) {
		cout << "[*] Opening Process Handle" << endl;
		hProc = OpenProcess(PROCESS_ALL_ACCESS, 0, PID);
	}
	cout << "[*] Adjusting Privileges of Process " << PID << endl;
	adjustPrivs(hProc);
	if (mode[0] == "code") {
		InjectCode(hProc, PID, sc, sclen, do_resume);
	}
	else if (mode[0] == "dll") {
		vector<string> dll_args = split(argv[2], ':');
		string dll_name = dll_args[0];
		string dll_func = "";
		if (dll_args.size()>1) {
			dll_func = dll_args[1];
		}
		if (dll_name.find(".dll") == (size_t)-1) dll_name = dll_name + ".dll";
		InjectDll(hProc, PID, dll_name.c_str(), dll_func.c_str(), do_resume);
	}
	else {
		cout << "[-] Invalid mode specified" << endl;
	}
	if (do_resume) {
		cout << "[*] Resuming Process" << endl;
		while (ResumeThread(processinfo.hThread));
	}
	cout << "[*] Closing Process Handle" << endl;
	CloseHandle(hProc);
	cout << "[*] Done!" << endl;
	if (PID == GetCurrentProcessId()) for (;;) Sleep(-1);
	return 0;
}
