#include <cstdint>
#include <cgv/gui/trigger.h>
#include <cgv/signal/rebind.h>
#include <cgv/base/node.h>
#include <cgv/utils/file.h>
#include <cgv/utils/advanced_scan.h>
#include <cgv/utils/progression.h>
#include <cgv/base/register.h>
#include <cgv/math/fvec.h>
#include <cgv/math/ftransform.h>
#include <cgv/gui/provider.h>
#include <cgv/gui/trigger.h>
#include <cgv/gui/event_handler.h>
#include <cgv/gui/key_event.h>
#include <cgv/render/drawable.h>
#include <cgv/media/color_scale.h>
#include <cgv_gl/sphere_renderer.h>
#include <cgv_gl/box_renderer.h>
#include <cgv_gl/gl/gl.h>
#include <random>
#include "endian.h"
#include "cae_file_format.h"
#include "logger_parser.h"
#include "model_parser.h"
#include "gzip_inflater.h"
#include "simulation_data.h"
#include <functional>
#include <cgv/utils/dir.h>

#include <vr/vr_state.h>
#include <vr/vr_kit.h>
#include <vr/vr_driver.h>
#include <cg_vr/vr_events.h>
#include <vr_view_interactor.h>
#include <plugins/vr_lab/vr_tool.h>

#include "intersection.h"

#include "GridTraverser.h"
#include "HashGrid.h"

