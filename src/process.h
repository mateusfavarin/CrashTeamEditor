#pragma once

#include <string>
#include <cstdint>

namespace Process
{
	static constexpr int INVALID_PID = -1;
	extern uint8_t* gEmuRAM;

	int GetPID(const std::string& name);
	bool OpenMemoryMap(const std::string& name, size_t size);

	template<typename T>
	inline T& At(const size_t address)
	{
		return *reinterpret_cast<T*>(&gEmuRAM[address & 0xFFFFFF]);
	}
}