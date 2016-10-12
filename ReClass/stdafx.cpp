#include "stdafx.h"
//#include "CMainFrame.h"

//Globals
HANDLE g_hProcess = NULL;
DWORD g_ProcessID = NULL;
size_t g_AttachedProcessAddress = NULL;
DWORD g_AttachedProcessSize = NULL;
CString g_ProcessName;
Symbols* g_SymLoader = nullptr;

std::vector<MemMapInfo> MemMap;
std::vector<MemMapInfo> MemMapCode;
std::vector<MemMapInfo> MemMapData;
std::vector<MemMapInfo> MemMapModule;
std::vector<AddressName> Exports;
std::vector<AddressName> CustomNames;

DWORD NodeCreateIndex = 0;

COLORREF crBackground = RGB(255, 255, 255);
COLORREF crSelect = RGB(240, 240, 240);
COLORREF crHidden = RGB(240, 240, 240);

COLORREF crOffset = RGB(255, 0, 0);
COLORREF crAddress = RGB(0, 200, 0);
COLORREF crType = RGB(0, 0, 255);
COLORREF crName = RGB(32, 32, 128);
COLORREF crIndex = RGB(32, 200, 200);
COLORREF crValue = RGB(255, 128, 0);
COLORREF crComment = RGB(0, 200, 0);

COLORREF crVTable = RGB(0, 255, 0);
COLORREF crFunction = RGB(255, 0, 255);
COLORREF crChar = RGB(0, 0, 255);
COLORREF crBits = RGB(0, 0, 255);
COLORREF crCustom = RGB(64, 128, 64);
COLORREF crHex = RGB(0, 0, 0);

CFont g_ViewFont;

int g_FontWidth;
int g_FontHeight;

bool gbAddress = true;
bool gbOffset = true;
bool gbText = true;
bool gbRTTI = true;
bool gbResizingFont = true;
bool gbSymbolResolution = true;
bool gbLoadModuleSymbol = false;

bool gbFloat = true;
bool gbInt = true;
bool gbString = true;
bool gbPointers = true;

bool gbTop = true;
bool gbClassBrowser = true;
bool gbFilterProcesses = false;
bool gbPrivatePadding = false;
bool gbClipboardCopy = false;

CString tdHex;
CString tdInt64;
CString tdInt32;
CString tdInt16;
CString tdInt8;
CString tdQWORD;
CString tdDWORD;
CString tdWORD;
CString tdBYTE;
CString tdFloat;
CString tdDouble;
CString tdVec2;
CString tdVec3;
CString tdQuat;
CString tdMatrix;
CString tdPChar;
CString tdPWChar;

std::vector<HICON> Icons;

#pragma region Plugins
std::vector<RECLASS_PLUGINS> LoadedPlugins;

MEMORY_OPERATION g_PluginOverrideMemoryWrite = nullptr;
MEMORY_OPERATION g_PluginOverrideMemoryRead = nullptr;
HANDLE_OPERATION g_PluginOverrideHandleProcess = nullptr;
HANDLE_OPERATION g_PluginOverrideHandleThread = nullptr;

void LoadPlugins()
{
	WIN32_FIND_DATA file_data;
	ZeroMemory( &file_data, sizeof( WIN32_FIND_DATA ) );

#ifdef _WIN64
	HANDLE findfile_tree = FindFirstFile( _T( "plugins\\*.rc-plugin64" ), &file_data );
#else
	HANDLE findfile_tree = FindFirstFile( _T( "plugins\\*.rc-plugin" ), &file_data );
#endif

	if ( findfile_tree != INVALID_HANDLE_VALUE )
	{
		CString message{ };

		do
		{
			HMODULE plugin_base = LoadLibrary( CString( _T( "plugins\\" ) ) + file_data.cFileName );
			if ( plugin_base == NULL )
			{
				message.Format( _T( "plugin %s was not able to be loaded!" ), file_data.cFileName );
				PrintOut( message );
				continue;
			}

			auto pfnPluginInit = reinterpret_cast<tPluginInit>(GetProcAddress(plugin_base, "PluginInit"));
			if (pfnPluginInit == nullptr)
			{
				message.Format( _T( "%s is not a reclass plugin!" ), file_data.cFileName );
				PrintOut( message );
				FreeLibrary( plugin_base );
				continue;
			}
			
			auto pfnPluginStateChange = reinterpret_cast<tPluginStateChange>(GetProcAddress(plugin_base, "PluginStateChange"));
			if (pfnPluginStateChange == nullptr)
			{
				message.Format(_T("%s doesnt have exported state change function! Unable to disable plugin on request, stop reclass and delete the plugin to disable it"), file_data.cFileName);
				PrintOut(message);
			}

			auto pfnPluginSettingDlgProc = reinterpret_cast<DLGPROC>(GetProcAddress(plugin_base, "PluginSettingsDlg"));

			RECLASS_PLUGINS plugin;
			ZeroMemory(&plugin, sizeof RECLASS_PLUGINS);
			wcscpy_s(plugin.FileName, file_data.cFileName);
			plugin.LoadedBase = plugin_base;
			plugin.InitFnc = pfnPluginInit;
			plugin.SettingDlgFnc = pfnPluginSettingDlgProc;
			plugin.StateChangeFnc = pfnPluginStateChange;

			if (pfnPluginInit(&plugin.Info))
			{
			#ifdef UNICODE
				plugin.State = theApp.GetProfileInt(L"PluginState", plugin.Info.Name, 1) == 1;
			#else
				plugin.State = theApp.GetProfileInt("PluginState", CW2A(plugin.Info.Name), 1) == 1;
			#endif
				if (plugin.Info.DialogID == -1)
					plugin.SettingDlgFnc = nullptr;
				PrintOut(_T("Loaded plugin %s (%ls version %ls) - %ls"), file_data.cFileName, plugin.Info.Name, plugin.Info.Version, plugin.Info.About);
				if (plugin.StateChangeFnc != nullptr) 
					plugin.StateChangeFnc(plugin.State);
				LoadedPlugins.push_back( plugin );
			}
			else 
			{
				message.Format( _T( "Failed to load plugin %s" ), file_data.cFileName );
				PrintOut( message );
				FreeLibrary( plugin_base );
			}
		} while ( FindNextFile( findfile_tree, &file_data ) );
	}
}

