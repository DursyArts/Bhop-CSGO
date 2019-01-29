#include "SlimMem.h"

namespace SlimUtils {
#pragma region Constructors/Destructors
	SlimMem::SlimMem(const SlimMem & copy)
	{
		DuplicateHandle(GetCurrentProcess(), copy.m_hProc, GetCurrentProcess(), &m_hProc, NULL, FALSE, DUPLICATE_SAME_ACCESS);
	}

	SlimMem::~SlimMem()
	{
		this->Close();
	}
#pragma endregion

#pragma region Open/Close
	void SlimMem::Close()
	{
		m_mModules.clear();

		//Close the handle to the process in case it's still open
		if (IsProcessHandleValid(m_hProc)) {
			ProperlyCloseHandle(m_hProc);
		}
	}

	bool SlimMem::Open(const wchar_t * lpwstrProcessName, ProcessAccess flags)
	{
		return this->Open(lpwstrProcessName, (DWORD)flags);
	}

	bool SlimMem::Open(const wchar_t * lpwstrProcessName, DWORD flags)
	{
		DWORD pid;
		if (GetPID(lpwstrProcessName, &pid))
			return this->Open(pid, flags);
		return false;
	}

	bool SlimMem::Open(DWORD dwPID, ProcessAccess flags)
	{
		return this->Open(dwPID, (DWORD)flags);
	}

	bool SlimMem::Open(DWORD dwPID, DWORD dwFlags)
	{
		if (this->HasProcessHandle())
			return false;

		m_hProc = OpenProcess(dwFlags | PROCESS_DUP_HANDLE, false, dwPID);
		m_dwPID = dwPID;
		if (this->HasProcessHandle())
			this->ParseModules();

		return this->HasProcessHandle();
	}
#pragma endregion

#pragma region Utility
	/*
	Attempts to find a process with a given name and sets the given PID
	Returns whether a matching process was found or not
	*/
	BOOL SlimMem::GetPID(const wchar_t * lpwstrProcessName, DWORD* pid)
	{
		PROCESSENTRY32W proc;
		proc.dwSize = sizeof(PROCESSENTRY32W);
		HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		*pid = -1;

		if (!IsHandleValid(hSnap))
			return false;

		if (Process32FirstW(hSnap, &proc)) {
			do {
				if (wcscmp(lpwstrProcessName, proc.szExeFile) == 0)
				{
					ProperlyCloseHandle(hSnap);
					*pid = proc.th32ProcessID;
					return true;
				}
			} while (Process32NextW(hSnap, &proc));
		}

		ProperlyCloseHandle(hSnap);
		return false;
	}

	/*
	Caches basic information of modules loaded by the opened-process

	*/
	bool SlimMem::ParseModules()
	{
		if (!this->HasProcessHandle())
			return false;

		m_mModules.clear();

		MODULEENTRY32W mod;
		mod.dwSize = sizeof(MODULEENTRY32W);

		HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, m_dwPID);
		if (!IsHandleValid(hSnap))
			return false;

		if (Module32FirstW(hSnap, &mod)) {
			do {
				try {
					if (m_mModules.find(std::wstring(mod.szModule)) == m_mModules.end())
						m_mModules[ToLower(mod.szModule)] = std::make_unique<SlimModule>(mod, *this);
				}
				catch (...) {
#ifdef REPORT_ERRORS
					std::cout << "[SlimMem] Failed to parse module \"" << mod.szModule << "\"" << std::endl;
#endif
				}
			} while (Module32NextW(hSnap, &mod));
		}

		ProperlyCloseHandle(hSnap);
		return true;
	}

	SigScanResult SlimMem::PerformSigScan(const BYTE * bufPattern, const char * lpcstrMask, const wchar_t * lpwstrModuleName, DWORD startFromOffset)
	{
		auto module = this->GetModule(lpwstrModuleName);
		if (module == nullptr)
			return SigScanResult(false);
		std::string mask(lpcstrMask);

		if (mask.empty())
			return SigScanResult(false);

		if (module->dwSize <= startFromOffset)
			return SigScanResult(false);

		if (startFromOffset > module->dwSize - mask.size())
			return SigScanResult(false);

		BYTE *dump = new BYTE[module->dwSize];

		SIZE_T bytesRead;
		if (!ReadProcessMemory(this->m_hProc, (LPCVOID)module->ptrBase, dump, module->dwSize, &bytesRead) || bytesRead != module->dwSize)
			return SigScanResult(false);

		bool found = false;
		for (DWORD i = startFromOffset; i < module->dwSize - mask.size(); i++) {
			found = true;
			for (DWORD idx = 0; idx < mask.size(); idx++) {
				if (mask[idx] == 'x' && bufPattern[idx] != dump[i + idx]) {
					found = false;
					break;
				}
			}
			if (found) {
				SigScanResult result(true, i, dump + i, mask.size());
				delete[] dump;
				return result;
			}
		}

		delete[] dump;

		return SigScanResult(false);
	}

	const SlimModule* SlimMem::GetModule(const wchar_t * lpwstrModuleName) const
	{
		std::wstring name = ToLower(std::wstring(lpwstrModuleName));
		auto val = m_mModules.find(name);
		if (val == m_mModules.end())
			return nullptr;

		return (*val).second.get();
	}

#pragma endregion
}