#pragma once

#include <fstream>

template<typename T> static inline void Read(std::ifstream& file, T& data)
{
	file.read(reinterpret_cast<char*>(&data), sizeof(data));
}

template<typename T> static inline void Write(std::ofstream& file, T* data, size_t size)
{
	file.write(reinterpret_cast<const char*>(data), size);
}