#pragma once

#include "cell_data.h"

cell_type::cell_type(const std::string& _name, const std::string& _cell_class) : name(_name), cell_class(_cell_class)
{

}
void cell_type::add_property(const std::string& property)
{
	properties.push_back(property);
}
cell::cell(unsigned int _id, unsigned int _type) : id(_id), type(_type)
{
	center_index = SIZE_MAX;

	nodes_start_index = SIZE_MAX;
	nodes_end_index = SIZE_MAX;

	properties_start_index = SIZE_MAX;
	properties_end_index = SIZE_MAX;
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
void cell::set_properties(size_t start_index, size_t end_index)
{
	properties_start_index = start_index;
	properties_end_index = end_index;
}

std::unordered_map<std::string, cell_type> cell::types;

std::vector<cgv::render::render_types::vec3> cell::centers;
std::vector<cgv::render::render_types::vec3> cell::nodes;
std::vector<float> cell::properties;
