#include "renderer.h"

#include "time.h"
#include "gui_render_settings.h"
#include "shader_templates.h"

#include "globalimguiglglfw.h"
#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"

Renderer::Renderer(int width, int height)
{
	//create framebuffer
	m_width = width;
	m_height = height;

	GL_CHECK(glGenFramebuffers(1, &m_framebuffer));
	GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer));

	// Create a texture to store color attachment
	GL_CHECK(glGenTextures(1, &m_texturebuffer));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, m_texturebuffer));
	GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
	//glBindTexture(GL_TEXTURE_2D, 0);
	GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texturebuffer, 0));

	// Create a renderbuffer for depth and stencil
	GL_CHECK(glGenRenderbuffers(1, &m_renderbuffer));
	GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, m_renderbuffer));
	GL_CHECK(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_width, m_height));
	//glBindRenderbuffer(GL_RENDERBUFFER, 0);
	GL_CHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_renderbuffer));

	// Check if framebuffer is complete
	GL_CHECK_RET(glCheckFramebufferStatus(GL_FRAMEBUFFER));

	GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
	GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, 0));

	m_shaderCache.reserve(ShaderTemplates::datasToShaderSourceMap.size());
	for (const auto& [datas, sources] : ShaderTemplates::datasToShaderSourceMap)
	{
		m_shaderCache.emplace(datas, Shader(sources.first.c_str(), sources.second.c_str()));
	}

	m_skyGradientShader = Shader(ShaderTemplates::vert_skygradient.c_str(), ShaderTemplates::frag_skygradient.c_str());
	GL_CHECK(glGenVertexArrays(1, &m_skyGradientVAO));
	GL_CHECK(glGenBuffers(1, &m_skyGradientVBO));
}

Renderer::~Renderer()
{
	for (Model* model : m_modelList) { delete model; }
}

Model* Renderer::CreateModel()
{
	m_modelList.push_back(new Model());
	return m_modelList.back();
}

bool Renderer::DeleteModel(Model* model)
{
	if (!model) { return false; }

	size_t count = std::erase(m_modelList, model);
	if (count == 0) { return false; }

	delete model;
	return true;
}

void Renderer::Render(bool skyGradientEnabled, const std::array<ColorGradient, NUM_GRADIENT>& skyGradients)
{
	if (m_width <= 0 || m_height <= 0) { return; }

	GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

	//clear screen with sky gradient or dark blue
	GL_CHECK(glEnable(GL_DEPTH_TEST));
	GL_CHECK(glEnable(GL_BLEND));
	GL_CHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

	GL_CHECK(glViewport(0, 0, m_width, m_height));

	if (skyGradientEnabled) { RenderSkyGradient(skyGradients); }
	else
	{
		GL_CHECK(glClearColor(0.0f, 0.05f, 0.1f, 1.0f)); // Default dark blue background
		GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));
	}

	//time
	m_time = clock() / static_cast<float>(CLOCKS_PER_SEC);
	if (m_deltaTime == -1.0f) { m_deltaTime = 0.016f; } // fake ~60fps deltaTime
	else
	{
		m_deltaTime = m_time - m_lastFrameTime;
		m_lastFrameTime = m_time;
	}

	//render
	const bool allowShortcuts = !ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow);
	m_camera.Update(allowShortcuts, m_deltaTime);

	const glm::vec3& camPos = m_camera.GetPosition();

	m_perspective = glm::perspective<float>(glm::radians(GuiRenderSettings::camFovDeg), (static_cast<float>(m_width) / static_cast<float>(m_height)), 0.1f, 10000.0f);
	static int lastUsedShader = -1;

	const glm::mat4 rootParentMatrix(1.0f);
	for (Model* m : m_modelList)
	{
		RenderModelRecursive(m, rootParentMatrix, camPos, lastUsedShader);
	}

	GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
}

