#pragma once

#include <cgv/render/render_types.h>

//struct cell_type
//{
//	std::string name;		// CellTypes > CellType name
//	std::string cell_class;	// CellTypes > CellType class
//};

struct cell : public cgv::render::render_types
{
	uint16_t id;	// CellPopulations > Population > Cell id
	uint8_t type;	// CellTypes > CellType name & class

	vec3 center;	// CellPopulations > Population > Cell > Center
	vec3 node;		// CellPopulations > Population > Cell > Nodes

	float b;		// CellPopulations > Population > Cell > PropertyData symbol-ref
	float b2;		// CellPopulations > Population > Cell > PropertyData symbol-ref

	rgb color;

	cell(uint16_t _id, uint8_t _type, vec3 _center, vec3 _node, float _b, float _b2) : id(_id), type(_type), center(_center), node(_node), b(_b), b2(_b2)
	{
		color = rgb(float(id) / 255.f, 0.f, 0.f);
	}
};

//class cell_vis : public cell
//{
//protected:
//	rgba8 color;
//
//public:
//	cell_vis(uint16_t cell_id, int16_t x, int16_t y, int16_t z, float cell_attr) : cell(cell_id, x, y, z, cell_attr), color(rgba8())
//	{}
//
//	cell_vis(uint16_t cell_time, uint16_t cell_id, uint8_t cell_type, int16_t x, int16_t y, int16_t z, float cell_attr, rgba8 cell_color) : cell(cell_time, cell_id, cell_type, x, y, z, cell_attr), color(cell_color)
//	{}
//
//	rgba8 get_color() const
//	{
//		return color;
//	}
//};
