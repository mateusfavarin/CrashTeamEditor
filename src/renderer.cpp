#include "renderer.h"

#include "time.h"
#include "gui_render_settings.h"
#include "shader_templates.h"
#include "utils.h"

#include "globalimguiglglfw.h"
#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"

#include <map>
#include <tuple>

#include <imgui.h>
#include <../deps/linmath.h>
#include <misc/cpp/imgui_stdlib.h>

Renderer::Renderer(float width, float height)
{
  //create framebuffer
  m_width = static_cast<int>(width);
  m_height = static_cast<int>(height);

  glGenFramebuffers(1, &m_framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

  // Create a texture to store color attachment
  glGenTextures(1, &m_texturebuffer);
  glBindTexture(GL_TEXTURE_2D, m_texturebuffer);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  //glBindTexture(GL_TEXTURE_2D, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texturebuffer, 0);

  // Create a renderbuffer for depth and stencil
  glGenRenderbuffers(1, &m_renderbuffer);
  glBindRenderbuffer(GL_RENDERBUFFER, m_renderbuffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_width, m_height);
  //glBindRenderbuffer(GL_RENDERBUFFER, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_renderbuffer);

  // Check if framebuffer is complete
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { fprintf(stderr, "Framebuffer is not complete\n"); }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

void Renderer::Render(std::vector<Model> models)
{
  glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

  //clear screen with dark blue
  glEnable(GL_DEPTH_TEST);

  glViewport(0, 0, m_width, m_height);
  glClearColor(0.0f, 0.05f, 0.1f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  //time
	m_time = clock() / static_cast<float>(CLOCKS_PER_SEC);
	if (m_deltaTime == -1.0f) { m_deltaTime = 0.016f; } // fake ~60fps deltaTime
  else
  {
		m_deltaTime = m_time - m_lastFrameTime;
		m_lastFrameTime = m_time;
  }

  //render
  float thisFrameMoveSpeed = 15.f * m_deltaTime * GuiRenderSettings::camMoveMult;
  float thisFrameLookSpeed = 50.f * m_deltaTime * GuiRenderSettings::camRotateMult;
	if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) { thisFrameMoveSpeed *= GuiRenderSettings::camSprintMult; }

  static glm::vec3 camFront = glm::vec3(0.f, 0.f, -1.f);
  static glm::vec3 camUp = glm::vec3(0.f, 1.f, 0.f);

  {
    static float pitch = 0.f;
    static float yaw = 0.f;
    float xpos = 0.f;
    float ypos = 0.f;
		if (ImGui::IsKeyDown(ImGuiKey_UpArrow)) { ypos += 1.f * thisFrameLookSpeed; }
		if (ImGui::IsKeyDown(ImGuiKey_DownArrow)) { ypos -= 1.f * thisFrameLookSpeed; }
		if (ImGui::IsKeyDown(ImGuiKey_LeftArrow)) { xpos -= 1.f * thisFrameLookSpeed; }
		if (ImGui::IsKeyDown(ImGuiKey_RightArrow)) { xpos += 1.f * thisFrameLookSpeed; }

    yaw += xpos;

		pitch += ypos;
		pitch = Clamp(pitch, -89.0f, 89.0f);

    glm::vec3 direction;
    direction.x = cos(glm::radians(yaw - 90)) * cos(glm::radians(pitch));
    direction.y = sin(glm::radians(pitch));
    direction.z = sin(glm::radians(yaw - 90)) * cos(glm::radians(pitch));
    camFront = glm::normalize(direction);
  }

	if (ImGui::IsKeyDown(ImGuiKey_W))
	{
    m_camWorldPos += glm::vec3(thisFrameMoveSpeed * camFront.x, thisFrameMoveSpeed * camFront.y, thisFrameMoveSpeed * camFront.z);
	}
	if (ImGui::IsKeyDown(ImGuiKey_S))
	{
    m_camWorldPos -= glm::vec3(thisFrameMoveSpeed * camFront.x, thisFrameMoveSpeed * camFront.y, thisFrameMoveSpeed * camFront.z);
	}
  if (ImGui::IsKeyDown(ImGuiKey_A))
  {
    glm::vec3 interm = glm::normalize(glm::cross(camFront, camUp));
    glm::vec3 moveBy = glm::vec3(thisFrameMoveSpeed * interm.x, thisFrameMoveSpeed * interm.y, thisFrameMoveSpeed * interm.z);
    m_camWorldPos -= moveBy;
  }
  if (ImGui::IsKeyDown(ImGuiKey_D))
  {
    glm::vec3 interm = glm::normalize(glm::cross(camFront, camUp));
    glm::vec3 moveBy = glm::vec3(thisFrameMoveSpeed * interm.x, thisFrameMoveSpeed * interm.y, thisFrameMoveSpeed * interm.z);
    m_camWorldPos += moveBy;
  }
  if (ImGui::IsKeyDown(ImGuiKey_Space))
  {
    glm::vec3 interm = glm::normalize(glm::cross(camFront, camUp));
    interm = glm::normalize(glm::cross(camFront, interm));
    glm::vec3 moveBy = glm::vec3(thisFrameMoveSpeed * interm.x, thisFrameMoveSpeed * interm.y, thisFrameMoveSpeed * interm.z);
    m_camWorldPos -= moveBy;
  }
  if (ImGui::IsKeyDown(ImGuiKey_LeftShift))
  {
    glm::vec3 interm = glm::normalize(glm::cross(camFront, camUp));
    interm = glm::normalize(glm::cross(camFront, interm));
    glm::vec3 moveBy = glm::vec3(thisFrameMoveSpeed * interm.x, thisFrameMoveSpeed * interm.y, thisFrameMoveSpeed * interm.z);
    m_camWorldPos += moveBy;
  }

  //if (ImGui::IsKeyDown(ImGuiKey_Scroll)) //todo zoom in and out

  m_perspective = glm::perspective<float>(glm::radians(GuiRenderSettings::camFovDeg), (static_cast<float>(m_width) / static_cast<float>(m_height)), 0.1f, 1000.0f);
  m_cameraView = glm::lookAt(m_camWorldPos, m_camWorldPos + camFront, camUp);

  static Shader* lastUsedShader = nullptr;
  for (Model m : models)
  {
		if (m.GetMesh() == nullptr) { continue; }

    Shader* shad = nullptr;
    int datas = m.GetMesh()->GetDatas();

		if (m_shaderCache.contains(datas)) { shad = &m_shaderCache[datas]; }
    else
    { //JIT shader compilation.
			m_shaderCache[datas] = Shader(std::get<0>(ShaderTemplates::datasToShaderSourceMap[datas]).c_str(), std::get<1>(ShaderTemplates::datasToShaderSourceMap[datas]).c_str(), std::get<2>(ShaderTemplates::datasToShaderSourceMap[datas]).c_str());
      shad = &m_shaderCache[datas];
      lastUsedShader = &m_shaderCache[datas];
      shad->Use();
    }

    if (lastUsedShader != shad) //ptr comparison
    { //need to swap shaders
      lastUsedShader->Unuse();
      shad->Use();
      lastUsedShader = shad;
    }

    //m.Setup();

    if ((m.GetMesh()->GetShaderSettings() & Mesh::ShaderSettings::DontOverrideShaderSettings) == 0)
    {
      int newShadSettings = Mesh::ShaderSettings::None;
		  if (GuiRenderSettings::showWireframe) { newShadSettings |= Mesh::ShaderSettings::DrawWireframe; }
		  if (GuiRenderSettings::showBackfaces) { newShadSettings |= Mesh::ShaderSettings::DrawBackfaces; }
      m.GetMesh()->SetShaderSettings(newShadSettings);
    }

    glm::mat4 model = m.CalculateModelMatrix();
    glm::mat4 mvp = m_perspective * m_cameraView * model;
    //world
    shad->SetUniform("mvp", mvp);
    shad->SetUniform("model", model);
    shad->SetUniform("camViewDir", camFront);
    shad->SetUniform("camWorldPos", m_camWorldPos);
    //draw variations
    shad->SetUniform("drawType", GuiRenderSettings::renderType);
    shad->SetUniform("shaderSettings", m.GetMesh()->GetShaderSettings());
    //misc
    shad->SetUniform("time", m_time);
    shad->SetUniform("lightDir", glm::normalize(glm::vec3(0.2f, -3.f, -1.f)));
    shad->SetUniform("wireframeWireThickness", .02f);
    GLuint tex = m.GetMesh()->GetTextureStore();
    if (tex)
    {
      shad->SetUniform("tex", 0); //"0" represents texture unit 0
    }
    m.Draw();
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
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

GLuint Renderer::GetTexBuffer() const
{
	return m_texturebuffer;
}

void Renderer::RescaleFramebuffer(float width, float height)
{
  int tempWidth = static_cast<int>(width);
  int tempHeight = static_cast<int>(height);
  if (tempWidth == m_width && tempHeight == m_height) { return; /*no need to resize*/ }

  m_width = tempWidth;
  m_height = tempHeight;

  glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
  glBindTexture(GL_TEXTURE_2D, m_texturebuffer);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_texturebuffer, 0);

  glBindRenderbuffer(GL_RENDERBUFFER, m_renderbuffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, m_width, m_height);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, m_renderbuffer);

  glBindTexture(GL_TEXTURE_2D, 0);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
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
  glm::vec3 s = m_camWorldPos - tri[0];
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
    glm::vec3 collisionPoint = glm::vec3(m_camWorldPos + (worldSpaceRay * t)); //this is the point *on* the triangle that was collided with.
    return std::make_tuple(collisionPoint, t);
  }
  else
  {
    //line intersects, but ray does not (i.e., behind camera)
    return std::make_tuple(glm::zero<glm::vec3>(), -1.0f);
  }
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

  glm::mat4 invView = glm::inverse(m_cameraView);
  glm::vec3 worldSpaceRay = glm::normalize(invView * rayCam);

  return worldSpaceRay;
}