#include "string.h"

std::vector<std::string> Split(const std::string& str, char sep)
{
	std::vector<std::string> ret;
	std::string s;
	for (const char c : str)
	{
		if (c == sep && !s.empty())
		{
			ret.push_back(s);
			s.clear();
		}
		else { s += c; }
	}
	if (!s.empty()) { ret.push_back(s); }
	return ret;
}