BOOL PLUGIN_CC ReClassOverrideMemoryOperations(MEMORY_OPERATION MemWrite, MEMORY_OPERATION MemRead, BOOL bForceSet)
{
	if (MemWrite == nullptr || MemRead == nullptr)
		return FALSE;

	if (g_PluginOverrideMemoryRead != nullptr && g_PluginOverrideMemoryWrite != nullptr && !bForceSet)
	{
		return FALSE;
	}
	else
	{
		g_PluginOverrideMemoryWrite = MemWrite;
		g_PluginOverrideMemoryRead = MemRead;
		return TRUE;
	}
}

BOOL PLUGIN_CC ReClassOverrideHandleOperations(HANDLE_OPERATION HandleProcess, HANDLE_OPERATION HandleThread, BOOL bForceSet)
{
	if (HandleProcess == nullptr || HandleThread == nullptr)
		return FALSE;
	if (g_PluginOverrideHandleProcess != nullptr && g_PluginOverrideHandleThread != nullptr && !bForceSet) {
		return FALSE;
	}
	else
	{
		g_PluginOverrideHandleProcess = HandleProcess;
		g_PluginOverrideHandleThread = HandleThread;
		return TRUE;
	}
	return FALSE;
}

void PLUGIN_CC ReClassPrintConsole(const wchar_t *format, ...)
{
	wchar_t buffer[6048];
	ZeroMemory(&buffer, sizeof(buffer));

	va_list va;
	va_start(va, format);
	vswprintf_s(buffer, format, va);
	va_end(va);

#ifndef UNICODE
	theApp.Console->PrintText(CW2A(buffer));
#else
	theApp.Console->PrintText(buffer);
#endif
}

LPHANDLE PLUGIN_CC ReClassGetProcessHandle()
{
	return &g_hProcess;
}

HWND PLUGIN_CC ReClassMainWindow()
{
	return *theApp.GetMainWnd();
}

CMFCRibbonBar* PLUGIN_CC ReClassRibbonInterface( )
{
	return theApp.GetRibbonBar();
}
#pragma endregion

BOOL ReClassReadMemory(LPVOID Address, LPVOID Buffer, SIZE_T Size, SIZE_T *num_read)
{
	if (g_PluginOverrideMemoryRead != nullptr)
		return g_PluginOverrideMemoryRead(Address, Buffer, Size, num_read);

	BOOL return_val = ReadProcessMemory(g_hProcess, (LPVOID)Address, Buffer, Size, num_read);
	if (!return_val) ZeroMemory(Buffer, Size);
	return return_val;
}

BOOL ReClassWriteMemory(LPVOID Address, LPVOID Buffer, SIZE_T Size, SIZE_T *num_wrote)
{
	if (g_PluginOverrideMemoryWrite != nullptr)
		return g_PluginOverrideMemoryWrite(Address, Buffer, Size, num_wrote);

	DWORD OldProtect;
	VirtualProtectEx(g_hProcess, (void*)Address, Size, PAGE_EXECUTE_READWRITE, &OldProtect);
	BOOL ret = WriteProcessMemory(g_hProcess, (void*)Address, Buffer, Size, num_wrote);
	VirtualProtectEx(g_hProcess, (void*)Address, Size, OldProtect, NULL);
	return ret;
}

