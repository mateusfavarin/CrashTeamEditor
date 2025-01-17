#include "renderer.h"

#include <../deps/linmath.h>
#include "globalimguiglglfw.h"
#include "stb_image.h"

// Simple helper function to load an image into a OpenGL texture with common settings
static bool LoadTextureFromMemory(const void* data, size_t data_size, GLuint* out_texture, int* out_width, int* out_height)
{
  // Load from file
  int image_width = 0;
  int image_height = 0;
  unsigned char* image_data = stbi_load_from_memory((const unsigned char*)data, (int)data_size, &image_width, &image_height, NULL, 4);
  if (image_data == NULL)
    return false;

  // Create a OpenGL texture identifier
  GLuint image_texture;
  glGenTextures(1, &image_texture);
  glBindTexture(GL_TEXTURE_2D, image_texture);

  // Setup filtering parameters for display
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // Upload pixels into texture
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
  stbi_image_free(image_data);

  *out_texture = image_texture;
  *out_width = image_width;
  *out_height = image_height;

  return true;
}

struct Vertex
{
	vec2 pos;
	vec3 col;
};

//static const Vertex vertices[3] =
//{
//		{ { -0.6f, -0.4f }, { 1.f, 0.f, 0.f } },
//		{ {  0.6f, -0.4f }, { 0.f, 1.f, 0.f } },
//		{ {   0.f,  0.6f }, { 0.f, 0.f, 1.f } }
//};

float vertices[] = {
    -0.5f, -0.5f,  // Bottom-left vertex
     0.5f, -0.5f,  // Bottom-right vertex
     0.0f,  0.5f   // Top vertex
};

//static const char* vertex_shader_text =
//"#version 330\n"
//"uniform mat4 MVP;\n"
//"in vec3 vCol;\n"
//"in vec2 vPos;\n"
//"out vec3 color;\n"
//"void main()\n"
//"{\n"
//"    gl_Position = MVP * vec4(vPos, 0.0, 1.0);\n"
//"    color = vCol;\n"
//"}\n";
//
//static const char* fragment_shader_text =
//"#version 330\n"
//"in vec3 color;\n"
//"out vec4 fragment;\n"
//"void main()\n"
//"{\n"
//"    fragment = vec4(color, 1.0);\n"
//"}\n";

// Vertex Shader source
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;

void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

// Fragment Shader source
const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

void main() {
    FragColor = vec4(0.2, 0.7, 0.3, 1.0); // Greenish color
}
)";

static void checkShaderCompilation(GLuint shader, const char* type) {
  GLint success;
  GLchar infoLog[512];
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    glGetShaderInfoLog(shader, 512, NULL, infoLog);
    fprintf(stderr, "ERROR::SHADER::%s::COMPILATION_FAILED\n%s", type, infoLog);
  }
}

