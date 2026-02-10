#include "skybox.h"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <portable-file-dialogs.h>

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <cstdio>

bool SkyboxConfig::LoadOBJ(const std::filesystem::path& path)
{
	std::ifstream file(path);
	if (!file.is_open()) { return false; }

	std::vector<SkyboxVertex> loadedVertices;
	std::vector<SkyboxFace> loadedFaces;
	std::string line;

	while (std::getline(file, line))
	{
		// Strip trailing carriage return (for Windows line endings in Unix-style streams)
		if (!line.empty() && line.back() == '\r') { line.pop_back(); }
		if (line.empty() || line[0] == '#') { continue; }

		std::istringstream iss(line);
		std::string prefix;
		iss >> prefix;

		if (prefix == "v")
		{
			SkyboxVertex vert;
			iss >> vert.pos.x >> vert.pos.y >> vert.pos.z;

			// OBJ vertex colors: "v x y z r g b" (0.0 - 1.0 float range)
			float r = 0.5f, g = 0.5f, b = 0.5f;
			if (iss >> r >> g >> b)
			{
				vert.color = Color(r, g, b);
			}
			else
			{
				vert.color = Color(static_cast<unsigned char>(128), 128, 128);
			}
			loadedVertices.push_back(vert);
		}
		else if (prefix == "f")
		{
			// Parse face - support "f v", "f v/vt", "f v/vt/vn", "f v//vn"
			std::vector<uint16_t> faceIndices;
			std::string token;
			while (iss >> token)
			{
				int vertexIndex = 0;
				std::istringstream tokenStream(token);
				std::string indexStr;
				std::getline(tokenStream, indexStr, '/');
				if (indexStr.empty()) { continue; }
				vertexIndex = std::stoi(indexStr);
				// OBJ indices are 1-based
				if (vertexIndex > 0) { faceIndices.push_back(static_cast<uint16_t>(vertexIndex - 1)); }
				else if (vertexIndex < 0) { faceIndices.push_back(static_cast<uint16_t>(loadedVertices.size() + vertexIndex)); }
			}

			// Triangulate the face (fan triangulation for polygons)
			for (size_t i = 2; i < faceIndices.size(); i++)
			{
				SkyboxFace face;
				face.a = faceIndices[0];
				face.b = faceIndices[i - 1];
				face.c = faceIndices[i];
				loadedFaces.push_back(face);
			}
		}
	}

	if (loadedVertices.empty() || loadedFaces.empty()) { return false; }

	// Validate face indices
	bool valid = true;
	for (const auto& face : loadedFaces)
	{
		if (face.a >= loadedVertices.size() || face.b >= loadedVertices.size() || face.c >= loadedVertices.size())
		{
			valid = false;
			break;
		}
	}
	if (!valid)
	{
		printf("[Skybox] ERROR: OBJ has face indices out of range (max vertex: %zu)\n", loadedVertices.size());
		return false;
	}

	vertices = std::move(loadedVertices);
	faces = std::move(loadedFaces);
	objPath = path;
	printf("[Skybox] Loaded OBJ: %zu vertices, %zu faces from %s\n",
		vertices.size(), faces.size(), path.filename().string().c_str());
	return true;
}

void SkyboxConfig::DistributeFaces(std::vector<std::vector<SkyboxFace>>& segments) const
{
	segments.clear();
	segments.resize(PSX::NUM_SKYBOX_SEGMENTS);

	if (faces.empty() || vertices.empty()) { return; }

	// Put all faces in segment 0 for now, basically always render the entire skybox
	// TODO: implement actual segmentation based on vertex positions or other criteria if needed
	for (size_t seg = 0; seg < PSX::NUM_SKYBOX_SEGMENTS; seg++)
	{
		segments[seg] = faces;
	}

	// Distribute faces evenly across segments (this is not perfect)
	// size_t facesPerSegment = faces.size() / PSX::NUM_SKYBOX_SEGMENTS;
	// size_t remainder = faces.size() % PSX::NUM_SKYBOX_SEGMENTS;
	// size_t faceIndex = 0;

	// for (size_t seg = 0; seg < PSX::NUM_SKYBOX_SEGMENTS; seg++)
	// {
	// 	size_t count = facesPerSegment + (seg < remainder ? 1 : 0);
	// 	for (size_t i = 0; i < count && faceIndex < faces.size(); i++, faceIndex++)
	// 	{
	// 		segments[seg].push_back(faces[faceIndex]);
	// 	}
	// }
}

