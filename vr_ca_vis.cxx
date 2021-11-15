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

#include <vr/vr_state.h>
#include <vr/vr_kit.h>
#include <vr/vr_driver.h>
#include <cg_vr/vr_events.h>
#include <vr_view_interactor.h>
#include <plugins/vr_lab/vr_tool.h>

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
protected:
	// stored data
	std::vector<vec3> points;
	std::vector<uint32_t> group_indices;
	std::vector<float> attr_values;
	std::vector<rgba8> colors;
	std::vector<vec3> extents;

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
			colors[i] = cgv::media::color_scale(v, clr_scale);
		}
	}

public:
	bool read_file(const std::string& file_name)
	{
		if (!read(file_name, points, group_indices, attr_values))
			return false;
		construct_group_information(nr_groups);
		update_colors();
		return true;
	}
	bool write_file(const std::string& file_name) const
	{
		return write(file_name, points, group_indices, attr_values);
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
	bool read_data_ascii(const std::string& file_name, float max_time_step = std::numeric_limits<float>::max())
	{
		// TODO
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
		//nr_attributes = uint32_t(attr_names.size());
		// TODO 
		nr_attributes = uint32_t(3);

		logger_parser parser(file_name);

		std::vector<std::string> headers = { "time", "cell.id", "cell.center.x", "cell.center.y", "cell.center.z" };
		parser.read_header(headers);

		double time, x, y, z, extent_x, extent_y, extent_z;
		int id;

		// TODO get extent from logger.csv
		extent_x = extent_y = extent_z = 2;

		while (parser.read_row(time, id, x, y, z))
		{
			std::vector<double> a(nr_attributes, 0.0);
			uint32_t ai;

			if (time >= max_time_step)
				break;

			if (times.empty() || times.back() != float(time)) {
				//std::cout << "t = " << t << " max = " << max_time_step << std::endl;
				time_step_start.push_back(points.size());
				times.push_back(float(time));
			}
			extents.push_back(vec3(float(extent_x), float(extent_y), float(extent_z)));
			points.push_back(vec3(float(x), float(y), float(z)));
			for (ai = 0; ai < nr_attributes; ++ai)
				attr_values.push_back(float(a[ai]));
			//rgba col(float(a[0]), float(a[1]), float(a[2]), 0.5f);
			rgba col(0.f, 0.f, 0.f, 0.5f);
			colors.push_back(col);
			group_indices.push_back(id);
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
		nr_points = points.size();
		nr_groups = uint32_t(group_colors.size());
		nr_time_steps = uint32_t(times.size());
		std::cout << "read " << file_name << " with "
			<< extents.size() << " extents, " << points.size() << " points, " << times.size() << " time steps, and " << group_colors.size() << " ids and "
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
		if (!read_time_step(file_name, ti, points, group_indices, attr_values))
			return false;
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
	bool convert_log_to_cae(const std::string& log_fn, const std::string& cae_fn)
	{	
		// extract attribute names from log file
		FILE* log_fp;
		if (!read_log_file_header(log_fn, attr_names, nr_attributes, &log_fp))
			return false;

		nr_time_steps = 0;
		times.clear();
		time_step_start.clear();
		nr_points = 0;
		nr_groups = 0;
		points.clear();
		group_indices.clear();
		attr_values.clear();

		if (!write_header(cae_fn))
			return false;

		std::vector<float> A(nr_attributes);
		char buffer[1024];
		int li = 1;
		float last_time = 0.0f;
		while (fgets(buffer, 1024, log_fp)) {
			cgv::utils::token l(buffer, buffer + strlen(buffer));
			// iterate over all lines and print progression in console in 20 steps
			int ai, max_id = 0;
			// split line into tokens
			std::vector<cgv::utils::token> toks;
			cgv::utils::split_to_tokens(l.begin, l.end, toks, "", true, "\"", "\"");
			// check for correct number of values
			if (toks.size() != 5 + nr_attributes)
				continue;

			// extract values from tokens and ignore line if not all values could be extracted
			int x, y, z, id;
			double t;
			if (!cgv::utils::is_double(toks[0].begin, toks[0].end, t))
				continue;
			if (!cgv::utils::is_integer(toks[1].begin, toks[1].end, x))
				continue;
			if (!cgv::utils::is_integer(toks[2].begin, toks[2].end, y))
				continue;
			if (!cgv::utils::is_integer(toks[3].begin, toks[3].end, z))
				continue;
			if (!cgv::utils::is_integer(toks[4].begin, toks[4].end, id))
				continue;
			bool attr_success = true;
			for (ai = 0; ai < int(nr_attributes); ++ai) {
				double a;
				if (!cgv::utils::is_double(toks[5].begin, toks[5].end, a)) {
					attr_success = false;
					break;
				}
				A[ai] = float(a);
			}
			if (!attr_success)
				continue;

			// extract time step information
			if (times.empty())
				last_time = float(t);
			else if (last_time != float(t)) {
				append_time_step(cae_fn, last_time, points, group_indices, attr_values);
				last_time = float(t);
				points.clear();
				group_indices.clear();
				attr_values.clear();
			}

			points.push_back(vec3(float(x), float(y), float(z)));
			group_indices.push_back(id);
			attr_values.insert(attr_values.end(), A.begin(), A.end());

			// keep track of line and point count and maximum cell id
			++li;
		}
		append_time_step(cae_fn, last_time, points, group_indices, attr_values);
		std::cout << "converted " << log_fn << " with " << nr_points << " points, " << times.size() << " time steps, and " << nr_groups << " ids." << std::endl;
		construct_group_information(nr_groups);
		ooc_file_name = cae_fn;
		time_step = uint32_t(times.size() - 1);
		post_redraw();
		return true;
	}
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
	}
	std::string get_type_name() const
	{
		return "vr_ca_vis";
	}
	void on_set(void* member_ptr)
	{
		if (member_ptr == &time_step) {
			if (ooc_mode && !ooc_file_name.empty())
				read_ooc_time_step(ooc_file_name, time_step);
		}
		if (member_ptr == &file_name) {
			//read_data_ascii(file_name, 0.1f);
			read_data_ascii(file_name);			

			time_step = 0;
			on_set(&time_step);
			post_recreate_gui();
		}
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
			rh.reflect_member("file_name", file_name);
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
		sr.set_position_array(ctx, points);
		sr.set_color_array(ctx, colors);
		sr.set_group_index_array(ctx, group_indices);
	}
	void draw_points(unsigned ti)
	{
		cgv::type::uint64_type beg = (ooc_mode?0:time_step_start[ti]);
		cgv::type::uint64_type end = (ooc_mode? points.size():((ti + 1 == time_step_start.size()) ? points.size() : time_step_start[ti+1]));
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
		else
			glDrawArrays(GL_POINTS, GLsizei(beg), GLsizei(cnt));
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
		if (!points.empty()) {
			ctx.mul_modelview_matrix(cgv::math::scale4<double>(dvec3(0.01)));
			if (blend) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}
			//b_renderer.set_extent(ctx, box_extent);
			b_renderer.set_extent_array<vec3>(ctx, extents);
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
};

cgv::base::object_registration<vr_ca_vis> or_vr_ca_vis("vr_ca_vis");