#include "mesh.h"

void Mesh::Bind()
{
  if (VAO != 0)
    glBindVertexArray(VAO);
}

void Mesh::Unbind()
{
  glBindVertexArray(0);
}

void Mesh::Draw()
{
  if (VAO != 0)
    glDrawArrays(GL_TRIANGLES, 0, dataBufSize / sizeof(float));
}

void Mesh::UpdateMesh(float data[], int dataBufSize)
{
  Dispose();

  this->dataBufSize = dataBufSize;

  glGenVertexArrays(1, &VAO);
  glBindVertexArray(VAO);

  glGenBuffers(1, &VBO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, dataBufSize, data, GL_STATIC_DRAW);

  //position
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(0);

  //normals
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  //vcolor
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
  glEnableVertexAttribArray(2);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
}

Mesh::Mesh(float data[], int dataBufSize)
{
  UpdateMesh(data, dataBufSize);
}

Mesh::Mesh()
{

}

void Mesh::Dispose()
{
  if (VAO != 0)
    glDeleteVertexArrays(1, &VAO);
  VAO = 0;
  if (VBO != 0)
    glDeleteBuffers(1, &VBO);
  VBO = 0;
  dataBufSize = 0;
}