std::vector<uint8_t> SkyboxConfig::Serialize(size_t baseOffset, std::vector<size_t>& ptrMapOffsets) const
{
	std::vector<std::vector<SkyboxFace>> segments;
	DistributeFaces(segments);

	// Layout: [PSX::Skybox header] [SkyboxVertex array] [SkyboxFace arrays per segment]
	const size_t headerSize = sizeof(PSX::Skybox);
	const size_t vertexDataSize = vertices.size() * sizeof(PSX::SkyboxVertex);
	size_t totalFaces = 0;
	for (const auto& seg : segments) { totalFaces += seg.size(); }
	const size_t faceDataSize = totalFaces * sizeof(PSX::SkyboxFace);
	const size_t totalSize = headerSize + vertexDataSize + faceDataSize;

	std::vector<uint8_t> buffer(totalSize, 0);

	// Fill header
	PSX::Skybox header = {};
	header.numVertex = static_cast<uint32_t>(vertices.size());

	const size_t offVertexData = baseOffset + headerSize;
	header.ptrVertex = static_cast<uint32_t>(offVertexData);

	// Register ptrVertex in pointer map
	ptrMapOffsets.push_back(baseOffset + offsetof(PSX::Skybox, ptrVertex));

	// Calculate face segment offsets
	// Empty segments get NULL pointers and are NOT registered in the pointer map.
	size_t offFaceData = offVertexData + vertexDataSize;
	for (size_t seg = 0; seg < PSX::NUM_SKYBOX_SEGMENTS; seg++)
	{
		header.numFaces[seg] = static_cast<int16_t>(segments[seg].size());
		if (segments[seg].size() > 0)
		{
			header.ptrFaces[seg] = static_cast<uint32_t>(offFaceData);
			ptrMapOffsets.push_back(baseOffset + offsetof(PSX::Skybox, ptrFaces) + seg * sizeof(uint32_t));
			offFaceData += segments[seg].size() * sizeof(PSX::SkyboxFace);
		}
		else
		{
			header.ptrFaces[seg] = 0; // NULL for empty segments
		}
	}

	// Write header
	memcpy(buffer.data(), &header, headerSize);

	// Write vertex data
	size_t writePos = headerSize;
	for (const SkyboxVertex& vert : vertices)
	{
		PSX::SkyboxVertex psxVert = {};
		psxVert.pos = ConvertVec3(vert.pos, FP_ONE_GEO);
		psxVert.pad = 0;
		psxVert.color.r = vert.color.r;
		psxVert.color.g = vert.color.g;
		psxVert.color.b = vert.color.b;
		psxVert.color.a = 0x30; // GPU command code for Gouraud-shaded triangle
		memcpy(&buffer[writePos], &psxVert, sizeof(PSX::SkyboxVertex));
		writePos += sizeof(PSX::SkyboxVertex);
	}

	// Write face data per segment
	for (size_t seg = 0; seg < PSX::NUM_SKYBOX_SEGMENTS; seg++)
	{
		for (const SkyboxFace& face : segments[seg])
		{
			PSX::SkyboxFace psxFace = {};
			// A, B, C are byte offsets into vertex array (index * sizeof(SkyboxVertex))
			psxFace.a = static_cast<uint16_t>(face.a * sizeof(PSX::SkyboxVertex));
			psxFace.b = static_cast<uint16_t>(face.b * sizeof(PSX::SkyboxVertex));
			psxFace.c = static_cast<uint16_t>(face.c * sizeof(PSX::SkyboxVertex));
			psxFace.d = 0;
			memcpy(&buffer[writePos], &psxFace, sizeof(PSX::SkyboxFace));
			writePos += sizeof(PSX::SkyboxFace);
		}
	}

	// Diagnostic output
	printf("[Skybox] Serialized: %zu vertices, %zu faces, %zu bytes at offset 0x%zX\n",
		vertices.size(), totalFaces, totalSize, baseOffset);
	printf("[Skybox] ptrVertex: 0x%X\n", header.ptrVertex);
	for (size_t seg = 0; seg < PSX::NUM_SKYBOX_SEGMENTS; seg++)
	{
		printf("[Skybox] segment[%zu]: %d faces, ptrFaces=0x%X\n",
			seg, header.numFaces[seg], header.ptrFaces[seg]);
	}

	return buffer;
}

bool SkyboxConfig::IsReady() const
{
	return enabled && !vertices.empty() && !faces.empty();
}

void SkyboxConfig::Clear()
{
	objPath.clear();
	vertices.clear();
	faces.clear();
	enabled = false;
}

bool SkyboxConfig::RenderUI()
{
	bool geometryChanged = false;

	ImGui::Checkbox("Enable Skybox", &enabled);

	if (!enabled) { return false; }

	ImGui::Separator();

	// OBJ file selection
	std::string displayPath = objPath.empty() ? "(none)" : objPath.filename().string();
	ImGui::Text("OBJ File:"); ImGui::SameLine();
	ImGui::SetNextItemWidth(200.0f);
	ImGui::BeginDisabled();
	ImGui::InputText("##skyboxobj", &displayPath, ImGuiInputTextFlags_ReadOnly);
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button("Browse##selectskybox"))
	{
		auto selection = pfd::open_file("Select Skybox OBJ", ".",
			{"OBJ Files", "*.obj", "All Files", "*"}).result();
		if (!selection.empty())
		{
			if (LoadOBJ(selection.front())) { geometryChanged = true; }
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Clear##clearskybox"))
	{
		Clear();
		enabled = true; // keep enabled state after clearing geometry
		geometryChanged = true;
	}

	// Status display
	ImGui::Separator();
	if (!vertices.empty() && !faces.empty())
	{
		ImGui::Text("Vertices: %zu", vertices.size());
		ImGui::Text("Faces: %zu (triangles)", faces.size());

		// Show per-segment distribution
		std::vector<std::vector<SkyboxFace>> segments;
		DistributeFaces(segments);
		if (ImGui::TreeNode("Segment Distribution"))
		{
			for (size_t i = 0; i < PSX::NUM_SKYBOX_SEGMENTS; i++)
			{
				ImGui::Text("Segment %zu: %zu faces", i, segments[i].size());
			}
			ImGui::TreePop();
		}

		if (IsReady())
		{
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Skybox ready!");
		}
	}
	else if (enabled)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No skybox geometry loaded");
	}

	ImGui::SetItemTooltip("Skyboxes are untextured geometry with vertex colors.\nLoad an OBJ file with vertex colors (v x y z r g b).");

	return geometryChanged;
}
