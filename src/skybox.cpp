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

bool Skybox::LoadOBJ(const std::filesystem::path& path)
{
	std::ifstream file(path);
	if (!file.is_open()) { return false; }

	Clear();

	std::vector<Point> loadedVertices;
	std::vector<uint16_t> loadedIndices;
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
			Point vert;
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
			// Triangulate directly using fan triangulation (first vertex as pivot)
			uint16_t firstOffset = 0, prevOffset = 0;
			int vertexCount = 0;
			std::string token;
			while (iss >> token)
			{
				std::istringstream tokenStream(token);
				std::string indexStr;
				std::getline(tokenStream, indexStr, '/');
				if (indexStr.empty()) { continue; }
				int vertexIndex = std::stoi(indexStr);
				// OBJ indices are 1-based, convert to byte offset
				if (vertexIndex > 0)
				{
					uint16_t offset = static_cast<uint16_t>((vertexIndex - 1) * sizeof(PSX::SkyboxVertex));
					if (vertexCount == 0) { firstOffset = offset; }
					else if (vertexCount >= 2)
					{
						// Store 4 values per face: 3 byte offsets + padding (0)
						loadedIndices.push_back(firstOffset);
						loadedIndices.push_back(prevOffset);
						loadedIndices.push_back(offset);
						loadedIndices.push_back(0);
					}
					prevOffset = offset;
					vertexCount++;
				}
			}
		}
	}

	if (loadedVertices.empty() || loadedIndices.empty()) { return false; }

	m_vertices = std::move(loadedVertices);
	m_indexBuffer = std::move(loadedIndices);
	m_objPath = path;
	return true;
}

