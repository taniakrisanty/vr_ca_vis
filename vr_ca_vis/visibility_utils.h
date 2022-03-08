#pragma once

// different possible visibility filter 
enum class visibility_filter_enum {
	none,
	by_id,
	by_type
};

inline bool is_cell_visible(const std::vector<int>* visibilities, visibility_filter_enum visibility_filter, unsigned int id, unsigned int type)
{
	// ignore if cell is invisible
	if (visibilities != NULL) {
		switch (visibility_filter)
		{
		case visibility_filter_enum::by_id:
			if (visibilities->at(id) < 1)
				return false;
			break;
		case visibility_filter_enum::by_type:
			if (visibilities->at(type) < 1)
				return false;
			break;
		default:
			break;
		}
	}

	return true;
}
