#pragma once

#include <cgv/utils/file.h>
#include <fstream>

#include "../3rd/rapidxml-1.13/rapidxml.hpp"

class model_parser
{
public:
	typedef cgv::render::drawable::vec2 vec2;
	typedef cgv::render::drawable::vec3 vec3;
public:
	model_parser() = delete;
	model_parser(const model_parser&) = delete;

	model_parser(const std::string& file_name, std::vector<int>& ids, std::vector<vec3>& points)
	{
		rapidxml::xml_document<> doc;
		rapidxml::xml_node<> *root_node;

		// Read the xml file into a vector
		std::ifstream file(file_name.c_str());
		std::vector<char> buffer((std::istreambuf_iterator<char>(file)), std:: istreambuf_iterator<char>());
		buffer.push_back('\0');
		// Parse the buffer using the xml file parsing library into doc 
		doc.parse<0>(&buffer[0]);
		// Find our root node
		root_node = doc.first_node("MorpheusModel");
		// Iterate over cell populations
		for (rapidxml::xml_node<> *cp_node = root_node->first_node("CellPopulations"); cp_node; cp_node = cp_node->next_sibling())
		{
			for (rapidxml::xml_node<> *p_node = cp_node->first_node("Population"); p_node; p_node = p_node->next_sibling())
			{
				for (rapidxml::xml_node<> *c_node = p_node->first_node("Cell"); c_node; c_node = c_node->next_sibling())
				{
					int id;
					if (!cgv::utils::is_integer(std::string(c_node->first_attribute("id")->value()), id) || id == 0)
						continue;

					rapidxml::xml_node<>* n_node = c_node->first_node("Nodes");
					if (n_node)
					{
						std::vector<cgv::utils::token> points_tokens;
						cgv::utils::split_to_tokens(std::string(n_node->value()), points_tokens, ";");

						for (auto points_token : points_tokens)
						{
							std::vector<cgv::utils::token> point_tokens;
							cgv::utils::split_to_tokens(points_token, point_tokens, ",");

							if (point_tokens.size() != 3)
								continue;

							ids.push_back(id);

							int x, y, z;
							if (!cgv::utils::is_integer(to_string(point_tokens[0]), x) || !cgv::utils::is_integer(to_string(point_tokens[1]), y) || !cgv::utils::is_integer(to_string(point_tokens[2]) , z))
								continue;

							points.push_back(vec3(x, y, z));
						}
					}
				}
			}
		}
	}
};
