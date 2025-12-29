#include "app.h"
#include "ui.h"
#include "gui_render_settings.h"

#include "globalimguiglglfw.h"
#include "renderer.h"

#include <nlohmann/json.hpp>
#include <fstream>

bool App::Init()
{
	bool success = true;
	InitUISettings();
	success &= InitGLFW();
	success &= InitImGui();
	return  success;
}

void App::Run()
{
	UI ui;
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
	ImVec4 clearColor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	while (!glfwWindowShouldClose(m_window))
	{
		glfwPollEvents();
		if (glfwGetWindowAttrib(m_window, GLFW_ICONIFIED) != 0)
		{
			ImGui_ImplGlfw_Sleep(10);
			continue;
		}

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		glClearColor(clearColor.x * clearColor.w, clearColor.y * clearColor.w, clearColor.z * clearColor.w, clearColor.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui::NewFrame();
		glViewport(0, 0, static_cast<int>(io.DisplaySize.x), static_cast<int>(io.DisplaySize.y));
		glfwGetWindowSize(m_window, &Windows::w_width, &Windows::w_height);
		ui.Render(Windows::w_width, Windows::w_height);
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(m_window);
	}
}

void App::Close()
{
	SaveUISettings();
	CloseImGui();
}

static void glfw_error_callback(int error, const char* description)
{
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

bool App::InitGLFW()
{
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit()) { return false; }

	// Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
		// GL ES 2.0 + GLSL 100 (WebGL 1.0)
	m_glslVer = "#version 100";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(IMGUI_IMPL_OPENGL_ES3)
		// GL ES 3.0 + GLSL 300 es (WebGL 2.0)
	m_glslVer = "#version 300 es";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
		// GL 3.2 + GLSL 150
	m_glslVer = "#version 150";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
		// GL 3.0 + GLSL 130
	m_glslVer = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
	//glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

	// Create window with graphics context
	const std::string title = "Crash Team Editor " + m_version;
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE); //resizeable window
	glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE); //high dpi
	m_window = glfwCreateWindow(Windows::w_width, Windows::w_height, title.c_str(), nullptr, nullptr);
	if (m_window == nullptr) { return false; }
	glfwMakeContextCurrent(m_window);
	glfwSwapInterval(1); // Enable vsync

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		fprintf(stderr, "Failed to initialize OpenGL loader!");
		return false;
	}
	return true;
}

bool App::InitImGui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io; //supress "variable not used" warnings, idk, the template code does this.
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

	ImGui::StyleColorsDark();

	bool success = true;
	success &= ImGui_ImplGlfw_InitForOpenGL(m_window, true);
#ifdef __EMSCRIPTEN__
	success &= ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
	success &= ImGui_ImplOpenGL3_Init(m_glslVer.c_str());
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	return success;
}

void App::InitUISettings()
{
	if (!std::filesystem::exists(m_configFile))
	{
		SaveUISettings();
		return;
	}
	nlohmann::json json = nlohmann::json::parse(std::ifstream(m_configFile));
	if (json.contains("Width")) { Windows::w_width = json["Width"]; }
	if (json.contains("Height")) { Windows::w_height = json["Height"]; }
	if (json.contains("AnimTex")) { Windows::w_animtex = json["AnimTex"]; }
	if (json.contains("BSP")) { Windows::w_bsp = json["BSP"]; }
	if (json.contains("Checkpoints")) { Windows::w_checkpoints = json["Checkpoints"]; }
	if (json.contains("Ghost")) { Windows::w_ghost = json["Ghost"]; }
	if (json.contains("Level")) { Windows::w_level = json["Level"]; }
	if (json.contains("Material")) { Windows::w_material = json["Material"]; }
	if (json.contains("Quadblocks")) { Windows::w_quadblocks = json["Quadblocks"]; }
	if (json.contains("Renderer")) { Windows::w_renderer = json["Renderer"]; }
	if (json.contains("Spawn")) { Windows::w_spawn = json["Spawn"]; }
	if (json.contains("LastOpenedFolder")) { Windows::lastOpenedFolder = json["LastOpenedFolder"]; }
	if (json.contains("Script")) { Windows::w_python = json["Script"]; }
	if (json.contains("CameraBindings"))
	{
		const nlohmann::json& bindings = json["CameraBindings"];
		if (bindings.contains("Forward")) { GuiRenderSettings::camKeyForward = bindings["Forward"]; }
		if (bindings.contains("Back")) { GuiRenderSettings::camKeyBack = bindings["Back"]; }
		if (bindings.contains("Left")) { GuiRenderSettings::camKeyLeft = bindings["Left"]; }
		if (bindings.contains("Right")) { GuiRenderSettings::camKeyRight = bindings["Right"]; }
		if (bindings.contains("Up")) { GuiRenderSettings::camKeyUp = bindings["Up"]; }
		if (bindings.contains("Down")) { GuiRenderSettings::camKeyDown = bindings["Down"]; }
		if (bindings.contains("Sprint")) { GuiRenderSettings::camKeySprint = bindings["Sprint"]; }
		if (bindings.contains("OrbitMouseButton")) { GuiRenderSettings::camOrbitMouseButton = bindings["OrbitMouseButton"]; }
	}
	if (json.contains("RendererSettings"))
	{
		const nlohmann::json& renderSettings = json["RendererSettings"];
		if (renderSettings.contains("FilterColor"))
		{
			const nlohmann::json& color = renderSettings["FilterColor"];
			if (color.is_array() && color.size() >= 3)
			{
				GuiRenderSettings::filterColor = Color(static_cast<unsigned char>(color[0]), static_cast<unsigned char>(color[1]), static_cast<unsigned char>(color[2]));
			}
		}
	}
}

void App::SaveUISettings()
{
	nlohmann::json json;
	json["Width"] = Windows::w_width;
	json["Height"] = Windows::w_height;
	json["AnimTex"] = Windows::w_animtex;
	json["BSP"] = Windows::w_bsp;
	json["Checkpoints"] = Windows::w_checkpoints;
	json["Ghost"] = Windows::w_ghost;
	json["Level"] = Windows::w_level;
	json["Material"] = Windows::w_material;
	json["Quadblocks"] = Windows::w_quadblocks;
	json["Renderer"] = Windows::w_renderer;
	json["Spawn"] = Windows::w_spawn;
	json["LastOpenedFolder"] = Windows::lastOpenedFolder;
	json["Script"] = Windows::w_python;
	json["CameraBindings"] = {
		{"Forward", GuiRenderSettings::camKeyForward},
		{"Back", GuiRenderSettings::camKeyBack},
		{"Left", GuiRenderSettings::camKeyLeft},
		{"Right", GuiRenderSettings::camKeyRight},
		{"Up", GuiRenderSettings::camKeyUp},
		{"Down", GuiRenderSettings::camKeyDown},
		{"Sprint", GuiRenderSettings::camKeySprint},
		{"OrbitMouseButton", GuiRenderSettings::camOrbitMouseButton},
	};
	json["RendererSettings"] = {
		{"FilterColor", {GuiRenderSettings::filterColor.r, GuiRenderSettings::filterColor.g, GuiRenderSettings::filterColor.b}}
	};
	std::ofstream file = std::ofstream(m_configFile);
	file << std::setw(4) << json << std::endl;
	file.close();
}

void App::CloseImGui()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}
