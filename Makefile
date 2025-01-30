TARGET := CrashTeamEditor
CC := g++

SRCS := $(wildcard src/*.cpp) \
	third_party/imgui/imgui.cpp \
	third_party/imgui/imgui_demo.cpp \
	third_party/imgui/imgui_draw.cpp \
	third_party/imgui/imgui_tables.cpp \
	third_party/imgui/imgui_widgets.cpp \
	third_party/imgui/backends/imgui_impl_glfw.cpp \
	third_party/imgui/backends/imgui_impl_opengl3.cpp \
	third_party/imgui/misc/cpp/imgui_stdlib.cpp \
	src/manual_third_party/glad.c


RELEASE_CXXFLAGS := -O2 -std=c++20
DEBUG_CXXFLAGS := -g -O0 -std=c++20

COMMON_CXXFLAGS := -Isrc \
	-Ithird_party/imgui \
	-Ithird_party/imgui/backends \
	-Ithird_party/glfw/include \
	-Ithird_party/portable-file-dialogs \
	-Ithird_party/json/include \
	-Ithird_party/glm/glm \
	-Isrc/manual_third_party

#glfw-build should probably be renamed for win/unix variants, so you can develop both on win+wsl
COMMON_LINKER_FLAGS := -Lglfw-build/src -lGL -lglfw3

release-build-glfw:
	mkdir -p glfw-build
	cd glfw-build && cmake -S ../third_party/glfw # -G "Unix Makefiles" causes problems.
	cd glfw-build && cmake --build . --config Release

debug-build-glfw:
	mkdir -p glfw-build
	cd glfw-build && cmake -S ../third_party/glfw
	cd glfw-build && cmake --build . --config Debug # this "Debug" is the only difference

release: release-build-glfw
	$(CC) $(SRCS) -o $(TARGET) $(RELEASE_CXXFLAGS) $(COMMON_LINKER_FLAGS) $(COMMON_CXXFLAGS)

debug: debug-build-glfw
	$(CC) $(SRCS) -o $(TARGET) $(DEBUG_CXXFLAGS) $(COMMON_LINKER_FLAGS) $(COMMON_CXXFLAGS)

clean:
	rm -f $(TARGET)
	rm -rf glfw-build
