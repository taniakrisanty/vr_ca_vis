#pragma once

#include <cgv/utils/file.h>
#include <fstream>

#include "../3rd/rapidxml-1.13/rapidxml.hpp"
#include "cell_data.h"

class model_parser
{
public:
	typedef cgv::render::drawable::vec3 vec3;
	typedef cgv::render::drawable::ivec3 ivec3;
public:
	model_parser() = delete;
	model_parser(const model_parser&) = delete;

	model_parser(const std::string& file_name, ivec3& extent, std::unordered_set<std::string>& types, std::vector<cell>& cells /* std::vector<uint32_t>& type_start, std::vector<unsigned int>& ids, std::vector<unsigned int>& group_ids, std::vector<vec3>& points */)
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

			// Iterate over cell populations
			node = root_node->first_node("CellPopulations");
			if (node != NULL)
			{
				uint8_t type_index = 0;

				for (rapidxml::xml_node<>* population_node = node->first_node("Population"); population_node != NULL; population_node = population_node->next_sibling())
				{
					std::string type(population_node->first_attribute("type")->value());

					//type_start.push_back(points.size());
					types.insert(type);

					for (rapidxml::xml_node<>* cell_node = population_node->first_node("Cell"); cell_node; cell_node = cell_node->next_sibling())
					{
						int id;
						if (!cgv::utils::is_integer(std::string(cell_node->first_attribute("id")->value()), id) || id == 0)
							continue;

						double b, b2;

						rapidxml::xml_node<>* property_node = cell_node->first_node("PropertyData");
						while (property_node != NULL)
						{
							std::string symbol_ref(property_node->first_attribute("symbol-ref")->value());
							if (symbol_ref == "b")
							{
								std::string value_str(property_node->first_attribute("value")->value());
								cgv::utils::is_double(value_str, b);

								break;
							}

							property_node = property_node->next_sibling();
						}

						property_node = cell_node->first_node("PropertyData");
						while (property_node != NULL)
						{
							std::string symbol_ref(property_node->first_attribute("symbol-ref")->value());
							if (symbol_ref == "b2")
							{
								std::string value_str(property_node->first_attribute("value")->value());
								cgv::utils::is_double(value_str, b2);

								break;
							}

							property_node = property_node->next_sibling();
						}

						vec3 center;

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

								//ids.push_back(id);

								//group_ids.push_back(type_index);

								//points.emplace_back(float(x), float(y), float(z));

								cells.emplace_back(id, type_index, center, vec3(float(x), float(y), float(z)), b, b2);
							}
						}
					}

					++type_index;
				}
			}
		}

		delete doc;
	}
};
