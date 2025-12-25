# CrashTeamEditor
Track editor for the Playstation 1 game Crash Team Racing

## Python bindings

Bindings are implemented with pybind11 (included under `third_party/pybind11`). The same `python_bindings/cte_bindings.cpp` file exposes a standalone `crashteameditor` module and also registers an embedded module that the editor's built-in Python console reuses.

### In-editor Python console

A `Window → Python Console` menu entry opens the embedded console, which combines:

- a multiline script editor pre-filled with a starter snippet,
- a **Run** button that executes the script inside the embedded interpreter while stdout/stderr are redirected back into the UI,
- a console pane that shows the captured output or traceback.

Scripts share the same `crashteameditor` bindings, plus a global `m_lev` object that points to the `Level` instance displayed in the UI, so you can inspect or mutate the currently loaded level on the fly.

### Building CrashTeamEditor with Python support

Because the editor now links `python_bindings/cte_bindings.cpp` directly, the Makefile and Visual Studio project both need access to Python headers and libraries. The Makefile probes `PYTHON_EXECUTABLE` (e.g. `python` or `py -3`) and derives `PYTHON_INCLUDE_DIR`, `PYTHON_LIBRARY_DIR`, and `PYTHON_LIBRARY_NAME` from `sysconfig`; override any of those if your interpreter lives in a custom location. The Visual Studio build respects `PYTHON_HOME` (or `PYTHON_INCLUDE_DIR`/`PYTHON_LIB`), so set them before opening the solution or pass `/p:PYTHON_INCLUDE_DIR=...` and `/p:PYTHON_LIB=...` to `msbuild`. These settings just point the compiler to `pybind11/embed.h` and the Python import library that the editor links.

### Standalone crashteameditor module

1. Configure the bindings build alongside the existing sources:
   ```sh
   cmake -S python_bindings -B python_bindings/build
   ```
   You may need to set `PYTHON_EXECUTABLE` if the default interpreter is not the one you intend to use.
2. Compile the module:
   ```sh
   cmake --build python_bindings/build --config Release
   ```
3. Import the generated `crashteameditor` module from Python scripts in the same interpreter whose headers were used during the build.

The `crashteameditor` module bundles the following bindings:

- Geometry helpers (`Vec2`, `Vec3`, `Color`, `Vertex`, `BoundingBox`) plus `Quadblock` with its properties, UVs, serialization, and neighbor helpers.
- `Checkpoint` allows reading and mutating checkpoint state, serializing to bytes, and pruning invalid indexes.
- `Path` exposes start/end markers, readiness checks, and JSON (via Python `json` objects) export/import helpers for the checkpoint routing data.
- `BSP` and `Level` wrap the existing spatial structure and level lifecycle APIs so Python scripts can generate BSPs, query quadblocks, and save/load `.lev` presets.
