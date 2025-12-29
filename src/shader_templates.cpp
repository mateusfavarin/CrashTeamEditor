#include "shader_templates.h"

#include <string>

#pragma region Shader Templates
//=====================================================================================================================
// Geometry shaders
//=====================================================================================================================

std::string ShaderTemplates::geom_vcolornormaltex = R"*()*";
std::string ShaderTemplates::geom_vcolornormal = R"*(
#version 330 core

layout(triangles) in;       // Input is a triangle
layout(triangle_strip, max_vertices = 3) out;  // Output is a triangle strip

out vec3 CalculatedFaceNormal;      // Output normal to vertex shader

void main() {
  // Get the vertex positions from the incoming triangle
  vec3 v0 = gl_in[0].gl_Position.xyz;
  vec3 v1 = gl_in[1].gl_Position.xyz;
  vec3 v2 = gl_in[2].gl_Position.xyz;

  // Calculate the face normal using cross product
  vec3 faceNormal = normalize(cross(v0 - v1, v2 - v0));

  // Emit the triangle vertices with the calculated face normal
  for (int i = 0; i < 3; ++i) {
      CalculatedFaceNormal = faceNormal;  // Set the face normal to be the same for all vertices
      gl_Position = gl_in[i].gl_Position;  // Pass through vertex position
      EmitVertex();  // Emit the vertex
  }
  EndPrimitive();  // Finish the triangle
}
)*";

//=====================================================================================================================
// Vertex shaders
//=====================================================================================================================

std::string ShaderTemplates::vert_vcolornormaltex = R"*(
#version 330

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec2 aTexCoord;
layout (location = 4) in int aTexIndex;

out vec3 FragModelPos;
out vec3 FragWorldPos;
out vec3 Normal;
out vec3 VertColor;
out vec2 TexCoord;
flat out int TexIndex;

//world
uniform mat4 mvp;
uniform mat4 model;
uniform vec3 camViewDir;
uniform vec3 camWorldPos;

//draw variations
uniform int drawType; //correlates to GuiRenderSettings::RenderType (not a flag)
uniform int shaderSettings; //correlates to Mesh::ShaderSettings (flag)

//misc
uniform float time;
uniform vec3 lightDir;

void main()
{
	gl_Position = mvp * vec4(aPos, 1.0);

  FragModelPos = vec3(model * vec4(aPos, 1.0));
  FragWorldPos = aPos;
  Normal = (model * vec4(aNormal, 1.0)).xyz;
  VertColor = aColor;
  TexCoord = aTexCoord;
  TexIndex = aTexIndex;
}
)*";
std::string ShaderTemplates::vert_vcolornormal = R"*(
#version 330

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;
layout (location = 2) in vec3 aNormal;

out vec3 FragModelPos;
out vec3 FragWorldPos;
out vec3 Normal;
out vec3 VertColor;

//world
uniform mat4 mvp;
uniform mat4 model;
uniform vec3 camViewDir;
uniform vec3 camWorldPos;

//draw variations
uniform int drawType; //correlates to GuiRenderSettings::RenderType (not a flag)
uniform int shaderSettings; //correlates to Mesh::ShaderSettings (flag)

//misc
uniform float time;
uniform vec3 lightDir;

void main()
{
	gl_Position = mvp * vec4(aPos, 1.0);

  FragModelPos = vec3(model * vec4(aPos, 1.0));
  FragWorldPos = aPos;
  Normal = (model * vec4(aNormal, 1.0)).xyz;
  VertColor = aColor;
}
)*";
std::string ShaderTemplates::vert_vcolor;
std::string ShaderTemplates::vert_normal;
std::string ShaderTemplates::vert_;

//=====================================================================================================================
// Fragment shaders
//=====================================================================================================================
std::string ShaderTemplates::frag_vcolornormaltex = R"*(
#version 330

out vec4 FragColor;

in vec3 FragModelPos;
in vec3 FragWorldPos;
in vec3 Normal;
in vec3 VertColor;
in vec2 TexCoord;
flat in int TexIndex;

//tex
uniform sampler2DArray tex;

//world
uniform mat4 mvp;
uniform mat4 model;
uniform vec3 camViewDir;
uniform vec3 camWorldPos;

//draw variations
uniform int drawType; //correlates to GuiRenderSettings::RenderType (not a flag)
uniform int shaderSettings; //correlates to Mesh::ShaderSettings (flag)

//misc
uniform float time;
uniform vec3 lightDir;
uniform float wireframeWireThickness;

