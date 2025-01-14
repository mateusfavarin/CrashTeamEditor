#include "utils.h"

std::vector<std::string> Split(const std::string& str, char sep)
{
	std::vector<std::string> ret;
	std::string s;
	for (const char c : str)
	{
		if (c == sep)
		{
			ret.push_back(s);
			s.clear();
		}
		else { s += c; }
	}
	if (!s.empty()) { ret.push_back(s); }
	return ret;
}

std::string Lower(const std::string& s)
{
	std::string ret;
	for (const char c : s) { ret.push_back(static_cast<char>(std::tolower(c))); }
	return ret;
}

bool Matches(const std::string& a, const std::string& b)
{
	std::string a_lower = Lower(a);
	std::string b_lower = Lower(b);
	return a_lower.find(b_lower) != std::string::npos;
}
