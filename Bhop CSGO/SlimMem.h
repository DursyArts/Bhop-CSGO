#pragma once

/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** SlimMem.h
** A simple to use memory-manipulation class
**
** Author: Zat
** More information: https://www.unknowncheats.me/forum/c-and-c/167302-slimmem-simple-use-memory-manipulation-class.html
** -------------------------------------------------------------------------*/

#include <windows.h>	//Windows-functions (OpenProcess, RPM, WPM, etc)
#include <tlhelp32.h>	//Functions that gather process-information
#include <Psapi.h>		//Functions that gather module-information
#include <map>			//Data-container that saves parsed modules 
#include <string>		//String/WString implementation
#include <cctype>		//tolower-function that converts a char to lowercase
#include <algorithm>	//transform-function that is used to apply the tolower-function to a wstring
#include <cassert>		//Used for debugging
#include <type_traits>	//Used to restrict usage of template functions Read and Write to specific types
#include <iostream>		//cout
#include <memory>		//unique_ptr

namespace SlimUtils {

	//Exclude module-names from SlimModule-structs
#define VERYSLIM_SLIMMODULE
	//Enable error-reports
#define REPORT_ERRORS

	inline bool IsProcessHandleValid(HANDLE h) { return h > 0 && h != INVALID_HANDLE_VALUE; }
	inline bool IsHandleValid(HANDLE h) { return h != INVALID_HANDLE_VALUE; }
	inline BOOL ProperlyCloseHandle(HANDLE h) {
		auto const b = CloseHandle(h);
		assert(b);
		return b;
	}


	static std::wstring ToLower(std::wstring string) {
		transform(string.begin(), string.end(), string.begin(), tolower);
		return string;
	}

	struct SlimModule;
	struct SigScanResult;
	class SlimMem;

	/*
	Contains basic information about a single module
	*/
	struct SlimModule {
		std::uintptr_t ptrBase;
		DWORD  dwSize;

		SlimModule(const MODULEENTRY32W& mod, const SlimMem& mem) {
			ptrBase = (std::uintptr_t)mod.modBaseAddr;
			dwSize = mod.modBaseSize;
		}
	};

	/*
	Holds information about a signature-scan
	*/
	struct SigScanResult {
		bool m_Success;
		BYTE *m_Data;
		DWORD m_DataLength;
		DWORD m_Offset;

		SigScanResult() : m_Success(false), m_Offset(0), m_DataLength(0), m_Data(nullptr) {
		}

		SigScanResult(bool p_Success) : m_Success(p_Success), m_Offset(0), m_DataLength(0), m_Data(nullptr) {
		}

		SigScanResult(bool p_Success, DWORD p_Offset, BYTE *p_Data, DWORD p_DataLength) : m_Success(p_Success), m_Offset(p_Offset), m_DataLength(p_DataLength) {
			if (p_Data != nullptr) {
				m_Data = new BYTE[m_DataLength];
				memcpy_s(m_Data, m_DataLength, p_Data, m_DataLength);
			}
		}

		SigScanResult(const SigScanResult& other) : SigScanResult(other.m_Success, other.m_Offset, other.m_Data, other.m_DataLength) {
		}

		~SigScanResult() {
			if (m_Data != nullptr)
				delete[] m_Data;
		}

		SigScanResult& operator=(const SigScanResult& other) {
			if (this->m_Data != nullptr) {
				delete[] this->m_Data;
				this->m_Data = nullptr;
			}
			this->m_Success = other.m_Success;
			this->m_Offset = other.m_Offset;

			if (other.m_Data != nullptr) {
				this->m_DataLength = other.m_DataLength;
				this->m_Data = new BYTE[other.m_DataLength];
				memcpy_s(this->m_Data, this->m_DataLength, other.m_Data, other.m_DataLength);
			}
			return *this;
		}

		template <typename T>
		bool Read(T& value, DWORD index) const {
			if (index + sizeof(T) >= m_DataLength)
				return false;

			value = *(T*)(reinterpret_cast<DWORD>(m_Data) + index);
			return true;
		}
	};