bool Renderer::RenderModelRecursive(Model* model, const glm::mat4& parentMatrix, const glm::vec3& camPos, int& lastUsedShader)
{
	if (!model) { return false; }

	model->SetParentMatrix(parentMatrix);
	if (!model->IsReady()) { return false; }

	int datas = model->GetMesh().GetDatas();
	if (!m_shaderCache.contains(datas)) { return false; }

	const Shader& shad = m_shaderCache.at(datas);
	if (lastUsedShader != datas)
	{
		if (lastUsedShader != -1) { m_shaderCache[lastUsedShader].Unuse(); }
		shad.Use();
		lastUsedShader = datas;
	}

	const unsigned currentRenderFlags = model->GetMesh().GetRenderFlags();
	const bool followCamera = (currentRenderFlags & Mesh::RenderFlags::FollowCamera) != 0;
	if ((currentRenderFlags & Mesh::RenderFlags::QuadblockLod) != 0)
	{
		model->GetMesh().SetUseLowLOD(GuiRenderSettings::showLowLOD);
	}

	const bool modifyRenderFlags = (currentRenderFlags & Mesh::RenderFlags::DontOverrideRenderFlags) == 0;
	if (modifyRenderFlags)
	{
		unsigned newRenderFlags = currentRenderFlags;
		if (GuiRenderSettings::showWireframe) { newRenderFlags |= Mesh::RenderFlags::DrawWireframe; }
		if (GuiRenderSettings::showBackfaces) { newRenderFlags |= Mesh::RenderFlags::DrawBackfaces; }
		model->GetMesh().SetRenderFlags(newRenderFlags);
	}

	glm::mat4 modelMatrix = model->GetModelMatrix();
	if (followCamera)
	{
		const Vec3 pos = model->GetWorldPosition();
		const Vec3 scale = model->GetWorldScale();
		const Vec3 rotation = model->GetWorldRotation();
		modelMatrix = m_camera.BuildBillboardMatrix(glm::vec3(pos.x, pos.y, pos.z), glm::vec3(scale.x, scale.y, scale.z));
	}

	glm::mat4 mvp = m_perspective * m_camera.GetViewMatrix() * modelMatrix;
	//world
	shad.SetUniform("mvp", mvp);
	shad.SetUniform("model", modelMatrix);
	shad.SetUniform("camWorldPos", camPos);
	//draw variations
	shad.SetUniform("drawType", GuiRenderSettings::renderType);
	shad.SetUniform("shaderSettings", model->GetMesh().GetShaderFlags());
	//misc
	shad.SetUniform("time", m_time);
	shad.SetUniform("lightDir", glm::normalize(glm::vec3(0.2f, -3.f, -1.f)));
	GLuint tex = model->GetMesh().GetTextureStore();
	if (tex) { shad.SetUniform("tex", 0); } // "0" represents texture unit 0
	model->GetMesh().Render();
	if (modifyRenderFlags) { model->GetMesh().SetRenderFlags(currentRenderFlags); }

	for (Model* child : model->m_child)
	{
		if (!child) { continue; }
		RenderModelRecursive(child, modelMatrix, camPos, lastUsedShader);
	}

	return true;
}

void Renderer::SetCameraToLevelSpawn(const Vec3& pos, const Vec3& rot)
{
	glm::vec3 position(pos.x, pos.y, pos.z);
	glm::vec3 rotation(rot.x, rot.y, rot.z);

	position.y += 1.0f;
	m_camera.SetPosition(position);
	m_camera.SetPitch(rotation.x);
	m_camera.SetYaw(rotation.y + 180.0f);
	m_camera.SetTarget(position + glm::vec3(0.0f, 0.0f, 0.0f));
	m_camera.SetDistance(10.0f);
}

float Renderer::GetLastDeltaTime() const
{
	return m_deltaTime;
}

float Renderer::GetLastTime() const
{
	return m_time;
}

float Renderer::GetWidth() const
{
	return static_cast<float>(m_width);
}

float Renderer::GetHeight() const
{
	return static_cast<float>(m_height);
}

void Renderer::RescaleFramebuffer(float width, float height)
{
	int tempWidth = static_cast<int>(width);
	int tempHeight = static_cast<int>(height);
	if (tempWidth == m_width && tempHeight == m_height) { return; /*no need to resize*/ }

	m_width = tempWidth;
	m_height = tempHeight;

	GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer));
	GL_CHECK(glBindTexture(GL_TEXTURE_2D, m_texturebuffer));
	GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
	GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
	GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texturebuffer, 0));

	GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, m_renderbuffer));
	GL_CHECK(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_width, m_height));
	GL_CHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_renderbuffer));

	GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
	GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, 0));
	GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));
}

void Renderer::SetViewportSize(float width, float height)
{
	int tempWidth = static_cast<int>(width);
	int tempHeight = static_cast<int>(height);
	if (tempWidth <= 0 || tempHeight <= 0) { return; }
	if (tempWidth == m_width && tempHeight == m_height) { return; }

	m_width = tempWidth;
	m_height = tempHeight;
}

