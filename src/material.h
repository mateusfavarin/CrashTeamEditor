#pragma once

#include <unordered_map>
#include <string>

template <typename T>
class MaterialProperty
{
public:
	MaterialProperty();
	void SetPreview(const std::string& material, const T& preview);
	void SetBackup(const std::string& material, const T& backup);
	void SetDefaultValue(const std::string& material, const T& value);
	void Restore();
	void Clear();
	T& GetPreview(const std::string& material);

private:
	std::unordered_map<std::string, T> m_backup;
	std::unordered_map<std::string, T> m_preview;
	std::vector<std::string> m_materialsChanged;
};