#pragma once

#include "cell_data.h"

cell_data::cell_data(uint16_t _id, uint8_t _type, const vec3& _center, float _b, float _b2) : id(_id), type(_type), center(_center), b(_b), b2(_b2)
{
	float t = (float)(id - 1) / (60.0f - 1.0f);
	color = cm.interpolate_color(t);
}

void cell_data::add_node(float x, float y, float z)
{
	nodes.emplace_back(x, y, z);
}

cgv::glutil::color_map create_color_map()
{
	cgv::glutil::color_map cm;

	cm.add_color_point(0.f, cgv::render::render_types::rgb(59.f / 255, 76.f / 255, 192.f / 255));
	cm.add_color_point(1.f, cgv::render::render_types::rgb(180.f / 255, 4.f / 255, 38.f / 255));

	return cm;
}

cgv::glutil::color_map cell_data::cm = cgv::glutil::color_map();

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
