# CrashTeamEditor Python API (`crashteameditor`)

This document describes the Python bindings exposed by `python_bindings/cte_bindings.cpp` via pybind11.

## Importing

```py
import crashteameditor as cte
```

In the in-editor Python window, the editor also injects globals `m_lev` (the currently loaded `Level`) and `m_rend` (the active `Renderer`). You still need to `import crashteameditor` to access helper types/constants such as `cte.TerrainType`.

Most bindings that return C++ vectors can be wrapped with `list(...)` in Python to get a plain list.

## Module-level constants

- `NONE_CHECKPOINT_INDEX: int` — sentinel index used by checkpoints.

## Helper enums / constant groups

These are exposed as classes with read-only static integer attributes (use as `cte.TerrainType.WOOD`, etc.).

### `cte.VertexFlags`

- `NONE: int`

### `cte.QuadblockTrigger` (enum)

- `NONE`
- `TURBO_PAD`
- `SUPER_TURBO_PAD`

### `cte.QuadFlags`

Bit flags for `Quadblock.flags`.

- `INVISIBLE`, `MOON_GRAVITY`, `REFLECTION`, `KICKERS`, `OUT_OF_BOUNDS`, `NEVER_USED`, `TRIGGER_SCRIPT`, `REVERB`, `KICKERS_TWO`, `MASK_GRAB`, `TIGER_TEMPLE_DOOR`, `COLLISION_TRIGGER`, `GROUND`, `WALL`, `NO_COLLISION`, `INVISIBLE_TRIGGER`
- `DEFAULT`

### `cte.TerrainType`

Terrain IDs for `Quadblock.terrain`.

- `ASPHALT`, `DIRT`, `GRASS`, `WOOD`, `WATER`, `STONE`, `ICE`, `TRACK`, `ICY_ROAD`, `SNOW`, `NONE`, `HARD_PACK`, `METAL`, `FAST_WATER`, `MUD`, `SIDE_SLIP`, `RIVER_ASPHALT`, `STEAM_ASPHALT`, `OCEAN_ASPHALT`, `SLOW_GRASS`, `SLOW_DIRT`
- `TURBO_PAD`, `SUPER_TURBO_PAD`

### `cte.FaceRotateFlip`

Rotation/flip IDs used by the editor’s face UV metadata.

- `NONE`, `ROTATE_90`, `ROTATE_180`, `ROTATE_270`, `FLIP_ROTATE_270`, `FLIP_ROTATE_180`, `FLIP_ROTATE_90`, `FLIP`

### `cte.BSPNode` (enum)

- `BRANCH`
- `LEAF`

### `cte.AxisSplit` (enum)

- `NONE`
- `X`
- `Y`
- `Z`

### `cte.BSPFlags`

- `NONE`, `LEAF`, `WATER`, `SUBDIV_4_1`, `SUBDIV_4_2`, `INVISIBLE`, `NO_COLLISION`

### `cte.BSPID`

- `LEAF`
- `EMPTY`

## Geometry / utility types

### `cte.Vec2`

Constructors:
- `Vec2()`
- `Vec2(x: float, y: float)`

Fields:
- `x: float`
- `y: float`

### `cte.Vec3`

Constructors:
- `Vec3()`
- `Vec3(x: float, y: float, z: float)`

Fields:
- `x: float`
- `y: float`
- `z: float`

Methods:
- `length() -> float`
- `normalize() -> None` (in-place)
- `dot(other: Vec3) -> float`
- `cross(other: Vec3) -> Vec3`

Operators:
- `a + b`, `a - b`, `a * float`, `a / float`

### `cte.Color`

Constructors:
- `Color()`
- `Color(r: int, g: int, b: int)` (bytes 0–255)

Fields:
- `r: int`
- `g: int`
- `b: int`
- `a: int`

### `cte.Point`

Constructors:
- `Point()`
- `Point(x: float, y: float, z: float)`
- `Point(x: float, y: float, z: float, r: int, g: int, b: int)`
- `Point(pos: Vec3, normal: Vec3, color: Color)`

Fields:
- `pos: Vec3`
- `normal: Vec3`
- `color: Color`
- `uv: Vec2`

### `cte.PrimitiveType` (enum)

- `TRI`
- `QUAD`
- `LINE`

### `cte.Primitive`

Constructors:
- `Primitive(type: PrimitiveType, point_count: int)`

Fields:
- `type: PrimitiveType`
- `texture: str`
- `point_count: int`

Methods:
Properties:
- `p0: Point` (live reference)
- `p1: Point` (live reference)
- `p2: Point` (live reference)
- `p3: Point` (live reference)

### `cte.Tri` / `cte.Quad` / `cte.Line`

Constructors:
- `Tri()`, `Quad()`, `Line()`
- `Tri(p0: Point, p1: Point, p2: Point)`
- `Quad(p0: Point, p1: Point, p2: Point, p3: Point)`
- `Line(p0: Point, p1: Point)`

