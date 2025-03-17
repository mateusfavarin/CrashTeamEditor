#include "geo.h"
#include "bsp.h"
#include "checkpoint.h"
#include "level.h"
#include "path.h"
#include "quadblock.h"
#include "vertex.h"
#include "utils.h"
#include "renderer.h"
#include "gui_render_settings.h"
#include "mesh.h"
#include "texture.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <portable-file-dialogs.h>

#include <string>
#include <chrono>
#include <functional>
#include <cmath>

static constexpr size_t MAX_QUADBLOCKS_LEAF = 32;
static constexpr float MAX_LEAF_AXIS_LENGTH = 60.0f;

class ButtonUI
{
public:
  ButtonUI();
  ButtonUI(long long timeout);
  bool Show(const std::string& label, const std::string& message, bool unsavedChanges);

private:
  static constexpr long long DEFAULT_TIMEOUT = 1;
  long long m_timeout;
  std::string m_labelTriggered;
  std::chrono::time_point<std::chrono::high_resolution_clock> m_messageTimeoutStart;
};

ButtonUI::ButtonUI()
{
  m_timeout = DEFAULT_TIMEOUT;
  m_labelTriggered = std::string();
  m_messageTimeoutStart = std::chrono::high_resolution_clock::now();
}

ButtonUI::ButtonUI(long long timeout)
{
  m_timeout = timeout;
  m_labelTriggered = std::string();
  m_messageTimeoutStart = std::chrono::high_resolution_clock::now();
}

bool ButtonUI::Show(const std::string& label, const std::string& message, bool unsavedChanges)
{
  bool ret = false;
  if (ImGui::Button(label.c_str()))
  {
    m_labelTriggered = label;
    m_messageTimeoutStart = std::chrono::high_resolution_clock::now();
    ret = true;
  }
  if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - m_messageTimeoutStart).count() < m_timeout
    && m_labelTriggered == label)
  {
    ImGui::Text(message.c_str());
  }
  else if (unsavedChanges)
  {
    const ImVec4 redColor = {247.0f / 255.0f, 44.0f / 255.0f, 37.0f / 255.0f, 1.0f};
    ImGui::PushStyleColor(ImGuiCol_Text, redColor);
    ImGui::Text("Unsaved changes.");
    ImGui::PopStyleColor();
  }
  return ret;
}

template<typename T>
static bool UIFlagCheckbox(T& var, const T flag, const std::string& title)
{
  bool active = var & flag;
  if (ImGui::Checkbox(title.c_str(), &active))
  {
    if (active) { var |= flag; }
    else { var &= ~flag; }
    return true;
  }
  return false;
}

void BoundingBox::RenderUI() const
{
  ImGui::Text("Max:"); ImGui::SameLine();
  ImGui::BeginDisabled();
  ImGui::InputFloat3("##max", const_cast<float*>(&max.x));
  ImGui::EndDisabled();
  ImGui::Text("Min:"); ImGui::SameLine();
  ImGui::BeginDisabled();
  ImGui::InputFloat3("##min", const_cast<float*>(&min.x));
  ImGui::EndDisabled();
}

void BSP::RenderUI(size_t& index, const std::vector<Quadblock>& quadblocks)
{
  std::string title = GetType() + " " + std::to_string(index++);
  if (ImGui::TreeNode(title.c_str()))
  {
    if (IsBranch()) { ImGui::Text(("Axis:  " + GetAxis()).c_str()); }
    ImGui::Text(("Quads: " + std::to_string(m_quadblockIndexes.size())).c_str());
    if (ImGui::TreeNode("Quadblock List:"))
    {
      constexpr size_t QUADS_PER_LINE = 10;
      for (size_t i = 0; i < m_quadblockIndexes.size(); i++)
      {
        ImGui::Text((quadblocks[m_quadblockIndexes[i]].GetName() + ", ").c_str());
        if (((i + 1) % QUADS_PER_LINE) == 0 || i == m_quadblockIndexes.size() - 1) { continue; }
        ImGui::SameLine();
      }
      ImGui::TreePop();
    }
    ImGui::Text("Bounding Box:");
    m_bbox.RenderUI();
    if (m_left) { m_left->RenderUI(++index, quadblocks); }
    if (m_right) { m_right->RenderUI(++index, quadblocks); }
    ImGui::TreePop();
  }
}

void Checkpoint::RenderUI(size_t numCheckpoints, const std::vector<Quadblock>& quadblocks)
{
  if (ImGui::TreeNode(("Checkpoint " + std::to_string(m_index)).c_str()))
  {
    ImGui::Text("Pos:       "); ImGui::SameLine(); ImGui::InputFloat3("##pos", m_pos.Data());
    ImGui::Text("Quad:      "); ImGui::SameLine();
    if (ImGui::BeginCombo("##quad", m_uiPosQuad.c_str()))
    {
      for (const Quadblock& quadblock : quadblocks)
      {
        if (ImGui::Selectable(quadblock.GetName().c_str()))
        {
          m_uiPosQuad = quadblock.GetName();
          m_pos = quadblock.GetCenter();
        }
      }
      ImGui::EndCombo();
    }
    ImGui::SetItemTooltip("Update checkpoint position by selecting a specific quadblock.");

    ImGui::Text("Distance:  "); ImGui::SameLine();
    if (ImGui::InputFloat("##dist", &m_distToFinish)) { m_distToFinish = m_distToFinish < 0.0f ? 0.0f : m_distToFinish; }
    ImGui::SetItemTooltip("Distance from checkpoint to the finish line.");

    auto LinkUI = [](int index, size_t numCheckpoints, int& dir, std::string& s, const std::string& title)
      {
        if (dir == NONE_CHECKPOINT_INDEX) { s = DEFAULT_UI_CHECKBOX_LABEL; }
        ImGui::Text(title.c_str()); ImGui::SameLine();
        if (ImGui::BeginCombo(("##" + title).c_str(), s.c_str()))
        {
          if (ImGui::Selectable(DEFAULT_UI_CHECKBOX_LABEL.c_str())) { dir = NONE_CHECKPOINT_INDEX; }
          for (int i = 0; i < numCheckpoints; i++)
          {
            if (i == index) { continue; }
            std::string str = "Checkpoint " + std::to_string(i);
            if (ImGui::Selectable(str.c_str()))
            {
              s = str;
              dir = i;
            }
          }
          ImGui::EndCombo();
        }
      };

    LinkUI(m_index, numCheckpoints, m_up, m_uiLinkUp, "Link up:   ");
    LinkUI(m_index, numCheckpoints, m_down, m_uiLinkDown, "Link down: ");
    LinkUI(m_index, numCheckpoints, m_left, m_uiLinkLeft, "Link left: ");
    LinkUI(m_index, numCheckpoints, m_right, m_uiLinkRight, "Link right:");

    if (ImGui::Button("Delete")) { m_delete = true; }
    ImGui::TreePop();
  }
}

