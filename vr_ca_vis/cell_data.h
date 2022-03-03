#pragma once

#include <cgv/render/render_types.h>
#include <cgv_glutil/color_map.h>

//struct cell_type
//{
//	std::string name;		// CellTypes > CellType name
//	std::string cell_class;	// CellTypes > CellType class
//};

struct cell : public cgv::render::render_types
{
	//static cgv::glutil::color_map cm;

	uint16_t id;	// CellPopulations > Population > Cell id
	uint8_t type;	// CellTypes > CellType name & class

	vec3 center;	// CellPopulations > Population > Cell > Center
	vec3 node;		// CellPopulations > Population > Cell > Nodes

	float b;		// CellPopulations > Population > Cell > PropertyData symbol-ref
	float b2;		// CellPopulations > Population > Cell > PropertyData symbol-ref

	rgb color;

	cell(uint16_t _id, uint8_t _type, const vec3& _center, const vec3 _node, float _b, float _b2, const cgv::glutil::color_map& cm) : id(_id), type(_type), center(_center), node(_node), b(_b), b2(_b2)
	{
		float t = (float)(id - 1) / (60.0f - 1.0f);
		color = cm.interpolate_color(t);

		//float r = (1.f - float(id) / 60) * 59.f + (float(id) / 60) * 180.f;
		//float g = 76.f - (float(id) / 60) * 72.f;
		//float b = 192.f - (float(id) / 60) * 38.f;

		//float r = type == 2 ? (1.f - float(id - 30) / 30) * 0.f + (float(id - 30) / 30) * 255.f : 0.f;
		//float g = 0.f;// (1.f - float(id) / 60) * 76.f + (float(id) / 60) * 4.f;
		//float b = type == 1 ? (1.f - float(id) / 30) * 0.f + (float(id) / 30) * 255.f : 0.f;

		//color = rgb(r / 255, g / 255, b / 255);
	}
};

//cgv::glutil::color_map create_color_map()
//{
//	cgv::glutil::color_map cm;
//
//	cm.add_color_point(0.f, cgv::render::render_types::rgb(59.f / 255, 76.f / 255, 192.f / 255));
//	cm.add_color_point(1.f, cgv::render::render_types::rgb(180.f / 255, 4.f / 255, 38.f / 255));
//
//	return cm;
//}

//cgv::glutil::color_map cell<T>::cm = cgv::glutil::color_map();

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