//note: many of the ways I accomplish things here (e.g., backface culling, wireframe) could be accomplished *MUCH* more
//efficiently with purpose-made opengl features, many function calls exist for stuff like that. This approach is simpler
//and requires no invasive changes to how the Shader/Mesh class works, but it might not be hard to impl.
void main()
{
  float diffuseLit = max(dot(-normalize(Normal), lightDir), 0.0);
  vec4 diffCol = vec4(diffuseLit, diffuseLit, diffuseLit, 1.0);
  if (drawType == 0) //0 == "Default"
  {
		vec4 vertColor = vec4(VertColor.rgb, 1.0);
    vec4 texColor = texture(tex, vec3(TexCoord, TexIndex));
    if (texColor.a <= 0.001) { discard; }
    FragColor = texColor * (vertColor * 2.0);
  }
  else if (drawType == 1) //1 == "Texture"
  {
    vec4 texColor = texture(tex, vec3(TexCoord, TexIndex));
    if (texColor.a <= 0.001) { discard; }
    FragColor = texColor;
  }
  else if (drawType == 2) //2 == "Vertex Color"
  {
    vec4 vertColor = vec4(VertColor.rgb, 1.0);
    FragColor = vertColor;
  }
  else if (drawType == 3) //3 == "Normals"
  { //Exterior normals=blue, interior normals=red
    //this logic for red/blue might be backwards idk (compare to blender to make sure).
    float normalDir = dot(-normalize(Normal), (camWorldPos - FragWorldPos));
    vec4 chosenColor;
    if (normalDir < 0)
      chosenColor = vec4(0.0, 0.0, 1.0, 1.0);
    else
      chosenColor = vec4(1.0, 0.0, 0.0, 1.0);
    chosenColor = (vec4(0.1, 0.1, 0.1, 1.0) + diffCol) * chosenColor;  //(ambient + diffuse) * objcolor;
    FragColor = chosenColor;
  }
  else
  {
    vec4 vertColor = vec4(VertColor.rgb, 1.0);
    FragColor = vertColor;
  }

  if ((shaderSettings & 128) != 0) //128 == "DiscardZeroColor"
  {
    if (VertColor.r <= 0.0 && VertColor.g <= 0.0 && VertColor.b <= 0.0)
    {
      discard;
    }
  }

  if ((shaderSettings & 32) != 0) //32 == "Blinky"
  {
    if (mod(time * 1.5, 1.0) < 0.5)
    {
      FragColor = vec4(1.0 - FragColor.r, 1.0 - FragColor.g, 1.0 - FragColor.b, FragColor.a);
    }
  }
}
)*";
std::string ShaderTemplates::frag_vcolornormal = R"*(
#version 330

out vec4 FragColor;

in vec3 FragModelPos;
in vec3 FragWorldPos;
in vec3 Normal;
in vec3 VertColor;

//world
uniform mat4 mvp;
uniform mat4 model;
uniform vec3 camViewDir;
uniform vec3 camWorldPos;

//draw variations
uniform int drawType; //correlates to GuiRenderSettings::RenderType (not a flag)
uniform int shaderSettings; //correlates to Mesh::ShaderSettings (flag)

//misc
uniform float time;
uniform vec3 lightDir;
uniform float wireframeWireThickness;

//note: many of the ways I accomplish things here (e.g., backface culling, wireframe) could be accomplished *MUCH* more
//efficiently with purpose-made opengl features, many function calls exist for stuff like that. This approach is simpler
//and requires no invasive changes to how the Shader/Mesh class works, but it might not be hard to impl.
void main()
{
  float diffuseLit = max(dot(-normalize(Normal), lightDir), 0.0);
  vec4 diffCol = vec4(diffuseLit, diffuseLit, diffuseLit, 1.0);
  if (drawType == 3) //3 == "Normals"
  { //Exterior normals=blue, interior normals=red
    //this logic for red/blue might be backwards idk (compare to blender to make sure).
    float normalDir = dot(-normalize(Normal), (camWorldPos - FragWorldPos));
    vec4 chosenColor;
    if (normalDir < 0)
      chosenColor = vec4(0.0, 0.0, 1.0, 1.0);
    else
      chosenColor = vec4(1.0, 0.0, 0.0, 1.0);
    chosenColor = (vec4(0.1, 0.1, 0.1, 1.0) + diffCol) * chosenColor;  //(ambient + diffuse) * objcolor;
    FragColor = chosenColor;
  }
  else
  {
    FragColor = vec4(VertColor.rgb, 1.0);
  }

  if ((shaderSettings & 128) != 0) //128 == "DiscardZeroColor"
  {
    if (VertColor.r <= 0.0 && VertColor.g <= 0.0 && VertColor.b <= 0.0)
    {
      discard;
    }
  }

  if ((shaderSettings & 32) != 0) //32 == "Blinky"
  {
    if (mod(time * 1.5, 1.0) < 0.5)
    {
      FragColor = vec4(1.0 - FragColor.r, 1.0 - FragColor.g, 1.0 - FragColor.b, FragColor.a);
    }
  }
}
)*";
std::string ShaderTemplates::frag_vcolor;
std::string ShaderTemplates::frag_normal;
std::string ShaderTemplates::frag_;

std::map<int, std::tuple<std::string, std::string, std::string>> ShaderTemplates::datasToShaderSourceMap =
{
  {
    (Mesh::VBufDataType::VertexPos | Mesh::VBufDataType::VertexColor | Mesh::VBufDataType::Normals),
    (std::make_tuple(ShaderTemplates::geom_vcolornormal, ShaderTemplates::vert_vcolornormal, ShaderTemplates::frag_vcolornormal))
  },
  {
    (Mesh::VBufDataType::VertexPos | Mesh::VBufDataType::VertexColor | Mesh::VBufDataType::Normals | Mesh::VBufDataType::STUV | Mesh::VBufDataType::TexIndex),
    (std::make_tuple(ShaderTemplates::geom_vcolornormaltex, ShaderTemplates::vert_vcolornormaltex, ShaderTemplates::frag_vcolornormaltex))
  },
};
#pragma endregion