template<typename T, MaterialType M>
void MaterialProperty<T, M>::RenderUI(const std::string& material, const std::vector<size_t>& quadblockIndexes, std::vector<Quadblock>& quadblocks)
{
  if constexpr (M == MaterialType::TERRAIN)
  {
    ImGui::Text("Terrain:"); ImGui::SameLine();
    if (ImGui::BeginCombo("##terrain", GetPreview(material).c_str()))
    {
      for (const auto& [label, terrain] : TerrainType::LABELS)
      {
        if (ImGui::Selectable(label.c_str()))
        {
          SetPreview(material, label);
        }
      }
      ImGui::EndCombo();
    } ImGui::SameLine();

    static ButtonUI terrainApplyButton = ButtonUI();
    if (terrainApplyButton.Show(("Apply##terrain" + material).c_str(), "Terrain type successfully updated.", UnsavedChanges(material)))
    {
      Apply(material, quadblockIndexes, quadblocks);
    }
  }
  else if constexpr (M == MaterialType::QUAD_FLAGS)
  {
    if (ImGui::TreeNode("Quad Flags"))
    {
      for (const auto& [label, flag] : QuadFlags::LABELS)
      {
        UIFlagCheckbox(GetPreview(material), flag, label);
      }

      static ButtonUI quadFlagsApplyButton = ButtonUI();
      if (quadFlagsApplyButton.Show(("Apply##quadflags" + material).c_str(), "Quad flags successfully updated.", UnsavedChanges(material)))
      {
        Apply(material, quadblockIndexes, quadblocks);
      }
      static ButtonUI killPlaneButton = ButtonUI();
      if (killPlaneButton.Show("Kill Plane##quadflags", "Modified quad flags to kill plane.", false))
      {
        SetPreview(material, QuadFlags::INVISIBLE_TRIGGER | QuadFlags::OUT_OF_BOUNDS | QuadFlags::MASK_GRAB | QuadFlags::WALL | QuadFlags::NO_COLLISION);
        Apply(material, quadblockIndexes, quadblocks);
      }
      ImGui::TreePop();
    }
  }
  else if constexpr (M == MaterialType::DRAW_FLAGS)
  {
    if (ImGui::TreeNode("Draw Flags"))
    {
      T& preview = GetPreview(material);
      ImGui::Checkbox("Double Sided", &preview);

      static ButtonUI drawFlagsApplyButton = ButtonUI();
      if (drawFlagsApplyButton.Show(("Apply##drawflags" + material).c_str(), "Draw flags successfully updated.", UnsavedChanges(material)))
      {
        Apply(material, quadblockIndexes, quadblocks);
      }
      ImGui::TreePop();
    }
  }
  else if constexpr (M == MaterialType::CHECKPOINT)
  {
    T& preview = GetPreview(material);
    ImGui::Checkbox("Checkpoint", &preview);
    ImGui::SameLine();

    static ButtonUI checkpointApplyButton = ButtonUI();
    if (checkpointApplyButton.Show(("Apply##checkpoint" + material).c_str(), "Checkpoint status successfully updated.", UnsavedChanges(material)))
    {
      Apply(material, quadblockIndexes, quadblocks);
    }
  }
}

