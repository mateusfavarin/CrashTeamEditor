#include "renderer.h"

#include <../deps/linmath.h>
#include "globalimguiglglfw.h"
#include "manual_third_party/glm/glm.hpp"
#include "manual_third_party/glm/gtc/matrix_transform.hpp"
#include "manual_third_party/glm/gtc/type_ptr.hpp"
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include "time.h"
//#include "manual_third_party/stb_image.h"

//pos (3) normal (3) 2 tris per face 6 faces cube
float vertices[] = {
  -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
   0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
   0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
   0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
  -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
  -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
  
  -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
   0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
   0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
   0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
  -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
  -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
  
  -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
  -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
  -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
  -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
  -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
  -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
  
   0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
   0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
   0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
   0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
   0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
   0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
  
  -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
   0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
   0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
   0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
  -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
  -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
  
  -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
   0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
   0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
   0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
  -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
  -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f
};

Mesh cubeMesh;

const char* vertexShaderSource = R"*(
#version 330

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;

out vec3 Normal;
out vec3 FragPos;
out vec3 VertColor;

uniform mat4 mvp;
uniform mat4 model;
//uniform float time;

void main()
{
	gl_Position = mvp * vec4(aPos, 1.0);
  FragPos = vec3(model * vec4(aPos, 1.0));
  Normal = (model * vec4(aNormal, 1.0)).xyz;
  VertColor = aColor;
}
)*";

const char* fragmentShaderSource = R"*(
#version 330

out vec4 FragColor;

in vec3 Normal;
in vec3 FragPos;
in vec3 VertColor;

uniform vec3 lightDir;

void main()
{
  //todo: get rid of this eventually.
  //this discard's if condition probably isn't worth it unless/until the frag shader is computationally expensive.
  //if (dot(Normal, vec3(0.0, 0.0, -1.0)) >= 0) //todo: change hardcoded vector to camera view dir.
  //  discard; //pixel facing away from camera



  //float light = max(dot(-normalize(Normal), lightDir), 0.0);
  //vec4 diffCol = vec4(0.0, light, 0.0, 1.0);
  //vec4 finalCol = (vec4(0.1, 0.0, 0.3, 1.0) + diffCol) * vec4(1.0, 1.0, 1.0, 1.0); //(ambient + diffuse) * objcolor;
	//FragColor = finalCol;
  FragColor = vec4(VertColor.rgb, 1.0);
}
)*";

Renderer::Renderer(int width, int height) : shader(vertexShaderSource, fragmentShaderSource) //field initializer creates shader
{
  cubeMesh = Mesh(vertices, sizeof(vertices));
  //this->models.push_back(Model(&cubeMesh)); //temp

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
  //TODO: the mesh builder should probably be smart enough to do the mesh correctly so that we can use this:
  glEnable(GL_CULL_FACE); //do not use these, the fragment shader does this based on normals (that way the vertex order doesn't matter).
  glCullFace(GL_FRONT);
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
  this->shader.use();

  constexpr float camMoveSpeed = 15.f;
  constexpr float camLookSpeed = 50.f;
  float thisFrameMoveSpeed = camMoveSpeed * deltaTime;
  if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl))
    thisFrameMoveSpeed *= 2.f; //twice as fast when holding ctrl.
  static glm::vec3 camPos = glm::vec3(0.f, 0.f, 3.f);
  static glm::vec3 camFront = glm::vec3(0.f, 0.f, -1.f);
  static glm::vec3 camUp = glm::vec3(0.f, 1.f, 0.f);

  {
    static float pitch = 0.f;
    static float yaw = 0.f;
    float xpos = 0.f;
    float ypos = 0.f;
    if (ImGui::IsKeyDown(ImGuiKey_UpArrow))
      ypos += 1.f * deltaTime * camLookSpeed;
    if (ImGui::IsKeyDown(ImGuiKey_DownArrow))
      ypos -= 1.f * deltaTime * camLookSpeed;
    if (ImGui::IsKeyDown(ImGuiKey_LeftArrow))
      xpos -= 1.f * deltaTime * camLookSpeed;
    if (ImGui::IsKeyDown(ImGuiKey_RightArrow))
      xpos += 1.f * deltaTime * camLookSpeed;

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

  /*
  * *** Show vcolor (bool) (if off, use diffuse or something else idk)
  * *** Show wireframe (bool) (if on, show only the wireframe)
  * *** Show Low LOD (bool)
  * *** Viewport FOV (float 20-150)
  * *** Show checkpoints (bool)
  * *** Enable viewport resizing
  * *** Show starting positions (bool)
  * Show bsp rects/tree (bool)
  * *** Camera look sensitivity (float)
  * *** Camera move sensitivity (float)
  * *** Camera sprint multiplier (float)
  * Show normals (blender style, red is up, blue is down)
  * Filter by material (highlight all quads with a specified subset of materials)
  * Filter by quad flags (higlight all quads with a specified subset of quadflags)
  * Filter by draw flags ""
  * Filter by terrain "" 
  * 
  * NOTE: resetBsp does not trigger when vertex color changes.
  * 
  * Make mesh read live data.
  * 
  * Editor features (edit in viewport blender style).
  */

  //if (ImGui::IsKeyDown(ImGuiKey_Scroll)) //todo zoom in and out

  glm::mat4 perspective = glm::perspective<float>(glm::radians(70.0f), ((float)this->width / (float)this->height), 0.1f, 1000.0f);
  glm::mat4 view = glm::lookAt(camPos, camPos + camFront, camUp);

  std::vector<Model>& itrModels = models.size() > 0 ? models : this->models;
  for (Model m : itrModels)
  {
    //temporary spin effect on the model
    //glm::mat4 rot = glm::mat4(1.0f);
    //rot = glm::rotate(rot, glm::radians(time * 60.f), glm::vec3(1.0f, 0.0f, 0.0f));
    //rot = glm::rotate(rot, glm::radians(time * 40.f), glm::vec3(0.0f, 0.2f, 0.4f));
    //
    //m.rotation = glm::quat_cast(rot);
    glm::mat4 model = m.CalculateModelMatrix();
    /*glm::mat4 view = glm::mat4(1.0f);
    view = glm::translate(view, -move);*/
    glm::mat4 mvp = perspective * view * model;
    this->shader.setUniform("mvp", mvp);
    this->shader.setUniform("model", model);
    this->shader.setUniform("lightDir", glm::normalize(glm::vec3(0.2f, -3.f, -1.f)));
    m.Draw();
  }
  this->shader.unuse();

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

Renderer::~Renderer(void) 
{

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