HANDLE ReClassOpenProcess(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessID)
{
	if (g_PluginOverrideHandleProcess != nullptr)
		return g_PluginOverrideHandleProcess(dwDesiredAccess, bInheritHandle, dwProcessID);
	return OpenProcess(dwDesiredAccess, bInheritHandle, dwProcessID);
}

HANDLE ReClassOpenThread(DWORD dwDesiredAccessFlags, BOOL bInheritHandle, DWORD dwThreadID)
{
	if (g_PluginOverrideHandleThread != nullptr)
		return g_PluginOverrideHandleThread(dwDesiredAccessFlags, bInheritHandle, dwThreadID);
	return OpenThread(dwDesiredAccessFlags, bInheritHandle, dwThreadID);
}

CStringA ReadMemoryStringA(size_t address, SIZE_T max)
{
	auto buffer = std::make_unique<char[]>(max + 1);
	SIZE_T bytesRead;

	if (ReClassReadMemory((PVOID)address, buffer.get(), max, &bytesRead) != 0)
	{
		for (int i = 0; i < bytesRead; i++)
		{
			if (!(isprint(buffer[i])) && buffer[i] != '\0')
				buffer[i] = '.';
		}

		buffer[bytesRead] = '\0';

		return CStringA(buffer.get());
	}
	else {
#ifdef _DEBUG
		PrintOut(_T("[ReadMemoryString]: Failed to read memory, GetLastError() = %s"), Utils::GetLastErrorString().GetString());
#endif
		return CStringA("..");
	}
}

CStringW ReadMemoryStringW(size_t address, SIZE_T max)
{
	auto buffer = std::make_unique<wchar_t[]>(max + 1);
	SIZE_T bytesRead;
	if (ReClassReadMemory((PVOID)address, buffer.get(), max * sizeof(wchar_t), &bytesRead) != 0)
	{
		bytesRead /= sizeof(wchar_t);

		for (int i = 0; i < bytesRead; i++)
		{
			if (!(iswprint(buffer[i])) && buffer[i] != '\0')
				buffer[i] = '.';
		}

		buffer[bytesRead] = '\0';

		return CStringW(buffer.get());
	}
	else 
	{
		#ifdef _DEBUG
		PrintOut(_T("[ReadMemoryString]: Failed to read memory, GetLastError() = %s"), Utils::GetLastErrorString().GetString());
		#endif
		return CStringW(L"..");
	}
}

bool PauseResumeThreadList(bool bResumeThread)
{
	if (g_hProcess == NULL)
		return 0;

	NTSTATUS status;
	ULONG bufferSize = 0x4000;
	HANDLE hHeap = GetProcessHeap();
	DWORD ProcessId = GetProcessId(g_hProcess);

	PVOID SystemProcessInfo = HeapAlloc(hHeap, HEAP_ZERO_MEMORY | HEAP_GENERATE_EXCEPTIONS, bufferSize);

	while (TRUE)
	{
		status = ntdll::NtQuerySystemInformation(SystemProcessInformation, SystemProcessInfo, bufferSize, &bufferSize);
		if (status == STATUS_BUFFER_TOO_SMALL || status == STATUS_INFO_LENGTH_MISMATCH)
		{
			if (SystemProcessInfo)
				HeapFree(hHeap, 0, SystemProcessInfo);
			SystemProcessInfo = HeapAlloc(hHeap, HEAP_ZERO_MEMORY | HEAP_GENERATE_EXCEPTIONS, bufferSize * 2);
		}
		else
			break;
	}

	if (!NT_SUCCESS(status))
	{
		if (SystemProcessInfo)
			HeapFree(hHeap, 0, SystemProcessInfo);
		return 0;
	}

	PSYSTEM_PROCESS_INFORMATION process;
	PSYSTEM_THREAD_INFORMATION threads;
	ULONG numberOfThreads;

	process = PROCESS_INFORMATION_FIRST_PROCESS(SystemProcessInfo);
	do {
		if (process->UniqueProcessId == (HANDLE)ProcessId)
			break;
	} while (process = PROCESS_INFORMATION_NEXT_PROCESS(process));

	if (!process)
	{
		// The process doesn't exist anymore :(
		return 0;
	}

	threads = process->Threads;
	numberOfThreads = process->NumberOfThreads;

	for (ULONG i = 0; i < numberOfThreads; i++)
	{
		PSYSTEM_THREAD_INFORMATION thread = &threads[i];
		if (!thread) 
			continue;
		DWORD thId = (DWORD)thread->ClientId.UniqueThread;
		if (!thId) 
			continue;

		HANDLE hThread = ReClassOpenThread(THREAD_SUSPEND_RESUME, FALSE, thId);

		if (bResumeThread) 
			ResumeThread(hThread);
		else 
			SuspendThread(hThread);

		CloseHandle(hThread);
	}

	if (SystemProcessInfo)
		HeapFree(hHeap, 0, SystemProcessInfo);

	return 1;
}