void Level::RenderUI()
{
	if (m_showLogWindow)
	{
		if (ImGui::Begin("Log", &m_showLogWindow))
		{
			if (!m_logMessage.empty()) { ImGui::Text(m_logMessage.c_str()); }
			if (!m_invalidQuadblocks.empty())
			{
				ImGui::Text("Error - the following quadblocks are not in the valid format:");
				for (size_t i = 0; i < m_invalidQuadblocks.size(); i++)
				{
					const std::string& quadblock = std::get<0>(m_invalidQuadblocks[i]);
					const std::string& errorMessage = std::get<1>(m_invalidQuadblocks[i]);
					if (ImGui::TreeNode((quadblock + "##" + std::to_string(i)).c_str()))
					{
						ImGui::Text(errorMessage.c_str());
						ImGui::TreePop();
					}
				}
			}
		}
		ImGui::End();
	}

	if (m_showHotReloadWindow)
	{
		if (ImGui::Begin("Hot Reload", &m_showHotReloadWindow))
		{
			static std::string levPath;
			static std::string vrmPath;
			if (levPath.empty() && !m_savedLevPath.empty()) { levPath = m_savedLevPath.string(); }
			if (vrmPath.empty() && !m_savedVRMPath.empty()) { vrmPath = m_savedVRMPath.string(); }

			ImGui::Text("Lev Path"); ImGui::SameLine();
			ImGui::InputText("##levpath", &levPath, ImGuiInputTextFlags_ReadOnly);
			ImGui::SetItemTooltip(levPath.c_str()); ImGui::SameLine();
			if (ImGui::Button("...##levhotreload"))
			{
				auto selection = pfd::open_file("Lev File", ".", {"Lev Files", "*.lev"}).result();
				if (!selection.empty()) { levPath = selection.front(); }
			}

			ImGui::Text("Vrm Path"); ImGui::SameLine();
			ImGui::InputText("##vrmpath", &vrmPath, ImGuiInputTextFlags_ReadOnly);
			ImGui::SetItemTooltip(vrmPath.c_str()); ImGui::SameLine();
			if (ImGui::Button("...##vrmhotreload"))
			{
				auto selection = pfd::open_file("Vrm File", ".", {"Vrm Files", "*.vrm"}).result();
				if (!selection.empty()) { vrmPath = selection.front(); }
			}

			const std::string successMessage = "Successfully hot reloaded.";
			const std::string failMessage = "Failed hot reloading.\nMake sure Duckstation is opened and that the game is unpaused.";

			bool disabled = levPath.empty();
			ImGui::BeginDisabled(disabled);
			static ButtonUI hotReloadButton = ButtonUI(5);
			static std::string hotReloadMessage;
			if (hotReloadButton.Show("Hot Reload##btn", hotReloadMessage, false))
			{
				if (HotReload(levPath, vrmPath, "duckstation")) { hotReloadMessage = successMessage; }
				else { hotReloadMessage = failMessage; }
			}
			ImGui::EndDisabled();
			if (disabled) { ImGui::SetItemTooltip("You must select the lev path before hot reloading."); }

			bool vrmDisabled = vrmPath.empty();
			ImGui::BeginDisabled(vrmDisabled);
			static ButtonUI vrmOnlyButton = ButtonUI(5);
			static std::string vrmOnlyMessage;
			if (vrmOnlyButton.Show("Vrm Only##btn", vrmOnlyMessage, false))
			{
				if (HotReload(std::string(), vrmPath, "duckstation")) { hotReloadMessage = successMessage; }
				else { hotReloadMessage = failMessage; }
			}
			ImGui::EndDisabled();
			if (vrmDisabled) { ImGui::SetItemTooltip("You must select the vrm path before hot reloading the vram."); }
		}
		ImGui::End();
	}

	if (!m_loaded) { return; }

	static bool w_spawn = false;
	static bool w_level = false;
	static bool w_material = false;
	static bool w_quadblocks = false;
	static bool w_checkpoints = false;
	static bool w_bsp = false;
	static bool w_renderer = false;
	static bool w_ghost = false;

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::MenuItem("Spawn")) { w_spawn = !w_spawn; }
		if (ImGui::MenuItem("Level")) { w_level = !w_level; }
		if (!m_materialToQuadblocks.empty() && ImGui::MenuItem("Material")) { w_material = !w_material; }
		if (ImGui::MenuItem("Quadblocks")) { w_quadblocks = !w_quadblocks; }
		if (ImGui::MenuItem("Checkpoints")) { w_checkpoints = !w_checkpoints; }
		if (ImGui::MenuItem("BSP Tree")) { w_bsp = !w_bsp; }
		if (ImGui::MenuItem("Renderer")) { w_renderer = !w_renderer; }
		if (ImGui::MenuItem("Ghosts")) { w_ghost = !w_ghost; }
		ImGui::EndMainMenuBar();
	}

  if (w_spawn)
  {
    if (ImGui::Begin("Spawn", &w_spawn))
    {
      for (size_t i = 0; i < NUM_DRIVERS; i++)
      {
        if (ImGui::TreeNode(("Driver " + std::to_string(i)).c_str()))
        {
          ImGui::Text("Pos:"); ImGui::SameLine(); ImGui::InputFloat3("##pos", m_spawn[i].pos.Data());
          ImGui::Text("Rot:"); ImGui::SameLine();
          if (ImGui::InputFloat3("##rot", m_spawn[i].rot.Data()))
          {
            m_spawn[i].rot.x = Clamp(m_spawn[i].rot.x, -360.0f, 360.0f);
            m_spawn[i].rot.y = Clamp(m_spawn[i].rot.y, -360.0f, 360.0f);
            m_spawn[i].rot.z = Clamp(m_spawn[i].rot.z, -360.0f, 360.0f);
          };
          GenerateRenderStartpointData(m_spawn);
          ImGui::TreePop();
        }
      }
    }
    ImGui::End();
  }

  if (w_level)
  {
    if (ImGui::Begin("Level", &w_level))
    {
      if (ImGui::TreeNode("Flags"))
      {
        UIFlagCheckbox(m_configFlags, LevConfigFlags::ENABLE_SKYBOX_GRADIENT, "Enable Skybox Gradient");
        UIFlagCheckbox(m_configFlags, LevConfigFlags::MASK_GRAB_UNDERWATER, "Mask Grab Underwater");
        UIFlagCheckbox(m_configFlags, LevConfigFlags::ANIMATE_WATER_VERTEX, "Animate Water Vertex");
        ImGui::TreePop();
      }
      if (ImGui::TreeNode("Sky Gradient"))
      {
        for (size_t i = 0; i < NUM_GRADIENT; i++)
        {
          if (ImGui::TreeNode(("Gradient " + std::to_string(i)).c_str()))
          {
            ImGui::Text("From:"); ImGui::SameLine(); ImGui::InputFloat("##pos_from", &m_skyGradient[i].posFrom);
            ImGui::Text("To:  "); ImGui::SameLine(); ImGui::InputFloat("##pos_to", &m_skyGradient[i].posTo);
            ImGui::Text("From:"); ImGui::SameLine();
            float colorFrom[3] = { m_skyGradient[i].colorFrom.Red(), m_skyGradient[i].colorFrom.Green(), m_skyGradient[i].colorFrom.Blue() };
            if (ImGui::ColorEdit3("##color_from", colorFrom))
            {
              m_skyGradient[i].colorFrom = Color(static_cast<float>(colorFrom[0]), colorFrom[1], colorFrom[2]);
            }
            ImGui::Text("To:  "); ImGui::SameLine();
            float colorTo[3] = { m_skyGradient[i].colorTo.Red(), m_skyGradient[i].colorTo.Green(), m_skyGradient[i].colorTo.Blue() };
            if (ImGui::ColorEdit3("##color_to", colorTo))
            {
              m_skyGradient[i].colorTo = Color(static_cast<float>(colorTo[0]), colorTo[1], colorTo[2]);
            }
            ImGui::TreePop();
          }
        }
        ImGui::TreePop();
      }
      if (ImGui::TreeNode("Clear Color"))
      {
        float clearColor[3] = { m_clearColor.Red(), m_clearColor.Green(), m_clearColor.Blue() };
        if (ImGui::ColorEdit3("##color", clearColor))
        {
          m_clearColor = Color(static_cast<float>(clearColor[0]), clearColor[1], clearColor[2]);
        }
        ImGui::TreePop();
      }
    }
    ImGui::End();
  }

  if (w_material)
  {
    if (ImGui::Begin("Material", &w_material))
    {
      for (const auto& [material, quadblockIndexes] : m_materialToQuadblocks)
      {
        if (ImGui::TreeNode(material.c_str()))
        {
          if (ImGui::TreeNode("Quadblocks"))
          {
            constexpr size_t QUADS_PER_LINE = 10;
            for (size_t i = 0; i < quadblockIndexes.size(); i++)
            {
              ImGui::Text((m_quadblocks[quadblockIndexes[i]].GetName() + ", ").c_str());
              if (((i + 1) % QUADS_PER_LINE) == 0 || i == quadblockIndexes.size() - 1) { continue; }
              ImGui::SameLine();
            }
            ImGui::TreePop();
          }

          m_propTerrain.RenderUI(material, quadblockIndexes, m_quadblocks);
          m_propQuadFlags.RenderUI(material, quadblockIndexes, m_quadblocks);
          m_propDoubleSided.RenderUI(material, quadblockIndexes, m_quadblocks);
          m_propCheckpoints.RenderUI(material, quadblockIndexes, m_quadblocks);

          if (m_materialToTexture.contains(material))
          {
            Texture& texture = m_materialToTexture[material];
            if (ImGui::TreeNode("Texture"))
            {
              ImGui::Text("Path:"); ImGui::SameLine();
              std::string texPath = texture.GetPath().string();
              ImGui::BeginDisabled();
              ImGui::InputText("##texpath", &texPath, ImGuiInputTextFlags_ReadOnly);
              ImGui::EndDisabled();
              ImGui::SetItemTooltip(texPath.c_str());
              if (!texture.Empty())
              {
                constexpr size_t NUM_BLEND_MODES = 4;
                const std::array<std::string, NUM_BLEND_MODES> BLEND_MODES = { "Half Transparent", "Additive", "Subtractive", "Additive Translucent" };
                uint16_t blendMode = texture.GetBlendMode();
                ImGui::Text("Blend Mode:"); ImGui::SameLine();
                if (ImGui::BeginCombo("##blendmode", BLEND_MODES[blendMode].c_str()))
                {
                  for (size_t i = 0; i < NUM_BLEND_MODES; i++)
                  {
                    if (ImGui::Selectable(BLEND_MODES[i].c_str()))
                    {
                      texture.SetBlendMode(i);
                    }
                  }
                  ImGui::EndCombo();
                }
              }
              ImGui::TreePop();
            }
          }

          ImGui::TreePop();
        }
      }
    }
    ImGui::End();
  }

  if (!w_material) { RestoreMaterials(); }

  static std::string quadblockQuery;
  if (w_quadblocks)
  {
    bool resetBsp = false;
    if (ImGui::Begin("Quadblocks", &w_quadblocks))
    {
      ImGui::InputTextWithHint("Search", "Search Quadblocks...", &quadblockQuery);
      for (Quadblock& quadblock : m_quadblocks)
      {
        if (!quadblock.Hide() && Matches(quadblock.GetName(), quadblockQuery))
        {
          if (quadblock.RenderUI(m_checkpoints.size() - 1, resetBsp))
          {
            ManageTurbopad(quadblock);
          }
        }
      }
    }
    ImGui::End();
    if (resetBsp && m_bsp.Valid())
    {
      m_bsp.Clear();
      GenerateRenderBspData(m_bsp);
      m_showLogWindow = true;
      m_logMessage = "Modifying quadblock position or turbo pad state automatically resets the BSP tree.";
    }
  }

  if (!quadblockQuery.empty() && !w_quadblocks) { quadblockQuery.clear(); }

  static std::string checkpointQuery;
  if (w_checkpoints)
  {
    if (ImGui::Begin("Checkpoints", &w_checkpoints))
    {
      ImGui::InputTextWithHint("Search##", "Search Quadblocks...", &checkpointQuery);
      if (ImGui::TreeNode("Checkpoints"))
      {
        std::vector<int> checkpointsDelete;
        for (int i = 0; i < m_checkpoints.size(); i++)
        {
          m_checkpoints[i].RenderUI(m_checkpoints.size(), m_quadblocks);
          if (m_checkpoints[i].GetDelete()) { checkpointsDelete.push_back(i); }
        }
        if (!checkpointsDelete.empty())
        {
          for (int i = static_cast<int>(checkpointsDelete.size()) - 1; i >= 0; i--)
          {
            m_checkpoints.erase(m_checkpoints.begin() + checkpointsDelete[i]);
          }
          for (int i = 0; i < m_checkpoints.size(); i++)
          {
            m_checkpoints[i].RemoveInvalidCheckpoints(checkpointsDelete);
            m_checkpoints[i].UpdateInvalidCheckpoints(checkpointsDelete);
            m_checkpoints[i].UpdateIndex(i);
          }
        }
        if (ImGui::Button("Add Checkpoint"))
        {
          m_checkpoints.emplace_back(static_cast<int>(m_checkpoints.size()));
        }
        ImGui::TreePop();
      }
    }
    if (ImGui::TreeNode("Generate"))
    {
      for (size_t i = 0; i < m_checkpointPaths.size(); i++)
      {
        bool insertAbove = false;
        bool removePath = false;
        Path& path = m_checkpointPaths[i];
        const std::string pathTitle = "Path " + std::to_string(path.GetIndex());
        path.RenderUI(pathTitle, m_quadblocks, checkpointQuery, true, insertAbove, removePath);
        if (insertAbove)
        {
          m_checkpointPaths.insert(m_checkpointPaths.begin() + path.GetIndex(), Path());
          for (size_t j = 0; j < m_checkpointPaths.size(); j++) { m_checkpointPaths[j].SetIndex(j); }
        }
        if (removePath)
        {
          m_checkpointPaths.erase(m_checkpointPaths.begin() + path.GetIndex());
          for (size_t j = 0; j < m_checkpointPaths.size(); j++) { m_checkpointPaths[j].SetIndex(j); }
        }
      }

      if (ImGui::Button("Create Path"))
      {
        m_checkpointPaths.push_back(Path(m_checkpointPaths.size()));
      }
      ImGui::SameLine();
      if (ImGui::Button("Delete Path"))
      {
        m_checkpointPaths.pop_back();
      }

      bool ready = !m_checkpointPaths.empty();
      for (const Path& path : m_checkpointPaths)
      {
        if (!path.Ready()) { ready = false; break; }
      }
      ImGui::BeginDisabled(!ready);
      static ButtonUI generateButton;
      if (generateButton.Show("Generate", "Checkspoints successfully generated.", false))
      {
        size_t checkpointIndex = 0;
        std::vector<size_t> linkNodeIndexes;
        std::vector<std::vector<Checkpoint>> pathCheckpoints;
        for (Path& path : m_checkpointPaths)
        {
          pathCheckpoints.push_back(path.GeneratePath(checkpointIndex, m_quadblocks));
          checkpointIndex += pathCheckpoints.back().size();
          linkNodeIndexes.push_back(path.GetStart());
          linkNodeIndexes.push_back(path.GetEnd());
        }
        m_checkpoints.clear();
        for (const std::vector<Checkpoint>& checkpoints : pathCheckpoints)
        {
          for (const Checkpoint& checkpoint : checkpoints)
          {
            m_checkpoints.push_back(checkpoint);
          }
        }

        int lastPathIndex = static_cast<int>(m_checkpointPaths.size()) - 1;
        const Checkpoint* currStartCheckpoint = &m_checkpoints[m_checkpointPaths[lastPathIndex].GetStart()];
        for (int i = lastPathIndex - 1; i >= 0; i--)
        {
          m_checkpointPaths[i].UpdateDist(currStartCheckpoint->GetDistFinish(), currStartCheckpoint->GetPos(), m_checkpoints);
          currStartCheckpoint = &m_checkpoints[m_checkpointPaths[i].GetStart()];
        }

        for (size_t i = 0; i < linkNodeIndexes.size(); i++)
        {
          Checkpoint& node = m_checkpoints[linkNodeIndexes[i]];
          if (i % 2 == 0)
          {
            size_t linkDown = (i == 0) ? linkNodeIndexes.size() - 1 : i - 1;
            node.UpdateDown(static_cast<int>(linkNodeIndexes[linkDown]));
          }
          else
          {
            size_t linkUp = (i + 1) % linkNodeIndexes.size();
            node.UpdateUp(static_cast<int>(linkNodeIndexes[linkUp]));
          }
        }
        GenerateRenderCheckpointData(m_checkpoints);
      }
      ImGui::EndDisabled();
      ImGui::TreePop();
    }
    ImGui::End();
  }

  if (!checkpointQuery.empty() && !w_checkpoints) { checkpointQuery.clear(); }

  if (w_bsp)
  {
    if (ImGui::Begin("BSP Tree", &w_bsp))
    {
      if (!m_bsp.Empty())
      {
        size_t bspIndex = 0;
        m_bsp.RenderUI(bspIndex, m_quadblocks);
      }

      static std::string buttonMessage;
      static ButtonUI generateBSPButton = ButtonUI();
      if (generateBSPButton.Show("Generate", buttonMessage, false))
      {
        std::vector<size_t> quadIndexes;
        for (size_t i = 0; i < m_quadblocks.size(); i++) { quadIndexes.push_back(i); }
        m_bsp.Clear();
        m_bsp.SetQuadblockIndexes(quadIndexes);
        m_bsp.Generate(m_quadblocks, MAX_QUADBLOCKS_LEAF, MAX_LEAF_AXIS_LENGTH);
        if (m_bsp.Valid()) { buttonMessage = "Successfully generated the BSP tree."; }
        else
        {
          m_bsp.Clear();
          buttonMessage = "Failed generating the BSP tree.";
        }
        GenerateRenderBspData(m_bsp);
      }
    }
    ImGui::End();
  }

  if (w_ghost)
  {
    if (ImGui::Begin("Ghost", &w_ghost))
    {
      static std::string ghostName;
      static ButtonUI saveGhostButton(20);
      static std::string saveGhostFeedback;
      ImGui::InputText("##saveghost", &ghostName); ImGui::SameLine();
      ImGui::BeginDisabled(ghostName.empty());
      if (saveGhostButton.Show("Save Ghost", saveGhostFeedback, false))
      {
        ghostName += ".ctrghost";
        saveGhostFeedback = "Failed retrieving ghost data from the emulator.\nMake sure that you have saved your ghost in-game\nbefore clicking this button.";

        auto selection = pfd::select_folder("Level Folder").result();
        if (!selection.empty())
        {
          const std::filesystem::path path = selection + "\\" + ghostName;
          if (SaveGhostData("duckstation", path)) { saveGhostFeedback = "Ghost file successfully saved."; }
        }
      }
      ImGui::EndDisabled();
      if (ghostName.empty()) { ImGui::SetItemTooltip("You must choose a filename before saving the ghost file."); }

      static std::string tropyPath;
      static ButtonUI tropyPathButton(20);
      static std::string tropyImportFeedback;
      ImGui::Text("Tropy Ghost:"); ImGui::SameLine();
      ImGui::InputText("##tropyghost", &tropyPath, ImGuiInputTextFlags_ReadOnly); ImGui::SameLine();
      if (tropyPathButton.Show("...##tropypath", tropyImportFeedback, false))
      {
        tropyImportFeedback = "Error: invalid ghost file format.";
        auto selection = pfd::open_file("CTR Ghost File", ".", {"CTR Ghost Files", "*.ctrghost"}).result();
        if (!selection.empty())
        {
          tropyPath = selection.front();
          if (SetGhostData(tropyPath, true)) { tropyImportFeedback = "Tropy ghost successfully set."; }
        }
      }

      static std::string oxidePath;
      static ButtonUI oxidePathButton(20);
      static std::string oxideImportFeedback;
      ImGui::Text("Oxide Ghost:"); ImGui::SameLine();
      ImGui::InputText("##oxideghost", &oxidePath, ImGuiInputTextFlags_ReadOnly); ImGui::SameLine();
      if (oxidePathButton.Show("...##oxidepath", oxideImportFeedback, false))
      {
        oxideImportFeedback = "Error: invalid ghost file format.";
        auto selection = pfd::open_file("CTR Ghost File", ".", {"CTR Ghost Files", "*.ctrghost"}).result();
        if (!selection.empty())
        {
          oxidePath = selection.front();
          if (SetGhostData(oxidePath, false)) { oxideImportFeedback = "Oxide ghost successfully set"; }
        }
      }
    }
    ImGui::End();
  }

  if (w_renderer)
  {

    static Renderer rend = Renderer(800.0f, 400.0f);
    static bool initRend = true;
    constexpr float bottomPaneHeight = 200.0f;

    if (initRend) { ImGui::SetNextWindowSize({rend.GetWidth(), rend.GetHeight() + bottomPaneHeight}); initRend = false; }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    if (ImGui::Begin("Renderer", &w_renderer, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
      ImVec2 pos = ImGui::GetCursorScreenPos();
      ImVec2 size = ImGui::GetWindowSize();
      rend.RescaleFramebuffer(size.x, size.y - bottomPaneHeight);
      ImTextureID tex = static_cast<ImTextureID>(rend.GetTexBuffer());
      float rendWidth = rend.GetWidth();
      float rendHeight = rend.GetHeight();

      ImGui::GetWindowDrawList()->AddImage(tex,
        ImVec2(pos.x, pos.y),
        ImVec2(pos.x + rendWidth, pos.y + rendHeight),
        ImVec2(0, 1),
        ImVec2(1, 0));

      ImVec2 mousePos = ImGui::GetMousePos();
      struct {
        int x, y;
      } pixelCoord((int)(mousePos.x - pos.x), (int)(mousePos.y - pos.y));
 
      if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
      {
        if (pixelCoord.x >= 0 && pixelCoord.x < rendWidth && pixelCoord.y >= 0 && pixelCoord.y < rendHeight)
        {
          //printf("Clicked pixel coord: (%d, %d)\n", pixelCoord.x, pixelCoord.y);
 
          ViewportClickHandleBlockSelection(pixelCoord.x, pixelCoord.y, rend);
        }
      }

      std::vector<Model> modelsToRender;

      if (GuiRenderSettings::showLevel)
      {
        if (GuiRenderSettings::showLowLOD)
        {
          if (GuiRenderSettings::showLevVerts)
          {
            modelsToRender.push_back(m_pointsLowLODLevelModel);
          }
          else
          {
            modelsToRender.push_back(m_lowLODLevelModel);
          }
        }
        else
        {
          if (GuiRenderSettings::showLevVerts)
          {
            modelsToRender.push_back(m_pointsHighLODLevelModel);
          }
          else
          {
            modelsToRender.push_back(m_highLODLevelModel);
          }
        }
      }
      if (GuiRenderSettings::showBspRectTree)
      {
        modelsToRender.push_back(m_bspModel);
      }
      if (GuiRenderSettings::showCheckpoints)
      {
        modelsToRender.push_back(m_checkModel);
      }
      if (GuiRenderSettings::showStartpoints)
      {
        modelsToRender.push_back(m_spawnsModel);
      }
      modelsToRender.push_back(m_selectedBlockModel);

      rend.Render(modelsToRender);

      ImGui::SetNextWindowPos(ImVec2(pos.x, pos.y + rendHeight));
      if (ImGui::BeginChild("Renderer Settings", ImVec2(rendWidth, bottomPaneHeight), ImGuiChildFlags_Border))
      {
        static float rollingOneSecond = 0;
        static int FPS = -1;
        float fm = fmod(rollingOneSecond, 1.f);
        if (fm != rollingOneSecond && rollingOneSecond >= 1.f) //2nd condition prevents fps not updating if deltaTime exactly equals 1.f
        {
          FPS = static_cast<int>(1.f / rend.GetLastDeltaTime());
          rollingOneSecond = fm;
        }
        rollingOneSecond += rend.GetLastDeltaTime();
        if (ImGui::TreeNode("Options"))
        {
          if (ImGui::BeginTable("Renderer Settings Table", 2))
          {
            /*
              * *** Enable viewport resizing
              * Filter by material (highlight all quads with a specified subset of materials)
              * Filter by quad flags (higlight all quads with a specified subset of quadflags)
              * Filter by draw flags ""
              * Filter by terrain ""
              *
              * draw bsp as wireframe or as transparent objs
              * 
              * draw (low lod) wireframe on top of the mesh as an option
              *
              * NOTE: resetBsp does not trigger when vertex color changes.
              *
              * Make mesh read live data.
              *
              * Editor features (edit in viewport blender style).
              */
            static std::string camMoveMult = std::to_string(GuiRenderSettings::camMoveMult);
            static std::string camRotateMult = std::to_string(GuiRenderSettings::camRotateMult);
            static std::string camSprintMult = std::to_string(GuiRenderSettings::camSprintMult);
            static std::string camFOV = std::to_string(GuiRenderSettings::camFovDeg);

            static bool showCheckpoints = false;
            static bool	showStartingPositions = false;
            static bool	showBspRectTree = false;

            float textFieldWidth = std::max(rendWidth / 6.0f, 50.0f);

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("FPS: %d", FPS);

            if (ImGui::Combo("Render", &GuiRenderSettings::renderType, GuiRenderSettings::renderTypeLabels.data(), static_cast<int>(GuiRenderSettings::renderTypeLabels.size()))) {}

            ImGui::Checkbox("Show Low LOD", &GuiRenderSettings::showLowLOD);
            ImGui::Checkbox("Show Wireframe", &GuiRenderSettings::showWireframe);
            ImGui::Checkbox("Show Backfaces", &GuiRenderSettings::showBackfaces);
            ImGui::Checkbox("Show Level Verts", &GuiRenderSettings::showLevVerts);
            ImGui::Checkbox("Show Checkpoints", &GuiRenderSettings::showCheckpoints);
            ImGui::Checkbox("Show Starting Positions", &GuiRenderSettings::showStartpoints);
            ImGui::Checkbox("Show BSP Rect Tree", &GuiRenderSettings::showBspRectTree);
            ImGui::PushItemWidth(textFieldWidth);
            if (ImGui::SliderInt("BSP Rect Tree top depth", &GuiRenderSettings::bspTreeTopDepth, 0, GuiRenderSettings::bspTreeMaxDepth)) //top changed
            {
              GuiRenderSettings::bspTreeBottomDepth = std::max(GuiRenderSettings::bspTreeBottomDepth, GuiRenderSettings::bspTreeTopDepth);
              GenerateRenderBspData(m_bsp);
            }
            if (ImGui::SliderInt("BSP Rect Tree bottom depth", &GuiRenderSettings::bspTreeBottomDepth, 0, GuiRenderSettings::bspTreeMaxDepth)) //bottom changed
            {
              GuiRenderSettings::bspTreeTopDepth = std::min(GuiRenderSettings::bspTreeTopDepth, GuiRenderSettings::bspTreeBottomDepth);
              GenerateRenderBspData(m_bsp);
            }
            if (ImGui::BeginCombo("(NOT IMPL) Mask by Materials", "..."))
            {
              ImGui::Selectable("(NOT IMPL)");
              ImGui::EndCombo();
            }
            if (ImGui::BeginCombo("(NOT IMPL) Mask by Quad flags", "..."))
            {
              ImGui::Selectable("(NOT IMPL)");
              ImGui::EndCombo();
            }
            if (ImGui::BeginCombo("(NOT IMPL) Mask by Draw flags", "..."))
            {
              ImGui::Selectable("(NOT IMPL)");
              ImGui::EndCombo();
            }
            if (ImGui::BeginCombo("(NOT IMPL) Mask by Terrain", "..."))
            {
              ImGui::Selectable("(NOT IMPL)");
              ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
            ImGui::TableSetColumnIndex(1);
            ImGui::Text(""
              "Camera Controls:\n"
              "\t* WASD to move in/out & pan\n"
              "\t* Arrow keys to rotate cam\n"
              "\t* Spacebar to move up, Shift to move down\n"
              "\t* Ctrl to \"Sprint\"");

            ImGui::PushItemWidth(textFieldWidth);
            {
              auto textUI = [](const std::string& label, std::string& uiValue, float& renderSetting, float minThres = -std::numeric_limits<float>::max(), float maxThres = std::numeric_limits<float>::max())
                {
                  float val;
                  if (ImGui::InputText(label.c_str(), &uiValue) && ParseFloat(uiValue, val))
                  {
                    val = Clamp(val, minThres, maxThres);
                    uiValue = std::to_string(val);
                    renderSetting = val;
                  }
                };

              textUI("Camera Move Multiplier", camMoveMult, GuiRenderSettings::camMoveMult, 0.01f);
              textUI("Camera Rotate Multiplier", camRotateMult, GuiRenderSettings::camRotateMult, 0.01f);
              textUI("Camera Sprint Multiplier", camSprintMult, GuiRenderSettings::camSprintMult, 1.0f);
              textUI("Camera FOV", camFOV, GuiRenderSettings::camFovDeg, 5.0f, 150.0f);
            }
            ImGui::PopItemWidth();

            ImGui::EndTable();
          }
          ImGui::TreePop();
        }
      }
      ImGui::EndChild();
    }
    ImGui::End();
    ImGui::PopStyleVar();
  }
}

void Path::RenderUI(const std::string& title, const std::vector<Quadblock>& quadblocks, const std::string& searchQuery, bool drawPathBtn, bool& insertAbove, bool& removePath)
{
  auto QuadListUI = [this](std::vector<size_t>& indexes, size_t& value, std::string& label, const std::string& title, const std::vector<Quadblock>& quadblocks, const std::string& searchQuery, ButtonUI& button)
    {
      if (ImGui::BeginChild(title.c_str(), {0, 0}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AutoResizeX))
      {
        ImGui::Text(title.substr(0, title.find("##")).c_str());
        if (ImGui::TreeNode("Quad list:"))
        {
          std::vector<size_t> deleteList;
          for (size_t i = 0; i < indexes.size(); i++)
          {
            ImGui::Text(quadblocks[indexes[i]].GetName().c_str()); ImGui::SameLine();
            if (ImGui::Button(("Remove##" + title + std::to_string(i)).c_str()))
            {
              deleteList.push_back(i);
            }
          }
          if (!deleteList.empty())
          {
            for (int i = static_cast<int>(deleteList.size()) - 1; i >= 0; i--)
            {
              indexes.erase(indexes.begin() + deleteList[i]);
            }
          }
          ImGui::TreePop();
        }

        if (ImGui::BeginCombo(("##" + title).c_str(), label.c_str()))
        {
          for (size_t i = 0; i < quadblocks.size(); i++)
          {
            if (Matches(quadblocks[i].GetName(), searchQuery))
            {
              if (ImGui::Selectable(quadblocks[i].GetName().c_str()))
              {
                label = quadblocks[i].GetName();
                value = i;
              }
            }
          }
          ImGui::EndCombo();
        }

        if (button.Show(("Add##" + title).c_str(), "Quadblock successfully\nadded to path.", false))
        {
          bool found = false;
          for (const size_t index : indexes)
          {
            if (index == value) { found = true; break; }
          }
          if (!found) { indexes.push_back(value); }
        }
      }
      ImGui::EndChild();
    };

  if (ImGui::TreeNode(title.c_str()))
  {
    if (ImGui::BeginChild(("##" + title).c_str(), {0, 0}, ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AutoResizeX))
    {
      bool dummyInsert, dummyRemove = false;
      if (m_left) { m_left->RenderUI("Left Path", quadblocks, searchQuery, false, dummyInsert, dummyRemove); }
      if (m_right) { m_right->RenderUI("Right Path", quadblocks, searchQuery, false, dummyInsert, dummyRemove); }

      static ButtonUI startButton = ButtonUI();
      static ButtonUI endButton = ButtonUI();
      static ButtonUI ignoreButton = ButtonUI();
      QuadListUI(m_quadIndexesStart, m_previewValueStart, m_previewLabelStart, "Start##" + title, quadblocks, searchQuery, startButton);
      ImGui::SameLine();
      QuadListUI(m_quadIndexesEnd, m_previewValueEnd, m_previewLabelEnd, "End##" + title, quadblocks, searchQuery, endButton);
      ImGui::SameLine();
      QuadListUI(m_quadIndexesIgnore, m_previewValueIgnore, m_previewLabelIgnore, "Ignore##" + title, quadblocks, searchQuery, ignoreButton);

      if (ImGui::Button("Add Left Path "))
      {
        if (!m_left) { m_left = new Path(m_index + 1); }
      } ImGui::SameLine();
      ImGui::BeginDisabled(m_left == nullptr);
      if (ImGui::Button("Delete Left Path "))
      {
        if (m_left)
        {
          delete m_left;
          m_left = nullptr;
        }
      }
      ImGui::EndDisabled();

      if (ImGui::Button("Add Right Path"))
      {
        if (!m_right) { m_right = new Path(m_index + 2); }
      }
      ImGui::SameLine();
      ImGui::BeginDisabled(m_right == nullptr);
      if (ImGui::Button("Delete Right Path"))
      {
        if (m_right)
        {
          delete m_right;
          m_right = nullptr;
        }
      }
      ImGui::EndDisabled();
    }
    ImGui::EndChild();

    if (drawPathBtn)
    {
      static ButtonUI insertAboveButton;
      static ButtonUI removePathButton;
      if (insertAboveButton.Show(("Insert Path Above##" + std::to_string(m_index)).c_str(), "You're editing the new path.", false)) { insertAbove = true; }
      if (removePathButton.Show(("Remove Current Path##" + std::to_string(m_index)).c_str(), "Path successfully deleted.", false)) { removePath = true; }
    }

    ImGui::TreePop();
  }
}

bool Quadblock::RenderUI(size_t checkpointCount, bool& resetBsp)
{
  bool ret = false;
  if (ImGui::TreeNode(m_name.c_str()))
  {
    if (ImGui::TreeNode("Vertices"))
    {
      for (size_t i = 0; i < NUM_VERTICES_QUADBLOCK; i++)
      {
        bool editedPos = false;
        m_p[i].RenderUI(i, editedPos);
        if (editedPos)
        {
          resetBsp = true;
          ComputeBoundingBox();
        }
      }
      ImGui::TreePop();
    }
    if (ImGui::TreeNode("Bounding Box"))
    {
      m_bbox.RenderUI();
      ImGui::TreePop();
    }
    if (ImGui::TreeNode("Terrain"))
    {
      std::string terrainLabel;
      for (const auto& [label, terrain] : TerrainType::LABELS)
      {
        if (terrain == m_terrain) { terrainLabel = label; break; }
      }
      if (ImGui::BeginCombo("##terrain", terrainLabel.c_str()))
      {
        for (const auto& [label, terrain] : TerrainType::LABELS)
        {
          if (ImGui::Selectable(label.c_str()))
          {
            m_terrain = terrain;
          }
        }
        ImGui::EndCombo();
      }
      ImGui::TreePop();
    }
    if (ImGui::TreeNode("Quad Flags"))
    {
      for (const auto& [label, flag] : QuadFlags::LABELS)
      {
        UIFlagCheckbox(m_flags, flag, label);
      }
      ImGui::TreePop();
    }
    if (ImGui::TreeNode("Draw Flags"))
    {
      ImGui::Checkbox("Double Sided", &m_doubleSided);
      ImGui::TreePop();
    }
    ImGui::Checkbox("Checkpoint", &m_checkpointStatus);
    ImGui::Text("Checkpoint Index: ");
    ImGui::SameLine();
    if (ImGui::InputInt("##cp", &m_checkpointIndex)) { m_checkpointIndex = Clamp(m_checkpointIndex, -1, static_cast<int>(checkpointCount)); }
    ImGui::Text("Trigger:");
    if (ImGui::RadioButton("None", m_trigger == QuadblockTrigger::NONE))
    {
      m_trigger = QuadblockTrigger::NONE;
      m_flags = QuadFlags::DEFAULT;
      resetBsp = true;
      ret = true;
    } ImGui::SameLine();
    if (ImGui::RadioButton("Turbo Pad", m_trigger == QuadblockTrigger::TURBO_PAD))
    {
      m_trigger = QuadblockTrigger::TURBO_PAD;
      resetBsp = true;
      ret = true;
    } ImGui::SameLine();
    if (ImGui::RadioButton("Super Turbo Pad", m_trigger == QuadblockTrigger::SUPER_TURBO_PAD))
    {
      m_trigger = QuadblockTrigger::SUPER_TURBO_PAD;
      resetBsp = true;
      ret = true;
    }
    ImGui::TreePop();
  }
  return ret;
}

void Vertex::RenderUI(size_t index, bool& editedPos)
{
  if (ImGui::TreeNode(("Vertex " + std::to_string(index)).c_str()))
  {
    ImGui::Text("Pos: "); ImGui::SameLine();
    if (ImGui::InputFloat3("##pos", m_pos.Data())) { editedPos = true; }
    ImGui::Text("High:"); ImGui::SameLine();
    float colorHighData[3] = { m_colorHigh.Red(), m_colorHigh.Green(), m_colorHigh.Blue() };
    if (ImGui::ColorEdit3("##high", colorHighData))
    {
      m_colorHigh = Color(static_cast<float>(colorHighData[0]), colorHighData[1], colorHighData[2]);
    }
    ImGui::Text("Low: "); ImGui::SameLine();
    float colorLowData[3] = { m_colorLow.Red(), m_colorLow.Green(), m_colorLow.Blue() };
    if (ImGui::ColorEdit3("##low", colorLowData))
    {
      m_colorLow = Color(static_cast<float>(colorLowData[0]), colorLowData[1], colorLowData[2]);
    }
    ImGui::TreePop();
  }
}