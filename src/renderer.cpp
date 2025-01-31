#include "renderer.h"

#include <../deps/linmath.h>
#include "globalimguiglglfw.h"
#include "glm.hpp"
#include "gtc/matrix_transform.hpp"
#include "gtc/type_ptr.hpp"
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include "time.h"
#include "gui_render_settings.h"
#include "shader_templates.h"
#include <map>
#include <tuple>
//#include "manual_third_party/stb_image.h"

Renderer::Renderer(int width, int height)
{
  //create framebuffer
  this->width = width;
  this->height = height;

  glGenFramebuffers(1, &framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

  // Create a texture to store color attachment
  glGenTextures(1, &texturebuffer);
  glBindTexture(GL_TEXTURE_2D, texturebuffer);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  //glBindTexture(GL_TEXTURE_2D, 0);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texturebuffer, 0);

  // Create a renderbuffer for depth and stencil
  glGenRenderbuffers(1, &renderbuffer);
  glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
  //glBindRenderbuffer(GL_RENDERBUFFER, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderbuffer);

  // Check if framebuffer is complete
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    fprintf(stderr, "Framebuffer is not complete\n");

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
}

void Renderer::Render(std::vector<Model> models)
{
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

  //clear screen with dark blue
  glEnable(GL_DEPTH_TEST);

  glViewport(0, 0, width, height);
  glClearColor(0.0f, 0.05f, 0.1f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  //time
  time = clock() / (float)CLOCKS_PER_SEC;
  if (deltaTime == -1.f)
    deltaTime = 0.016f; // fake ~60fps deltaTime
  else
  {
    deltaTime = time - lastFrameTime;
    lastFrameTime = time;
  }

  //render
  float thisFrameMoveSpeed = 15.f * deltaTime * GuiRenderSettings::camMoveMult;
  float thisFrameLookSpeed = 50.f * deltaTime * GuiRenderSettings::camRotateMult;
  if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl))
    thisFrameMoveSpeed *= GuiRenderSettings::camSprintMult;
  static glm::vec3 camPos = glm::vec3(0.f, 0.f, 3.f);
  static glm::vec3 camFront = glm::vec3(0.f, 0.f, -1.f);
  static glm::vec3 camUp = glm::vec3(0.f, 1.f, 0.f);

  {
    static float pitch = 0.f;
    static float yaw = 0.f;
    float xpos = 0.f;
    float ypos = 0.f;
    if (ImGui::IsKeyDown(ImGuiKey_UpArrow))
      ypos += 1.f * thisFrameLookSpeed;
    if (ImGui::IsKeyDown(ImGuiKey_DownArrow))
      ypos -= 1.f * thisFrameLookSpeed;
    if (ImGui::IsKeyDown(ImGuiKey_LeftArrow))
      xpos -= 1.f * thisFrameLookSpeed;
    if (ImGui::IsKeyDown(ImGuiKey_RightArrow))
      xpos += 1.f * thisFrameLookSpeed;

    yaw += xpos;
    pitch += ypos;

    if (pitch > 89.0f)
      pitch = 89.0f;
    if (pitch < -89.0f)
      pitch = -89.0f;

    glm::vec3 direction;
    direction.x = cos(glm::radians(yaw - 90)) * cos(glm::radians(pitch));
    direction.y = sin(glm::radians(pitch));
    direction.z = sin(glm::radians(yaw - 90)) * cos(glm::radians(pitch));
    camFront = glm::normalize(direction);
  }

  if (ImGui::IsKeyDown(ImGuiKey_W))
    camPos += glm::vec3(thisFrameMoveSpeed * camFront.x, thisFrameMoveSpeed * camFront.y, thisFrameMoveSpeed * camFront.z);
  if (ImGui::IsKeyDown(ImGuiKey_S))
    camPos -= glm::vec3(thisFrameMoveSpeed * camFront.x, thisFrameMoveSpeed * camFront.y, thisFrameMoveSpeed * camFront.z);
  if (ImGui::IsKeyDown(ImGuiKey_A))
  {
    glm::vec3 interm = glm::normalize(glm::cross(camFront, camUp));
    glm::vec3 moveBy = glm::vec3(thisFrameMoveSpeed * interm.x, thisFrameMoveSpeed * interm.y, thisFrameMoveSpeed * interm.z);
    camPos -= moveBy;
  }
  if (ImGui::IsKeyDown(ImGuiKey_D))
  {
    glm::vec3 interm = glm::normalize(glm::cross(camFront, camUp));
    glm::vec3 moveBy = glm::vec3(thisFrameMoveSpeed * interm.x, thisFrameMoveSpeed * interm.y, thisFrameMoveSpeed * interm.z);
    camPos += moveBy;
  }
  if (ImGui::IsKeyDown(ImGuiKey_Space))
  {
    glm::vec3 interm = glm::normalize(glm::cross(camFront, camUp));
    interm = glm::normalize(glm::cross(camFront, interm));
    glm::vec3 moveBy = glm::vec3(thisFrameMoveSpeed * interm.x, thisFrameMoveSpeed * interm.y, thisFrameMoveSpeed * interm.z);
    camPos -= moveBy;
  }
  if (ImGui::IsKeyDown(ImGuiKey_LeftShift))
  {
    glm::vec3 interm = glm::normalize(glm::cross(camFront, camUp));
    interm = glm::normalize(glm::cross(camFront, interm));
    glm::vec3 moveBy = glm::vec3(thisFrameMoveSpeed * interm.x, thisFrameMoveSpeed * interm.y, thisFrameMoveSpeed * interm.z);
    camPos += moveBy;
  }

  //if (ImGui::IsKeyDown(ImGuiKey_Scroll)) //todo zoom in and out

  glm::mat4 perspective = glm::perspective<float>(glm::radians(GuiRenderSettings::camFovDeg), ((float)this->width / (float)this->height), 0.1f, 1000.0f);
  glm::mat4 view = glm::lookAt(camPos, camPos + camFront, camUp);

  static Shader* lastUsedShader = nullptr;
  for (Model m : models)
  {
    if (m.GetMesh() == nullptr)
      continue;

    Shader* shad = nullptr;
    int datas = m.GetMesh()->GetDatas();

    if (shaderCache.contains(datas))
      shad = &shaderCache[datas];
    else
    { //JIT shader compilation.
      shaderCache[datas] = Shader(std::get<0>(ShaderTemplates::datasToShaderSourceMap[datas]).c_str(), std::get<1>(ShaderTemplates::datasToShaderSourceMap[datas]).c_str(), std::get<2>(ShaderTemplates::datasToShaderSourceMap[datas]).c_str());
      shad = &shaderCache[datas];
      lastUsedShader = &shaderCache[datas];
      shad->use();
    }

    if (lastUsedShader != shad) //ptr comparison
    { //need to swap shaders
      lastUsedShader->unuse();
      shad->use();
      lastUsedShader = shad;
    }

    int newShadSettings = Mesh::ShaderSettings::None;
    if (GuiRenderSettings::showWireframe)
      newShadSettings |= (int)Mesh::ShaderSettings::DrawWireframe;
    if (GuiRenderSettings::showBackfaces)
      newShadSettings |= (int)Mesh::ShaderSettings::DrawBackfaces;
    m.GetMesh()->SetShaderSettings(newShadSettings);

    glm::mat4 model = m.CalculateModelMatrix();
    glm::mat4 mvp = perspective * view * model;
    //world
    shad->setUniform("mvp", mvp);
    shad->setUniform("model", model);
    shad->setUniform("camViewDir", camFront);
    shad->setUniform("camWorldPos", camPos);
    //draw variations
    shad->setUniform("drawType", GuiRenderSettings::renderType);
    shad->setUniform("shaderSettings", (int)m.GetMesh()->GetShaderSettings());
    //misc
    shad->setUniform("time", time);
    shad->setUniform("lightDir", glm::normalize(glm::vec3(0.2f, -3.f, -1.f)));
    shad->setUniform("wireframeWireThickness", .02f);
    m.Draw();
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

float Renderer::GetLastDeltaTime()
{
  return this->deltaTime;
}

float Renderer::GetLastTime()
{
  return this->time;
}

void Renderer::RescaleFramebuffer(float width, float height)
{
  this->width = (int)width;
  this->height = (int)height;
  glBindTexture(GL_TEXTURE_2D, texturebuffer);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texturebuffer, 0);

  glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderbuffer);
}