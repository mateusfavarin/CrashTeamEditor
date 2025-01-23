#include "material.h"

#include <vector>
#include <cstdint>

template class MaterialProperty<std::string>;
template class MaterialProperty<uint16_t>;
template class MaterialProperty<bool>;

static std::vector<MaterialBase*> g_materials;

void ClearMaterials()
{
	for (MaterialBase* material : g_materials) { material->Clear(); }
}

void RestoreMaterials()
{
	for (MaterialBase* material : g_materials) { material->Restore(); }
}

template<typename T>
MaterialProperty<T>::MaterialProperty()
{
	m_preview = std::unordered_map<std::string, T>();
	m_backup = std::unordered_map<std::string, T>();
	g_materials.push_back(this);
}

template<typename T>
void MaterialProperty<T>::SetPreview(const std::string& material, const T& preview)
{
	m_preview[material] = preview;
	m_materialsChanged.insert(material);
}

template<typename T>
void MaterialProperty<T>::SetBackup(const std::string& material, const T& backup)
{
	m_backup[material] = backup;
	if (m_materialsChanged.contains(material)) { m_materialsChanged.erase(material); }
}

template<typename T>
void MaterialProperty<T>::SetDefaultValue(const std::string& material, const T& value)
{
	m_preview[material] = value;
	m_backup[material] = value;
}

template<typename T>
bool MaterialProperty<T>::UnsavedChanges(const std::string& material) const
{
	if (!m_materialsChanged.contains(material)) { return false; }
	return m_preview.at(material) != m_backup.at(material);
}

template<typename T>
void MaterialProperty<T>::Restore()
{
	if (m_materialsChanged.empty()) { return; }

	for (const std::string& material : m_materialsChanged)
	{
		m_preview[material] = m_backup[material];
	}
	m_materialsChanged.clear();
}

template<typename T>
void MaterialProperty<T>::Clear()
{
	m_preview.clear();
	m_backup.clear();
	m_materialsChanged.clear();
}

template<typename T>
T& MaterialProperty<T>::GetPreview(const std::string& material)
{
	m_materialsChanged.insert(material);
	return m_preview[material];
}
