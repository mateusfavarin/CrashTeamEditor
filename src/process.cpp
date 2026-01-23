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
#include <vector>
#include <algorithm>
#include <sys/mman.h>
#include <unistd.h>

static int GetPIDLinux(const std::string& name)
{
	std::vector<std::string> tags;
	auto AddTag = [&tags](const std::string& tag)
		{
			if (tag.empty()) { return; }
			if (std::find(tags.begin(), tags.end(), tag) == tags.end())
			{
				tags.push_back(tag);
			}
		};

	if (!name.empty()) { AddTag(name + "_"); }
#if defined(_DEBUG)
	AddTag("pcsx-redux-wram-");
#else
	AddTag("duckstation_");
#endif

	try
	{
		for (const auto& entry : std::filesystem::directory_iterator("/dev/shm/"))
		{
			const std::string filename = entry.path().filename().string();
			for (const std::string& tag : tags)
			{
				const size_t matchPos = filename.find(tag);
				if (matchPos == std::string::npos) { continue; }
				size_t pos = matchPos + tag.size();
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
	}
	catch (const std::exception&)
	{
		return Process::INVALID_PID;
	}
	return Process::INVALID_PID;
}

static bool OpenMemoryMapLinux(const std::string& name, size_t size)
{
	auto TryOpenMap = [size](const std::string& rawName) -> uint8_t*
		{
			std::string mapName = rawName;
			if (mapName.empty()) { return nullptr; }
			if (mapName.front() != '/') { mapName.insert(mapName.begin(), '/'); }
			int fd = shm_open(mapName.c_str(), O_RDWR, 0600);
			if (fd == -1) { return nullptr; }
			void* addr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
			close(fd);
			if (addr == MAP_FAILED) { return nullptr; }
			return static_cast<uint8_t*>(addr);
		};

	Process::gEmuRAM = TryOpenMap(name);
	if (Process::gEmuRAM) { return true; }

	std::string trimmed = name;
	if (!trimmed.empty() && trimmed.front() == '/') { trimmed.erase(trimmed.begin()); }

	size_t digitPos = trimmed.size();
	while (digitPos > 0 && std::isdigit(static_cast<unsigned char>(trimmed[digitPos - 1])))
	{
		--digitPos;
	}
	if (digitPos == trimmed.size()) { return false; }
	const std::string pidStr = trimmed.substr(digitPos);

#if defined(_DEBUG)
	const std::string altName = "pcsx-redux-wram-" + pidStr;
#else
	const std::string altName = "duckstation_" + pidStr;
#endif

	if (altName != trimmed)
	{
		Process::gEmuRAM = TryOpenMap(altName);
	}
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