	/*
	Offers a simple collection of combination of process-access flags
	*/
	enum ProcessAccess : DWORD
	{
		Full = PROCESS_ALL_ACCESS,
		ReadOnly = PROCESS_VM_OPERATION | PROCESS_VM_READ,
		WriteOnly = PROCESS_VM_OPERATION | PROCESS_VM_WRITE,
		ReadWrite = ReadOnly | WriteOnly
	};

	/*
	A class that provides basic functions that are used to write to/read from process memory
	*/
	class SlimMem {
	public:
		SlimMem() : m_hProc(INVALID_HANDLE_VALUE) { }
		SlimMem(const SlimMem& copy);
		~SlimMem();

		bool Open(const wchar_t* lpwstrProcessName, ProcessAccess flags);
		bool Open(const wchar_t* lpwstrProcessName, DWORD dwFlags);
		bool Open(DWORD dwPID, ProcessAccess flags);
		bool Open(DWORD dwPID, DWORD dwFlags);
		void Close();

		bool HasProcessHandle() const { return IsProcessHandleValid(m_hProc); }
		const SlimModule* GetModule(const wchar_t* lpwstrModuleName) const;
		bool ParseModules();
		SigScanResult PerformSigScan(const BYTE *bufPattern, const char* lpcstrMask, const wchar_t* lpwstrModuleName, DWORD startFromOffset = 0);

		template <typename T>
		T Read(std::uintptr_t ptrAddress) const;

		template <typename T>
		bool Read(std::uintptr_t ptrAddress, T& value) const;

		template <typename T>
		T ReadPtr(std::uintptr_t ptrAddress, std::initializer_list<std::uintptr_t> ilOffsets) const;

		template <typename T>
		bool Write(std::uintptr_t ptrAddress, T value) const;

		static BOOL GetPID(const wchar_t* lpwstrProcessName, DWORD* pid);
	private:
		HANDLE m_hProc;
		DWORD m_dwPID;
		std::map<std::wstring, std::unique_ptr<SlimModule>> m_mModules;
	};

#pragma region Read/Write
	/*
	Reads data from memory
	Returns the value read from memory; Returns default-value in case an error occured
	*/
	template<typename T>
	inline T SlimMem::Read(std::uintptr_t ptrAddress) const
	{
		static_assert(std::is_trivially_copyable<T>::value, "Invalid RPM/WPM type");

		T val = T();
		if (!this->HasProcessHandle())
			return val;

		ReadProcessMemory(this->m_hProc, (LPCVOID)ptrAddress, &val, sizeof(T), NULL);
		return val;
	}

	/*
	Reads data from memory
	Returns whether the operation was successful or not!
	*/
	template<typename T>
	inline bool SlimUtils::SlimMem::Read(std::uintptr_t ptrAddress, T & value) const
	{
		static_assert(std::is_trivially_copyable<T>::value, "Invalid RPM/WPM type");

		SIZE_T bytesRead;

		if (!this->HasProcessHandle())
			return false;

		return ReadProcessMemory(m_hProc, (LPCVOID)ptrAddress, &value, sizeof(T), &bytesRead) && bytesRead == sizeof(T);
	}

	/*
	Reads data from memory
	Returns the value read from memory; Returns default-value in case an error occured
	*/
	template<typename T>
	inline T SlimMem::ReadPtr(std::uintptr_t ptrAddress, std::initializer_list<std::uintptr_t> ilOffsets) const
	{
		static_assert(std::is_trivially_copyable<T>::value, "Invalid RPM/WPM type");

		if (!this->HasProcessHandle())
			return false;
		for (auto it = ilOffsets.begin(); it != ilOffsets.end(); it++) {
			if ((std::uintptr_t*)(it + 1) == ilOffsets.end()) {
				//Read value
				return this->Read<T>(ptrAddress + *it);
			}
			else {
				//Read offset
				ptrAddress = this->Read<std::uintptr_t>(ptrAddress + *it);
			}
		}
		return T();
	}

	/*
	Writes data to memory
	Returns whether the operation was successful or not!
	*/
	template<typename T>
	inline bool SlimMem::Write(std::uintptr_t ptrAddress, T value) const
	{
		static_assert(std::is_trivially_copyable<T>::value, "Invalid RPM/WPM type");

		if (!this->HasProcessHandle())
			return false;
		return WriteProcessMemory(this->m_hProc, (LPVOID)ptrAddress, &value, sizeof(T), NULL);
	}
#pragma endregion
}