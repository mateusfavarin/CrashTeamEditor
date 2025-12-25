TARGET := CrashTeamEditor.out
CC := g++

PYTHON_EXECUTABLE ?= python
PYTHON_INCLUDE_DIR ?= $(shell $(PYTHON_EXECUTABLE) -c "import sysconfig, os; v = sysconfig.get_config_var('INCLUDEPY') or sysconfig.get_path('include'); print(v or '')" 2>/dev/null)
PYTHON_LIBRARY_DIR ?= $(shell $(PYTHON_EXECUTABLE) -c "import sysconfig, os; base = sysconfig.get_config_var('base') or sysconfig.get_config_var('prefix'); libdir = sysconfig.get_config_var('LIBDIR'); libdir = libdir or (os.path.join(base, 'libs') if base else ''); print(libdir or '')" 2>/dev/null)
PYTHON_LIBRARY_NAME ?= $(shell $(PYTHON_EXECUTABLE) -c "import sysconfig, os; lib = sysconfig.get_config_var('LDLIBRARY') or sysconfig.get_config_var('LIBRARY'); lib = lib or 'python' + sysconfig.get_config_var('py_version_nodot'); root, ext = os.path.splitext(lib); lib = lib if ext else lib + ('.lib' if os.name == 'nt' else '.so'); print(lib or '')" 2>/dev/null)
PYTHON_LIBRARY_PATH := $(PYTHON_LIBRARY_DIR)/$(PYTHON_LIBRARY_NAME)

ifeq ($(strip $(PYTHON_INCLUDE_DIR)),)
$(error Unable to detect the Python include directory. Set PYTHON_EXECUTABLE or override PYTHON_INCLUDE_DIR.)
endif
ifeq ($(strip $(PYTHON_LIBRARY_DIR)),)
$(error Unable to detect the Python library directory. Set PYTHON_EXECUTABLE or override PYTHON_LIBRARY_DIR.)
endif
ifeq ($(strip $(PYTHON_LIBRARY_NAME)),)
$(error Unable to detect the Python library name. Set PYTHON_EXECUTABLE or override PYTHON_LIBRARY_NAME.)
endif

SRCS := $(wildcard src/*.cpp) \
	third_party/imgui/imgui.cpp \
	third_party/imgui/imgui_demo.cpp \
	third_party/imgui/imgui_draw.cpp \
	third_party/imgui/imgui_tables.cpp \
	third_party/imgui/imgui_widgets.cpp \
	third_party/imgui/backends/imgui_impl_glfw.cpp \
	third_party/imgui/backends/imgui_impl_opengl3.cpp \
	third_party/imgui/misc/cpp/imgui_stdlib.cpp \
	src/manual_third_party/glad.c \
	python_bindings/cte_bindings.cpp


RELEASE_CXXFLAGS := -O2 -std=c++20
DEBUG_CXXFLAGS := -g -O0 -std=c++20

COMMON_CXXFLAGS := -Isrc \
	-Ithird_party/imgui \
	-Ithird_party/imgui/backends \
	-Ithird_party/glfw/include \
	-Ithird_party/portable-file-dialogs \
	-Ithird_party/json/include \
	-Ithird_party/pybind11/include \
	-I"$(PYTHON_INCLUDE_DIR)" \
	-Ithird_party/glm/glm \
	-Isrc/manual_third_party

COMMON_LINKER_FLAGS := -Lglfw-build-unix/src -lGL -lglfw3 -L"$(PYTHON_LIBRARY_DIR)" "$(PYTHON_LIBRARY_PATH)"

release-build-glfw:
	mkdir -p glfw-build-unix
	cd glfw-build-unix && cmake -S ../third_party/glfw # -G "Unix Makefiles" causes problems.
	cd glfw-build-unix && cmake --build . --config Release

debug-build-glfw:
	mkdir -p glfw-build-unix
	cd glfw-build-unix && cmake -S ../third_party/glfw
	cd glfw-build-unix && cmake --build . --config Debug # this "Debug" is the only difference

release: release-build-glfw
	$(CC) $(SRCS) -o $(TARGET) $(RELEASE_CXXFLAGS) $(COMMON_LINKER_FLAGS) $(COMMON_CXXFLAGS)

debug: debug-build-glfw
	$(CC) $(SRCS) -o $(TARGET) $(DEBUG_CXXFLAGS) $(COMMON_LINKER_FLAGS) $(COMMON_CXXFLAGS)

clean:
	rm -f $(TARGET)
	rm -rf glfw-build-unix
	rm -rf python_bindings/build
