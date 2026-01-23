TARGET := CrashTeamEditor.out
CC := g++

PYTHON_EXECUTABLE ?= python
PYTHON_INCLUDE_DIR ?= $(shell $(PYTHON_EXECUTABLE) -c "import sysconfig, os; v = sysconfig.get_config_var('INCLUDEPY') or sysconfig.get_path('include'); print(v or '')" 2>/dev/null)
PYTHON_LIBRARY_DIR ?= $(shell $(PYTHON_EXECUTABLE) -c "import sysconfig, os; base = sysconfig.get_config_var('base') or sysconfig.get_config_var('prefix'); libdir = sysconfig.get_config_var('LIBDIR'); libdir = libdir or (os.path.join(base, 'libs') if base else ''); print(libdir or '')" 2>/dev/null)
PYTHON_LIBRARY_NAME ?= $(shell $(PYTHON_EXECUTABLE) -c "import sysconfig, os; lib = sysconfig.get_config_var('LDLIBRARY') or sysconfig.get_config_var('LIBRARY'); lib = lib or 'python' + sysconfig.get_config_var('py_version_nodot'); root, ext = os.path.splitext(lib); lib = lib if ext else lib + ('.lib' if os.name == 'nt' else '.so'); print(lib or '')" 2>/dev/null)
PYTHON_LIBRARY_PATH := $(PYTHON_LIBRARY_DIR)/$(PYTHON_LIBRARY_NAME)
PYTHON_VERSION_SHORT := $(shell $(PYTHON_EXECUTABLE) -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')")
PYTHON_STDLIB_DIR := $(shell $(PYTHON_EXECUTABLE) -c "import sysconfig; print(sysconfig.get_path('stdlib') or '')")
PYTHON_PURELIB_DIR := $(shell $(PYTHON_EXECUTABLE) -c "import sysconfig; print(sysconfig.get_path('purelib') or '')")
PYTHON_PLATLIB_DIR := $(shell $(PYTHON_EXECUTABLE) -c "import sysconfig; print(sysconfig.get_path('platlib') or '')")
PYTHON_BUNDLE_DIR ?= python
PYTHON_LIB_TARGET := $(PYTHON_BUNDLE_DIR)/lib/python$(PYTHON_VERSION_SHORT)

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


RELEASE_CXXFLAGS := -O2 -std=c++20 -DNDEBUG
DEBUG_CXXFLAGS := -g -O0 -std=c++20 -D_DEBUG

COMMON_CXXFLAGS := -DCTE_EMBEDDED_BUILD -fopenmp -Isrc \
	-Ithird_party/imgui \
	-Ithird_party/imgui/backends \
	-Ithird_party/glfw/include \
	-Ithird_party/portable-file-dialogs \
	-Ithird_party/json/include \
	-Ithird_party/pybind11/include \
	-I"$(PYTHON_INCLUDE_DIR)" \
	-Ithird_party/glm/glm \
	-Ithird_party/stb \
	-Isrc/manual_third_party

COMMON_LINKER_FLAGS := -Lglfw-build-unix/src -lGL -lglfw3 -Wl,-rpath,'$$ORIGIN/$(PYTHON_BUNDLE_DIR)/lib' -L"$(PYTHON_LIBRARY_DIR)" "$(PYTHON_LIBRARY_PATH)"

release-build-glfw:
	mkdir -p glfw-build-unix
	cd glfw-build-unix && cmake -S ../third_party/glfw -DGLFW_BUILD_EXAMPLES=OFF -DGLFW_BUILD_TESTS=OFF -DGLFW_BUILD_DOCS=OFF # -G "Unix Makefiles" causes problems.
	cd glfw-build-unix && cmake --build . --config Release

debug-build-glfw:
	mkdir -p glfw-build-unix
	cd glfw-build-unix && cmake -S ../third_party/glfw -DGLFW_BUILD_EXAMPLES=OFF -DGLFW_BUILD_TESTS=OFF -DGLFW_BUILD_DOCS=OFF
	cd glfw-build-unix && cmake --build . --config Debug

release: release-build-glfw bundle-python
	$(CC) $(SRCS) -o $(TARGET) $(RELEASE_CXXFLAGS) $(COMMON_LINKER_FLAGS) $(COMMON_CXXFLAGS)

debug: debug-build-glfw bundle-python
	$(CC) $(SRCS) -o $(TARGET) $(DEBUG_CXXFLAGS) $(COMMON_LINKER_FLAGS) $(COMMON_CXXFLAGS)

bundle-python:
	mkdir -p "$(PYTHON_LIB_TARGET)"
	if [ -n "$(PYTHON_STDLIB_DIR)" ] && [ -d "$(PYTHON_STDLIB_DIR)" ]; then \
		cp -a "$(PYTHON_STDLIB_DIR)/." "$(PYTHON_LIB_TARGET)/"; \
	fi
	if [ -n "$(PYTHON_PURELIB_DIR)" ] && [ -d "$(PYTHON_PURELIB_DIR)" ]; then \
		dest="$(PYTHON_LIB_TARGET)/$(notdir $(PYTHON_PURELIB_DIR))"; \
		mkdir -p "$$dest"; \
		cp -a "$(PYTHON_PURELIB_DIR)/." "$$dest/"; \
	fi
	if [ -n "$(PYTHON_PLATLIB_DIR)" ] && [ -d "$(PYTHON_PLATLIB_DIR)" ] && [ "$(PYTHON_PLATLIB_DIR)" != "$(PYTHON_PURELIB_DIR)" ]; then \
		dest="$(PYTHON_LIB_TARGET)/$(notdir $(PYTHON_PLATLIB_DIR))"; \
		mkdir -p "$$dest"; \
		cp -a "$(PYTHON_PLATLIB_DIR)/." "$$dest/"; \
	fi
	if [ -n "$(PYTHON_LIBRARY_DIR)" ] && [ -n "$(PYTHON_LIBRARY_NAME)" ] && [ -f "$(PYTHON_LIBRARY_PATH)" ]; then \
		mkdir -p "$(PYTHON_BUNDLE_DIR)/lib"; \
		cp -a "$(PYTHON_LIBRARY_PATH)" "$(PYTHON_BUNDLE_DIR)/lib/"; \
	fi

clean:
	rm -f $(TARGET)
	rm -rf glfw-build-unix
	rm -rf python_bindings/build
	rm -rf $(PYTHON_BUNDLE_DIR)
