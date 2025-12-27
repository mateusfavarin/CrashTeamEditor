#include <pybind11/embed.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <pybind11/stl/filesystem.h>

#include <filesystem>
#include <sstream>

#include "geo.h"
#include "quadblock.h"
#include "vertex.h"
#include "bsp.h"
#include "checkpoint.h"
#include "level.h"
#include "path.h"

namespace py = pybind11;

PYBIND11_MAKE_OPAQUE(std::vector<Quadblock>);
PYBIND11_MAKE_OPAQUE(std::vector<Checkpoint>);
PYBIND11_MAKE_OPAQUE(std::vector<Path>);

namespace
{
std::string FormatVec2(const Vec2& v)
{
	std::ostringstream oss;
	oss << "<cte.Vec2 x=" << v.x << " y=" << v.y << ">";
	return oss.str();
}

std::string FormatVec3(const Vec3& v)
{
	std::ostringstream oss;
	oss << "<cte.Vec3 x=" << v.x << " y=" << v.y << " z=" << v.z << ">";
	return oss.str();
}

std::string FormatColor(const Color& c)
{
	std::ostringstream oss;
	oss << "<cte.Color r=" << static_cast<int>(c.r)
		<< " g=" << static_cast<int>(c.g)
		<< " b=" << static_cast<int>(c.b)
		<< " a=" << c.a << ">";
	return oss.str();
}

std::string FormatBoundingBox(const BoundingBox& bbox)
{
	std::ostringstream oss;
	oss << "<cte.BoundingBox min=" << FormatVec3(bbox.min)
		<< " max=" << FormatVec3(bbox.max) << ">";
	return oss.str();
}

std::string FormatVertex(const Vertex& vertex)
{
	std::ostringstream oss;
	oss << "<cte.Vertex pos=" << FormatVec3(vertex.m_pos)
		<< " normal=" << FormatVec3(vertex.m_normal) << ">";
	return oss.str();
}
} // namespace

