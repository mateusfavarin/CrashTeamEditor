#pragma once

#include <string>
#include <vector>

std::vector<std::string> Split(const std::string& str, char sep = ' ');
std::string Lower(const std::string& s);
bool Matches(const std::string& a, const std::string& b);
bool ParseFloat(const std::string& str, float& out);

template<typename T>
T Clamp(T n, T min, T max) { return std::max(std::min(n, max), min); }

template<typename T>
void Swap(T& x, T& y)
{
	T temp = y;
	y = x;
	x = temp;
}