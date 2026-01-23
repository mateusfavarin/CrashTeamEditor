#include "process.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Psapi.h>
#pragma comment(lib, "psapi.lib")
static int GetPIDWindows(const std::string& name)
{
	DWORD aProcesses[1024], cbNeeded, cProcesses;
	EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded);
	cProcesses = cbNeeded / sizeof(DWORD);
	for (DWORD i = 0; i < cProcesses; i++)
	{
		DWORD processID = aProcesses[i];
		if (processID != 0)
		{
			HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);
			if (NULL != hProcess)
			{
				HMODULE hMod;
				if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded))
				{
					std::string processName; processName.resize(MAX_PATH);
					GetModuleBaseNameA(hProcess, hMod, processName.data(), static_cast<DWORD>(processName.size() * sizeof(CHAR)));

					if (processName.find(name) != std::string::npos)
					{
						return static_cast<int>(processID);
					}
				}
			}
		}
	}
	return Process::INVALID_PID;
}

static bool OpenMemoryMapWindows(const std::string& name, size_t size)
{
	std::wstring wideMapName = std::wstring(name.begin(), name.end());
	HANDLE hFile = OpenFileMapping(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, const_cast<wchar_t*>(wideMapName.c_str()));
	if (!hFile) { return false; }
	Process::gEmuRAM = static_cast<uint8_t*>(MapViewOfFile(hFile, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, size));
	return Process::gEmuRAM != nullptr;
}

#define GetPIDFunc GetPIDWindows
#define OpenMemoryMapFunc OpenMemoryMapWindows
#else
#include <cctype>
#include <filesystem>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

static int GetPIDLinux(const std::string& name)
{
	if (name.empty()) { return Process::INVALID_PID; }
	try
	{
		for (const auto& entry : std::filesystem::directory_iterator("/dev/shm/"))
		{
			const std::string filename = entry.path().filename().string();
			if (filename.rfind(name, 0) != 0) { continue; }
			size_t pos = name.size();
			if (pos >= filename.size() || !std::isdigit(static_cast<unsigned char>(filename[pos]))) { continue; }

			std::string pidStr;
			while (pos < filename.size() && std::isdigit(static_cast<unsigned char>(filename[pos])))
			{
				pidStr.push_back(filename[pos]);
				++pos;
			}
			if (!pidStr.empty())
			{
				return std::stoi(pidStr);
			}
		}
	}
	catch (const std::exception&)
	{
		return Process::INVALID_PID;
	}
	return Process::INVALID_PID;
}

static bool OpenMemoryMapLinux(const std::string& name, size_t size)
{
	std::string mapName = name;
	if (mapName.empty()) { return false; }
	if (mapName.front() != '/') { mapName.insert(mapName.begin(), '/'); }
	int fd = shm_open(mapName.c_str(), O_RDWR, 0600);
	if (fd == -1) { return false; }
	void* addr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (addr == MAP_FAILED) { return false; }
	Process::gEmuRAM = static_cast<uint8_t*>(addr);
	return Process::gEmuRAM != nullptr;
}

#define GetPIDFunc GetPIDLinux
#define OpenMemoryMapFunc OpenMemoryMapLinux
#endif

namespace Process
{
	uint8_t* gEmuRAM = nullptr;

	int GetPID(const std::string& name)
	{
		return GetPIDFunc(name);
	}

	bool OpenMemoryMap(const std::string& name, size_t size)
	{
		return OpenMemoryMapFunc(name, size);
	}
}
