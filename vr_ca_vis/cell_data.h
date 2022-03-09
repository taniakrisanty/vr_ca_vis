#pragma once

#include <cgv/render/render_types.h>
#include <cgv_glutil/color_map.h>

//struct cell : public cgv::render::render_types
//{
//	uint16_t id;		// CellPopulations > Population > Cell id
//	unsigned int type;	// CellTypes > CellType name & class
//
//	vec3 center;		// CellPopulations > Population > Cell > Center
//	vec3 node;			// CellPopulations > Population > Cell > Nodes
//
//	float b;			// CellPopulations > Population > Cell > PropertyData symbol-ref
//	float b2;			// CellPopulations > Population > Cell > PropertyData symbol-ref
//
//	rgba color;
//
//	cell(uint16_t _id, unsigned int _type, const vec3& _center, const vec3& _node, float _b, float _b2) : id(_id), type(_type), center(_center), node(_node), b(_b), b2(_b2)
//	{
//		//float t = (float)(id - 1) / (60.0f - 1.0f);
//		//color = cm.interpolate_color(t);
//	}
//};

struct cell_type
{
	std::string name;		// CellTypes > CellType name
	std::string cell_class;	// CellTypes > CellType class

	std::vector<std::string> properties;

	cell_type(const std::string& name, const std::string& cell_class);
	void add_property(const std::string& property);
};

struct cell : public cgv::render::render_types
{
	unsigned int id;					// CellPopulations > Population > Cell id
	unsigned int type;					// CellTypes > CellType name & class

	size_t center_index;				// CellPopulations > Population > Cell > Center

	size_t nodes_start_index;			// CellPopulations > Population > Cell > Nodes
	size_t nodes_end_index;				// CellPopulations > Population > Cell > Nodes

	//std::vector<float> properties;		// CellPopulations > Population > Cell > PropertyData symbol-ref

	//rgba color;

//public:
	cell(unsigned int id, unsigned int type, const std::vector<float>& properties);
	void set_center(size_t index);
	void set_nodes(size_t start_index, size_t end_index);
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
