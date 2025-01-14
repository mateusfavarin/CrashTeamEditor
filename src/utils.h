#pragma once

#include <string>
#include <vector>

std::vector<std::string> Split(const std::string& str, char sep = ' ');
std::string Lower(const std::string& s);
bool Matches(const std::string& a, const std::string& b);