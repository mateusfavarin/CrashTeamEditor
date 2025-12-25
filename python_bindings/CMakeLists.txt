cmake_minimum_required(VERSION 3.18)

project(CrashTeamEditorPythonBindings LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(CTE_SOURCE_DIR ${REPO_ROOT}/src)
set(IMGUI_DIR ${REPO_ROOT}/third_party/imgui)
set(GLFW_SOURCE_DIR ${REPO_ROOT}/glfw-build-win/src)

add_subdirectory(${REPO_ROOT}/third_party/pybind11 ${CMAKE_BINARY_DIR}/pybind11 EXCLUDE_FROM_ALL)

file(GLOB CTE_SOURCES ${CTE_SOURCE_DIR}/*.cpp)
list(REMOVE_ITEM CTE_SOURCES ${CTE_SOURCE_DIR}/main.cpp)

file(GLOB C_MANUAL_SOURCES ${CTE_SOURCE_DIR}/manual_third_party/*.c)

set(IMGUI_SOURCES
	${IMGUI_DIR}/imgui.cpp
	${IMGUI_DIR}/imgui_demo.cpp
	${IMGUI_DIR}/imgui_draw.cpp
	${IMGUI_DIR}/imgui_tables.cpp
	${IMGUI_DIR}/imgui_widgets.cpp
	${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
	${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
)

add_library(cte_core STATIC
	${CTE_SOURCES}
	${C_MANUAL_SOURCES}
	${IMGUI_SOURCES}
)

set_target_properties(cte_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_features(cte_core PUBLIC cxx_std_20)

target_include_directories(cte_core PUBLIC
	${CTE_SOURCE_DIR}
	${IMGUI_DIR}
	${IMGUI_DIR}/backends
	${REPO_ROOT}/third_party/glfw/include
	${REPO_ROOT}/third_party/json/include
	${REPO_ROOT}/third_party/portable-file-dialogs
	${REPO_ROOT}/third_party/glm
	${REPO_ROOT}/third_party/glm/glm
	${REPO_ROOT}/third_party/stb
)

target_compile_definitions(cte_core PRIVATE UNICODE _UNICODE NOMINMAX _USE_MATH_DEFINES)

target_link_libraries(cte_core PRIVATE
	opengl32
	${GLFW_SOURCE_DIR}/$<CONFIG>/glfw3.lib
)

pybind11_add_module(crashteameditor cte_bindings.cpp)
target_link_libraries(crashteameditor PRIVATE cte_core)
target_compile_features(crashteameditor PUBLIC cxx_std_20)
target_compile_definitions(crashteameditor PRIVATE CTE_EXTENSION_BUILD=1)
