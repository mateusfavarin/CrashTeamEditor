#include "shader_templates.h"

#include <string>

/*
  enum RenderType {
    VColor,
    Diffuse,
    Normals,
    DontDraw //specifically don't draw "faces", but still draw wireframe (if enabled) or verts (if enabled) etc.
  };
  enum ShaderSettings
  {
    DrawWireframe,
    DrawVertsAsPoints,
  };
  */

#pragma region Shader Templates
//=====================================================================================================================
// Geometry shaders
//=====================================================================================================================

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

std::string ShaderTemplates::vert_vcolornormal = R"*(
#version 330

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aBarycentric;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec3 aColor;

out vec3 FragModelPos;
out vec3 FragWorldPos;
out vec3 Normal;
out vec3 VertColor;
out vec3 BarycentricCoords;
//out vec3 CalculatedFaceNormal;

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
  BarycentricCoords = aBarycentric;
}
)*";
std::string ShaderTemplates::vert_vcolor;
std::string ShaderTemplates::vert_normal;
std::string ShaderTemplates::vert_;


/*
  enum RenderType {
    VColor,
    Diffuse,
    Normals,
    DontDraw //specifically don't draw "faces", but still draw wireframe (if enabled) or verts (if enabled) etc.
  };
  enum ShaderSettings
  {
    DrawWireframe,
    DrawVertsAsPoints,
    DrawBackfaces,
  };
  */

//=====================================================================================================================
// Fragment shaders
//=====================================================================================================================

std::string ShaderTemplates::frag_vcolornormal = R"*(
#version 330

out vec4 FragColor;

in vec3 FragModelPos;
in vec3 FragWorldPos;
in vec3 Normal;
in vec3 VertColor;
in vec3 BarycentricCoords;
//in vec3 CalculatedFaceNormal;

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
  if (drawType != 3) //3 == "DontDraw", so only draw if neq 3.
  {
    float diffuseLit = max(dot(-normalize(Normal), lightDir), 0.0);
    vec4 diffCol = vec4(diffuseLit, diffuseLit, diffuseLit, 1.0);
    if (drawType == 0) //0 == "VColor"
      FragColor = vec4(VertColor.rgb, 1.0);
    else if (drawType == 1) //1 == "Diffuse"
    {
      vec4 finalCol = (vec4(0.1, 0.0, 0.3, 1.0) + diffCol) * vec4(0.2, 1.0, 0.2, 1.0); //(ambient + diffuse) * objcolor;
	    FragColor = finalCol;
    }
    else if (drawType == 2) //2 == "Normals"
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
  }
  //remaining code runs regardless (e.g., wireframe, dots).
  bool shouldDiscard = true;
  if ((shaderSettings & 1) == 1) //&1 == wireframe
  {
    shouldDiscard = false;
    //https://stackoverflow.com/questions/137629/how-do-you-render-primitives-as-wireframes-in-opengl
    float f_closest_edge = min(BarycentricCoords.x,
      min(BarycentricCoords.y, BarycentricCoords.z)); // see to which edge this pixel is the closest
    float f_width = fwidth(f_closest_edge); // calculate derivative (divide wireframeWireThickness by this to have the line width constant in screen-space)
    float f_alpha = smoothstep(wireframeWireThickness, wireframeWireThickness + f_width, f_closest_edge); // calculate alpha
    FragColor = vec4(vec3(.0), f_alpha);
  }
  if ((shaderSettings & 2) == 2) //&2 == drawvertsaspoints
  {
    shouldDiscard = false;
    //todo
  }
  if ((!((shaderSettings & 4) == 4) && shouldDiscard)) //&4 == drawbackfaces
  {
    if (dot(normalize(Normal), camViewDir) >= 0 || drawType == 3) //discard backfaces if drawType == 3 (i.e., "DontDraw")
    {
      discard; //pixel facing away from camera
      return; //https://community.khronos.org/t/use-of-discard-and-return/68293#:~:text=You%20do%20need%20to%20call%20return%20as%20well%20to%20cancel%20execution
    }
  }
  else if (drawType == 3 && shouldDiscard)
  { //discard because drawbackfaces is disabled
    discard;
    return;
  }
}
)*";
std::string ShaderTemplates::frag_vcolor;
std::string ShaderTemplates::frag_normal;
std::string ShaderTemplates::frag_;

std::map<Mesh::VBufDataType, std::tuple<std::string, std::string, std::string>> ShaderTemplates::datasToShaderSourceMap =
{
  { 
    ((Mesh::VBufDataType)(Mesh::VBufDataType::VertexPos | Mesh::VBufDataType::Barycentric | Mesh::VBufDataType::VColor | Mesh::VBufDataType::Normals)),
    (std::make_tuple(ShaderTemplates::geom_vcolornormal, ShaderTemplates::vert_vcolornormal, ShaderTemplates::frag_vcolornormal))
  },
};
#pragma endregion