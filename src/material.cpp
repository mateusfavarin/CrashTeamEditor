#include "material.h"

#include <vector>
#include <cstdint>
#include <functional>

template class MaterialProperty<std::string, MaterialType::TERRAIN>;
template class MaterialProperty<uint16_t, MaterialType::QUAD_FLAGS>;
template class MaterialProperty<bool, MaterialType::DRAW_FLAGS>;
template class MaterialProperty<bool, MaterialType::CHECKPOINT>;

static std::unordered_map<Level*, std::vector<MaterialBase*>> g_materials;

void ClearMaterials(Level* level)
{
	for (MaterialBase* material : g_materials[level]) { material->Clear(); }
}

void RestoreMaterials(Level* level)
{
	for (MaterialBase* material : g_materials[level]) { material->Restore(); }
}

void DeleteMaterials(Level* level)
{
	g_materials.erase(level);
}

template<typename T, MaterialType M>
MaterialProperty<T, M>::MaterialProperty()
{
	m_preview = std::unordered_map<std::string, T>();
	m_backup = std::unordered_map<std::string, T>();
}

template<typename T, MaterialType M>
void MaterialProperty<T, M>::RegisterMaterial(Level* level)
{
	if (g_materials.contains(level)) { g_materials[level].push_back(this); }
	else { g_materials.insert({level, { this }}); }
}

template<typename T, MaterialType M>
void MaterialProperty<T, M>::SetPreview(const std::string& material, const T& preview)
{
	m_preview[material] = preview;
	m_materialsChanged.insert(material);
}

template<typename T, MaterialType M>
void MaterialProperty<T, M>::SetDefaultValue(const std::string& material, const T& value)
{
	m_preview[material] = value;
	m_backup[material] = value;
}

template<typename T, MaterialType M>
bool MaterialProperty<T, M>::UnsavedChanges(const std::string& material) const
{
	return m_materialsChanged.contains(material);
}

template<typename T, MaterialType M>
void MaterialProperty<T, M>::Restore()
{
	if (m_materialsChanged.empty()) { return; }

	for (const std::string& material : m_materialsChanged)
	{
		m_preview[material] = m_backup[material];
	}
	m_materialsChanged.clear();
}

template<typename T, MaterialType M>
void MaterialProperty<T, M>::Clear()
{
	m_preview.clear();
	m_backup.clear();
	m_materialsChanged.clear();
}

template<typename T, MaterialType M>
T& MaterialProperty<T, M>::GetPreview(const std::string& material)
{
	if (m_preview[material] != m_backup[material]) { m_materialsChanged.insert(material); }
	return m_preview[material];
}

template<typename T, MaterialType M>
const T& MaterialProperty<T, M>::GetBackup(const std::string& material)
{
	return m_backup[material];
}

template<typename T, MaterialType M>
void MaterialProperty<T, M>::Apply(const std::string& material, const std::vector<size_t>& quadblockIndexes, std::vector<Quadblock>& quadblocks)
{
	T preview = m_preview[material];
	for (const size_t index : quadblockIndexes)
	{
		Quadblock& quadblock = quadblocks[index];
		if (!quadblock.Hide())
		{
			if constexpr (M == MaterialType::TERRAIN) { quadblock.SetTerrain(TerrainType::LABELS.at(preview)); }
			else if constexpr (M == MaterialType::QUAD_FLAGS) { quadblock.SetFlag(preview); }
			else if constexpr (M == MaterialType::DRAW_FLAGS) { quadblock.SetDrawDoubleSided(preview); }
			else if constexpr (M == MaterialType::CHECKPOINT) { quadblock.CheckpointStatus() = preview; }
		}
	}
	m_backup[material] = preview;
	if (m_materialsChanged.contains(material)) { m_materialsChanged.erase(material); }
}