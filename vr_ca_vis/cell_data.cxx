#pragma once

#include "cell_data.h"

cell_type::cell_type(const std::string& _name, const std::string& _cell_class) : name(_name), cell_class(_cell_class)
{

}
void cell_type::add_property(const std::string& property)
{
	properties.push_back(property);
}

cell::cell(unsigned int _id, unsigned int _type, const std::vector<float>& _properties) : id(_id), type(_type)//, properties(_properties)
{
	//float t = (float)(id - 1) / (60.0f - 1.0f);
	//color = cm.interpolate_color(t);

	center_index = SIZE_MAX;

	nodes_start_index = SIZE_MAX;
	nodes_end_index = SIZE_MAX;
}
void cell::set_center(size_t index)
{
	center_index = index;
}
void cell::set_nodes(size_t start_index, size_t end_index)
{
	nodes_start_index = start_index;
	nodes_end_index = end_index;
}

//cgv::glutil::color_map create_color_map()
//{
//	cgv::glutil::color_map cm;
//
//	cm.add_color_point(0.f, cgv::render::render_types::rgb(59.f / 255, 76.f / 255, 192.f / 255));
//	cm.add_color_point(1.f, cgv::render::render_types::rgb(180.f / 255, 4.f / 255, 38.f / 255));
//
//	return cm;
//}
//
//cgv::glutil::color_map cell_data::cm = cgv::glutil::color_map();