std::tuple<glm::vec3, float> Renderer::WorldspaceRayTriIntersection(glm::vec3 worldSpaceRay, const glm::vec3 tri[3]) const
{
	constexpr float epsilon = glm::epsilon<float>();

	//moller-trumbore intersection test
	//https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm

	glm::vec3 edge_1 = tri[1] - tri[0], edge_2 = tri[2] - tri[0];
	glm::vec3 ray_cross_e2 = glm::cross(worldSpaceRay, edge_2);
	float det = glm::dot(edge_1, ray_cross_e2);

	if (glm::abs(det) < epsilon)
		return std::make_tuple(glm::zero<glm::vec3>(), -1.0f); //ray is parallel to plane, couldn't possibly intersect with triangle.

	float inv_det = 1.0f / det;
	glm::vec3 s = m_camera.GetPosition() - tri[0];
	float u = inv_det * glm::dot(s, ray_cross_e2);

	if ((u < 0 && glm::abs(u) > epsilon) || (u > 1 && glm::abs(u - 1.0f) > epsilon))
		return std::make_tuple(glm::zero<glm::vec3>(), -1.0f); //fails a barycentric test

	glm::vec3 s_cross_e1 = glm::cross(s, edge_1);
	float v = inv_det * glm::dot(worldSpaceRay, s_cross_e1);

	if ((v < 0 && glm::abs(v) > epsilon) || (u + v > 1 && glm::abs(u + v - 1) > epsilon))
		return std::make_tuple(glm::zero<glm::vec3>(), -1.0f); //fails a barycentric test

	float t = inv_det * glm::dot(edge_2, s_cross_e1); //time value (interpolant)

	if (t > epsilon)
	{
		glm::vec3 collisionPoint = glm::vec3(m_camera.GetPosition() + (worldSpaceRay * t)); //this is the point *on* the triangle that was collided with.
		return std::make_tuple(collisionPoint, t);
	}

	// line intersects, but ray does not (i.e., behind camera)
	return std::make_tuple(glm::zero<glm::vec3>(), -1.0f);
}

glm::vec3 Renderer::ScreenspaceToWorldRay(int pixelX, int pixelY) const
{
	float normalizeDeviceX = ((2.0f * static_cast<float>(pixelX)) / static_cast<float>(m_width)) - 1.0f;
	float normalizeDeviceY = 1.0f - ((2.0f * static_cast<float>(pixelY)) / static_cast<float>(m_height));

	glm::vec4 cameraSpaceRay = glm::vec4(normalizeDeviceX, normalizeDeviceY, -1.0f, 0.0f);

	glm::mat4 invProj = glm::inverse(m_perspective);
	glm::vec4 rayCam = invProj * cameraSpaceRay;
	rayCam.z = -1.0f;
	rayCam.w = 0.0f;

	glm::mat4 invView = glm::inverse(m_camera.GetViewMatrix());
	glm::vec3 worldSpaceRay = glm::normalize(invView * rayCam);

	return worldSpaceRay;
}

