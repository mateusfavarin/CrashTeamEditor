#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include <functional>

std::vector<std::string> Split(const std::string& str, char sep = ' ');
std::string Lower(const std::string& s);
bool Matches(const std::string& a, const std::string& b);

template<typename T>
T Clamp(T n, T min, T max) { return std::max(std::min(n, max), min); }

template<typename T>
inline void HashCombine(std::size_t& seed, const T& value) noexcept
{
	seed ^= std::hash<T>{}(value) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
}

template<typename T>
void Swap(T& x, T& y)
{
	T temp = y;
	y = x;
	x = temp;
}
