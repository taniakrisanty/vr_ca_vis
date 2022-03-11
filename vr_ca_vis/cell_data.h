#pragma once

#include <cgv/render/render_types.h>

#include <unordered_map>

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
	static std::unordered_map<std::string, cell_type> types;

	static std::vector<vec3> centers;
	static std::vector<vec3> nodes;
	static std::vector<float> properties;

	unsigned int id;					// CellPopulations > Population > Cell id
	unsigned int type;					// CellTypes > CellType name & class

	size_t center_index;				// CellPopulations > Population > Cell > Center

	size_t nodes_start_index;			// CellPopulations > Population > Cell > Nodes
	size_t nodes_end_index;				// CellPopulations > Population > Cell > Nodes

	size_t properties_start_index;		// CellPopulations > Population > Cell > PropertyData symbol-ref
	size_t properties_end_index;		// CellPopulations > Population > Cell > PropertyData symbol-ref

	cell(unsigned int id, unsigned int type);
	void set_center(size_t index);
	void set_nodes(size_t start_index, size_t end_index);
	void set_properties(size_t start_index, size_t end_index);
};