size_t GetBase()
{
	if (MemMap.size() > 1)
		return g_AttachedProcessAddress;
#ifdef _WIN64
	return 0x140000000;
#else
	return 0x400000;
#endif
}

bool IsCode(size_t Address)
{
	for (UINT i = 0; i < MemMapCode.size(); i++) 
	{
		if (Address >= MemMapCode[i].Start && Address <= MemMapCode[i].End)
			return true;
	}
	return false;
}

bool IsData(size_t Address)
{
	for (UINT i = 0; i < MemMapData.size(); i++) 
	{
		if (Address >= MemMapData[i].Start && Address <= MemMapData[i].End)
			return true;
	}
	return false;
}

bool IsMemory(size_t Address)
{
	for (UINT i = 0; i < MemMap.size(); i++)
	{
		if (Address >= MemMap[i].Start && Address <= MemMap[i].End)
			return true;
	}
	return false;
}

bool IsModule(size_t Address)
{
	for (UINT i = 0; i < MemMapModule.size(); i++)
	{
		if (Address >= MemMapModule[i].Start && Address <= MemMapModule[i].End)
			return true;
	}
	return false;
}

CString GetAddressName(size_t Address, bool bHEX)
{
	CString txt;

	for (UINT i = 0; i < CustomNames.size(); i++)
	{
		if (Address == CustomNames[i].Address)
		{
#ifdef _WIN64
			txt.Format(_T("%s.%IX"), CustomNames[i].Name, Address);
#else
			txt.Format(_T("%s.%X"), CustomNames[i].Name, Address);
#endif
			return txt;
		}
	}

	for (UINT i = 0; i < Exports.size(); i++)
	{
		if (Address == Exports[i].Address)
		{
#ifdef _WIN64
			txt.Format(_T("%s.%IX"), Exports[i].Name, Address);
#else
			txt.Format(_T("%s.%X"), Exports[i].Name, Address);
#endif
			return txt;
		}
	}

	for (UINT i = 0; i < MemMapCode.size(); i++)
	{
		if (Address >= MemMapCode[i].Start && Address <= MemMapCode[i].End)
		{
#ifdef _WIN64
			txt.Format(_T("<CODE>%s.%IX"), MemMapCode[i].Name, Address);
#else
			txt.Format(_T("<CODE>%s.%X"), MemMapCode[i].Name, Address);
#endif
			return txt;
		}
	}

	for (UINT i = 0; i < MemMapData.size(); i++)
	{
		if (Address >= MemMapData[i].Start && Address <= MemMapData[i].End)
		{
#ifdef _WIN64
			txt.Format(_T("<DATA>%s.%IX"), MemMapData[i].Name, Address);
#else
			txt.Format(_T("<DATA>%s.%X"), MemMapData[i].Name, Address);
#endif
			return txt;
		}
	}

	for (UINT i = 0; i < MemMapModule.size(); i++)
	{
		if (Address >= MemMapModule[i].Start && Address <= MemMapModule[i].End)
		{
#ifdef _WIN64
			txt.Format(_T("%s.%IX"), MemMapModule[i].Name, Address);
#else
			txt.Format(_T("%s.%X"), MemMapModule[i].Name, Address);
#endif
			return txt;
		}
	}

	for (UINT i = 0; i < MemMap.size(); i++)
	{
		if (Address >= MemMap[i].Start && Address <= MemMap[i].End)
		{
#ifdef _WIN64
			txt.Format(_T("%IX"), Address);
#else
			txt.Format(_T("%X"), Address);
#endif
			return txt;
		}
	}

	if (bHEX)
	{
#ifdef _WIN64
		txt.Format(_T("%IX"), Address);
#else
		txt.Format(_T("%X"), Address);
#endif
	}


	return txt;
}

CString GetModuleName(size_t Address)
{
	for (unsigned int i = 0; i < MemMapModule.size(); i++) {
		if (Address >= MemMapModule[i].Start && Address <= MemMapModule[i].End)
			return MemMapModule[i].Name;
	}
	return CString();
}

size_t GetAddressFromName(CString moduleName)
{
	size_t moduleAddress = 0;
	for (unsigned int i = 0; i < MemMapModule.size(); i++) 
	{
		if (MemMapModule[i].Name == moduleName) 
		{
			moduleAddress = MemMapModule[i].Start;
			break;
		}
	}
	return moduleAddress;
}

bool IsProcessHandleValid(HANDLE hProc)
{
	if (!hProc)
		return false;
	const DWORD RetVal = WaitForSingleObject(hProc, 0);
	if (RetVal == WAIT_FAILED)
		return false;
	return (RetVal == WAIT_TIMEOUT);
}

