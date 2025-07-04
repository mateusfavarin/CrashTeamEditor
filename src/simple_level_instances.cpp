#include "simple_level_instances.h"
#include "geo.h"
#include "utils.h"

#include <vector>
#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <tuple>

const std::unordered_map<std::string, uint16_t> SimpleLevelInstances::mesh_prefixes_to_enum = {
		{"letter_c", LETTER_C},
		{"letter_t", LETTER_T},
		{"letter_r", LETTER_R},
		{"wumpa_fruit", WUMPA_FRUIT},
		{"wumpa_crate", WUMPA_CRATE},
		{"item_crate", ITEM_CRATE},
		{"time_crate_1", TIME_CRATE_1},
		{"time_crate_2", TIME_CRATE_2},
		{"time_crate_3", TIME_CRATE_3},
		{"startflag", START_FLAG},
};

void SimpleLevelInstances::GenerateRenderLevInstData()
{
  for (const auto& e : FILEPATHS)
  {
    std::vector<float> geom_data;

		std::string line;
		std::filesystem::path path(std::get<0>(e.second));
		std::ifstream file(path);

		std::vector<Point> vertices;
		std::vector<Vec3> normals;
		std::vector<Vec2> uvs;

		while (std::getline(file, line))
		{
			std::vector<std::string> tokens = Split(line);
			if (tokens.empty()) { continue; }
			const std::string& command = tokens[0];
			if (command == "v")
			{
				if (tokens.size() < 4) { continue; }
				vertices.emplace_back(std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]));
				if (tokens.size() < 7) { continue; }
				vertices.back().color = Color(std::stof(tokens[4]), std::stof(tokens[5]), std::stof(tokens[6]));
			}
			else if (command == "vt")
			{
				if (tokens.size() == 2)
				{
					Vec2 uv = { std::stof(tokens[1]), 0.0f };
					uvs.emplace_back(uv);
				}
				else if (tokens.size() == 3)
				{
					Vec2 uv = { std::stof(tokens[1]), 1.0f - std::stof(tokens[2]) };
					uvs.emplace_back(uv);
				}
			}
			else if (command == "f")
			{
				if (tokens.size() == 4) //triangle
				{ //v vt vn (pos, texture, normal), for tex, only u is required, vw are implicitly 0, normal may not be normalized.
					std::vector<std::string> token0 = Split(tokens[1], '/');
					int v1 = std::stoi(token0[0]);
					int vt1 = std::stoi(token0[1]);
					int vn1 = std::stoi(token0[2]);
					geom_data.push_back(vertices[v1].pos.x);
					geom_data.push_back(vertices[v1].pos.y);
					geom_data.push_back(vertices[v1].pos.z);
					std::vector<std::string> token1 = Split(tokens[2], '/');
					int v2 = std::stoi(token1[0]);
					int vt2 = std::stoi(token1[1]);
					int vn2 = std::stoi(token1[2]);
					geom_data.push_back(vertices[v2].pos.x);
					geom_data.push_back(vertices[v2].pos.y);
					geom_data.push_back(vertices[v2].pos.z);
					std::vector<std::string> token2 = Split(tokens[3], '/');
					int v3 = std::stoi(token2[0]);
					int vt3 = std::stoi(token2[1]);
					int vn3 = std::stoi(token2[2]);
					geom_data.push_back(vertices[v3].pos.x);
					geom_data.push_back(vertices[v3].pos.y);
					geom_data.push_back(vertices[v3].pos.z);
				}
				else if (tokens.size() == 5) //quad
				{ //012 321
					std::vector<std::string> token0 = Split(tokens[1], '/');
					int v1 = std::stoi(token0[0]);
					int vt1 = std::stoi(token0[1]);
					int vn1 = std::stoi(token0[2]);
					std::vector<std::string> token1 = Split(tokens[2], '/');
					int v2 = std::stoi(token1[0]);
					int vt2 = std::stoi(token1[1]);
					int vn2 = std::stoi(token1[2]);
					std::vector<std::string> token2 = Split(tokens[3], '/');
					int v3 = std::stoi(token2[0]);
					int vt3 = std::stoi(token2[1]);
					int vn3 = std::stoi(token2[2]);
					std::vector<std::string> token3 = Split(tokens[4], '/');
					int v4 = std::stoi(token3[0]);
					int vt4 = std::stoi(token3[1]);
					int vn4 = std::stoi(token3[2]);
					geom_data.push_back(vertices[v1].pos.x);
					geom_data.push_back(vertices[v1].pos.y);
					geom_data.push_back(vertices[v1].pos.z);
					geom_data.push_back(vertices[v2].pos.x);
					geom_data.push_back(vertices[v2].pos.y);
					geom_data.push_back(vertices[v2].pos.z);
					geom_data.push_back(vertices[v3].pos.x);
					geom_data.push_back(vertices[v3].pos.y);
					geom_data.push_back(vertices[v3].pos.z);
					geom_data.push_back(vertices[v4].pos.x);
					geom_data.push_back(vertices[v4].pos.y);
					geom_data.push_back(vertices[v4].pos.z);
					geom_data.push_back(vertices[v3].pos.x);
					geom_data.push_back(vertices[v3].pos.y);
					geom_data.push_back(vertices[v3].pos.z);
					geom_data.push_back(vertices[v2].pos.x);
					geom_data.push_back(vertices[v2].pos.y);
					geom_data.push_back(vertices[v2].pos.z);
				}
			}
		}

    Mesh mesh;
    mesh.UpdateMesh(geom_data, (Mesh::VBufDataType::VertexPos), Mesh::ShaderSettings::None);
		mesh_instances[e.first] = mesh;
  }
}

const Mesh& SimpleLevelInstances::GetMeshInstance(uint16_t instanceType)
{
	return SimpleLevelInstances::mesh_instances[instanceType];
}