void init_crashteameditor(py::module_& m)
{
	m.doc() = "Pybind11 bindings for CrashTeamEditor";

	py::class_<Vec2> vec2(m, "Vec2");
	vec2.def(py::init<>())
		.def(py::init<float, float>())
		.def_readwrite("x", &Vec2::x)
		.def_readwrite("y", &Vec2::y)
		.def("__repr__", [](const Vec2& v) { return FormatVec2(v); });

	py::class_<Vec3> vec3(m, "Vec3");
	vec3.def(py::init<>())
		.def(py::init<float, float, float>())
		.def_readwrite("x", &Vec3::x)
		.def_readwrite("y", &Vec3::y)
		.def_readwrite("z", &Vec3::z)
		.def("length", &Vec3::Length)
		.def("normalize", &Vec3::Normalize)
		.def("dot", &Vec3::Dot)
		.def("cross", &Vec3::Cross)
		.def("__repr__", [](const Vec3& v) { return FormatVec3(v); })
		.def(py::self + py::self)
		.def(py::self - py::self)
		.def(py::self * float())
		.def(py::self / float());

	py::class_<Color> color(m, "Color");
	color.def(py::init<>())
		.def(py::init<unsigned char, unsigned char, unsigned char>())
		.def_readwrite("r", &Color::r)
		.def_readwrite("g", &Color::g)
		.def_readwrite("b", &Color::b)
		.def_readwrite("a", &Color::a)
		.def("__repr__", [](const Color& c) { return FormatColor(c); });

	py::class_<BoundingBox> bbox(m, "BoundingBox");
	bbox.def(py::init<>())
		.def_readwrite("min", &BoundingBox::min)
		.def_readwrite("max", &BoundingBox::max)
		.def("area", &BoundingBox::Area)
		.def("semi_perimeter", &BoundingBox::SemiPerimeter)
		.def("axis_length", &BoundingBox::AxisLength)
		.def("midpoint", &BoundingBox::Midpoint)
		.def("__repr__", [](const BoundingBox& b) { return FormatBoundingBox(b); });

	py::class_<Vertex> vertex(m, "Vertex");
	vertex.def(py::init<>())
		.def_readwrite("pos", &Vertex::m_pos)
		.def_readwrite("normal", &Vertex::m_normal)
		.def("get_color", &Vertex::GetColor, py::arg("high") = true)
		.def("__repr__", [](const Vertex& v) { return FormatVertex(v); });

	py::class_<VertexFlags>(m, "VertexFlags")
		.def_readonly_static("NONE", &VertexFlags::NONE);

	py::enum_<QuadblockTrigger>(m, "QuadblockTrigger")
		.value("NONE", QuadblockTrigger::NONE)
		.value("TURBO_PAD", QuadblockTrigger::TURBO_PAD)
		.value("SUPER_TURBO_PAD", QuadblockTrigger::SUPER_TURBO_PAD)
		.export_values();

	py::class_<QuadFlags>(m, "QuadFlags")
		.def_readonly_static("INVISIBLE", &QuadFlags::INVISIBLE)
		.def_readonly_static("MOON_GRAVITY", &QuadFlags::MOON_GRAVITY)
		.def_readonly_static("REFLECTION", &QuadFlags::REFLECTION)
		.def_readonly_static("KICKERS", &QuadFlags::KICKERS)
		.def_readonly_static("OUT_OF_BOUNDS", &QuadFlags::OUT_OF_BOUNDS)
		.def_readonly_static("NEVER_USED", &QuadFlags::NEVER_USED)
		.def_readonly_static("TRIGGER_SCRIPT", &QuadFlags::TRIGGER_SCRIPT)
		.def_readonly_static("REVERB", &QuadFlags::REVERB)
		.def_readonly_static("KICKERS_TWO", &QuadFlags::KICKERS_TWO)
		.def_readonly_static("MASK_GRAB", &QuadFlags::MASK_GRAB)
		.def_readonly_static("TIGER_TEMPLE_DOOR", &QuadFlags::TIGER_TEMPLE_DOOR)
		.def_readonly_static("COLLISION_TRIGGER", &QuadFlags::COLLISION_TRIGGER)
		.def_readonly_static("GROUND", &QuadFlags::GROUND)
		.def_readonly_static("WALL", &QuadFlags::WALL)
		.def_readonly_static("NO_COLLISION", &QuadFlags::NO_COLLISION)
		.def_readonly_static("INVISIBLE_TRIGGER", &QuadFlags::INVISIBLE_TRIGGER)
		.def_readonly_static("DEFAULT", &QuadFlags::DEFAULT);

	py::class_<TerrainType>(m, "TerrainType")
		.def_readonly_static("TURBO_PAD", &TerrainType::TURBO_PAD)
		.def_readonly_static("SUPER_TURBO_PAD", &TerrainType::SUPER_TURBO_PAD)
		.def_readonly_static("ASPHALT", &TerrainType::ASPHALT)
		.def_readonly_static("DIRT", &TerrainType::DIRT)
		.def_readonly_static("GRASS", &TerrainType::GRASS)
		.def_readonly_static("WOOD", &TerrainType::WOOD)
		.def_readonly_static("WATER", &TerrainType::WATER)
		.def_readonly_static("STONE", &TerrainType::STONE)
		.def_readonly_static("ICE", &TerrainType::ICE)
		.def_readonly_static("TRACK", &TerrainType::TRACK)
		.def_readonly_static("ICY_ROAD", &TerrainType::ICY_ROAD)
		.def_readonly_static("SNOW", &TerrainType::SNOW)
		.def_readonly_static("NONE", &TerrainType::NONE)
		.def_readonly_static("HARD_PACK", &TerrainType::HARD_PACK)
		.def_readonly_static("METAL", &TerrainType::METAL)
		.def_readonly_static("FAST_WATER", &TerrainType::FAST_WATER)
		.def_readonly_static("MUD", &TerrainType::MUD)
		.def_readonly_static("SIDE_SLIP", &TerrainType::SIDE_SLIP)
		.def_readonly_static("RIVER_ASPHALT", &TerrainType::RIVER_ASPHALT)
		.def_readonly_static("STEAM_ASPHALT", &TerrainType::STEAM_ASPHALT)
		.def_readonly_static("OCEAN_ASPHALT", &TerrainType::OCEAN_ASPHALT)
		.def_readonly_static("SLOW_GRASS", &TerrainType::SLOW_GRASS)
		.def_readonly_static("SLOW_DIRT", &TerrainType::SLOW_DIRT);

	py::class_<FaceRotateFlip>(m, "FaceRotateFlip")
		.def_readonly_static("NONE", &FaceRotateFlip::NONE)
		.def_readonly_static("ROTATE_90", &FaceRotateFlip::ROTATE_90)
		.def_readonly_static("ROTATE_180", &FaceRotateFlip::ROTATE_180)
		.def_readonly_static("ROTATE_270", &FaceRotateFlip::ROTATE_270)
		.def_readonly_static("FLIP_ROTATE_270", &FaceRotateFlip::FLIP_ROTATE_270)
		.def_readonly_static("FLIP_ROTATE_180", &FaceRotateFlip::FLIP_ROTATE_180)
		.def_readonly_static("FLIP_ROTATE_90", &FaceRotateFlip::FLIP_ROTATE_90)
		.def_readonly_static("FLIP", &FaceRotateFlip::FLIP);

	py::class_<Quadblock> quadblock(m, "Quadblock");
	quadblock
		.def_property("name", &Quadblock::GetName, &Quadblock::SetName)
		.def_property_readonly("center", &Quadblock::GetCenter, py::return_value_policy::copy)
		.def_property_readonly("normal", &Quadblock::GetNormal, py::return_value_policy::copy)
		.def_property("terrain", &Quadblock::GetTerrain, &Quadblock::SetTerrain)
		.def_property("flags", &Quadblock::GetFlags, &Quadblock::SetFlag)
		.def_property("trigger", &Quadblock::GetTrigger, &Quadblock::SetTrigger)
		.def_property("turbo_pad_index", &Quadblock::GetTurboPadIndex, &Quadblock::SetTurboPadIndex)
		.def_property("bsp_id", &Quadblock::GetBSPID, &Quadblock::SetBSPID)
		.def_property("hide", &Quadblock::GetHide, &Quadblock::SetHide)
		.def_property("animated", &Quadblock::GetAnimated, &Quadblock::SetAnimated)
		.def_property("checkpoint_index", &Quadblock::GetCheckpoint, &Quadblock::SetCheckpoint)
		.def_property("checkpoint_status", &Quadblock::GetCheckpointStatus, &Quadblock::SetCheckpointStatus)
		.def_property("tex_path",
			[](const Quadblock& qb) { return std::filesystem::path(qb.GetTexPath()); },
			&Quadblock::SetTexPath)
		.def_property_readonly("bounding_box", &Quadblock::GetBoundingBox, py::return_value_policy::copy)
		.def_property_readonly("uvs", &Quadblock::GetUVs, py::return_value_policy::copy)
		.def("is_quadblock", &Quadblock::IsQuadblock)
		.def("set_double_sided", &Quadblock::SetDrawDoubleSided)
		.def("set_speed_impact", &Quadblock::SetSpeedImpact)
		.def("translate_normal", &Quadblock::TranslateNormalVec, py::arg("ratio"))
		.def("distance_closest_vertex", [](const Quadblock& qb, const Vec3& point) {
			Vec3 closest;
			float distance = qb.DistanceClosestVertex(closest, point);
			return py::make_tuple(closest, distance);
		})
		.def("neighbours", &Quadblock::Neighbours, py::arg("other"), py::arg("threshold") = 0.1f)
		.def("get_vertices", &Quadblock::GetVertices)
		.def("get_unswizzled_vertices", [](const Quadblock& qb) {
			py::list verts;
			const Vertex* ptr = qb.GetUnswizzledVertices();
			if (ptr)
			{
				for (size_t i = 0; i < NUM_VERTICES_QUADBLOCK; ++i) { verts.append(ptr[i]); }
			}
			return verts;
		})
		.def("get_quad_uv", &Quadblock::GetQuadUV, py::arg("quad"), py::return_value_policy::copy)
		.def("set_texture_id", &Quadblock::SetTextureID, py::arg("texture_id"), py::arg("quad"))
		.def("set_anim_texture_offset", &Quadblock::SetAnimTextureOffset, py::arg("rel_offset"), py::arg("lev_offset"), py::arg("quad"))
		.def("set_checkpoint_status", &Quadblock::SetCheckpointStatus)
		.def("set_trigger", &Quadblock::SetTrigger)
		.def("compute_normal_vector", &Quadblock::ComputeNormalVector, py::arg("id0"), py::arg("id1"), py::arg("id2"));

	py::class_<Checkpoint> checkpoint(m, "Checkpoint");
	checkpoint
		.def(py::init<int>())
		.def(py::init<int, const Vec3&, const std::string&>(),
			py::arg("index"), py::arg("position"), py::arg("quad_name"))
		.def("get_index", &Checkpoint::GetIndex)
		.def("set_index", &Checkpoint::SetIndex)
		.def("get_dist_finish", &Checkpoint::GetDistFinish)
		.def("get_pos", &Checkpoint::GetPos, py::return_value_policy::copy)
		.def("get_up", &Checkpoint::GetUp)
		.def("get_down", &Checkpoint::GetDown)
		.def("get_left", &Checkpoint::GetLeft)
		.def("get_right", &Checkpoint::GetRight)
		.def("update_dist_finish", &Checkpoint::UpdateDistFinish)
		.def("update_up", &Checkpoint::UpdateUp)
		.def("update_down", &Checkpoint::UpdateDown)
		.def("update_left", &Checkpoint::UpdateLeft)
		.def("update_right", &Checkpoint::UpdateRight)
		.def("get_delete", &Checkpoint::GetDelete)
		.def("remove_invalid_checkpoints", &Checkpoint::RemoveInvalidCheckpoints)
		.def("update_invalid_checkpoints", &Checkpoint::UpdateInvalidCheckpoints);

	m.attr("NONE_CHECKPOINT_INDEX") = NONE_CHECKPOINT_INDEX;

	py::class_<Path> path(m, "Path");
	path
		.def(py::init<>())
		.def(py::init<size_t>(), py::arg("index"))
		.def(py::init<const Path&>(), py::arg("other"))
		.def("get_index", &Path::GetIndex)
		.def("get_start", &Path::GetStart)
		.def("get_end", &Path::GetEnd)
		.def("is_ready", &Path::IsReady)
		.def("set_index", &Path::SetIndex)
		.def("update_dist", &Path::UpdateDist, py::arg("dist"), py::arg("ref_point"), py::arg("checkpoints"))
		.def("generate_path", &Path::GeneratePath, py::arg("path_start_index"), py::arg("quadblocks"))
		.def("__copy__", [](const Path& p) { return Path(p); })
		.def("__deepcopy__", [](const Path& p, py::dict) { return Path(p); }, py::arg("memo"));

	py::bind_vector<std::vector<Quadblock>>(m, "QuadblockList");
	py::bind_vector<std::vector<Checkpoint>>(m, "CheckpointList");
	py::bind_vector<std::vector<Path>>(m, "PathList");

	py::enum_<BSPNode> bspNode(m, "BSPNode");
	bspNode
		.value("BRANCH", BSPNode::BRANCH)
		.value("LEAF", BSPNode::LEAF)
		.export_values();

	py::enum_<AxisSplit> axisSplit(m, "AxisSplit");
	axisSplit
		.value("NONE", AxisSplit::NONE)
		.value("X", AxisSplit::X)
		.value("Y", AxisSplit::Y)
		.value("Z", AxisSplit::Z)
		.export_values();

	py::class_<BSPFlags>(m, "BSPFlags")
		.def_readonly_static("NONE", &BSPFlags::NONE)
		.def_readonly_static("LEAF", &BSPFlags::LEAF)
		.def_readonly_static("WATER", &BSPFlags::WATER)
		.def_readonly_static("SUBDIV_4_1", &BSPFlags::SUBDIV_4_1)
		.def_readonly_static("SUBDIV_4_2", &BSPFlags::SUBDIV_4_2)
		.def_readonly_static("INVISIBLE", &BSPFlags::INVISIBLE)
		.def_readonly_static("NO_COLLISION", &BSPFlags::NO_COLLISION);

	py::class_<BSPID>(m, "BSPID")
		.def_readonly_static("LEAF", &BSPID::LEAF)
		.def_readonly_static("EMPTY", &BSPID::EMPTY);

	py::class_<BSP> bsp(m, "BSP");
	bsp
		.def(py::init<>())
		.def(py::init([](BSPNode type, const std::vector<size_t>& quadblockIndexes, py::object parent, const std::vector<Quadblock>& quadblocks)
			{
				BSP* parentPtr = parent.is_none() ? nullptr : parent.cast<BSP*>();
				return BSP(type, quadblockIndexes, parentPtr, quadblocks);
			}),
			py::arg("type"), py::arg("quadblock_indexes"), py::arg("parent") = py::none(), py::arg("quadblocks"))
		.def("get_id", &BSP::GetId)
		.def("is_empty", &BSP::IsEmpty)
		.def("is_valid", &BSP::IsValid)
		.def("is_branch", &BSP::IsBranch)
		.def("flags", &BSP::GetFlags)
		.def("type", &BSP::GetType)
		.def("axis", &BSP::GetAxis)
		.def("bounding_box", &BSP::GetBoundingBox, py::return_value_policy::copy)
		.def("quadblock_indexes", &BSP::GetQuadblockIndexes, py::return_value_policy::copy)
		.def("left_child", &BSP::GetLeftChildren, py::return_value_policy::reference_internal)
		.def("right_child", &BSP::GetRightChildren, py::return_value_policy::reference_internal)
		.def("parent", &BSP::GetParent, py::return_value_policy::reference_internal)
		.def("tree", &BSP::GetTree)
		.def("leaves", &BSP::GetLeaves)
		.def("set_quadblock_indexes", &BSP::SetQuadblockIndexes)
		.def("clear", &BSP::Clear)
		.def("generate", &BSP::Generate, py::arg("quadblocks"), py::arg("max_quads_per_leaf"), py::arg("max_axis_length"));

	py::class_<Level> level(m, "Level");
	level
		.def(py::init<>())
		.def("load", &Level::Load, py::arg("filename"))
		.def("save", &Level::Save, py::arg("path"))
		.def_property_readonly("is_loaded", &Level::IsLoaded)
		.def("open_hot_reload_window", &Level::OpenHotReloadWindow)
		.def("clear", &Level::Clear, py::arg("clear_errors") = true)
		.def_property_readonly("name", &Level::GetName, py::return_value_policy::copy)
		.def_property_readonly("quadblocks", &Level::GetQuadblocks, py::return_value_policy::reference_internal)
		.def_property_readonly("bsp", &Level::GetBSP, py::return_value_policy::reference_internal)
		.def_property_readonly("checkpoints", &Level::GetCheckpoints, py::return_value_policy::reference_internal)
		.def_property_readonly("checkpoint_paths", &Level::GetCheckpointPaths, py::return_value_policy::reference_internal)
		.def_property_readonly("parent_path", &Level::GetParentPath, py::return_value_policy::copy)
		.def("load_preset", &Level::LoadPreset, py::arg("filename"))
		.def("save_preset", &Level::SavePreset, py::arg("path"));
}

#if defined(CTE_EXTENSION_BUILD)
PYBIND11_MODULE(crashteameditor, m)
{
	init_crashteameditor(m);
}
#endif

#if defined(CTE_EMBEDDED_BUILD)
PYBIND11_EMBEDDED_MODULE(crashteameditor, m)
{
	init_crashteameditor(m);
}
#endif