bool UpdateMemoryMap(void)
{
	MemMap.clear();
	MemMapCode.clear();
	MemMapData.clear();
	MemMapModule.clear();
	Exports.clear();
	CustomNames.clear();

	if (g_hProcess == NULL)
		return false;

	if (!IsProcessHandleValid(g_hProcess))
	{
		g_hProcess = NULL;
		return false;
	}

	SYSTEM_INFO SysInfo;
	GetSystemInfo(&SysInfo);

	MEMORY_BASIC_INFORMATION MemInfo;
	size_t pMemory = (size_t)SysInfo.lpMinimumApplicationAddress;
	while (pMemory < (size_t)SysInfo.lpMaximumApplicationAddress)
	{
		if (VirtualQueryEx(g_hProcess, (LPCVOID)pMemory, &MemInfo, sizeof(MEMORY_BASIC_INFORMATION)) != 0)
		{
			if (MemInfo.State == MEM_COMMIT /*&& MemInfo.Type == MEM_PRIVATE*/)
			{
				MemMapInfo Mem;
				Mem.Start = (size_t)pMemory;
				Mem.End = (size_t)pMemory + MemInfo.RegionSize - 1;
				MemMap.push_back(Mem);
			}
			pMemory = (ULONG_PTR)MemInfo.BaseAddress + MemInfo.RegionSize;
		}
		else
		{
			pMemory += 1024;
		}
	}

	PPROCESS_BASIC_INFORMATION ProcessInfo = NULL;
	PEB Peb;
	PEB_LDR_DATA LdrData;

	// Try to allocate buffer 
	HANDLE hHeap = GetProcessHeap();
	DWORD dwSize = sizeof(PROCESS_BASIC_INFORMATION);
	ProcessInfo = (PPROCESS_BASIC_INFORMATION)HeapAlloc(hHeap, HEAP_ZERO_MEMORY | HEAP_GENERATE_EXCEPTIONS, dwSize);

	ULONG dwSizeNeeded = 0;
	NTSTATUS status = ntdll::NtQueryInformationProcess(g_hProcess, ProcessBasicInformation, ProcessInfo, dwSize, &dwSizeNeeded);
	if (status >= 0 && dwSize < dwSizeNeeded)
	{
		if (ProcessInfo)
			HeapFree(hHeap, 0, ProcessInfo);

		ProcessInfo = (PPROCESS_BASIC_INFORMATION)HeapAlloc(hHeap, HEAP_ZERO_MEMORY | HEAP_GENERATE_EXCEPTIONS, dwSizeNeeded);
		if (!ProcessInfo)
		{
#ifdef _DEBUG
			PrintOut(_T("[UpdateMemoryMap]: Couldn't allocate heap buffer!"));
#endif
			return false;
		}

		status = ntdll::NtQueryInformationProcess(g_hProcess, ProcessBasicInformation, ProcessInfo, dwSizeNeeded, &dwSizeNeeded);
	}

	// Did we successfully get basic info on process
	if (NT_SUCCESS(status))
	{
		// Check for PEB
		if (!ProcessInfo->PebBaseAddress)
		{
#ifdef _DEBUG
			PrintOut(_T("[UpdateMemoryMap]: PEB is null! Aborting UpdateExports!"));
#endif
			if (ProcessInfo)
				HeapFree(hHeap, 0, ProcessInfo);
			return false;
		}

		// Read Process Environment Block (PEB)
		SIZE_T dwBytesRead = 0;
		if (ReClassReadMemory(ProcessInfo->PebBaseAddress, &Peb, sizeof(PEB), &dwBytesRead) == 0)
		{
#ifdef _DEBUG
			PrintOut(_T("[UpdateMemoryMap]: Failed to read PEB! Aborting UpdateExports!"));
#endif
			if (ProcessInfo)
				HeapFree(hHeap, 0, ProcessInfo);
			return false;
		}

		// Get Ldr
		dwBytesRead = 0;
		if (ReClassReadMemory(Peb.Ldr, &LdrData, sizeof(LdrData), &dwBytesRead) == 0)
		{
#ifdef _DEBUG
			PrintOut(_T("[UpdateMemoryMap]: Failed to read PEB Ldr Data! Aborting UpdateExports!"));
#endif
			if (ProcessInfo)
				HeapFree(hHeap, 0, ProcessInfo);
			return false;
		}

		LIST_ENTRY *pLdrListHead = (LIST_ENTRY *)LdrData.InLoadOrderModuleList.Flink;
		LIST_ENTRY *pLdrCurrentNode = LdrData.InLoadOrderModuleList.Flink;
		do
		{
			LDR_DATA_TABLE_ENTRY lstEntry = { 0 };
			dwBytesRead = 0;
			if (!ReClassReadMemory((void*)pLdrCurrentNode, &lstEntry, sizeof(LDR_DATA_TABLE_ENTRY), &dwBytesRead))
			{
#ifdef _DEBUG
				PrintOut(_T("[UpdateMemoryMap]: Could not read list entry from LDR list. Error = %s"), Utils::GetLastErrorString().GetString());
#endif
				if (ProcessInfo)
					HeapFree(hHeap, 0, ProcessInfo);
				return false;
			}

			pLdrCurrentNode = lstEntry.InLoadOrderLinks.Flink;

			if (lstEntry.DllBase != NULL /*&& lstEntry.SizeOfImage != 0*/)
			{
				unsigned char* ModuleBase = (unsigned char*)lstEntry.DllBase;
				DWORD ModuleSize = lstEntry.SizeOfImage;
				wchar_t wcsFullDllName[MAX_PATH] = { 0 };
				wchar_t* wcsModule = 0;
				if (lstEntry.FullDllName.Length > 0)
				{
					dwBytesRead = 0;
					if (ReClassReadMemory((LPVOID)lstEntry.FullDllName.Buffer, &wcsFullDllName, lstEntry.FullDllName.Length, &dwBytesRead))
					{
						wcsModule = wcsrchr(wcsFullDllName, L'\\');
						if (!wcsModule)
							wcsModule = wcsrchr(wcsFullDllName, L'/');
						wcsModule++;

						if (g_AttachedProcessAddress == NULL)
						{
							wchar_t filename[MAX_PATH];
							GetModuleFileNameExW(g_hProcess, NULL, filename, MAX_PATH);
							if (_wcsicmp(filename, wcsFullDllName) == 0)
							{
								g_AttachedProcessAddress = (size_t)ModuleBase;
								g_AttachedProcessSize = ModuleSize;
							}
						}
					}
				}

				// module info
				MemMapInfo Mem;
				Mem.Start = (size_t)ModuleBase;
				Mem.End = Mem.Start + ModuleSize;
				Mem.Size = ModuleSize;
#ifdef UNICODE
				Mem.Name = wcsModule;
				Mem.Path = wcsFullDllName;
#else
				Mem.Name = CW2A(wcsModule);
				Mem.Path = CW2A(wcsFullDllName);
#endif
				MemMapModule.push_back(Mem);

				// module code
				IMAGE_DOS_HEADER DosHdr;
				IMAGE_NT_HEADERS NtHdr;

				ReClassReadMemory(ModuleBase, &DosHdr, sizeof(IMAGE_DOS_HEADER), NULL);
				ReClassReadMemory(ModuleBase + DosHdr.e_lfanew, &NtHdr, sizeof(IMAGE_NT_HEADERS), NULL);
				DWORD sectionsSize = (DWORD)NtHdr.FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER);
				PIMAGE_SECTION_HEADER sections = (PIMAGE_SECTION_HEADER)malloc(sectionsSize);
				ReClassReadMemory(ModuleBase + DosHdr.e_lfanew + sizeof(IMAGE_NT_HEADERS), sections, sectionsSize, NULL);
				for (int i = 0; i < NtHdr.FileHeader.NumberOfSections; i++)
				{
					CString txt;
					MemMapInfo Mem;
					txt.Format(_T("%.8s"), sections[i].Name); txt.MakeLower();
					if (txt == ".text" || txt == "code")
					{
						Mem.Start = (size_t)ModuleBase + sections[i].VirtualAddress;
						Mem.End = Mem.Start + sections[i].Misc.VirtualSize;
						Mem.Name = wcsModule;
						MemMapCode.push_back(Mem);
					}
					else if (txt == ".data" || txt == "data" || txt == ".rdata" || txt == ".idata")
					{
						Mem.Start = (size_t)ModuleBase + sections[i].VirtualAddress;
						Mem.End = Mem.Start + sections[i].Misc.VirtualSize;
						Mem.Name = wcsModule;
						MemMapData.push_back(Mem);
					}
				}
				// Free sections
				free(sections);
			}

		} while (pLdrListHead != pLdrCurrentNode);
	}
	else
	{
		#ifdef _DEBUG
		PrintOut(_T("[UpdateExports]: NtQueryInformationProcess failed! Aborting..."));
		#endif
		if (ProcessInfo)
			HeapFree(hHeap, 0, ProcessInfo);
		return 0;
	}

	if (ProcessInfo)
		HeapFree(hHeap, 0, ProcessInfo);

	for (UINT i = 0; i < MemMap.size(); i++)
	{
		if (IsModule(MemMap[i].Start))
			MemMap[i].Name = GetModuleName(MemMap[i].Start);
	}

	return true;
}