class vr_ca_vis :
	public cgv::base::node,
	public cgv::render::drawable,
	public cgv::gui::event_handler,
	public cgv::gui::provider,
	public vr::vr_tool,
	public cae::binary_file
{
public:
	typedef cgv::render::drawable::vec2 vec2;
	typedef cgv::render::drawable::vec3 vec3;
	typedef cgv::render::drawable::vec4 vec4;
protected:
	// different interaction states for the controllers
	enum InteractionState {
		IS_NONE,
		IS_OVER,
		IS_GRAB
	};

	// intersection points
	std::vector<vec3> intersection_points;
	std::vector<rgb>  intersection_colors;
	std::vector<int>  intersection_box_indices;
	std::vector<int>  intersection_controller_indices;

	float ray_length = 2;

	// state of current interaction with boxes for all controllers
	InteractionState state[4];

	// render style for interaction
	cgv::render::sphere_render_style srs;

	// keep reference to vr_view_interactor
	vr_view_interactor* vr_view_ptr;

	std::vector<HashGrid<Point>> vertexGrids;

	float a, b, c, d;

	//simulation_data data;

	// stored data
	std::vector<std::string> types;
	std::vector<vec3> points;
	std::vector<uint32_t> group_indices;
	std::vector<float> attr_values;
	std::vector<rgba8> colors;

	std::vector<vec3> ps;

	// attributes
	uint32_t selected_attr;

	// per group information
	std::vector<rgba> group_colors;
	std::vector<vec3> group_translations;
	std::vector<vec4> group_rotations;

	// scaling factor for dataset
	double scale;

	// current time step 
	uint32_t time_step;

	// ooc handling
	bool ooc_mode;
	std::string ooc_file_name;

	std::string dir_name;
	std::string file_name;
	// render parameters
	bool use_boxes;
	vec3 box_extent;
	bool sort_points;
	bool blend;
	bool animate;
	float opacity;
	float trigger[2];
	float time_delta;
	cgv::media::ColorScale clr_scale;
	// render objects
	std::vector<unsigned> indices;
	cgv::render::view* view_ptr;
	cgv::render::sphere_render_style sphere_style;
	cgv::render::sphere_renderer s_renderer;
	cgv::render::surface_render_style box_style;
	cgv::render::box_renderer b_renderer;

	void construct_group_information(unsigned nr_groups)
	{
		 group_colors.resize(nr_groups);
		group_translations.resize(nr_groups, vec3(0, 0, 0));
		group_rotations.resize(nr_groups, vec4(0, 0, 0, 1));
		// define colors from hls
		for (unsigned i = 0; i < nr_groups; ++i) {
			float hue = float(i) / nr_groups;
			group_colors[i] = cgv::media::color<float, cgv::media::HLS, cgv::media::OPACITY>(hue, 0.5f, 1.0f, 0.5f);
		}
	}
	// update colors from attribute values
	void update_colors()
	{
		if (colors.size() != points.size())
			colors.resize(points.size());
		
		float offset = 0.0;
		float scale = 1.0f;
		if (((format.flags & cae::FF_ATTRIBUTE_RANGES) != 0) && attribute_ranges.size() == nr_attributes) {
			offset = attribute_ranges[selected_attr][0];
			scale = 1.0f / (attribute_ranges[selected_attr][1] - attribute_ranges[selected_attr][0]);
		}
		for (size_t i = 0; i < colors.size(); ++i) {
			float v = scale*(attr_values[i*nr_attributes + selected_attr] - offset);
			//float v = scale * (data.cells[i * nr_attributes + selected_attr].get_attr() - offset);
			colors[i] = cgv::media::color_scale(v, clr_scale);
		}
	}

public:
	bool read_file(const std::string& file_name)
	{
		//std::vector<cgv::math::fvec<int16_t, 3>> points;
		//std::vector<uint16_t> ids;
		//std::vector<float> attr;

		if (!read(file_name, points, group_indices, attr_values))
			return false;

		//data.cells.reserve(points.size());

		//for (int i = 0; i < points.size(); ++i)
		//{
		//	data.cells.push_back(cell_vis(ids[i], points[i].x(), points[i].y(), points[i].z(), attr[i]));
		//}

		construct_group_information(nr_groups);
		update_colors();
		return true;
	}
	bool write_file(const std::string& file_name) const
	{
		return write(file_name, points, group_indices, attr_values);

		//std::vector<cgv::math::fvec<int16_t, 3>> points;

		//std::transform(data.cells.begin(), data.cells.end(), std::back_inserter(points),
		//	std::mem_fn(&cell_vis::get_point));

		//std::vector<uint16_t> ids;

		//std::transform(data.cells.begin(), data.cells.end(), std::back_inserter(ids),
		//	std::mem_fn(&cell_vis::get_id));

		//std::vector<float> attr;

		//std::transform(data.cells.begin(), data.cells.end(), std::back_inserter(attr),
		//	std::mem_fn(&cell_vis::get_attr));

		//return write(file_name, points, ids, attr);
	}
	//bool read_data_ascii(const std::string& file_name, float max_time_step = std::numeric_limits<float>::max())
	//{
	//	std::string fn = cgv::utils::file::drop_extension(file_name) + ".cae";
	//	if (cgv::utils::file::exists(fn))
	//		if (read_file(fn))
	//			return true;

	//	std::string content;
	//	if (!cgv::utils::file::read(file_name, content, true))
	//		return false;
	//	std::vector<cgv::utils::line> lines;
	//	cgv::utils::split_to_lines(content, lines);
	//	cgv::utils::progression pr("parse lines", lines.size(), 20);
	//	// extract the attribute names from first line
	//	std::vector<cgv::utils::token> toks;
	//	cgv::utils::split_to_tokens(lines.begin()->begin, lines.begin()->end, toks, "");
	//	size_t ti = 0;
	//	for (auto tok : toks) {
	//		if (*tok.begin == '"')
	//			++tok.begin;
	//		if (tok.end > tok.begin && tok.end[-1] == '"')
	//			--tok.end;
	//		std::string name = to_string(tok);
	//		switch (ti) {
	//		case 0:
	//			std::cout << "name of time: " << name << std::endl;
	//			break;
	//		case 1: 
	//		case 2:
	//		case 3: 
	//			std::cout << "name of " << std::string("xyz")[ti-1] << ": " << name << std::endl;
	//			break;
	//		case 4:
	//			std::cout << "name of cell id: " << name << std::endl;
	//			break;
	//		default:
	//			attr_names.push_back(name);
	//			std::cout << "name of attr" << ti - 5 << ": " << name << std::endl;
	//			break;
	//		}
	//		++ti;
	//	}
	//	nr_attributes = uint32_t(attr_names.size());
	//	// iterate through all lines
	//	unsigned li = 0;
	//	for (auto l : lines) {
	//		pr.step();
	//		if (++li % (lines.size() / 50) == 0) {
	//			std::cout << "read " << file_name << " with " << points.size() << " points, " << times.size() << " time steps, and " << group_colors.size() << " ids." << std::endl;
	//		}
	//		std::vector<cgv::utils::token> toks;
	//		cgv::utils::split_to_tokens(l.begin, l.end, toks, "");
	//		if (toks.size() < 5)
	//			continue;
	//		int x, y, z, id;
	//		double t;
	//		std::vector<double> a(nr_attributes, 0.0);
	//		if (!cgv::utils::is_double(toks[0].begin, toks[0].end, t))
	//			continue;
	//		if (!cgv::utils::is_integer(toks[1].begin, toks[1].end, x))
	//			continue;
	//		if (!cgv::utils::is_integer(toks[2].begin, toks[2].end, y))
	//			continue;
	//		if (!cgv::utils::is_integer(toks[3].begin, toks[3].end, z))
	//			continue;
	//		if (!cgv::utils::is_integer(toks[4].begin, toks[4].end, id))
	//			continue;
	//		uint32_t ai, attr_parse_cnt = std::min(uint32_t(toks.size()) - 5, nr_attributes);
	//		for (ai = 0; ai < attr_parse_cnt; ++ai)
	//			if (!cgv::utils::is_double(toks[ai+5].begin, toks[ai + 5].end, a[ai]))
	//				continue;

	//		if (t >= max_time_step)
	//			break;
	//		if (times.empty() || times.back() != float(t)) {
	//			//std::cout << "t = " << t << " max = " << max_time_step << std::endl;
	//			time_step_start.push_back(points.size());
	//			times.push_back(float(t));
	//		}
	//		points.push_back(vec3(float(x), float(y), float(z)));
	//		for (ai = 0; ai < nr_attributes; ++ai)
	//			attr_values.push_back(float(a[ai]));
	//		rgba col(float(a[0]), float(a[1]), float(a[2]), 0.5f);
	//		colors.push_back(col);
	//		group_indices.push_back(id);
	//		while (id >= int(group_colors.size())) {
	//			group_colors.push_back(rgba(1, 1, 1, 0.5f));
	//			group_translations.push_back(vec3(0, 0, 0));
	//			group_rotations.push_back(vec4(0, 0, 0, 1));
	//		}
	//	}
	//	// define colors from hls
	//	for (unsigned i = 0; i < group_colors.size(); ++i) {
	//		float hue = float(i) / group_colors.size();
	//		group_colors[i] = cgv::media::color<float, cgv::media::HLS, cgv::media::OPACITY>(hue, 0.5f, 1.0f, 0.5f);
	//	}
	//	nr_points = points.size();
	//	nr_groups = uint32_t(group_colors.size());
	//	nr_time_steps = uint32_t(times.size());
	//	std::cout << "read " << file_name << " with " 
	//		<< points.size() << " points, " << times.size() << " time steps, and " << group_colors.size() << " ids and " 
	//		<< nr_attributes << " attributes" << std::endl;
	//	// concatenate 
	//	write_file(fn);
	//	return true;
	//}
	bool read_xml_dir(const std::string& dir_name)
	{
		std::vector<std::string> file_names;
		if (cgv::utils::dir::glob(dir_name, file_names, "*.xml"))
		{
			for (auto file_name : file_names)
			{
				std::string time_str = cgv::utils::file::get_file_name(file_name);
				time_str = cgv::utils::file::drop_extension(time_str);
				time_str = time_str.substr(time_str.size() - 6);

				int time;
				if (!cgv::utils::is_integer(time_str, time))
					continue;

				if (times.empty() || times.back() != float(time)) {
					//std::cout << "t = " << t << " max = " << max_time_step << std::endl;
					time_step_start.push_back(points.size());
					times.push_back(float(time));
				}

				//types.push_back("1");
				//group_indices.push_back(1);
				//points.push_back(vec3(0));

				model_parser parser(file_name, types, group_indices, points);

				//rgba color(0.f, 0.f, 0.f, 0.5f);
				////data.cells.push_back(cell_vis(time, id, type, x, y, z, b, color));

				//points.push_back(vec3(float(x), float(y), float(z)));
				////rgba col(float(a[0]), float(a[1]), float(a[2]), 0.5f);
				//rgba col(0.f, 0.f, 0.f, 0.5f);
				//colors.push_back(col);
				//group_indices.push_back(id);
				//// cells of the same type should have the same color
				//while (id >= int(group_colors.size())) {
				//	group_colors.push_back(rgba(1, 1, 1, 0.5f));
				//	group_translations.push_back(vec3(0, 0, 0));
				//	group_rotations.push_back(vec4(0, 0, 0, 1));
				//}
			}
		}

		return file_names.size() > 0;
	}
	bool read_gz_dir(const std::string& dir_name)
	{
		std::vector<std::string> file_names;
		if (cgv::utils::dir::glob(dir_name, file_names, "*.xml.gz"))
		{
			for (auto file_name : file_names)
			{
				std::string xml_file_name = cgv::utils::file::drop_extension(file_name);

				gzip_inflater(file_name, xml_file_name);
				break;
			}
		}

		return file_names.size() > 0;
	}
	bool read_data_dir_ascii(const std::string& dir_name)
	{
		//std::string fn = cgv::utils::file::drop_extension(file_name) + ".cae";
		//if (cgv::utils::file::exists(fn))
		//	if (read_file(fn))
		//		return true;

		//std::string content;
		//if (!cgv::utils::file::read(file_name, content, true))
		//	return false;
		//std::vector<cgv::utils::line> lines;
		//cgv::utils::split_to_lines(content, lines);
		//cgv::utils::progression pr("parse lines", lines.size(), 20);
		//// extract the attribute names from first line
		//std::vector<cgv::utils::token> toks;
		//cgv::utils::split_to_tokens(lines.begin()->begin, lines.begin()->end, toks, "");
		//size_t ti = 0;
		//for (auto tok : toks) {
		//	if (*tok.begin == '"')
		//		++tok.begin;
		//	if (tok.end > tok.begin && tok.end[-1] == '"')
		//		--tok.end;
		//	std::string name = to_string(tok);
		//	switch (ti) {
		//	case 0:
		//		std::cout << "name of time: " << name << std::endl;
		//		break;
		//	case 1:
		//		std::cout << "name of cell id: " << name << std::endl;
		//		break;
		//	case 2:
		//		std::cout << "name of boundary: " << name << std::endl;
		//		break;
		//	case 5:
		//	case 6:
		//	case 7:
		//		std::cout << "name of " << std::string("xyz")[ti - 5] << ": " << name << std::endl;
		//		break;
		//	default:
		//		attr_names.push_back(name);
		//		std::cout << "name of attr: " << name << std::endl;
		//		break;
		//	}
		//	++ti;
		//}
		attr_names.push_back("b");
		nr_attributes = uint32_t(attr_names.size());

		if (read_xml_dir(dir_name) || read_gz_dir(dir_name) && read_xml_dir(dir_name))
		{
			// reset simulation data
			//data = {};

			//logger_parser parser(file_name);

			//parser.read_header({ "time", "cell.id", "cell.type", "l.x", "l.y", "l.z", "b"});

			//double time, x, y, z, b;
			//int id, type;

			//while (parser.read_row(time, id, type, x, y, z, b))
			//{
			//	if (type == 0) // medium is ignored in code, maybe should just require user to uncheck medium in Morpheus logging? 
			//		continue;

			//	//std::vector<double> a(nr_attributes, 0.0);
			//	//uint32_t ai;

			//	if (time >= max_time_step)
			//		break;

			//	if (times.empty() || times.back() != float(time)) {
			//		//std::cout << "t = " << t << " max = " << max_time_step << std::endl;
			//		time_step_start.push_back(points.size());
			//		times.push_back(float(time));
			//	}

			//	rgba color(0.f, 0.f, 0.f, 0.5f);
			//	//data.cells.push_back(cell_vis(time, id, type, x, y, z, b, color));

			//	points.push_back(vec3(float(x), float(y), float(z)));
			//	//rgba col(float(a[0]), float(a[1]), float(a[2]), 0.5f);
			//	rgba col(0.f, 0.f, 0.f, 0.5f);
			//	colors.push_back(col);
			//	group_indices.push_back(id);
			//	// cells of the same type should have the same color
			//	while (id >= int(group_colors.size())) {
			//		group_colors.push_back(rgba(1, 1, 1, 0.5f));
			//		group_translations.push_back(vec3(0, 0, 0));
			//		group_rotations.push_back(vec4(0, 0, 0, 1));
			//	}
			//}

			for (auto id : group_indices)
			{
				rgba col(0.f, 0.f, 0.f, 0.5f);
				colors.push_back(col);
				// cells of the same type should have the same color
				while (id >= int(group_colors.size())) {
					group_colors.push_back(rgba(1, 1, 1, 0.5f));
					group_translations.push_back(vec3(0, 0, 0));
					group_rotations.push_back(vec4(0, 0, 0, 1));
				}
			}

			// define colors from hls
			for (unsigned i = 0; i < group_colors.size(); ++i) {
				float hue = float(i) / group_colors.size();
				group_colors[i] = cgv::media::color<float, cgv::media::HLS, cgv::media::OPACITY>(hue, 0.5f, 1.0f, 0.5f);
			}
			nr_points = points.size(); //data.cells.size();
			nr_groups = uint32_t(group_colors.size());
			nr_time_steps = uint32_t(times.size());
			std::cout << "read " << file_name << " with "
				<< points.size() << " points, " << times.size() << " time steps, and " << group_colors.size() << " ids and "
				<< nr_attributes << " attributes" << std::endl;
			//// concatenate
			//write_file(fn);
			return true;
		}
		else
		{
			return false;
		}
	}
	bool read_data_ascii(const std::string& file_name, float max_time_step = std::numeric_limits<float>::max())
	{
		std::string fn = cgv::utils::file::drop_extension(file_name) + ".cae";
		//if (cgv::utils::file::exists(fn))
		//	if (read_file(fn))
		//		return true;

		//std::string content;
		//if (!cgv::utils::file::read(file_name, content, true))
		//	return false;
		//std::vector<cgv::utils::line> lines;
		//cgv::utils::split_to_lines(content, lines);
		//cgv::utils::progression pr("parse lines", lines.size(), 20);
		//// extract the attribute names from first line
		//std::vector<cgv::utils::token> toks;
		//cgv::utils::split_to_tokens(lines.begin()->begin, lines.begin()->end, toks, "");
		//size_t ti = 0;
		//for (auto tok : toks) {
		//	if (*tok.begin == '"')
		//		++tok.begin;
		//	if (tok.end > tok.begin && tok.end[-1] == '"')
		//		--tok.end;
		//	std::string name = to_string(tok);
		//	switch (ti) {
		//	case 0:
		//		std::cout << "name of time: " << name << std::endl;
		//		break;
		//	case 1:
		//		std::cout << "name of cell id: " << name << std::endl;
		//		break;
		//	case 2:
		//		std::cout << "name of boundary: " << name << std::endl;
		//		break;
		//	case 5:
		//	case 6:
		//	case 7:
		//		std::cout << "name of " << std::string("xyz")[ti - 5] << ": " << name << std::endl;
		//		break;
		//	default:
		//		attr_names.push_back(name);
		//		std::cout << "name of attr: " << name << std::endl;
		//		break;
		//	}
		//	++ti;
		//}
		attr_names.push_back("b");
		nr_attributes = uint32_t(attr_names.size());

		model_parser parser(file_name, types, group_indices, points);

		// reset simulation data
		//data = {};

		//logger_parser parser(file_name);

		//parser.read_header({ "time", "cell.id", "cell.type", "l.x", "l.y", "l.z", "b"});

		double time, x, y, z, b;
		int id, type;

		//while (parser.read_row(time, id, type, x, y, z, b))
		//{
		//	if (type == 0) // medium is ignored in code, maybe should just require user to uncheck medium in Morpheus logging? 
		//		continue;

		//	//std::vector<double> a(nr_attributes, 0.0);
		//	//uint32_t ai;

		//	if (time >= max_time_step)
		//		break;

		//	if (times.empty() || times.back() != float(time)) {
		//		//std::cout << "t = " << t << " max = " << max_time_step << std::endl;
		//		time_step_start.push_back(points.size());
		//		times.push_back(float(time));
		//	}

		//	rgba color(0.f, 0.f, 0.f, 0.5f);
		//	//data.cells.push_back(cell_vis(time, id, type, x, y, z, b, color));

		//	points.push_back(vec3(float(x), float(y), float(z)));
		//	//rgba col(float(a[0]), float(a[1]), float(a[2]), 0.5f);
		//	rgba col(0.f, 0.f, 0.f, 0.5f);
		//	colors.push_back(col);
		//	group_indices.push_back(id);
		//	// cells of the same type should have the same color
		//	while (id >= int(group_colors.size())) {
		//		group_colors.push_back(rgba(1, 1, 1, 0.5f));
		//		group_translations.push_back(vec3(0, 0, 0));
		//		group_rotations.push_back(vec4(0, 0, 0, 1));
		//	}
		//}

		time_step_start.push_back(0);
		times.push_back(0.f);

		for (auto id : group_indices)
		{
			rgba col(0.f, 0.f, 0.f, 0.5f);
			colors.push_back(col);
			// cells of the same type should have the same color
			while (id >= int(group_colors.size())) {
				group_colors.push_back(rgba(1, 1, 1, 0.5f));
				group_translations.push_back(vec3(0, 0, 0));
				group_rotations.push_back(vec4(0, 0, 0, 1));
			}
		}

		// define colors from hls
		for (unsigned i = 0; i < group_colors.size(); ++i) {
			float hue = float(i) / group_colors.size();
			group_colors[i] = cgv::media::color<float, cgv::media::HLS, cgv::media::OPACITY>(hue, 0.5f, 1.0f, 0.5f);
		}
		nr_points = points.size(); //data.cells.size();
		nr_groups = uint32_t(group_colors.size());
		nr_time_steps = uint32_t(times.size());
		std::cout << "read " << file_name << " with "
			<< points.size() << " points, " << times.size() << " time steps, and " << group_colors.size() << " ids and "
			<< nr_attributes << " attributes" << std::endl;
		//// concatenate 
		//write_file(fn);
		return true;
	}
	bool open_ooc(const std::string& file_name)
	{
		if (!read_header(file_name + ".cae"))
			return false;
		construct_group_information(nr_groups);
		ooc_file_name = file_name;
		return true;
	}
	bool read_ooc_time_step(const std::string& file_name, unsigned ti)
	{
		//if (!read_time_step(file_name, ti, data.points, group_indices, attr_values))
		//	return false;
		update_colors();
		return true;
	}
	static bool read_log_file_header(const std::string& file_name, std::vector<std::string>& attr_names, uint32_t& nr_attributes, FILE** fpp = 0)
	{
		FILE* fp = fopen(file_name.c_str(), "ra");
		if (!fp)
			return false;
		char buffer[1024];
		if (fgets(buffer, 1024, fp)) {
			cgv::utils::token l(buffer, buffer+strlen(buffer));
			// split line into tokens
			std::vector<cgv::utils::token> toks;
			cgv::utils::split_to_tokens(l.begin, l.end, toks, "", true, "\"", "\"");

			// in case of first line, extract nr_attributs and attribute names
			nr_attributes = uint32_t(toks.size() - 5);
			for (unsigned ai = 0; ai < int(nr_attributes); ++ai) {
				auto tok = toks[ai + 5];
				// prune away quotes if necessary
				if (tok.begin[0] == '"') {
					++tok.begin;
					if (tok.end > tok.begin && tok.end[-1] == '"')
						--tok.end;
				}
				attr_names.push_back(to_string(tok));
			}
			if (fpp)
				*fpp = fp;
			else
				fclose(fp);
			return true;
		}
		fclose(fp);
		return false;
	}
	// convert a csv log file to a cae file based on previously to be configured format member
	//bool convert_log_to_cae(const std::string& log_fn, const std::string& cae_fn)
	//{	
	//	// extract attribute names from log file
	//	FILE* log_fp;
	//	if (!read_log_file_header(log_fn, attr_names, nr_attributes, &log_fp))
	//		return false;

	//	nr_time_steps = 0;
	//	times.clear();
	//	time_step_start.clear();
	//	nr_points = 0;
	//	nr_groups = 0;
	//	data.cells.clear();
	//	group_indices.clear();
	//	attr_values.clear();

	//	if (!write_header(cae_fn))
	//		return false;

	//	std::vector<float> A(nr_attributes);
	//	char buffer[1024];
	//	int li = 1;
	//	float last_time = 0.0f;
	//	while (fgets(buffer, 1024, log_fp)) {
	//		cgv::utils::token l(buffer, buffer + strlen(buffer));
	//		// iterate over all lines and print progression in console in 20 steps
	//		int ai, max_id = 0;
	//		// split line into tokens
	//		std::vector<cgv::utils::token> toks;
	//		cgv::utils::split_to_tokens(l.begin, l.end, toks, "", true, "\"", "\"");
	//		// check for correct number of values
	//		if (toks.size() != 5 + nr_attributes)
	//			continue;

	//		// extract values from tokens and ignore line if not all values could be extracted
	//		int x, y, z, id;
	//		double t;
	//		if (!cgv::utils::is_double(toks[0].begin, toks[0].end, t))
	//			continue;
	//		if (!cgv::utils::is_integer(toks[1].begin, toks[1].end, x))
	//			continue;
	//		if (!cgv::utils::is_integer(toks[2].begin, toks[2].end, y))
	//			continue;
	//		if (!cgv::utils::is_integer(toks[3].begin, toks[3].end, z))
	//			continue;
	//		if (!cgv::utils::is_integer(toks[4].begin, toks[4].end, id))
	//			continue;
	//		bool attr_success = true;
	//		for (ai = 0; ai < int(nr_attributes); ++ai) {
	//			double a;
	//			if (!cgv::utils::is_double(toks[5].begin, toks[5].end, a)) {
	//				attr_success = false;
	//				break;
	//			}
	//			A[ai] = float(a);
	//		}
	//		if (!attr_success)
	//			continue;

	//		// extract time step information
	//		if (times.empty())
	//			last_time = float(t);
	//		else if (last_time != float(t)) {
	//			append_time_step(cae_fn, last_time, data.points, group_indices, attr_values);
	//			last_time = float(t);
	//			data.points.clear();
	//			group_indices.clear();
	//			attr_values.clear();
	//		}

	//		data.points.push_back(vec3(float(x), float(y), float(z)));
	//		group_indices.push_back(id);
	//		attr_values.insert(attr_values.end(), A.begin(), A.end());

	//		// keep track of line and point count and maximum cell id
	//		++li;
	//	}
	//	append_time_step(cae_fn, last_time, data.points, group_indices, attr_values);
	//	std::cout << "converted " << log_fn << " with " << nr_points << " points, " << times.size() << " time steps, and " << nr_groups << " ids." << std::endl;
	//	construct_group_information(nr_groups);
	//	ooc_file_name = cae_fn;
	//	time_step = uint32_t(times.size() - 1);
	//	post_redraw();
	//	return true;
	//}
	void step()
	{
		++time_step;
		if (time_step >= times.size())
			time_step = 0;
		on_set(&time_step);
	}
	void step_back()
	{
		if (time_step == 0)
			time_step = uint32_t(times.size() - 1);
		else
			--time_step;
		on_set(&time_step);
	}
	void timer_event(double, double dt)
	{
		if (trigger[1] > 0.1f) {
			time_delta += 60 * trigger[1] * dt;
			if (time_delta >= 1.0f) {
				time_step += (int)time_delta;
				time_delta -= (int)time_delta;
				if (time_step >= times.size())
					time_step = times.size() - 1;
				on_set(&time_step);
			}
		}
		if (animate) {
			step();
		}
	}
	/// define format and texture filters in constructor
	vr_ca_vis() : cgv::base::node("vr_ca_vis"), box_extent(1, 1, 1)
	{
		animate = false;
		sort_points = false;
		//sort_points = true;
		use_boxes = true;
		blend = false;
		ooc_mode = false;
		opacity = 1.0f;
		scale = 1.0f;
		trigger[0] = trigger[1] = 0.0f;
		time_delta = 0.0;
		clr_scale = cgv::media::ColorScale::CS_TEMPERATURE;
		sphere_style.radius = 0.5f;
		sphere_style.use_group_color = true;
		box_style.map_color_to_material = cgv::render::CM_COLOR;
		box_style.use_group_color = true;
		time_step = 0;
		connect(cgv::gui::get_animation_trigger().shoot, this, &vr_ca_vis::timer_event);

		vr_view_ptr = 0;

		srs.radius = 0.005f;
	}
	std::string get_type_name() const
	{
		return "vr_ca_vis";
	}
	void on_set(void* member_ptr)
	{
		if (member_ptr == &time_step) {
			cgv::type::uint64_type beg = (ooc_mode ? 0 : time_step_start[time_step]);
			cgv::type::uint64_type end = (ooc_mode ? points.size() : ((time_step + 1 == time_step_start.size()) ? points.size() : time_step_start[time_step + 1]));

			std::vector<vec3> p(points.begin() + beg, points.begin() + end);

			//if (time_step >= vertexGrids.size())
			//{
			//	HashGrid<Point> grid;
			//	BuildHashGridFromVertices(p, grid, vec3(1, 1, 1));

			//	vertexGrids.push_back(grid);
			//}

			if (ooc_mode && !ooc_file_name.empty())
				read_ooc_time_step(ooc_file_name, time_step);
		}
		if (member_ptr == &dir_name) {
			read_data_dir_ascii(dir_name);			

			time_step = 0;
			on_set(&time_step);
			post_recreate_gui();
		}
		//if (member_ptr == &file_name) {
		//	//read_data_ascii(file_name, 0.1f);
		//	read_data_ascii(file_name);			

		//	time_step = 0;
		//	on_set(&time_step);
		//	post_recreate_gui();
		//}
		if (member_ptr == &opacity) {
			if (use_boxes ? box_style.use_group_color : sphere_style.use_group_color)
				for (auto& c : group_colors) {
					c.alpha() = opacity;
					update_member(&c);
				}
			else
				for (auto& c : colors)
					c.alpha() = cgv::type::uint8_type(255*opacity);
		}
		update_member(member_ptr);
		post_redraw();
	}
	bool self_reflect(cgv::reflect::reflection_handler& rh)
	{
		return
			rh.reflect_member("animate", animate) &&
			rh.reflect_member("file_name", file_name) &&
			rh.reflect_member("dir_name", dir_name);
	}
	bool init(cgv::render::context& ctx)
	{
		ctx.set_bg_clr_idx(4);

		if (!b_renderer.init(ctx))
			return false;
		b_renderer.set_render_style(box_style);

		if (!s_renderer.init(ctx))
			return false;
		s_renderer.set_render_style(sphere_style);

		if (view_ptr = find_view_as_node()) {
			view_ptr->set_focus(vec3(0.5f, 0.5f, 0.5f));
			s_renderer.set_y_view_angle(float(view_ptr->get_y_view_angle()));

			//view_ptr->set_eye_keep_view_angle(dvec3(0, 4, -4));
			// if the view points to a vr_view_interactor
			vr_view_ptr = dynamic_cast<vr_view_interactor*>(view_ptr);
			if (vr_view_ptr) {
				// configure vr event processing
				vr_view_ptr->set_event_type_flags(
					cgv::gui::VREventTypeFlags(
						cgv::gui::VRE_DEVICE +
						cgv::gui::VRE_STATUS +
						cgv::gui::VRE_KEY +
						cgv::gui::VRE_ONE_AXIS_GENERATES_KEY +
						cgv::gui::VRE_ONE_AXIS +
						cgv::gui::VRE_TWO_AXES +
						cgv::gui::VRE_TWO_AXES_GENERATES_DPAD +
						cgv::gui::VRE_POSE
					));
				vr_view_ptr->enable_vr_event_debugging(false);
				// configure vr rendering
				vr_view_ptr->draw_action_zone(false);
				vr_view_ptr->draw_vr_kits(true);
				vr_view_ptr->enable_blit_vr_views(true);
				vr_view_ptr->set_blit_vr_view_width(200);
			}
		}
		return true;
	}
	void clear(cgv::render::context& ctx)
	{
		s_renderer.clear(ctx);
		b_renderer.clear(ctx);
	}
	void set_group_geometry(cgv::render::context& ctx, cgv::render::group_renderer& sr)
	{
		sr.set_group_colors(ctx, group_colors);
		sr.set_group_translations(ctx, group_translations);
		sr.set_group_rotations(ctx, group_rotations);
	}
	void set_geometry(cgv::render::context& ctx, cgv::render::group_renderer& sr)
	{
		if (ps.size() > 0)
		sr.set_position_array(ctx, ps);
		sr.set_color_array(ctx, colors);
		sr.set_group_index_array(ctx, group_indices);
	}
	void draw_points(unsigned ti) // draw voxels
	{
		cgv::type::uint64_type beg = (ooc_mode? 0 : time_step_start[ti]);
		cgv::type::uint64_type end = (ooc_mode? points.size() : ((ti + 1 == time_step_start.size()) ? points.size() : time_step_start[ti+1]));
		cgv::type::uint64_type cnt = end - beg;
		if (sort_points) {
			indices.resize(size_t(cnt));
			for (unsigned i = 0; i < indices.size(); ++i)
				indices[i] = unsigned(i+beg);
			struct sort_pred {
				const std::vector<vec3>& points;
				const vec3& view_dir;
				bool operator () (GLint i, GLint j) const {
					return dot(points[i], view_dir) > dot(points[j], view_dir);
				}
				sort_pred(const std::vector<vec3>& _points, const vec3& _view_dir) : points(_points), view_dir(_view_dir) {}
			};
			vec3 view_dir = view_ptr->get_view_dir();

			std::sort(indices.begin(), indices.end(), sort_pred(points, view_dir));
			glDrawElements(GL_POINTS, GLsizei(cnt), GL_UNSIGNED_INT, &indices.front());
		}
		else if (ps.size() > 0)
			glDrawArrays(GL_POINTS, GLsizei(0), GLsizei(ps.size()));
			//glDrawArrays(GL_POINTS, GLsizei(beg), GLsizei(cnt));
	}
	void draw_box(cgv::render::context& ctx)
	{
		ctx.ref_default_shader_program().enable(ctx);
		ctx.set_color(rgb(1.0f, 0.0f, 1.0f));
		glLineWidth(2.0f);
		box3 B(vec3(0.0f), vec3(1.0f));
		ctx.tesselate_box(B, false, true);
		ctx.ref_default_shader_program().disable(ctx);
	}
	void draw(cgv::render::context& ctx)
	{
		ctx.push_modelview_matrix();
		if (get_scene_ptr() && get_scene_ptr()->is_coordsystem_valid(vr::vr_scene::CS_TABLE))
			ctx.mul_modelview_matrix(pose4(get_scene_ptr()->get_coordsystem(vr::vr_scene::CS_TABLE)));
		ctx.mul_modelview_matrix(cgv::math::scale4<double>(dvec3(scale)));
		ctx.mul_modelview_matrix(cgv::math::translate4<double>(dvec3(-0.5,0.0,-0.5)));

		draw_box(ctx);

		//if (!points.empty()) {
		if (!ps.empty()) {
			ctx.mul_modelview_matrix(cgv::math::scale4<double>(dvec3(0.01)));
			if (blend) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}
			b_renderer.set_extent(ctx, box_extent);
			cgv::render::group_renderer& renderer = use_boxes ? static_cast<cgv::render::group_renderer&>(b_renderer) : s_renderer;
			set_group_geometry(ctx, renderer);
			set_geometry(ctx, renderer);
			renderer.validate_and_enable(ctx);
			draw_points(time_step);
			renderer.disable(ctx);
			if (blend)
				glDisable(GL_BLEND);
		}
		ctx.pop_modelview_matrix();

		if (vr_view_ptr) {
			//if ((!shared_texture && camera_tex.is_created()) || (shared_texture && camera_tex_id != -1)) {
			//	if (vr_view_ptr->get_rendered_vr_kit() != 0 && vr_view_ptr->get_rendered_vr_kit() == vr_view_ptr->get_current_vr_kit()) {
			//		int eye = vr_view_ptr->get_rendered_eye();

			//		// compute billboard
			//		dvec3 vd = vr_view_ptr->get_view_dir_of_kit();
			//		dvec3 y = vr_view_ptr->get_view_up_dir_of_kit();
			//		dvec3 x = normalize(cross(vd, y));
			//		y = normalize(cross(x, vd));
			//		x *= camera_aspect * background_extent * background_distance;
			//		y *= background_extent * background_distance;
			//		vd *= background_distance;
			//		dvec3 eye_pos = vr_view_ptr->get_eye_of_kit(eye);
			//		std::vector<vec3> P;
			//		std::vector<vec2> T;
			//		P.push_back(eye_pos + vd - x - y);
			//		P.push_back(eye_pos + vd + x - y);
			//		P.push_back(eye_pos + vd - x + y);
			//		P.push_back(eye_pos + vd + x + y);
			//		double v_offset = 0.5 * (1 - eye);
			//		T.push_back(dvec2(0.0, 0.5 + v_offset));
			//		T.push_back(dvec2(1.0, 0.5 + v_offset));
			//		T.push_back(dvec2(0.0, v_offset));
			//		T.push_back(dvec2(1.0, v_offset));

			//		cgv::render::shader_program& prog = seethrough;
			//		cgv::render::attribute_array_binding::set_global_attribute_array(ctx, prog.get_position_index(), P);
			//		cgv::render::attribute_array_binding::set_global_attribute_array(ctx, prog.get_texcoord_index(), T);
			//		cgv::render::attribute_array_binding::enable_global_array(ctx, prog.get_position_index());
			//		cgv::render::attribute_array_binding::enable_global_array(ctx, prog.get_texcoord_index());

			//		GLint active_texture, texture_binding;
			//		if (shared_texture) {
			//			glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture);
			//			glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture_binding);
			//			glActiveTexture(GL_TEXTURE0);
			//			glBindTexture(GL_TEXTURE_2D, camera_tex_id);
			//		}
			//		else
			//			camera_tex.enable(ctx, 0);
			//		prog.set_uniform(ctx, "texture", 0);
			//		prog.set_uniform(ctx, "seethrough_gamma", seethrough_gamma);
			//		prog.set_uniform(ctx, "use_matrix", use_matrix);

			//		// use of convenience function
			//		vr::configure_seethrough_shader_program(ctx, prog, frame_width, frame_height,
			//			vr_view_ptr->get_current_vr_kit(), *vr_view_ptr->get_current_vr_state(),
			//			0.01f, 2 * background_distance, eye, undistorted);

			//		/* equivalent detailed code relies on more knowledge on program parameters
			//		mat4 TM = vr::get_texture_transform(vr_view_ptr->get_current_vr_kit(), *vr_view_ptr->get_current_vr_state(), 0.01f, 2 * background_distance, eye, undistorted);
			//		prog.set_uniform(ctx, "texture_matrix", TM);
			//		prog.set_uniform(ctx, "extent_texcrd", extent_texcrd);
			//		prog.set_uniform(ctx, "frame_split", frame_split);
			//		prog.set_uniform(ctx, "center_left", center_left);
			//		prog.set_uniform(ctx, "center_right", center_right);
			//		prog.set_uniform(ctx, "eye", eye);
			//		*/
			//		prog.enable(ctx);
			//		ctx.set_color(rgba(1, 1, 1, 1));

			//		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);


			//		prog.disable(ctx);

			//		if (shared_texture) {
			//			glActiveTexture(active_texture);
			//			glBindTexture(GL_TEXTURE_2D, texture_binding);
			//		}
			//		else
			//			camera_tex.disable(ctx);

			//		cgv::render::attribute_array_binding::disable_global_array(ctx, prog.get_position_index());
			//		cgv::render::attribute_array_binding::disable_global_array(ctx, prog.get_texcoord_index());
			//	}
			//}

			if (vr_view_ptr) {
				std::vector<vec3> P;
				std::vector<float> R;
				std::vector<rgb> C;
				const vr::vr_kit_state* state_ptr = vr_view_ptr->get_current_vr_state();
				if (state_ptr) {
					for (int ci = 0; ci < 4; ++ci)
						if (state_ptr->controller[ci].status == vr::VRS_TRACKED) {
							vec3 ray_origin, ray_direction;
							state_ptr->controller[ci].put_ray(&ray_origin(0), &ray_direction(0));														//P.push_back(ray_origin);
							P.push_back(ray_origin);
							R.push_back(0.002f);
							P.push_back(ray_origin + ray_length * ray_direction);
							R.push_back(0.003f);
							rgb c(float(1 - ci), 0.5f * (int)state[ci], float(ci));
							C.push_back(c);
							C.push_back(c);
						}
				}
				if (P.size() > 0) {
					//auto& cr = cgv::render::ref_rounded_cone_renderer(ctx);
					//cr.set_render_style(cone_style);
					//cr.set_eye_position(vr_view_ptr->get_eye_of_kit());
					//cr.set_position_array(ctx, P);
					//cr.set_color_array(ctx, C);
					//cr.set_radius_array(ctx, R);
					//if (!cr.render(ctx, 0, P.size())) {
						cgv::render::shader_program& prog = ctx.ref_default_shader_program();
						int pi = prog.get_position_index();
						int ci = prog.get_color_index();
						cgv::render::attribute_array_binding::set_global_attribute_array(ctx, pi, P);
						cgv::render::attribute_array_binding::enable_global_array(ctx, pi);
						cgv::render::attribute_array_binding::set_global_attribute_array(ctx, ci, C);
						cgv::render::attribute_array_binding::enable_global_array(ctx, ci);
						glLineWidth(3);
						prog.enable(ctx);
						glDrawArrays(GL_LINES, 0, (GLsizei)P.size());
						prog.disable(ctx);
						cgv::render::attribute_array_binding::disable_global_array(ctx, pi);
						cgv::render::attribute_array_binding::disable_global_array(ctx, ci);
						glLineWidth(1);
					//}
				}
			}
		}

		// draw intersection points
		if (!intersection_points.empty()) {
			auto& sr = cgv::render::ref_sphere_renderer(ctx);
			sr.set_position_array(ctx, intersection_points);
			sr.set_color_array(ctx, intersection_colors);
			sr.set_render_style(srs);
			sr.render(ctx, 0, intersection_points.size());
		}
	}
	bool handle(cgv::gui::event& e)
	{
		// check if vr event flag is not set and don't process events in this case
		if ((e.get_flags() & cgv::gui::EF_VR) != 0) {
			switch (e.get_kind()) {
			case cgv::gui::EID_KEY:
			{
				cgv::gui::vr_key_event& vrke = static_cast<cgv::gui::vr_key_event&>(e);
				if (vrke.get_action() != cgv::gui::KA_RELEASE) {
					switch (vrke.get_key()) {
					case vr::VR_DPAD_RIGHT:
						if (vrke.get_controller_index() == 0) { // only left controller
							if (time_step + 1 < times.size()) {
								time_step += 1;
								on_set(&time_step);
							}
							return true;
						}
						return false;
					case vr::VR_DPAD_LEFT:
						if (time_step > 0) {
							time_step -= 1;
							on_set(&time_step);
						}
						return true;
					}
				}
				break;
			}
			case cgv::gui::EID_THROTTLE:
			{
				cgv::gui::vr_throttle_event& vrte = static_cast<cgv::gui::vr_throttle_event&>(e);
				trigger[vrte.get_controller_index()] = vrte.get_value();
				update_member(&trigger[vrte.get_controller_index()]);
				return true;
			}
			case cgv::gui::EID_POSE:
			{
				cgv::gui::vr_pose_event& vrpe = static_cast<cgv::gui::vr_pose_event&>(e);
				// check for controller pose events
				int ci = vrpe.get_trackable_index();
				if (ci != -1) {
					if (state[ci] == IS_GRAB) {
						// in grab mode apply relative transformation to grabbed boxes

						// get previous and current controller position
						vec3 last_pos = vrpe.get_last_position();
						vec3 pos = vrpe.get_position();
						// get rotation from previous to current orientation
						// this is the current orientation matrix times the
						// inverse (or transpose) of last orientation matrix:
						// vrpe.get_orientation()*transpose(vrpe.get_last_orientation())
						mat3 rotation = vrpe.get_rotation_matrix();
						// iterate intersection points of current controller
						for (size_t i = 0; i < intersection_points.size(); ++i) {
							if (intersection_controller_indices[i] != ci)
								continue;
							// extract box index
							unsigned bi = intersection_box_indices[i];
							// update translation with position change and rotation
							//movable_box_translations[bi] =
							//	rotation * (movable_box_translations[bi] - last_pos) + pos;
							// update orientation with rotation, note that quaternions
							// need to be multiplied in oposite order. In case of matrices
							// one would write box_orientation_matrix *= rotation
							//movable_box_rotations[bi] = quat(rotation) * movable_box_rotations[bi];
							// update intersection points
							intersection_points[i] = rotation * (intersection_points[i] - last_pos) + pos;
						}
					}
					else {// not grab
						// clear intersections of current controller 
						size_t i = 0;
						while (i < intersection_points.size()) {
							if (intersection_controller_indices[i] == ci) {
								intersection_points.erase(intersection_points.begin() + i);
								intersection_colors.erase(intersection_colors.begin() + i);
								intersection_box_indices.erase(intersection_box_indices.begin() + i);
								intersection_controller_indices.erase(intersection_controller_indices.begin() + i);
							}
							else
								++i;
						}

						// compute intersections
						vec3 origin, direction;
						vrpe.get_state().controller[ci].put_ray(&origin(0), &direction(0));

						//a = direction.x();
						//b = direction.y();
						//c = direction.z();
						//d = (a * origin.x()) + (b * origin.y()) + (c * origin.z());
						//d = -d;

						direction.normalize();

						compute_intersections(origin, direction, ci, ci == 0 ? rgb(1, 0, 0) : rgb(0, 0, 1));
						//label_outofdate = true;

						if (intersection_points.size() > 0)
						{
							std::cout << "ok" << std::endl;
						}

						// update state based on whether we have found at least 
						// one intersection with controller ray
						//if (intersection_points.size() == i)
						//	state[ci] = IS_NONE;
						//else
						//	if (state[ci] == IS_NONE)
						//		state[ci] = IS_OVER;
					}
					post_redraw();
				}
				return true;
			}
			return false;
			}
		}

		if (e.get_kind() != cgv::gui::EID_KEY)
			return false;
		cgv::gui::key_event& ke = static_cast<cgv::gui::key_event&>(e);
		if (ke.get_action() == cgv::gui::KA_RELEASE)
			return false;
		switch (ke.get_key()) {
		case cgv::gui::KEY_Right:
			step();
			return true;
		case cgv::gui::KEY_Left:
			step_back();
			return true;
		case cgv::gui::KEY_Home:
			time_step = 0;
			on_set(&time_step);
			return true;
		case cgv::gui::KEY_End:
			time_step = uint32_t(times.size()-1);
			on_set(&time_step);
			return true;
		}
		return false;
	}
	void stream_help(std::ostream & os)
	{
		os << "vr_ca_vis: use <left>, <right>, <home>, and <end> to navigate time\n";
	}
	void create_gui()
	{
		add_decorator("bio math", "heading");
		//add_view("tigger_0", trigger[0]);
		//add_view("tigger_1", trigger[1]);
		add_member_control(this, "animate", animate, "toggle", "shortcut='A'");
		add_member_control(this, "scale", scale, "value_slider", "min=0.0001;step=0.00001;max=10;log=true;ticks=true");
		add_member_control(this, "time_step", time_step, "value_slider", "min=0;ticks=true");
		if (find_control(time_step))
			find_control(time_step)->set("max", times.size() - 1);
		add_member_control(this, "blend", blend, "toggle", "shortcut='B'");
		add_member_control(this, "sort_points", sort_points, "toggle", "shortcut='S'");
		add_member_control(this, "opacity", opacity, "value_slider", "min=0;max=1;ticks=true");
		add_member_control(this, "color_scale", clr_scale, "dropdown", "enums='Temperature,Hue,Hue+Luminance'");
		//rgba color;
		//add_member_control(this, "color", color);
		add_member_control(this, "use_boxes", use_boxes, "toggle", "shortcut='M'");
		add_gui("box_extent", box_extent, "", "gui_type='value_slider';options='min=0;max=1'");

		if (begin_tree_node("geometry and groups", time_step, true)) {
			align("\a");
			if (begin_tree_node("group colors", group_colors, false)) {
				align("\a");
				for (unsigned i = 0; i < group_colors.size(); ++i) {
					add_member_control(this, std::string("C") + cgv::utils::to_string(i), reinterpret_cast<rgba&>(group_colors[i]));
				}
				align("\b");
				end_tree_node(group_colors);
			}
			if (begin_tree_node("group transformations", group_translations, false)) {
				align("\a");
				for (unsigned i = 0; i < group_translations.size(); ++i) {
					add_gui(std::string("T") + cgv::utils::to_string(i), group_translations[i]);
					add_gui(std::string("Q") + cgv::utils::to_string(i), group_rotations[i], "direction");
				}
				align("\b");
				end_tree_node(group_translations);
			}
			align("\b");
			end_tree_node(time_step);
		}
		if (begin_tree_node("Sphere Rendering", sphere_style, false)) {
			align("\a");
			add_gui("sphere_style", sphere_style);
			align("\b");
			end_tree_node(sphere_style);
		}
		if (begin_tree_node("Box Rendering", box_style, false)) {
			align("\a");
			add_gui("box_style", box_style);
			align("\b");
			end_tree_node(box_style);
		}
	}
	/// compute intersection points of controller ray with movable boxes
	void compute_intersections(const vec3& origin, const vec3& direction, int ci, const rgb& color)
	{
		ps.clear();

		for (size_t i = 0; i < time_step_start[time_step]; ++i) {
		//for (size_t i = 0; i < points.size(); ++i) {
			vec4 p(points[i], 1.f);

			mat4 mat;
			mat.identity();

			if (get_scene_ptr() && get_scene_ptr()->is_coordsystem_valid(vr::vr_scene::CS_TABLE))
				mat *= pose4(get_scene_ptr()->get_coordsystem(vr::vr_scene::CS_TABLE));
			mat *= cgv::math::scale4<double>(dvec3(scale));
			mat *= cgv::math::translate4<double>(dvec3(-0.5, 0.0, -0.5));
			mat *= cgv::math::scale4<double>(dvec3(0.01));

			vec4 point4(mat * p);
			vec3 point(point4 / point4.w());

			//float sign = (a * point.x()) + (b * point.y()) + (c * point.z()) + d;

			float sign = dot(direction, point) - origin.length();

			if (sign < 0)
			{
				ps.push_back(points[i]);
			}
			else
			{ 
				//std::cout << "wow" << std::endl;
			}

			//if (cgv::media::ray_axis_aligned_box_intersection(
			//	origin_box_i, direction_box_i,
			//	points[i],
			//	t_result, p_result, n_result, 0.000001f)) {

			//	// transform result back to world coordinates
			//	//group_rotations[i].rotate(p_result);
			//	p_result += group_translations[i];
			//	//group_rotations[i].rotate(n_result);

			//	// store intersection information
			//	intersection_points.push_back(p_result);
			//	intersection_colors.push_back(color);
			//	intersection_box_indices.push_back((int)i);
			//	intersection_controller_indices.push_back(ci);
			//}
		}
	}
};

cgv::base::object_registration<vr_ca_vis> or_vr_ca_vis("vr_ca_vis");