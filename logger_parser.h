// Parse logger.csv generated by Morpheus CellSorting_3D simulation, not yet optimized / checked for bugs
//
// Usage:
// logger_parser lp(<file_name>);
// std::vector<std::string> headers = { "time", "id", "cell.center.x", ... }; // column names does not have to be in the same order as in the csv
//
// double time, x, ...;
// int id;
//
// while (lp.read_row(time, id, x, ...)) // variables have to be in the order as in headers vector
// {
//    // process time, id, x, ...
// }

#pragma once

#include <cgv/utils/file.h>

class logger_parser
{
private:
	std::string content;

	std::vector<cgv::utils::line> lines;

	int line_index;
	
	std::vector<std::string> column_names;
	std::vector<int> column_orders;

	void parse(const cgv::utils::token& token, double& value)
	{
		cgv::utils::is_double(token.begin, token.end, value);
	}

	void parse(const cgv::utils::token& token, int& value)
	{
		cgv::utils::is_integer(token.begin, token.end, value);
	}

	void parse_row(int column_index, const std::vector<cgv::utils::token>& tokens) {}

	template<class T, class ...Ts>
	void parse_row(int column_index, const std::vector<cgv::utils::token>& tokens, T& column_type, Ts&...column_types)
	{
		parse(tokens[column_orders[column_index]], column_type);

		parse_row(column_index + 1, tokens, column_types...);
	}

public:
	logger_parser() = delete;
	logger_parser(const logger_parser&) = delete;

	logger_parser(const std::string& file_name) : line_index(1)
	{
		if (!cgv::utils::file::read(file_name, content, true))
			std::cerr << "couldn't read logger file " << file_name.c_str() << std::endl;

		cgv::utils::split_to_lines(content, lines);
	}

	void read_header(const std::vector<std::string>& column_names)
	{
		this->column_names = column_names;
		
		column_orders.clear();

		std::vector<cgv::utils::token> tokens;
		cgv::utils::split_to_tokens(lines.begin()->begin, lines.begin()->end, tokens, "");

		for (int i = 0, column_count = column_names.size(); i < column_count; ++i)
		{
			int token_index = 0;
			for (auto token : tokens)
			{
				if (*token.begin == '"')
					++token.begin;

				if (token.end > token.begin && token.end[-1] == '"')
					--token.end;

				if (to_string(token) == column_names[i])
				{
					column_orders.push_back(token_index);
					break;
				}

				++token_index;
			}

			if (column_orders.size() <= i)
			{
				// throw error column not found
				break;
			}
		}
	}

	template<class ...Ts>
	bool read_row(Ts&...column_types)
	{
		assert(sizeof...(Ts) == column_orders.size());

		while (line_index < lines.size())
		{
			std::vector<cgv::utils::token> tokens;
			cgv::utils::split_to_tokens(lines[line_index].begin, lines[line_index].end, tokens, "");

			++line_index;

			if (sizeof...(Ts) <= tokens.size())
			{
				parse_row(0, tokens, column_types...);
				return true;
			}
		}

		return false;
	}
};