### `cte.BoundingBox`

Constructors:
- `BoundingBox()`

Fields:
- `min: Vec3`
- `max: Vec3`

Methods:
- `area() -> float`
- `semi_perimeter() -> float`
- `axis_length() -> Vec3`
- `midpoint() -> Vec3`
- `to_geometry() -> list[Primitive]`

### `cte.Vertex`

Constructors:
- `Vertex()`

Fields:
- `pos: Vec3`
- `normal: Vec3`

Methods:
- `get_color(high: bool = True) -> Color`
- `to_geometry(high_color: bool = True) -> list[Primitive]`

## Rendering types

### `cte.Transform`

Methods:
- `clear() -> None`
- `set_position(pos: Vec3) -> None`
- `set_scale(scale: Vec3) -> None`
- `set_scale(scale: float) -> None`
- `mult_scale(factor: float) -> None`
- `set_rotation(rotation: Vec3) -> None`
- `get_position() -> Vec3`
- `get_scale() -> Vec3`
- `get_rotation() -> Vec3`
- `get_world_position() -> Vec3`
- `get_world_scale() -> Vec3`
- `get_world_rotation() -> Vec3`
- `get_model_matrix() -> list[float]` (16 floats, column-major)

### `cte.MeshRenderFlags`

- `NONE`, `DRAW_WIREFRAME`, `FORCE_DRAW_ON_TOP`, `DRAW_BACKFACES`, `DRAW_LINES_AA`, `DONT_OVERRIDE_RENDER_FLAGS`, `THICK_LINES`, `ALLOW_POINT_RENDER`, `DRAW_POINTS`, `QUADBLOCK_LOD`, `FOLLOW_CAMERA`

### `cte.MeshShaderFlags`

- `NONE`, `BLINKY`, `DISCARD_ZERO_COLOR`

### `cte.Mesh`

Constructors:
- `Mesh()`

Methods:
- `set_geometry(primitives: list[Primitive], render_flags: int = MeshRenderFlags.NONE, shader_flags: int = MeshShaderFlags.NONE) -> None`
- `set_geometry(label: str, align: text3d.Align, color: Color, scale_mult: float = 1.0) -> None`
- `update_primitive(primitive: Primitive, index: int) -> int`
- `get_render_flags() -> int`
- `set_render_flags(render_flags: int) -> None`
- `get_shader_flags() -> int`
- `set_shader_flags(shader_flags: int) -> None`
- `clear() -> None`
- `is_ready() -> bool`

### `cte.Model` (inherits `Transform`)

Constructors:
- `Model()`

Methods:
- `get_mesh() -> Mesh` (live reference)
- `set_render_condition(render_condition: callable) -> None`
  - `render_condition` must be a callable with no args that returns `bool`.
  - Python callables are converted to a C++ `std::function<bool()>` via pybind11.
- `add_model() -> Model` (live reference)
- `clear_models() -> None`
- `remove_model(model: Model) -> bool`
- `clear(models: bool) -> None`
- `is_ready() -> bool`

### `cte.Renderer`

Methods:
- `create_model() -> Model` (live reference)
- `delete_model(model: Model) -> bool`
- `get_last_delta_time() -> float`
- `get_last_time() -> float`
- `get_width() -> float`
- `get_height() -> float`
- `set_camera_to_level_spawn(pos: Vec3, rot: Vec3) -> None`

## Text rendering

### `cte.text3d.Align` (enum)

- `LEFT`
- `CENTER`
- `RIGHT`

### `cte.text3d.to_geometry`

`to_geometry(label: str, align: text3d.Align, color: Color, scale_mult: float = 1.0) -> list[Primitive]`

## Editor domain types

### `cte.Quadblock`

Properties:
- `name: str`
- `center: Vec3` (copy)
- `normal: Vec3` (copy)
- `terrain: int`
- `flags: int` (note: assignment overwrites the full bitfield)
- `trigger: QuadblockTrigger`
- `turbo_pad_index: int`
- `bsp_id: int`
- `hide: bool`
- `animated: bool`
- `filter: bool`
- `filter_color: Color`
- `checkpoint_index: int`
- `checkpoint_status: bool`
- `checkpoint_pathable: bool`
- `tex_path: pathlib.Path`
- `bounding_box: BoundingBox` (copy)
- `uvs: list` (copy of the UV array)

Methods:
- `is_quadblock() -> bool`
- `to_geometry(filter_triangles: bool = False, override_uvs: list | None = None, override_texture_path: pathlib.Path | None = None) -> list[Primitive]`
  - `override_uvs` should be a list of 5 items (quad 0-3 plus low LOD), each item a list of 4 `Vec2`.