Renderer::Renderer(int width, int height)
{
  this->width = width;
  this->height = height;
  this->buf = new unsigned char[width * height * 4]; //RGBA

  // Create framebuffer
  glGenFramebuffers(1, &framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

  // Create a texture to store color attachment
  glGenTextures(1, &textureColorbuffer);
  glBindTexture(GL_TEXTURE_2D, textureColorbuffer);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorbuffer, 0);

  // Create a renderbuffer for depth and stencil
  glGenRenderbuffers(1, &rbo);
  glBindRenderbuffer(GL_RENDERBUFFER, rbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

  // Check if framebuffer is complete
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    //std::cerr << "ERROR: Framebuffer is not complete!" << std::endl;
    throw 0; //throw exception
  }
  glBindFramebuffer(GL_FRAMEBUFFER, 0); // Unbind

  // Build and compile shaders
  GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
  glCompileShader(vertexShader);
  checkShaderCompilation(vertexShader, "VERTEX");

  GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
  glCompileShader(fragmentShader);
  checkShaderCompilation(fragmentShader, "FRAGMENT");

  shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, vertexShader);
  glAttachShader(shaderProgram, fragmentShader);
  glLinkProgram(shaderProgram);

  // Delete shaders after linking
  glDeleteShader(vertexShader);
  glDeleteShader(fragmentShader);

  // Set up vertex data and buffers
  glGenVertexArrays(1, &VAO);
  glGenBuffers(1, &VBO);

  glBindVertexArray(VAO);

  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

 // fprintf(stdout, "Renderer Init\n");
 // glfwSetErrorCallback(glfw_error_callback);
 // if (!glfwInit())
 //   return false;
	//glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); //"invisible window" (hacky buffer).
	//GLFWwindow* window = glfwCreateWindow(Renderer::width, Renderer::width, "", NULL, NULL);
	//if (window == NULL)
 //   return false;
 // this->window = (int*)window;
	//glfwMakeContextCurrent((GLFWwindow*)window);
  //gladLoadGL(glfwGetProcAddress);
	//glfwSwapInterval(1);
	//glfwGetFramebufferSize((GLFWwindow*)window, NULL, NULL);
	//glViewport(0, 0, Renderer::width, Renderer::height);
  //
  //GLuint vertex_buffer;
  //glGenBuffers(1, &vertex_buffer);
  //glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
  //glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  //
  //const GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  //glShaderSource(vertex_shader, 1, &vertex_shader_text, NULL);
  //glCompileShader(vertex_shader);
  //
  //const GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  //glShaderSource(fragment_shader, 1, &fragment_shader_text, NULL);
  //glCompileShader(fragment_shader);
  //
  //this->program = glCreateProgram();
  //glAttachShader(this->program, vertex_shader);
  //glAttachShader(this->program, fragment_shader);
  //glLinkProgram(this->program);
  //
  //this->mvp_location = (int)glGetUniformLocation(this->program, "MVP");
  //const GLint vpos_location = glGetAttribLocation(this->program, "vPos");
  //const GLint vcol_location = glGetAttribLocation(this->program, "vCol");
  //
  //glGenVertexArrays(1, ((GLuint*)&this->vertex_array));
  //glBindVertexArray(((GLuint)this->vertex_array));
  //glEnableVertexAttribArray(vpos_location);
  //glVertexAttribPointer(vpos_location, 2, GL_FLOAT, GL_FALSE,
  //  sizeof(Vertex), (void*)offsetof(Vertex, pos));
  //glEnableVertexAttribArray(vcol_location);
  //glVertexAttribPointer(vcol_location, 3, GL_FLOAT, GL_FALSE,
  //  sizeof(Vertex), (void*)offsetof(Vertex, col));
  //return true;
}

void Renderer::Render(void)

{
  //if (((GLFWwindow*)this->window) == nullptr)
  //  return;
  //if (glfwWindowShouldClose((GLFWwindow*)this->window))
  //{
  //  glfwDestroyWindow((GLFWwindow*)this->window);
  //  return;
  //}
  //const float ratio = Renderer::width / (float)Renderer::height;

  //glViewport(0, 0, Renderer::width, Renderer::height);
  //glClear(GL_COLOR_BUFFER_BIT);

  //mat4x4 m, p, mvp;
  //mat4x4_identity(m);
  //mat4x4_rotate_Z(m, m, (float)glfwGetTime());
  //mat4x4_ortho(p, -ratio, ratio, -1.f, 1.f, 1.f, -1.f);
  //mat4x4_mul(mvp, p, m);

  //glUseProgram(this->program);
  //glUniformMatrix4fv(this->mvp_location, 1, GL_FALSE, (const GLfloat*)&mvp);
  //glBindVertexArray(this->vertex_array);
  //glDrawArrays(GL_TRIANGLES, 0, 3);

  //glfwSwapBuffers((GLFWwindow*)this->window);
  //glfwPollEvents();

  // Render to framebuffer
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  glViewport(0, 0, width, height); // Match framebuffer size
  glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Render your scene here...
  // Draw triangle
  glUseProgram(shaderProgram);
  glBindVertexArray(VAO);
  glDrawArrays(GL_TRIANGLES, 0, 3);

  // Copy framebuffer data
  CopySysNow();

  // Render to screen
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, width, height); // Match screen size
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::CopySysNow(void)
{
  glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buf);
  for (int i = 0; i < width * height; i += 4)
  {
    buf[0] = 255;
    buf[1] = 0;
    buf[2] = 0;
    buf[3] = 255;
  }

}

Renderer::~Renderer(void) 
{
  delete[] buf;
  glDeleteVertexArrays(1, &VAO);
  glDeleteBuffers(1, &VBO);
  glDeleteProgram(shaderProgram);
}