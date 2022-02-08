#pragma once

#include <cgv/utils/file.h>
#include <fstream>

#include "../3rd/rapidxml-1.13/rapidxml.hpp"

class model_parser
{
public:
	typedef cgv::render::drawable::vec3 vec3;
	typedef cgv::render::drawable::ivec3 ivec3;
public:
	model_parser() = delete;
	model_parser(const model_parser&) = delete;

	model_parser(const std::string& file_name, ivec3& extent, std::vector<unsigned int>& ids, std::vector<unsigned int>& group_ids, std::vector<vec3>& points, std::unordered_set<std::string>& types)
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
			rapidxml::xml_node<>* s_node = root_node->first_node("Space");
			if (s_node != NULL &&
			   (s_node = s_node->first_node("Lattice")) != NULL &&
			   (s_node = s_node->first_node("Size")) != NULL)
			{
				std::vector<std::string> values_vector;

				char* token = strtok(s_node->first_attribute("value")->value(), " ");

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
			rapidxml::xml_node<>* cp_node = root_node->first_node("CellPopulations");
			if (cp_node != NULL)
			{
				unsigned int type_index = 0;

				for (rapidxml::xml_node<>* p_node = cp_node->first_node("Population"); p_node; p_node = p_node->next_sibling())
				{
					std::string type(p_node->first_attribute("type")->value());

					//type_start.push_back(points.size());
					types.insert(type);

					for (rapidxml::xml_node<>* c_node = p_node->first_node("Cell"); c_node; c_node = c_node->next_sibling())
					{
						int id;
						if (!cgv::utils::is_integer(std::string(c_node->first_attribute("id")->value()), id) || id == 0)
							continue;

						rapidxml::xml_node<>* n_node = c_node->first_node("Nodes");
						if (n_node != NULL)
						{
							std::vector<std::string> points_vector;

							char* token = strtok(n_node->value(), ";");

							// Keep printing tokens while one of the
							// delimiters present in str[].
							while (token != NULL)
							{
								points_vector.push_back(token);
								token = strtok(NULL, ";");
							}

							//std::vector<cgv::utils::token> points_tokens;
							//cgv::utils::split_to_tokens(std::string(n_node->value()), points_tokens, ";");

							for (auto point : points_vector)
							{
								std::vector<std::string> point_vector;

								token = strtok(&point[0], ", ");

								// Keep printing tokens while one of the
								// delimiters present in str[].
								while (token != NULL)
								{
									point_vector.push_back(token);
									token = strtok(NULL, ", ");
								}

								//std::vector<cgv::utils::token> point_tokens;
								//cgv::utils::split_to_tokens(points_token, point_tokens, ", ");

								if (point_vector.size() != 3)
									continue;

								int x, y, z;
								if (!cgv::utils::is_integer(point_vector[0], x) || !cgv::utils::is_integer(point_vector[1], y) || !cgv::utils::is_integer(point_vector[2], z))
									continue;

								ids.push_back(id);

								group_ids.push_back(type_index);

								points.emplace_back(float(x), float(y), float(z));
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
