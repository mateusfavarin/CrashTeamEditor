#pragma once

#include "mesh.h"
#include "transform.h"

#include <string>

class Text3D : public Transform
{
public:
	enum class TextAlign
	{
		LEFT,
		CENTER,
		RIGHT
	};

	Text3D();
	explicit Text3D(const std::string& label,
		TextAlign align = TextAlign::LEFT,
		const Color& color = Color(static_cast<unsigned char>(0u), static_cast<unsigned char>(0u), static_cast<unsigned char>(0u), static_cast<unsigned char>(255u)),
		const Color& backgroundColor = Color(static_cast<unsigned char>(0u), static_cast<unsigned char>(0u), static_cast<unsigned char>(0u), static_cast<unsigned char>(0u)));
	const std::string& GetLabel() const;
	void SetLabel(const std::string& label);
	TextAlign GetAlign() const;
	void SetAlign(TextAlign align);

	const Color& GetColor() const;
	void SetColor(const Color& color);
	const Color& GetBackgroundColor() const;
	void SetBackgroundColor(const Color& color);
	float GetSize() const;
	void SetSize(float size);

	Mesh& GetMesh();
	const Mesh& GetMesh() const;

private:
	void UpdateMeshFromLabel();

private:
	std::string m_label;
	TextAlign m_align = TextAlign::LEFT;
	Color m_color = Color(static_cast<unsigned char>(0u), static_cast<unsigned char>(0u), static_cast<unsigned char>(0u), static_cast<unsigned char>(255u));
	Color m_backgroundColor = Color(static_cast<unsigned char>(0u), static_cast<unsigned char>(0u), static_cast<unsigned char>(0u), static_cast<unsigned char>(0u));
	float m_size = 1.0f;
	Mesh m_mesh;
};
