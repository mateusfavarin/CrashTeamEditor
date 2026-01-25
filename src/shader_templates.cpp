#include "shader_templates.h"

#include <string>

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

out vec3 FragWorldPos;
out vec3 Normal;
out vec3 VertColor;
out vec2 TexCoord;
flat out int TexIndex;

//world
uniform mat4 mvp;
uniform mat4 model;

void main()
{
	gl_Position = mvp * vec4(aPos, 1.0);

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

out vec3 FragWorldPos;
out vec3 Normal;
out vec3 VertColor;

//world
uniform mat4 mvp;
uniform mat4 model;

void main()
{
	gl_Position = mvp * vec4(aPos, 1.0);

  FragWorldPos = aPos;
  Normal = (model * vec4(aNormal, 1.0)).xyz;
  VertColor = aColor;
}
)*";

//=====================================================================================================================
// Fragment shaders
//=====================================================================================================================
std::string ShaderTemplates::frag_vcolornormaltex = R"*(
#version 330

out vec4 FragColor;

in vec3 FragWorldPos;
in vec3 Normal;
in vec3 VertColor;
in vec2 TexCoord;
flat in int TexIndex;

//tex
uniform sampler2DArray tex;

//world
uniform vec3 camWorldPos;

//draw variations
uniform int drawType; //correlates to GuiRenderSettings::RenderType (not a flag)
uniform int shaderSettings; //correlates to Mesh::ShaderFlags (flag)

//misc
uniform float time;
uniform vec3 lightDir;

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

  if ((shaderSettings & 2) != 0) // "DiscardZeroColor"
  {
    if (VertColor.r <= 0.0 && VertColor.g <= 0.0 && VertColor.b <= 0.0)
    {
      discard;
    }
  }

  if ((shaderSettings & 1) != 0) // "Blinky"
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

in vec3 FragWorldPos;
in vec3 Normal;
in vec3 VertColor;

//world
uniform vec3 camWorldPos;

//draw variations
uniform int drawType; //correlates to GuiRenderSettings::RenderType (not a flag)
uniform int shaderSettings; //correlates to Mesh::ShaderFlags (flag)

//misc
uniform float time;
uniform vec3 lightDir;

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

  if ((shaderSettings & 2) != 0) // "DiscardZeroColor"
  {
    if (VertColor.r <= 0.0 && VertColor.g <= 0.0 && VertColor.b <= 0.0)
    {
      discard;
    }
  }

  if ((shaderSettings & 1) != 0) // "Blinky"
  {
    if (mod(time * 1.5, 1.0) < 0.5)
    {
      FragColor = vec4(1.0 - FragColor.r, 1.0 - FragColor.g, 1.0 - FragColor.b, FragColor.a);
    }
  }
}
)*";
std::map<int, std::pair<std::string, std::string>> ShaderTemplates::datasToShaderSourceMap =
{
  {
    Mesh::VBufDataType::None,
    (std::make_pair(ShaderTemplates::vert_vcolornormal, ShaderTemplates::frag_vcolornormal))
  },
  {
    Mesh::VBufDataType::UV,
    (std::make_pair(ShaderTemplates::vert_vcolornormaltex, ShaderTemplates::frag_vcolornormaltex))
  },
};
