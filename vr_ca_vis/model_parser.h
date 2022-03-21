#pragma once

#include <cgv/render/render_types.h>

#include <fstream>
#include <unordered_map>

#include "../3rd/rapidxml-1.13/rapidxml.hpp"
#include "cell_data.h"

class model_parser : public cgv::render::render_types
{
public:
	model_parser() = delete;
	model_parser(const model_parser&) = delete;

	model_parser(const std::string& file_name, ivec3& extent, std::vector<cell>& cells)
	{
		// Set lattice extent to default
		extent.set(100, 100, 100);

		// Read the xml file into a vector
		std::ifstream file(file_name.c_str());
		std::vector<char> buffer((std::istreambuf_iterator<char>(file)), std:: istreambuf_iterator<char>());
		buffer.emplace_back('\0');

		// Parse the buffer using the xml file parsing library into doc 
		rapidxml::xml_document<>* doc = new rapidxml::xml_document<>();
		doc->parse<0>(&buffer[0]);
		
		// Find our root node
		rapidxml::xml_node<>* root_node = doc->first_node("MorpheusModel");		
		if (root_node != NULL)
		{
			// Check space symbol
			rapidxml::xml_node<>* node = root_node->first_node("Space");
			if (node != NULL &&
			   (node = node->first_node("Lattice")) != NULL &&
			   (node = node->first_node("Size")) != NULL)
			{
				std::vector<std::string> values_vector;

				char* token = strtok(node->first_attribute("value")->value(), " ");

				// Keep printing tokens while one of the
				// delimiters present in str[].
				while (token != NULL)
				{
					values_vector.push_back(token);
					token = strtok(NULL, " ");
				}

				if (values_vector.size() == 3)
				{
					int x, y, z;
					if (cgv::utils::is_integer(values_vector[0], x) && cgv::utils::is_integer(values_vector[1], y) && !cgv::utils::is_integer(values_vector[2], z))
					{
						extent.set(x, y, z);
					}
				}
			}

			// Iterate over cell types
			node = root_node->first_node("CellTypes");
			if (node != NULL)
			{
				for (rapidxml::xml_node<>* type_node = node->first_node("CellType"); type_node != NULL; type_node = type_node->next_sibling("CellType"))
				{
					std::string name(type_node->first_attribute("name")->value());
					std::string cell_class(type_node->first_attribute("class")->value());

					cell_type type(name, cell_class);

					for (rapidxml::xml_node<>* property_node = type_node->first_node("Property"); property_node != NULL; property_node = property_node->next_sibling("Property"))
					{
						type.add_property(property_node->first_attribute("symbol")->value());
					}

					cell::types.emplace(name, type);
				}
			}

			// Iterate over cell populations 
			node = root_node->first_node("CellPopulations");
			if (node != NULL)
			{
				for (rapidxml::xml_node<>* population_node = node->first_node("Population"); population_node != NULL; population_node = population_node->next_sibling("Population"))
				{
					std::string type(population_node->first_attribute("type")->value());

					std::unordered_map<std::string, cell_type>::const_iterator t = cell::types.find(type);
					if (t == cell::types.end())
						continue;

					std::ptrdiff_t type_index = std::distance<std::unordered_map<std::string, cell_type>::const_iterator>(cell::types.begin(), t);

					for (rapidxml::xml_node<>* cell_node = population_node->first_node("Cell"); cell_node; cell_node = cell_node->next_sibling())
					{
						int id;
						if (!cgv::utils::is_integer(std::string(cell_node->first_attribute("id")->value()), id))
							continue;

						cell cell(id, type_index);

						// set cell properties
						size_t start_index = cell::properties.size();

						for (const auto& property_symbol : t->second.properties)
						{
							rapidxml::xml_node<>* property_node = cell_node->first_node("PropertyData");
							while (property_node != NULL)
							{
								std::string symbol_ref(property_node->first_attribute("symbol-ref")->value());
								if (symbol_ref == property_symbol)
								{
									std::string value_str(property_node->first_attribute("value")->value());
									
									double property;
									cgv::utils::is_double(value_str, property);

									cell::properties.push_back(float(property));

									break;
								}

								property_node = property_node->next_sibling("PropertyData");
							}
						}

						cell.set_properties(start_index, cell::properties.size());

						vec3 center(0.f);

						rapidxml::xml_node<>* center_node = cell_node->first_node("Center");
						if (center_node != NULL)
						{
							std::vector<std::string> center_str_vector;

							char* token = strtok(center_node->value(), ",");

							// Keep printing tokens while one of the
							// delimiters present in str[].
							while (token != NULL)
							{
								center_str_vector.push_back(token);
								token = strtok(NULL, ",");
							}

							if (center_str_vector.size() == 3)
							{
								double x, y, z;
								if (cgv::utils::is_double(center_str_vector[0], x) && cgv::utils::is_double(center_str_vector[1], y) && cgv::utils::is_double(center_str_vector[2], z))
								{
									center.set(float(x), float(y), float(z));
								}
							}
						}

						// set cell center
						cell.set_center(cell::centers.size());

						cell::centers.push_back(center);

						// set cell nodes
						start_index = cell::nodes.size();

						rapidxml::xml_node<>* nodes_node = cell_node->first_node("Nodes");
						if (nodes_node != NULL)
						{
							std::vector<std::string> nodes_str_vector;

							char* token = strtok(nodes_node->value(), ";");

							// Keep printing tokens while one of the
							// delimiters present in str[].
							while (token != NULL)
							{
								nodes_str_vector.push_back(token);
								token = strtok(NULL, ";");
							}

							for (auto node_str : nodes_str_vector)
							{
								std::vector<std::string> node_str_vector;

								token = strtok(&node_str[0], ", ");

								// Keep printing tokens while one of the
								// delimiters present in str[].
								while (token != NULL)
								{
									node_str_vector.push_back(token);
									token = strtok(NULL, ", ");
								}

								if (node_str_vector.size() != 3)
									continue;

								int x, y, z;
								if (!cgv::utils::is_integer(node_str_vector[0], x) || !cgv::utils::is_integer(node_str_vector[1], y) || !cgv::utils::is_integer(node_str_vector[2], z))
									continue;

								cell::nodes.emplace_back(x, y, z);
							}
						}

						cell.set_nodes(start_index, cell::nodes.size());

						cells.push_back(cell);
					}
				}
			}
		}

		delete doc;
	}
};