- `set_double_sided(active: bool) -> None`
- `set_speed_impact(speed: int) -> None`
- `translate(ratio: float, direction: Vec3) -> None`
- `distance_closest_vertex(point: Vec3) -> tuple[Vec3, float]` (returns `(closest_point, distance)`)
- `neighbours(other: Quadblock, threshold: float = 0.1) -> bool`
- `get_vertices() -> list[Vertex]` (copy)
- `get_unswizzled_vertices() -> list[Vertex]` (copy)
- `get_quad_uv(quad: int) -> object` (UV record; returned as a copy)
- `set_texture_id(texture_id: int, quad: int) -> None`
- `set_anim_texture_offset(rel_offset: int, lev_offset: int, quad: int) -> None`
- `set_checkpoint_status(active: bool) -> None`
- `set_trigger(trigger: QuadblockTrigger) -> None`
- `compute_normal_vector(id0: int, id1: int, id2: int) -> Vec3`

### `cte.Checkpoint`

Constructors:
- `Checkpoint(index: int)`
- `Checkpoint(index: int, position: Vec3, quad_name: str)`

Properties:
- `color: Color` (tint applied to the checkpoint when rendered)

Methods:
- `get_index() -> int`
- `set_index(idx: int) -> None`
- `get_dist_finish() -> float`
- `get_pos() -> Vec3` (copy)
- `get_up() -> int`
- `get_down() -> int`
- `get_left() -> int`
- `get_right() -> int`
- `update_dist_finish(dist: float) -> None`
- `update_up(up: int) -> None`
- `update_down(down: int) -> None`
- `update_left(left: int) -> None`
- `update_right(right: int) -> None`
- `get_delete() -> bool`
- `remove_invalid_checkpoints(invalid_indexes: list[int]) -> None`
- `update_invalid_checkpoints(invalid_indexes: list[int]) -> None`

### `cte.Path`

Constructors:
- `Path()`
- `Path(index: int)`
- `Path(other: Path)` (copy constructor)

Properties:
- `startIndexes: list[int]` (live reference)
- `endIndexes: list[int]` (live reference)
- `ignoreIndexes: list[int]` (live reference)
- `color: Color` (base tint assigned to checkpoints generated by this path)

Methods:
- `get_index() -> int`
- `get_start() -> int`
- `get_end() -> int`
- `is_ready() -> bool`
- `set_index(index: int) -> None`
- `update_dist(dist: float, ref_point: Vec3, checkpoints: list[Checkpoint]) -> None`
- `generate_path(path_start_index: int, quadblocks: list[Quadblock]) -> list[Checkpoint]`

Copy helpers:
- `copy.copy(path)` / `copy.deepcopy(path)` are supported.

### `cte.BSP`

Constructors:
- `BSP()`
- `BSP(type: BSPNode, quadblock_indexes: list[int], parent: BSP | None = None, quadblocks: list[Quadblock])`

Methods:
- `get_id() -> int`
- `is_empty() -> bool`
- `is_valid() -> bool`
- `is_branch() -> bool`
- `flags() -> int`
- `type() -> str`
- `axis() -> str`
- `bounding_box() -> BoundingBox` (copy)
- `quadblock_indexes() -> list[int]` (copy)
- `left_child() -> BSP | None`
- `right_child() -> BSP | None`
- `parent() -> BSP | None`
- `tree() -> list[BSP]` (live references)
- `leaves() -> list[BSP]` (live references)
- `set_quadblock_indexes(quadblock_indexes: list[int]) -> None`
- `clear() -> None`
- `generate(quadblocks: list[Quadblock], max_quads_per_leaf: int, max_axis_length: float) -> None`

### `cte.Level`

Constructors:
- `Level()`

Methods:
- `load(filename: pathlib.Path) -> bool`
- `save(path: pathlib.Path) -> bool`
- `clear(clear_errors: bool = True) -> None`
- `reset_filter() -> None`
- `get_material_names() -> list[str]` (copy)
- `get_material_quadblock_indexes(material: str) -> list[int]` (copy)
- `load_preset(filename: pathlib.Path) -> bool`
- `save_preset(path: pathlib.Path) -> bool`
- `get_renderer_selected_data() -> tuple[list[Quadblock], Vec3]` (returns `(quadblocks, query_point)`; Quadblock entries are live references, Vec3 is a copy)

Properties:
- `is_loaded: bool`
- `name: str`
- `quadblocks: list[Quadblock]` (live references)
- `bsp: BSP` (live reference)
- `checkpoints: list[Checkpoint]` (live references)
- `checkpoint_paths: list[Path]` (live references)
- `model_level: Model` (live reference)
- `model_bsp: Model` (live reference)
- `model_spawn: Model` (live reference)
- `model_checkpoint: Model` (live reference)
- `model_selected: Model` (live reference)
- `model_multi_selected: Model` (live reference)
- `model_filter: Model` (live reference)
- `parent_path: pathlib.Path` (copy)
