#include <pybind11/embed.h>
#include <pybind11/operators.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <pybind11/stl/filesystem.h>
#include <pybind11/functional.h>

#include <filesystem>
#include <sstream>
#include <array>

#include "geo.h"
#include "text3d.h"
#include "quadblock.h"
#include "vertex.h"
#include "bsp.h"
#include "checkpoint.h"
#include "level.h"
#include "mesh.h"
#include "model.h"
#include "renderer.h"
#include "transform.h"
#include "path.h"

namespace py = pybind11;

PYBIND11_MAKE_OPAQUE(std::vector<Quadblock>);
PYBIND11_MAKE_OPAQUE(std::vector<Checkpoint>);
PYBIND11_MAKE_OPAQUE(std::vector<Path>);
PYBIND11_MAKE_OPAQUE(std::vector<size_t>);

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

std::string FormatPoint(const Point& point)
{
	std::ostringstream oss;
	oss << "<cte.Point pos=" << FormatVec3(point.pos)
		<< " normal=" << FormatVec3(point.normal) << ">";
	return oss.str();
}

std::array<float, 16> FormatMat4(const glm::mat4& matrix)
{
	std::array<float, 16> out = {};
	size_t index = 0;
	for (int col = 0; col < 4; ++col)
	{
		for (int row = 0; row < 4; ++row)
		{
			out[index++] = matrix[col][row];
		}
	}
	return out;
}

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

	py::class_<Point> point(m, "Point");
	point.def(py::init<>())
		.def(py::init<float, float, float>(), py::arg("x"), py::arg("y"), py::arg("z"))
		.def(py::init<float, float, float, unsigned char, unsigned char, unsigned char>(),
			py::arg("x"), py::arg("y"), py::arg("z"), py::arg("r"), py::arg("g"), py::arg("b"))
		.def(py::init<const Vec3&, const Vec3&, const Color&>(), py::arg("pos"), py::arg("normal"), py::arg("color"))
		.def_readwrite("pos", &Point::pos)
		.def_readwrite("normal", &Point::normal)
		.def_readwrite("color", &Point::color)
		.def_readwrite("uv", &Point::uv)
		.def("__repr__", [](const Point& p) { return FormatPoint(p); });

	py::enum_<PrimitiveType>(m, "PrimitiveType")
		.value("TRI", PrimitiveType::TRI)
		.value("QUAD", PrimitiveType::QUAD)
		.value("LINE", PrimitiveType::LINE)
		.export_values();

	py::class_<Primitive> primitive(m, "Primitive");
	primitive
		.def(py::init<PrimitiveType, unsigned>(), py::arg("type"), py::arg("point_count"))
		.def_readwrite("type", &Primitive::type)
		.def_readwrite("texture", &Primitive::texture)
		.def_readwrite("point_count", &Primitive::pointCount)
		.def_property("p0",
			[](Primitive& prim) -> Point& { return prim.p[0]; },
			[](Primitive& prim, const Point& value) { prim.p[0] = value; },
			py::return_value_policy::reference_internal)
		.def_property("p1",
			[](Primitive& prim) -> Point& { return prim.p[1]; },
			[](Primitive& prim, const Point& value) { prim.p[1] = value; },
			py::return_value_policy::reference_internal)
		.def_property("p2",
			[](Primitive& prim) -> Point& { return prim.p[2]; },
			[](Primitive& prim, const Point& value) { prim.p[2] = value; },
			py::return_value_policy::reference_internal)
		.def_property("p3",
			[](Primitive& prim) -> Point& { return prim.p[3]; },
			[](Primitive& prim, const Point& value) { prim.p[3] = value; },
			py::return_value_policy::reference_internal);

	py::class_<Tri, Primitive> tri(m, "Tri");
	tri.def(py::init<>())
		.def(py::init<const Point&, const Point&, const Point&>(), py::arg("p0"), py::arg("p1"), py::arg("p2"));

	py::class_<Quad, Primitive> quad(m, "Quad");
	quad.def(py::init<>())
		.def(py::init<const Point&, const Point&, const Point&, const Point&>(), py::arg("p0"), py::arg("p1"), py::arg("p2"), py::arg("p3"));

	py::class_<Line, Primitive> line(m, "Line");
	line.def(py::init<>())
		.def(py::init<const Point&, const Point&>(), py::arg("p0"), py::arg("p1"));

	py::class_<BoundingBox> bbox(m, "BoundingBox");
	bbox.def(py::init<>())
		.def_readwrite("min", &BoundingBox::min)
		.def_readwrite("max", &BoundingBox::max)
		.def("area", &BoundingBox::Area)
		.def("semi_perimeter", &BoundingBox::SemiPerimeter)
		.def("axis_length", &BoundingBox::AxisLength)
		.def("midpoint", &BoundingBox::Midpoint)
		.def("to_geometry", &BoundingBox::ToGeometry)
		.def("__repr__", [](const BoundingBox& b) { return FormatBoundingBox(b); });

	py::class_<Vertex> vertex(m, "Vertex");
	vertex.def(py::init<>())
		.def_readwrite("pos", &Vertex::m_pos)
		.def_readwrite("normal", &Vertex::m_normal)
		.def("get_color", &Vertex::GetColor, py::arg("high") = true)
		.def("to_geometry", &Vertex::ToGeometry, py::arg("high_color") = true)
		.def("__repr__", [](const Vertex& v) { return FormatVertex(v); });

	py::class_<Transform> transform(m, "Transform");
	transform.def(py::init<>())
		.def("clear", &Transform::Clear)
		.def("set_position", &Transform::SetPosition, py::arg("pos"))
		.def("set_scale", py::overload_cast<const Vec3&>(&Transform::SetScale), py::arg("scale"))
		.def("set_scale", py::overload_cast<float>(&Transform::SetScale), py::arg("scale"))
		.def("mult_scale", &Transform::MultScale, py::arg("factor"))
		.def("set_rotation", &Transform::SetRotation, py::arg("rotation"))
		.def("get_position", &Transform::GetPosition)
		.def("get_scale", &Transform::GetScale)
		.def("get_rotation", &Transform::GetRotation)
		.def("get_world_position", &Transform::GetWorldPosition)
		.def("get_world_scale", &Transform::GetWorldScale)
		.def("get_world_rotation", &Transform::GetWorldRotation)
		.def("get_model_matrix", [](const Transform& t) { return FormatMat4(t.GetModelMatrix()); });

	py::module_ text3d = m.def_submodule("text3d");
	py::enum_<Text3D::Align>(text3d, "Align")
		.value("LEFT", Text3D::Align::LEFT)
		.value("CENTER", Text3D::Align::CENTER)
		.value("RIGHT", Text3D::Align::RIGHT)
		.export_values();
	text3d.def("to_geometry", &Text3D::ToGeometry, py::arg("label"), py::arg("align"), py::arg("color"), py::arg("scale_mult") = 1.0f);

	py::class_<Mesh::RenderFlags>(m, "MeshRenderFlags")
		.def_readonly_static("NONE", &Mesh::RenderFlags::None)
		.def_readonly_static("DRAW_WIREFRAME", &Mesh::RenderFlags::DrawWireframe)
		.def_readonly_static("FORCE_DRAW_ON_TOP", &Mesh::RenderFlags::ForceDrawOnTop)
		.def_readonly_static("DRAW_BACKFACES", &Mesh::RenderFlags::DrawBackfaces)
		.def_readonly_static("DRAW_LINES_AA", &Mesh::RenderFlags::DrawLinesAA)
		.def_readonly_static("DONT_OVERRIDE_RENDER_FLAGS", &Mesh::RenderFlags::DontOverrideRenderFlags)
		.def_readonly_static("THICK_LINES", &Mesh::RenderFlags::ThickLines)
		.def_readonly_static("ALLOW_POINT_RENDER", &Mesh::RenderFlags::AllowPointRender)
		.def_readonly_static("DRAW_POINTS", &Mesh::RenderFlags::DrawPoints)
		.def_readonly_static("QUADBLOCK_LOD", &Mesh::RenderFlags::QuadblockLod)
		.def_readonly_static("FOLLOW_CAMERA", &Mesh::RenderFlags::FollowCamera);

	py::class_<Mesh::ShaderFlags>(m, "MeshShaderFlags")
		.def_readonly_static("NONE", &Mesh::ShaderFlags::None)
		.def_readonly_static("BLINKY", &Mesh::ShaderFlags::Blinky)
		.def_readonly_static("DISCARD_ZERO_COLOR", &Mesh::ShaderFlags::DiscardZeroColor);

	py::class_<Mesh> mesh(m, "Mesh");
	mesh.def(py::init<>())
		.def("set_geometry", py::overload_cast<const std::vector<Primitive>&, unsigned, unsigned>(&Mesh::SetGeometry),
			py::arg("primitives"), py::arg("render_flags") = Mesh::RenderFlags::None, py::arg("shader_flags") = Mesh::ShaderFlags::None)
		.def("set_geometry", py::overload_cast<const std::string&, Text3D::Align, const Color&, float>(&Mesh::SetGeometry),
			py::arg("label"), py::arg("align"), py::arg("color"), py::arg("scale_mult") = 1.0f)
		.def("update_primitive", &Mesh::UpdatePrimitive, py::arg("primitive"), py::arg("index"))
		.def("get_render_flags", &Mesh::GetRenderFlags)
		.def("set_render_flags", &Mesh::SetRenderFlags, py::arg("render_flags"))
		.def("get_shader_flags", &Mesh::GetShaderFlags)
		.def("set_shader_flags", &Mesh::SetShaderFlags, py::arg("shader_flags"))
		.def("clear", &Mesh::Clear)
		.def("is_ready", &Mesh::IsReady);

	py::class_<Model, Transform> model(m, "Model");
	model.def(py::init<>())
		.def("get_mesh", &Model::GetMesh, py::return_value_policy::reference_internal)
		.def("set_render_condition", &Model::SetRenderCondition, py::arg("render_condition"))
		.def("add_model", &Model::AddModel, py::return_value_policy::reference_internal)
		.def("clear_models", &Model::ClearModels)
		.def("remove_model", &Model::RemoveModel, py::arg("model"))
		.def("clear", &Model::Clear, py::arg("models"))
		.def("is_ready", &Model::IsReady);

	py::class_<Renderer> renderer(m, "Renderer");
	renderer
		.def("create_model", &Renderer::CreateModel, py::return_value_policy::reference_internal)
		.def("delete_model", &Renderer::DeleteModel, py::arg("model"))
		.def("get_last_delta_time", &Renderer::GetLastDeltaTime)
		.def("get_last_time", &Renderer::GetLastTime)
		.def("get_width", &Renderer::GetWidth)
		.def("get_height", &Renderer::GetHeight)
		.def("set_camera_to_level_spawn", &Renderer::SetCameraToLevelSpawn, py::arg("pos"), py::arg("rot"));

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
		.def_property("filter", &Quadblock::GetFilter, &Quadblock::SetFilter)
		.def_property("filter_color", &Quadblock::GetFilterColor, &Quadblock::SetFilterColor)
		.def_property("checkpoint_index", &Quadblock::GetCheckpoint, &Quadblock::SetCheckpoint)
		.def_property("checkpoint_status", &Quadblock::GetCheckpointStatus, &Quadblock::SetCheckpointStatus)
		.def_property("checkpoint_pathable", &Quadblock::GetCheckpointPathable, &Quadblock::SetCheckpointPathable)
		.def_property("tex_path",
			[](const Quadblock& qb) { return std::filesystem::path(qb.GetTexPath()); },
			&Quadblock::SetTexPath)
		.def_property_readonly("bounding_box", &Quadblock::GetBoundingBox, py::return_value_policy::copy)
		.def_property_readonly("uvs", &Quadblock::GetUVs, py::return_value_policy::copy)
		.def("is_quadblock", &Quadblock::IsQuadblock)
		.def("to_geometry", [](const Quadblock& qb, bool filterTriangles, py::object overrideUvsObj, py::object overrideTextureObj) {
			const std::array<QuadUV, NUM_FACES_QUADBLOCK + 1>* uvsPtr = nullptr;
			const std::filesystem::path* texPtr = nullptr;
			std::array<QuadUV, NUM_FACES_QUADBLOCK + 1> uvs = {};
			std::filesystem::path texPath;
			if (!overrideUvsObj.is_none())
			{
				uvs = overrideUvsObj.cast<std::array<QuadUV, NUM_FACES_QUADBLOCK + 1>>();
				uvsPtr = &uvs;
			}
			if (!overrideTextureObj.is_none())
			{
				texPath = overrideTextureObj.cast<std::filesystem::path>();
				texPtr = &texPath;
			}
			return qb.ToGeometry(filterTriangles, uvsPtr, texPtr);
		}, py::arg("filter_triangles") = false, py::arg("override_uvs") = py::none(), py::arg("override_texture_path") = py::none())
		.def("set_double_sided", &Quadblock::SetDrawDoubleSided)
		.def("set_speed_impact", &Quadblock::SetSpeedImpact)
		.def("translate", &Quadblock::Translate, py::arg("ratio"), py::arg("direction"))
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
		.def_property("color",
			[](const Checkpoint& cp) { return cp.GetColor(); },
			[](Checkpoint& cp, const Color& color) { cp.SetColor(color); })
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
		.def_property_readonly("startIndexes", &Path::GetStartIndexes, py::return_value_policy::reference_internal)
		.def_property_readonly("endIndexes", &Path::GetEndIndexes, py::return_value_policy::reference_internal)
		.def_property_readonly("ignoreIndexes", &Path::GetIgnoreIndexes, py::return_value_policy::reference_internal)
		.def("is_ready", &Path::IsReady)
		.def("set_index", &Path::SetIndex)
		.def("update_dist", &Path::UpdateDist, py::arg("dist"), py::arg("ref_point"), py::arg("checkpoints"))
		.def("generate_path", &Path::GeneratePath, py::arg("path_start_index"), py::arg("quadblocks"))
		.def_property("color",
			[](const Path& p) { return p.GetColor(); },
			[](Path& p, const Color& color) { p.SetColor(color); })
		.def("__copy__", [](const Path& p) { return Path(p); })
		.def("__deepcopy__", [](const Path& p, py::dict) { return Path(p); }, py::arg("memo"));

	py::bind_vector<std::vector<Quadblock>>(m, "QuadblockList");
	py::bind_vector<std::vector<Checkpoint>>(m, "CheckpointList");
	py::bind_vector<std::vector<Path>>(m, "PathList");
	py::bind_vector<std::vector<size_t>>(m, "IndexList");

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
		.def("tree", [](BSP& bsp) {
			py::list nodes;
			const std::vector<const BSP*> tree = bsp.GetTree();
			py::object owner = py::cast(&bsp);
			for (const BSP* node : tree)
			{
				nodes.append(py::cast(const_cast<BSP*>(node), py::return_value_policy::reference_internal, owner));
			}
			return nodes;
		})
		.def("leaves", [](BSP& bsp) {
			py::list nodes;
			const std::vector<const BSP*> leaves = bsp.GetLeaves();
			py::object owner = py::cast(&bsp);
			for (const BSP* node : leaves)
			{
				nodes.append(py::cast(const_cast<BSP*>(node), py::return_value_policy::reference_internal, owner));
			}
			return nodes;
		})
		.def("set_quadblock_indexes", &BSP::SetQuadblockIndexes)
		.def("clear", &BSP::Clear)
		.def("generate", &BSP::Generate, py::arg("quadblocks"), py::arg("max_quads_per_leaf"), py::arg("max_axis_length"));

	py::class_<Level> level(m, "Level");
	level
		.def(py::init<>())
		.def("load", &Level::Load, py::arg("filename"))
		.def("save", &Level::Save, py::arg("path"))
		.def_property_readonly("is_loaded", &Level::IsLoaded)
		.def("clear", &Level::Clear, py::arg("clear_errors") = true)
		.def("reset_filter", &Level::ResetFilter)
		.def("update_renderer_checkpoints", &Level::UpdateRenderCheckpointData)
		.def_property_readonly("name", &Level::GetName, py::return_value_policy::copy)
		.def_property_readonly("quadblocks", &Level::GetQuadblocks, py::return_value_policy::reference_internal)
		.def_property_readonly("bsp", &Level::GetBSP, py::return_value_policy::reference_internal)
		.def_property_readonly("checkpoints", &Level::GetCheckpoints, py::return_value_policy::reference_internal)
		.def_property_readonly("checkpoint_paths", &Level::GetCheckpointPaths, py::return_value_policy::reference_internal)
		.def_property_readonly("model_level", &Level::GetLevelModel, py::return_value_policy::reference_internal)
		.def_property_readonly("model_bsp", &Level::GetBspModel, py::return_value_policy::reference_internal)
		.def_property_readonly("model_spawn", &Level::GetSpawnModel, py::return_value_policy::reference_internal)
		.def_property_readonly("model_checkpoint", &Level::GetCheckpointModel, py::return_value_policy::reference_internal)
		.def_property_readonly("model_selected", &Level::GetSelectedModel, py::return_value_policy::reference_internal)
		.def_property_readonly("model_multi_selected", &Level::GetMultiSelectedModel, py::return_value_policy::reference_internal)
		.def_property_readonly("model_filter", &Level::GetFilterModel, py::return_value_policy::reference_internal)
		.def_property_readonly("parent_path", &Level::GetParentPath, py::return_value_policy::copy)
		.def("get_material_names", &Level::GetMaterialNames, py::return_value_policy::copy)
		.def("get_material_quadblock_indexes", &Level::GetMaterialQuadblockIndexes, py::arg("material"), py::return_value_policy::copy)
		.def("load_preset", &Level::LoadPreset, py::arg("filename"))
		.def("save_preset", &Level::SavePreset, py::arg("path"))
		.def("get_renderer_selected_data", [](Level& level) {
			auto selection = level.GetRendererSelectedData();
			const auto& quadblocks = std::get<0>(selection);
			py::list quadblockList;
			for (Quadblock* quadblock : quadblocks)
			{
				py::object quadblockObj = py::cast(quadblock, py::return_value_policy::reference_internal, py::cast(&level));
				quadblockList.append(quadblockObj);
			}
			return py::make_tuple(quadblockList, std::get<1>(selection));
		});
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