void Skybox::DistributeFaces(std::vector<std::vector<uint16_t>>& segments) const
{
	segments.clear();
	segments.resize(PSX::NUM_SKYBOX_SEGMENTS);

	if (m_indexBuffer.empty() || m_vertices.empty()) { return; }

	// Put all faces in segment 0 for now, basically always render the entire skybox
	// TODO: implement actual segmentation based on vertex positions or other criteria if needed
	for (size_t seg = 0; seg < PSX::NUM_SKYBOX_SEGMENTS; seg++)
	{
		segments[seg] = m_indexBuffer;
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

std::vector<uint8_t> Skybox::Serialize(size_t baseOffset, std::vector<size_t>& ptrMapOffsets) const
{
	std::vector<std::vector<uint16_t>> segments;
	DistributeFaces(segments);

	// Layout: [PSX::Skybox header] [SkyboxVertex array] [Face index arrays per segment]
	const size_t headerSize = sizeof(PSX::Skybox);
	const size_t vertexDataSize = m_vertices.size() * sizeof(PSX::SkyboxVertex);
	size_t totalFaceIndices = 0;
	for (const auto& seg : segments) { totalFaceIndices += seg.size(); }
	const size_t faceDataSize = totalFaceIndices * sizeof(uint16_t);
	const size_t totalSize = headerSize + vertexDataSize + faceDataSize;

	std::vector<uint8_t> buffer(totalSize, 0);

	// Fill header
	PSX::Skybox header = {};
	header.numVertex = static_cast<uint32_t>(m_vertices.size());

	const size_t offVertexData = baseOffset + headerSize;
	header.offVertex = static_cast<uint32_t>(offVertexData);

	// Register offVertex in pointer map
	ptrMapOffsets.push_back(baseOffset + offsetof(PSX::Skybox, offVertex));

	// Calculate face segment offsets
	// Empty segments get NULL offsets and are NOT registered in the pointer map.
	size_t offFaceData = offVertexData + vertexDataSize;
	for (size_t seg = 0; seg < PSX::NUM_SKYBOX_SEGMENTS; seg++)
	{
		const size_t faceCount = segments[seg].size() / PSX::SKYBOX_FACE_STRIDE;
		header.numFaces[seg] = static_cast<int16_t>(faceCount);
		if (faceCount > 0)
		{
			header.offFaces[seg] = static_cast<uint32_t>(offFaceData);
			ptrMapOffsets.push_back(baseOffset + offsetof(PSX::Skybox, offFaces) + seg * sizeof(uint32_t));
			offFaceData += segments[seg].size() * sizeof(uint16_t);
		}
		else
		{
			header.offFaces[seg] = 0; // NULL for empty segments
		}
	}

	// Write header
	memcpy(buffer.data(), &header, headerSize);

	// Write vertex data
	size_t writePos = headerSize;
	for (const Point& vert : m_vertices)
	{
		PSX::SkyboxVertex psxVert = {};
		psxVert.pos = ConvertVec3(vert.pos, FP_ONE_GEO);
		psxVert.color.r = vert.color.r;
		psxVert.color.g = vert.color.g;
		psxVert.color.b = vert.color.b;
		psxVert.color.a = 0x30; // GPU command code for Gouraud-shaded triangle
		memcpy(&buffer[writePos], &psxVert, sizeof(PSX::SkyboxVertex));
		writePos += sizeof(PSX::SkyboxVertex);
	}

	// Write face data per segment (indices already contain byte offsets + padding)
	for (size_t seg = 0; seg < PSX::NUM_SKYBOX_SEGMENTS; seg++)
	{
		const std::vector<uint16_t>& indices = segments[seg];
		if (!indices.empty())
		{
			memcpy(&buffer[writePos], indices.data(), indices.size() * sizeof(uint16_t));
			writePos += indices.size() * sizeof(uint16_t);
		}
	}

	return buffer;
}

std::vector<Primitive> Skybox::ToGeometry(const BoundingBox& levelBounds) const
{
	std::vector<Primitive> triangles;
	if (!IsReady()) { return triangles; }

	const Vec3 levelCenter = levelBounds.Midpoint();
	const Vec3 levelAxis = levelBounds.AxisLength();
	const float levelExtent = std::max(levelAxis.x, std::max(levelAxis.y, levelAxis.z));

	// Calculate skybox local bounds (OBJ space)
	BoundingBox skyboxBounds = {};
	skyboxBounds.min = m_vertices[0].pos;
	skyboxBounds.max = m_vertices[0].pos;
	for (const Point& v : m_vertices)
	{
		skyboxBounds.min.x = std::min(skyboxBounds.min.x, v.pos.x);
		skyboxBounds.min.y = std::min(skyboxBounds.min.y, v.pos.y);
		skyboxBounds.min.z = std::min(skyboxBounds.min.z, v.pos.z);
		skyboxBounds.max.x = std::max(skyboxBounds.max.x, v.pos.x);
		skyboxBounds.max.y = std::max(skyboxBounds.max.y, v.pos.y);
		skyboxBounds.max.z = std::max(skyboxBounds.max.z, v.pos.z);
	}

	const Vec3 skyboxAxis = skyboxBounds.max - skyboxBounds.min;
	const float skyboxExtent = std::max(skyboxAxis.x, std::max(skyboxAxis.y, skyboxAxis.z));
	const float targetExtent = levelExtent * 3.0f;
	const float skyboxScale = (skyboxExtent > 0.0f) ? (targetExtent / skyboxExtent) : 1.0f;

	const size_t faceCount = m_indexBuffer.size() / PSX::SKYBOX_FACE_STRIDE;
	triangles.reserve(faceCount);

	for (size_t i = 0; i < m_indexBuffer.size(); i += PSX::SKYBOX_FACE_STRIDE)
	{
		// Convert byte offsets back to vertex indices
		const size_t idxA = m_indexBuffer[i] / sizeof(PSX::SkyboxVertex);
		const size_t idxB = m_indexBuffer[i + 1] / sizeof(PSX::SkyboxVertex);
		const size_t idxC = m_indexBuffer[i + 2] / sizeof(PSX::SkyboxVertex);

		const Point& va = m_vertices[idxA];
		const Point& vb = m_vertices[idxB];
		const Point& vc = m_vertices[idxC];

		// Scale vertex positions relative to level center
		Vec3 posA = levelCenter + (va.pos * skyboxScale);
		Vec3 posB = levelCenter + (vb.pos * skyboxScale);
		Vec3 posC = levelCenter + (vc.pos * skyboxScale);

		Tri tri(
			Point(posA, Vec3(), va.color),
			Point(posB, Vec3(), vb.color),
			Point(posC, Vec3(), vc.color)
		);
		triangles.push_back(tri);
	}

	return triangles;
}

bool Skybox::IsReady() const
{
	return !m_vertices.empty() && !m_indexBuffer.empty();
}

void Skybox::Clear()
{
	m_objPath.clear();
	m_vertices.clear();
	m_indexBuffer.clear();
}

bool Skybox::RenderUI()
{
	bool stateChanged = false;

	ImGui::Separator();

	// OBJ file selection
	std::string displayPath = m_objPath.empty() ? "(none)" : m_objPath.filename().string();
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
			if (LoadOBJ(selection.front()))
			{
				stateChanged = true;
			}
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Clear##clearskybox"))
	{
		Clear();
		stateChanged = true;
	}

	// Status display
	ImGui::Separator();
	if (IsReady())
	{
		ImGui::Text("Vertices: %zu", m_vertices.size());
		ImGui::Text("Faces: %zu (triangles)", m_indexBuffer.size() / PSX::SKYBOX_FACE_STRIDE);

		// Show per-segment distribution
		std::vector<std::vector<uint16_t>> segments;
		DistributeFaces(segments);
		if (ImGui::TreeNode("Segment Distribution"))
		{
			for (size_t i = 0; i < PSX::NUM_SKYBOX_SEGMENTS; i++)
			{
				ImGui::Text("Segment %zu: %zu faces", i, segments[i].size() / PSX::SKYBOX_FACE_STRIDE);
			}
			ImGui::TreePop();
		}

		ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Skybox ready!");
	}
	else
	{
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No skybox geometry loaded");
	}

	ImGui::SetItemTooltip("Skyboxes are untextured geometry with vertex colors.\nLoad an OBJ file with vertex colors (v x y z r g b).");

	return stateChanged;
}