void Renderer::RenderSkyGradient(const std::array<ColorGradient, NUM_GRADIENT>& skyGradients)
{
	constexpr int SKY_GRADIENT_TRIANGLES_PER_BAND = 2;
	constexpr int SKY_GRADIENT_VERTICES_PER_TRIANGLE = 3;
	constexpr int SKY_GRADIENT_FLOATS_PER_VERTEX = 5; // 2 for position, 3 for color
	constexpr int SKY_GRADIENT_FLOATS_PER_TRIANGLE = SKY_GRADIENT_VERTICES_PER_TRIANGLE * SKY_GRADIENT_FLOATS_PER_VERTEX;
	constexpr int SKY_GRADIENT_TRIANGLES_TOTAL = NUM_GRADIENT * SKY_GRADIENT_TRIANGLES_PER_BAND;
	constexpr int SKY_GRADIENT_FLOATS_TOTAL = SKY_GRADIENT_TRIANGLES_TOTAL * SKY_GRADIENT_FLOATS_PER_TRIANGLE;
	constexpr int SKY_GRADIENT_POS_OFFSET = 0;
	constexpr int SKY_GRADIENT_COLOR_OFFSET = 2;
	constexpr int SKY_GRADIENT_VERTEX_STRIDE = SKY_GRADIENT_FLOATS_PER_VERTEX * sizeof(float);

	constexpr float SKY_GRADIENT_COORD_MAX = 100.0f;
	constexpr float SKY_GRADIENT_COORD_MIN = -100.0f;

	// Clear with black background first
	// TODO: should use level "clear color" but it doesnt work on the editor atm
	GL_CHECK(glClearColor(0.0f, 0.0f, 0.0f, 1.0f));
	GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

	GLboolean depthTestEnabled = GL_CHECK_RET(glIsEnabled(GL_DEPTH_TEST));
	GLboolean blendEnabled = GL_CHECK_RET(glIsEnabled(GL_BLEND));
	GLboolean cullFaceEnabled = GL_CHECK_RET(glIsEnabled(GL_CULL_FACE));
	GLint polygonMode[2];
	GL_CHECK(glGetIntegerv(GL_POLYGON_MODE, polygonMode));

	GL_CHECK(glDisable(GL_DEPTH_TEST));
	GL_CHECK(glDisable(GL_BLEND));
	GL_CHECK(glDisable(GL_CULL_FACE));
	GL_CHECK(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));

	if (m_skyGradientVAO == 0 || m_skyGradientVBO == 0) { return; }

	auto coordToNDC = [SKY_GRADIENT_COORD_MAX, SKY_GRADIENT_COORD_MIN](float coord) -> float {
		float clamped = std::max(std::min(coord, SKY_GRADIENT_COORD_MAX), SKY_GRADIENT_COORD_MIN);
		return clamped / SKY_GRADIENT_COORD_MAX;
		};

	std::vector<float> quadData;
	quadData.reserve(SKY_GRADIENT_FLOATS_TOTAL);

	for (size_t i = 0; i < NUM_GRADIENT; i++) {
		const ColorGradient& grad = skyGradients[i];

		float yTop = coordToNDC(grad.posTo);
		float yBottom = coordToNDC(grad.posFrom);

		const Color& topColor = grad.colorTo;
		const Color& bottomColor = grad.colorFrom;

		// Triangle 1: bottom-left, bottom-right, top-right
		quadData.push_back(-1.0f); quadData.push_back(yBottom);
		quadData.push_back(bottomColor.Red()); quadData.push_back(bottomColor.Green()); quadData.push_back(bottomColor.Blue());

		quadData.push_back(1.0f); quadData.push_back(yBottom);
		quadData.push_back(bottomColor.Red()); quadData.push_back(bottomColor.Green()); quadData.push_back(bottomColor.Blue());

		quadData.push_back(1.0f); quadData.push_back(yTop);
		quadData.push_back(topColor.Red()); quadData.push_back(topColor.Green()); quadData.push_back(topColor.Blue());

		// Triangle 2: bottom-left, top-right, top-left
		quadData.push_back(-1.0f); quadData.push_back(yBottom);
		quadData.push_back(bottomColor.Red()); quadData.push_back(bottomColor.Green()); quadData.push_back(bottomColor.Blue());

		quadData.push_back(1.0f); quadData.push_back(yTop);
		quadData.push_back(topColor.Red()); quadData.push_back(topColor.Green()); quadData.push_back(topColor.Blue());

		quadData.push_back(-1.0f); quadData.push_back(yTop);
		quadData.push_back(topColor.Red()); quadData.push_back(topColor.Green()); quadData.push_back(topColor.Blue());
	}

	GL_CHECK(glBindVertexArray(m_skyGradientVAO));
	GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, m_skyGradientVBO));
	GL_CHECK(glBufferData(GL_ARRAY_BUFFER, quadData.size() * sizeof(float), quadData.data(), GL_DYNAMIC_DRAW));

	GL_CHECK(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, SKY_GRADIENT_VERTEX_STRIDE, (void*) SKY_GRADIENT_POS_OFFSET));
	GL_CHECK(glEnableVertexAttribArray(0));
	GL_CHECK(glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, SKY_GRADIENT_VERTEX_STRIDE, (void*) (SKY_GRADIENT_COLOR_OFFSET * sizeof(float))));
	GL_CHECK(glEnableVertexAttribArray(1));

	m_skyGradientShader.Use();
	GL_CHECK(glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(quadData.size() / SKY_GRADIENT_FLOATS_PER_VERTEX)));

	GL_CHECK(glBindVertexArray(0));
	GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, 0));
	m_skyGradientShader.Unuse();

	// Restore state
	if (depthTestEnabled) { GL_CHECK(glEnable(GL_DEPTH_TEST)); }
	if (blendEnabled) { GL_CHECK(glEnable(GL_BLEND)); }
	if (cullFaceEnabled) { GL_CHECK(glEnable(GL_CULL_FACE)); }
	GL_CHECK(glPolygonMode(GL_FRONT_AND_BACK, polygonMode[0]));  // Restore polygon mode
}