bool UpdateExports()
{
	Exports.clear();
	//if (!gbExports) 
	//	return;

	PPROCESS_BASIC_INFORMATION ProcessInfo = NULL;
	PEB Peb;
	PEB_LDR_DATA LdrData;

	// Try to allocate buffer 
	HANDLE hHeap = GetProcessHeap();
	DWORD dwSize = sizeof(PROCESS_BASIC_INFORMATION);
	ProcessInfo = (PPROCESS_BASIC_INFORMATION)HeapAlloc(hHeap, HEAP_ZERO_MEMORY | HEAP_GENERATE_EXCEPTIONS, dwSize);

	ULONG dwSizeNeeded = 0;
	NTSTATUS status = ntdll::NtQueryInformationProcess(g_hProcess, ProcessBasicInformation, ProcessInfo, dwSize, &dwSizeNeeded);
	if (status >= 0 && dwSize < dwSizeNeeded)
	{
		if (ProcessInfo)
			HeapFree(hHeap, 0, ProcessInfo);

		ProcessInfo = (PPROCESS_BASIC_INFORMATION)HeapAlloc(hHeap, HEAP_ZERO_MEMORY | HEAP_GENERATE_EXCEPTIONS, dwSizeNeeded);
		if (!ProcessInfo)
		{
			#ifdef _DEBUG
			PrintOut(_T("[UpdateExports]: Couldn't allocate heap buffer!"));
			#endif
			return 0;
		}

		status = ntdll::NtQueryInformationProcess(g_hProcess, ProcessBasicInformation, ProcessInfo, dwSizeNeeded, &dwSizeNeeded);
	}

	// Did we successfully get basic info on process
	if (NT_SUCCESS(status))
	{
		// Check for PEB
		if (!ProcessInfo->PebBaseAddress)
		{
			#ifdef _DEBUG
			PrintOut(_T("[UpdateExports]: PEB is null! Aborting..."));
			#endif
			if (ProcessInfo)
				HeapFree(hHeap, 0, ProcessInfo);
			return 0;
		}

		// Read Process Environment Block (PEB)
		SIZE_T dwBytesRead = 0;
		if (ReClassReadMemory(ProcessInfo->PebBaseAddress, &Peb, sizeof(PEB), &dwBytesRead) == 0)
		{
			#ifdef _DEBUG
			PrintOut(_T("[UpdateExports]: Failed to read PEB! Aborting UpdateExports."));
			#endif
			if (ProcessInfo)
				HeapFree(hHeap, 0, ProcessInfo);
			return 0;
		}

		// Get Ldr
		dwBytesRead = 0;
		if (ReClassReadMemory(Peb.Ldr, &LdrData, sizeof(LdrData), &dwBytesRead) == 0)
		{
			#ifdef _DEBUG
			PrintOut(_T("[UpdateExports]: Failed to read PEB Ldr Data! Aborting UpdateExports."));
			#endif
			if (ProcessInfo)
				HeapFree(hHeap, 0, ProcessInfo);
			return 0;
		}

		LIST_ENTRY *pLdrListHead = (LIST_ENTRY *)LdrData.InLoadOrderModuleList.Flink;
		LIST_ENTRY *pLdrCurrentNode = LdrData.InLoadOrderModuleList.Flink;
		do
		{
			LDR_DATA_TABLE_ENTRY lstEntry = { 0 };
			dwBytesRead = 0;
			if (!ReClassReadMemory((void*)pLdrCurrentNode, &lstEntry, sizeof(LDR_DATA_TABLE_ENTRY), &dwBytesRead))
			{
				#ifdef _DEBUG
				PrintOut(_T("[UpdateExports]: Could not read list entry from LDR list. Error = %s"), Utils::GetLastErrorString().GetString());
				#endif
				if (ProcessInfo)
					HeapFree(hHeap, 0, ProcessInfo);
				return 0;
			}

			pLdrCurrentNode = lstEntry.InLoadOrderLinks.Flink;

			if (lstEntry.DllBase != nullptr && lstEntry.SizeOfImage != 0)
			{
				unsigned char* ModuleHandle = (unsigned char*)lstEntry.DllBase;
				wchar_t wcsDllName[MAX_PATH] = { 0 };
				wchar_t ModuleName[MAX_PATH] = { 0 };
				if (lstEntry.BaseDllName.Length > 0)
				{
					dwBytesRead = 0;
					if (ReClassReadMemory((LPVOID)lstEntry.BaseDllName.Buffer, &wcsDllName, lstEntry.BaseDllName.Length, &dwBytesRead))
					{
						wcscpy_s(ModuleName, wcsDllName);
					}
				}

				IMAGE_DOS_HEADER DosHdr;
				IMAGE_NT_HEADERS NtHdr;

				ReClassReadMemory(ModuleHandle, &DosHdr, sizeof(DosHdr), NULL);
				ReClassReadMemory(ModuleHandle + DosHdr.e_lfanew, &NtHdr, sizeof(IMAGE_NT_HEADERS), NULL);
				if (NtHdr.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress != 0)
				{
					IMAGE_EXPORT_DIRECTORY ExpDir;
					ReClassReadMemory((LPVOID)(ModuleHandle + NtHdr.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress), &ExpDir, sizeof(ExpDir), NULL);
					PVOID pName = (PVOID)(ModuleHandle + ExpDir.AddressOfNames);
					PVOID pOrd = (PVOID)(ModuleHandle + ExpDir.AddressOfNameOrdinals);
					PVOID pAddress = (PVOID)(ModuleHandle + ExpDir.AddressOfFunctions);

					ULONG aNames[MAX_EXPORTS];
					WORD aOrds[MAX_EXPORTS];
					ULONG aAddresses[MAX_EXPORTS];
					ReClassReadMemory((LPVOID)pName, aNames, ExpDir.NumberOfNames * sizeof(aNames[0]), NULL);
					ReClassReadMemory((LPVOID)pOrd, aOrds, ExpDir.NumberOfNames * sizeof(aOrds[0]), NULL);
					ReClassReadMemory((LPVOID)pAddress, aAddresses, ExpDir.NumberOfFunctions * sizeof(aAddresses[0]), NULL);

					for (DWORD i = 0; i < ExpDir.NumberOfNames; i++)
					{
						char ExportName[256];
						ReClassReadMemory((LPVOID)(ModuleHandle + aNames[i]), ExportName, 256, NULL);
						DWORD_PTR Address = (DWORD_PTR)ModuleHandle + aAddresses[aOrds[i]];

						AddressName Entry;
						Entry.Name.Format(_T("%ls.%hs"), ModuleName, ExportName);
						Entry.Address = Address;
						// Add export entry to array
						Exports.push_back(Entry);
					}

				}
			}
		} 
		while (pLdrListHead != pLdrCurrentNode);
	}
	else
	{
		#ifdef _DEBUG
		PrintOut(_T("[UpdateExports]: NtQueryInformationProcess failed! Aborting..."));
		#endif
		if (ProcessInfo)
			HeapFree(hHeap, 0, ProcessInfo);
		return 0;
	}

	if (ProcessInfo)
		HeapFree(hHeap, 0, ProcessInfo);

	return 1;
}

