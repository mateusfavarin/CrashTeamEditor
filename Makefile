TARGET := CrashTeamEditor
CC := g++

SRCS := $(wildcard src/*.cpp) \
	third_party/imgui/imgui.cpp \
	third_party/imgui/imgui_demo.cpp \
	third_party/imgui/imgui_draw.cpp \
	third_party/imgui/imgui_tables.cpp \
	third_party/imgui/imgui_widgets.cpp \
	third_party/imgui/backends/imgui_impl_opengl3.cpp \
	third_party/imgui/backends/imgui_impl_sdl2.cpp \
	third_party/imgui/misc/cpp/imgui_stdlib.cpp

RELEASE_CXXFLAGS := -O2 -std=c++20
DEBUG_CXXFLAGS := -g -O0 -std=c++20

COMMON_CXXFLAGS := -Isrc \
	-I/usr/include/SDL2 \
	-Ithird_party/imgui \
	-Ithird_party/portable-file-dialogs \
	-Ithird_party/json/include

COMMON_LINKER_FLAGS := -lGL -lSDL2main -lSDL2

release:
	$(CC) $(SRCS) -o $(TARGET) $(RELEASE_CXXFLAGS) $(COMMON_LINKER_FLAGS) $(COMMON_CXXFLAGS)

debug:
	$(CC) $(SRCS) -o $(TARGET) $(DEBUG_CXXFLAGS) $(COMMON_LINKER_FLAGS) $(COMMON_CXXFLAGS)

clean:
	rm -f $(TARGET)
