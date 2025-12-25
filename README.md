# CrashTeamEditor
Track editor for the Playstation 1 game Crash Team Racing

## Building

### Windows (Visual Studio)

1. Clone and fetch submodules (pybind11, glfw, etc.):
   - `git submodule update --init --recursive`
2. Install Python (the editor embeds Python via pybind11) and set `PYTHON_HOME` to your Python install root (the folder that contains `include/` and `libs/`).
   - Example: `PYTHON_HOME=C:\Users\you\AppData\Local\Programs\Python\Python312`
3. Open `CrashTeamEditor.sln` and build `x64` `Debug`/`Release`.
4. If the executable fails to start due to a missing `python3xx.dll`, add these to your debugger environment `PATH` (or system `PATH`):
   - `$(PYTHON_HOME)`
   - `$(PYTHON_HOME)\DLLs`

### Makefile (Linux/macOS/MSYS2)

- `make release` (or `make debug`)
- The Makefile uses `PYTHON_EXECUTABLE` (default: `python`) to detect include/lib paths via `sysconfig`.

## Python bindings

Bindings are implemented with pybind11 (included under `third_party/pybind11`). The same `python_bindings/cte_bindings.cpp` file exposes a standalone `crashteameditor` module and also registers an embedded module that the editor's built-in Python console reuses.

### Building CrashTeamEditor with Python support

Because the editor links `python_bindings/cte_bindings.cpp` directly, the build needs access to Python headers and libraries. See the `Building` section above for platform-specific setup (especially `PYTHON_HOME` on Windows).

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