int SplitString(const CString& input, const CString& delimiter, CStringArray& results)
{
	int iPos = 0;
	int newPos = -1;
	int sizeS2 = delimiter.GetLength();
	int isize = input.GetLength();

	CArray<INT, int> positions;

	newPos = input.Find(delimiter, 0);
	if (newPos < 0)
		return 0;

	int numFound = 0;
	while (newPos > iPos)
	{
		numFound++;
		positions.Add(newPos);
		iPos = newPos;
		newPos = input.Find(delimiter, iPos + sizeS2 + 1);
	}

	for (int i = 0; i <= positions.GetSize(); i++)
	{
		CString s;
		if (i == 0)
		{
			s = input.Mid(i, positions[i]);
		}
		else
		{
			int offset = positions[i - 1] + sizeS2;
			if (offset < isize)
			{
				if (i == positions.GetSize())
					s = input.Mid(offset);
				else if (i > 0)
					s = input.Mid(positions[i - 1] + sizeS2, positions[i] - positions[i - 1] - sizeS2);
			}
		}

		if (s.GetLength() > 0)
		{
			results.Add(s);
		}
	}
	return numFound;
}

size_t ConvertStrToAddress(CString str)
{
	CStringArray chunks;
	if (SplitString(str, "+", chunks) == 0)
		chunks.Add(str);

	size_t Final = 0;

	for (UINT i = 0; i < (UINT)chunks.GetCount(); i++)
	{
		CString a = chunks[i];

		a.MakeLower();  // Make all lowercase
		a.Trim();		// Trim whitespace
		a.Remove(_T('\"')); // Remove quotes

		bool bPointer = false;
		bool bMod = false;

		if (a.Find(_T(".exe")) != -1 || a.Find(_T(".dll")) != -1)
		{
			bMod = true;
		}

		if (a[0] == _T('*'))
		{
			bPointer = true;
			a = a.Mid(1);
		}
		else if (a[0] == _T('&'))
		{
			bMod = true;
			a = a.Mid(1);
		}

		size_t curadd = 0;

		if (bMod)
		{
			for (UINT i = 0; i < MemMapModule.size(); i++)
			{
				CString ModName = MemMapModule[i].Name;
				ModName.MakeLower();
				if (StrStr(ModName, a) != NULL)
				{
					curadd = MemMapModule[i].Start;
					bMod = true;
					break;
				}
			}
		}
		else 
		{
			curadd = (size_t)_tcstoui64(a.GetBuffer(), NULL, 16); //StrToNum
		}

		Final += curadd;

		if (bPointer)
		{
			if (!ReClassReadMemory((void*)Final, &Final, sizeof(Final), NULL)) 
			{
				// Causing memory leaks when Final doesnt point to a valid address.
				#ifdef _DEBUG
				// PrintOut(_T("[ConvertStrToAddress]: Failed to read memory. Error: %s"), Utils::GetLastErrorString().c_str());
				#endif
			}
		}
	}

	return Final;
}
