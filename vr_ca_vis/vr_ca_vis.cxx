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
#include <cgv_gl/surfel_renderer.h>
#include <cgv_gl/gl/gl.h>
#include <random>
#include <unordered_set>
#include "endian.h"
#include "cae_file_format.h"
#include "cells_container.h"
#include "clipping_planes_container.h"
#include "clipping_planes_bag.h"
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

#include <cg_nui/focusable.h>
#include <cg_nui/transforming.h>

class vr_ca_vis :
	public cgv::base::group,
	public cgv::render::drawable,
	public cgv::nui::focusable,
	public cgv::nui::transforming,
	public cgv::gui::event_handler,
	public cgv::gui::provider,
	public vr::vr_tool,
	public cae::binary_file,
	public cells_container_listener,
	public clipping_planes_container_listener,
	public clipping_planes_bag_listener
{
public:
	typedef cgv::render::drawable::vec2 vec2;
	typedef cgv::render::drawable::vec3 vec3;
	typedef cgv::render::drawable::vec4 vec4;

	// different possible object states
	enum class state_enum {
		idle,
		close,
		pointed,
		grabbed,
		triggered
	};

protected:
	// different interaction states for the controllers
	enum InteractionState {
		IS_NONE,
		IS_OVER,
		IS_GRAB
	};

	// hid with focus on object
	cgv::nui::hid_identifier hid_id;
	// state of object
	state_enum state = state_enum::idle;

	cells_container_ptr cells_ctr;
	clipping_planes_container_ptr clipping_planes_ctr;
	clipping_planes_bag_ptr clipping_planes_b;

	// state of current interaction with boxes for all controllers
	//InteractionState state[4];

	// keep reference to vr_view_interactor
	vr_view_interactor* vr_view_ptr;

	//simulation_data data;

	// stored data
	std::unordered_set<std::string> types;
	std::vector<uint32_t> ids;
	std::vector<vec3> points;
	std::vector<uint32_t> group_indices;
	std::vector<float> attr_values;
	std::vector<rgba8> colors;

	// clipping plane
	bool clipping_plane_grabbed;
	// index of temporary clipping plane in clipping planes container
	int temp_clipping_plane_idx = -1;
	// clipping planes that are sent to cells container, to be used by clipped_box geometry shader, in the form of ax + by + cz + d = 0
	std::vector<vec4> shader_clipping_planes;
	// clipping planes that are sent to clipping planes container, in the form of origin and direction

	// extent
	ivec3 extent;
	dvec3 extent_scale;

	// visible data
	std::vector<vec3> visible_points;
	std::vector<uint32_t> visible_types;
	std::vector<rgb> visible_colors;

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
	// first frame in current time step
	bool first_frame;

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
	cgv::render::surfel_render_style surf_rs;

	// Help GUI
	/// label index to show statistics
	uint32_t li_clipping_plane_stats, li_cell_stats;
	/// visibility of statistics label
	bool li_clipping_plane_visible, li_cell_visible;
	/// background color of statistics label
	rgba stats_bgclr;

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
			float v = scale * (attr_values[i * nr_attributes + selected_attr] - offset);
			//float v = scale * (data.cells[i * nr_attributes + selected_attr].get_attr() - offset);
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

				size_t previous_num_points = points.size();

				model_parser parser(file_name, extent, ids, group_indices, points, types);
				extent_scale = dvec3(1.0 / extent.x(), 1.0 / extent.y(), 1.0 / extent.z());

				std::cout << "read " << file_name << " with " << points.size() - previous_num_points << " points" << std::endl;
			}

			visible_points.reserve(points.size());
			visible_types.reserve(points.size());
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
			}
		}

		return file_names.size() > 0;
	}
	bool read_data_dir_ascii(const std::string& dir_name)
	{
		attr_names.push_back("b");
		nr_attributes = uint32_t(attr_names.size());

		if (read_xml_dir(dir_name) || read_gz_dir(dir_name) && read_xml_dir(dir_name))
		{
			for (auto id : group_indices)
			{
				rgba col(0.f, 0.f, 0.f, 0.5f);
				colors.push_back(col);
				// cells of the same type should have the same color
				//while (id >= int(group_colors.size())) {
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
			cgv::utils::token l(buffer, buffer + strlen(buffer));
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
			double d = 60.0 * (double)trigger[1] * dt;
			time_delta += (float)d;
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
	vr_ca_vis() : cgv::base::group("vr_ca_vis"), box_extent(1, 1, 1)
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
		first_frame = true;
		connect(cgv::gui::get_animation_trigger().shoot, this, &vr_ca_vis::timer_event);

		vr_view_ptr = 0;

		surf_rs.illumination_mode = cgv::render::IlluminationMode::IM_OFF;
		surf_rs.culling_mode = cgv::render::CullingMode::CM_OFF;
		surf_rs.measure_point_size_in_pixel = false;
		surf_rs.blend_points = true;
		surf_rs.point_size = 1.8f;
		surf_rs.percentual_halo_width = 5.0f;
		surf_rs.surface_color = rgba(0, 0.8f, 1.0f);
		surf_rs.material.set_transparency(0.75f);
		surf_rs.halo_color = rgba(0, 0.8f, 1.0f, 0.8f);

		cells_ctr = new cells_container(this, "Cells");
		append_child(cells_ctr);

		clipping_planes_ctr = new clipping_planes_container(this, "Clipping Planes");
		append_child(clipping_planes_ctr);

		// when put behind the head (z+) the grabbing does not work
		clipping_planes_b = new clipping_planes_bag(this, "Clipping Planes Bag", vec3(0.f, -1.5f, -1.f));
		append_child(clipping_planes_b);

		// clipping plane
		clipping_plane_grabbed = false;

		// GUI
		li_clipping_plane_stats = li_cell_stats = -1;
		li_clipping_plane_visible = li_cell_visible = false;
		stats_bgclr = rgba(0.8f, 0.6f, 0.0f, 0.6f);
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

			if (ooc_mode && !ooc_file_name.empty())
				read_ooc_time_step(ooc_file_name, time_step);

			first_frame = true;
		}
		if (member_ptr == &dir_name) {
			read_data_dir_ascii(dir_name);

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
					c.alpha() = cgv::type::uint8_type(255 * opacity);
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
		cgv::render::ref_surfel_renderer(ctx, 1);
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
	void init_frame(cgv::render::context& ctx)
	{
		vr::vr_scene* scene_ptr = get_scene_ptr();
		if (!scene_ptr)
			return;

		if (first_frame)
		{
			first_frame = false;

			compute_visible_points();

			cells_ctr->set_cells(visible_points, visible_colors);
		}

		if (clipping_plane_grabbed)
			compute_clipping_planes();

		cells_ctr->set_scale_matrix(cgv::math::scale4<double>(extent_scale));
		cells_ctr->set_clipping_planes(shader_clipping_planes);

		mat4 mv;
		mv.identity();

		if (scene_ptr->is_coordsystem_valid(vr::vr_scene::CS_TABLE))
			mv *= pose4(scene_ptr->get_coordsystem(vr::vr_scene::CS_TABLE));
		mv *= cgv::math::scale4<double>(dvec3(scale));
		mv *= cgv::math::translate4<double>(dvec3(-0.5, 0.0, -0.5));

		clipping_planes_b->set_modelview_matrix(mv);

		mat4 h;
		h.identity();

		if (scene_ptr->is_coordsystem_valid(vr::vr_scene::CS_HEAD))
			h *= pose4(scene_ptr->get_coordsystem(vr::vr_scene::CS_HEAD));

		//h.set_col(0, vec4(1, 0, 0, 0));
		//h.set_col(1, vec4(0, 1, 0, 0));
		//h.set_col(2, vec4(0, 0, 1, 0));

		clipping_planes_b->set_head_matrix(h);
	}
	void clear(cgv::render::context& ctx)
	{
		s_renderer.clear(ctx);
		b_renderer.clear(ctx);
		cgv::render::ref_surfel_renderer(ctx, -1);
	}
	void set_group_geometry(cgv::render::context& ctx, cgv::render::group_renderer& sr)
	{
		sr.set_group_colors(ctx, group_colors);
		sr.set_group_translations(ctx, group_translations);
		sr.set_group_rotations(ctx, group_rotations);
	}
	void set_geometry(cgv::render::context& ctx, cgv::render::group_renderer& sr)
	{
		if (!visible_points.empty())
			sr.set_position_array(ctx, visible_points);
		//if (!points.empty())
		//	sr.set_position_array(ctx, points);
		sr.set_color_array(ctx, colors);
		//sr.set_group_index_array(ctx, group_indices);
		sr.set_group_index_array(ctx, visible_types);
	}
	//void draw_points(unsigned ti) // draw voxels
	//{
	//	cgv::type::uint64_type beg = (ooc_mode? 0 : time_step_start[ti]);
	//	cgv::type::uint64_type end = (ooc_mode? points.size() : ((ti + 1 == time_step_start.size()) ? points.size() : time_step_start[ti+1]));
	//	cgv::type::uint64_type cnt = end - beg;
	//	if (sort_points) {
	//		indices.resize(size_t(cnt));
	//		for (unsigned i = 0; i < indices.size(); ++i)
	//			indices[i] = unsigned(i + beg);
	//		struct sort_pred {
	//			const std::vector<vec3>& points;
	//			const vec3& view_dir;
	//			bool operator () (GLint i, GLint j) const {
	//				return dot(points[i], view_dir) > dot(points[j], view_dir);
	//			}
	//			sort_pred(const std::vector<vec3>& _points, const vec3& _view_dir) : points(_points), view_dir(_view_dir) {}
	//		};
	//		vec3 view_dir = view_ptr->get_view_dir();

	//		std::sort(indices.begin(), indices.end(), sort_pred(points, view_dir));
	//		glDrawElements(GL_POINTS, GLsizei(cnt), GL_UNSIGNED_INT, &indices.front());
	//	}
	//	else if (visible_points.size() > 0)
	//		glDrawArrays(GL_POINTS, GLsizei(0), GLsizei(visible_points.size()));
	//	//else if (!points.empty())
	//	//	glDrawArrays(GL_POINTS, GLsizei(beg), GLsizei(cnt));
	//}
	void draw_box(cgv::render::context& ctx)
	{
		ctx.ref_default_shader_program().enable(ctx);
		ctx.set_color(rgb(1.0f, 0.0f, 1.0f));
		glLineWidth(2.0f);
		box3 B(vec3(0.0f), vec3(1.0f));
		ctx.tesselate_box(B, false, true);
		ctx.ref_default_shader_program().disable(ctx);
	}
	void draw_circle(cgv::render::context& ctx, const vec3& position, const vec3& normal)
	{
		auto& sr = cgv::render::ref_surfel_renderer(ctx);
		sr.set_reference_point_size(1.0f);
		sr.set_render_style(surf_rs);
		sr.set_position(ctx, position);
		sr.set_normal(ctx, normal);
		sr.render(ctx, 0, 1);
	}
	void finish_frame(cgv::render::context& ctx)
	{
		vr::vr_scene* scene_ptr = get_scene_ptr();
		if (!scene_ptr)
			return;

		// TODO: draw infinite clipping plane (as a disc) only when outside of wireframe box
		if (clipping_plane_grabbed && temp_clipping_plane_idx == -1 && scene_ptr->is_coordsystem_valid(vr::vr_scene::CS_RIGHT_CONTROLLER))
		{
			ctx.push_modelview_matrix();
			ctx.mul_modelview_matrix(pose4(scene_ptr->get_coordsystem(vr::vr_scene::CS_RIGHT_CONTROLLER)));
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			draw_circle(ctx, vec3(0.0f), vec3(0, 0, 1));
			glDisable(GL_BLEND);
			ctx.pop_modelview_matrix();
		}

		// draw information about clipping planes
		if (li_clipping_plane_stats == -1) {
			li_clipping_plane_stats = scene_ptr->add_label(get_clipping_planes_stats(), stats_bgclr);
			scene_ptr->fix_label_size(li_clipping_plane_stats);
			scene_ptr->place_label(li_clipping_plane_stats, vec3(0.f, 0.15f, -0.03f), quat(vec3(1, 0, 0), -1.5f), vr::vr_scene::CS_LEFT_CONTROLLER, vr::vr_scene::LA_TOP);
		}

		// draw information about cell that is being grabbed
		if (li_cell_stats == -1) {
			li_cell_stats = scene_ptr->add_label("Cell with id none and type none", stats_bgclr);
			scene_ptr->fix_label_size(li_cell_stats);
			scene_ptr->place_label(li_cell_stats, vec3(0.05f, 0.f, 0.f), quat(vec3(1, 0, 0), -1.5f), vr::vr_scene::CS_LEFT_CONTROLLER, vr::vr_scene::LA_RIGHT);
		}

		if (li_clipping_plane_visible)
			scene_ptr->show_label(li_clipping_plane_stats);
		else
			scene_ptr->hide_label(li_clipping_plane_stats);

		if (li_cell_visible)
			scene_ptr->show_label(li_cell_stats);
		else
			scene_ptr->hide_label(li_cell_stats);
	}
	void draw(cgv::render::context& ctx)
	{
		vr::vr_scene* scene_ptr = get_scene_ptr();
		if (!scene_ptr)
			return;

		// draw wireframe box and cells on top of the table
		if (!scene_ptr->is_coordsystem_valid(vr::vr_scene::CS_TABLE))
			return;

		mat4 model_transform(pose4(scene_ptr->get_coordsystem(vr::vr_scene::CS_TABLE)));

		model_transform *= cgv::math::scale4<double>(dvec3(scale));
		model_transform *= cgv::math::translate4<double>(dvec3(-0.5, 0.0, -0.5));

		set_model_transform(model_transform);

		ctx.push_modelview_matrix();
		ctx.mul_modelview_matrix(model_transform);

		draw_box(ctx);

		//ctx.mul_modelview_matrix(cgv::math::scale4<double>(extent_scale));

		if (blend) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
	}
	void finish_draw(cgv::render::context& ctx)
	{
		if (blend)
			glDisable(GL_BLEND);

		ctx.pop_modelview_matrix();
	}
	bool focus_change(cgv::nui::focus_change_action action, cgv::nui::refocus_action rfa, const cgv::nui::focus_demand& demand, const cgv::gui::event& e, const cgv::nui::dispatch_info& dis_info)
	{
		return false;
	}
	bool handle(cgv::gui::event& e)
	{
		// check if vr event flag is not set and don't process events in this case
		if ((e.get_flags() & cgv::gui::EF_VR) == 0)
			return false;
		switch (e.get_kind()) {
		case cgv::gui::EID_KEY:
		{
			cgv::gui::vr_key_event& vrke = static_cast<cgv::gui::vr_key_event&>(e);
			if (vrke.get_controller_index() == 0) { // only left controller
				if (vrke.get_action() != cgv::gui::KA_RELEASE) {
					switch (vrke.get_key()) {
						// put current clipping plane permanently
					case vr::VR_DPAD_LEFT:
						set_clipping_plane();
						return true;
						// forward
					case vr::VR_DPAD_RIGHT:
						if (time_step + 1 < times.size())
							time_step += 1;
						else
							time_step = 0;

						on_set(&time_step);
						return true;
					}
				}
				else {
					switch (vrke.get_key()) {
						// toggle help
					case vr::VR_MENU:
						li_clipping_plane_visible = !li_clipping_plane_visible;
						return true;
					}
				}
			}
			else if (vrke.get_controller_index() == 1) { // only right controller
				if (vrke.get_action() != cgv::gui::KA_RELEASE) {
					switch (vrke.get_key()) {
						// remove clipping plane
					case vr::VR_DPAD_LEFT:
						if (temp_clipping_plane_idx > -1)
						{
							clipping_planes_ctr->delete_clipping_plane(temp_clipping_plane_idx);

							shader_clipping_planes.erase(shader_clipping_planes.begin() + temp_clipping_plane_idx);
						}

						temp_clipping_plane_idx = -1;

						clipping_plane_grabbed = false;
						return true;
					}
				}
			}
			break;
		}
		}
		return false;
	}
	bool handle(const cgv::gui::event& e, const cgv::nui::dispatch_info& dis_info, cgv::nui::focus_request& request)
	{
		return false;
	}
	void stream_help(std::ostream& os)
	{
		os << "vr_ca_vis: use <left>, <right>, <home>, and <end> to navigate time\n";
		os << "vr_ca_vis: select draw <M>ode, press vr pad or trigger to draw, grip to change color" << std::endl;
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
		if (begin_tree_node("Surfel Rendering", surf_rs, false)) {
			align("\a");
			add_gui("surfel_style", surf_rs);
			align("\b");
			end_tree_node(surf_rs);
		}
		if (begin_tree_node("Box Rendering", box_style, false)) {
			align("\a");
			add_gui("box_style", box_style);
			align("\b");
			end_tree_node(box_style);
		}
		//if (begin_tree_node("Cells", cells_ctr)) {
		//	align("\a");
		//	inline_object_gui(cells_ctr);
		//	align("\b");
		//	end_tree_node(cells_ctr);
		//}
		if (begin_tree_node("Clipping Planes", clipping_planes_ctr)) {
			align("\a");
			inline_object_gui(clipping_planes_ctr);
			align("\b");
			end_tree_node(clipping_planes_ctr);
		}
	}
	/// compute visible points, i.e. points that are past the cutting plane in the direction of the controller
	void compute_visible_points()
	{
		if (false)
		{
			vr_view_interactor* vr_view_ptr = get_view_ptr();
			if (!vr_view_ptr)
				return;

			const vr::vr_kit_state* state_ptr = vr_view_ptr->get_current_vr_state();
			if (!state_ptr)
				return;

			visible_points.clear();
			visible_types.clear();
			visible_colors.clear();

			vec3 control_direction = -reinterpret_cast<const vec3&>(state_ptr->controller[1].pose[6]);
			vec3 control_origin = reinterpret_cast<const vec3&>(state_ptr->controller[1].pose[9]);

			mat4 mat;
			mat.identity();

			if (get_scene_ptr() && get_scene_ptr()->is_coordsystem_valid(vr::vr_scene::CS_TABLE))
				mat *= pose4(get_scene_ptr()->get_coordsystem(vr::vr_scene::CS_TABLE));
			mat *= cgv::math::scale4<double>(dvec3(scale));
			mat *= cgv::math::translate4<double>(dvec3(-0.5, 0.0, -0.5));
			mat *= cgv::math::scale4<double>(extent_scale);

			//mat = inv(mat);

			//vec4 origin4(mat * vec4(origin, 1.f)), direction4(mat * vec4(direction, 1.f));
			//vec3 o(origin4 / origin4.w()), d(direction4 / direction4.w());
			//d.normalize();

			for (uint64_t i = time_step_start[time_step], j = (time_step + 1 == time_step_start.size() ? points.size() : time_step_start[time_step + 1]); i < j; ++i)
			{
				vec4 point4(mat * vec4(points[i], 1.f));
				vec3 point(point4 / point4.w());

				float sign = dot(control_direction, point - control_origin);

				//float sign = dot(d, points[i] - o);

				if (sign >= 0)
				{
					visible_points.push_back(points[i]);
					visible_types.push_back(group_indices[i]);
					visible_colors.push_back(group_colors[group_indices[i]]);
				}
			}
		}
		else
		{
			visible_points.clear();
			visible_types.clear();
			visible_colors.clear();

			uint64_t start = time_step_start[time_step];
			uint64_t end = (time_step + 1 == time_step_start.size() ? points.size() : time_step_start[time_step + 1]);

			visible_points.insert(visible_points.end(), points.begin() + start, points.begin() + end);
			visible_types.insert(visible_types.end(), group_indices.begin() + start, group_indices.begin() + end);

			for (size_t i = 0; i < visible_types.size(); ++i)
			{
				visible_colors.push_back(group_colors[group_indices[i]]);
			}
		}
	}
	std::string get_clipping_planes_stats()
	{
		size_t num_clipping_planes = clipped_box_renderer::MAX_CLIPPING_PLANES - clipping_planes_ctr->get_num_clipping_planes();

		if (temp_clipping_plane_idx > -1)
			num_clipping_planes += 1;

		return "Clipping Planes: " + std::to_string(num_clipping_planes) + "/" + std::to_string(clipped_box_renderer::MAX_CLIPPING_PLANES);
	}
	void compute_clipping_planes()
	{
		if (temp_clipping_plane_idx > -1)
		{
			clipping_planes_ctr->delete_clipping_plane(temp_clipping_plane_idx);

			shader_clipping_planes.erase(shader_clipping_planes.begin() + temp_clipping_plane_idx);
		}

		temp_clipping_plane_idx = -1;

		if (get_scene_ptr() && get_scene_ptr()->is_coordsystem_valid(vr::vr_scene::CS_RIGHT_CONTROLLER))
		{
			vr_view_interactor* vr_view_ptr = get_view_ptr();
			if (!vr_view_ptr)
				return;

			const vr::vr_kit_state* state_ptr = vr_view_ptr->get_current_vr_state();
			if (!state_ptr)
				return;

			vec3 control_direction = -reinterpret_cast<const vec3&>(state_ptr->controller[1].pose[6]);
			// control_origin have to be transformed to local space of the cells
			vec3 control_origin = reinterpret_cast<const vec3&>(state_ptr->controller[1].pose[9]);

			mat4 mat;
			mat.identity();

			if (get_scene_ptr() && get_scene_ptr()->is_coordsystem_valid(vr::vr_scene::CS_TABLE))
				mat *= pose4(get_scene_ptr()->get_coordsystem(vr::vr_scene::CS_TABLE));
			mat *= cgv::math::scale4<double>(dvec3(scale));
			mat *= cgv::math::translate4<double>(dvec3(-0.5, 0.0, -0.5));

			vec4 o4(inv(mat) * vec4(control_origin, 1.f));
			vec3 o(o4 / o4.w());

			// TODO: check if plane actually intersects the box, otherwise draw the infinite cutting plane
			if (o.x() < 0.f || o.x() > 1.f || o.y() < 0.f || o.y() > 1.f || o.z() < 0.f || o.z() > 1.f)
				return;

			clipping_planes_ctr->create_clipping_plane(o, control_direction);
			post_recreate_gui();

			mat *= cgv::math::scale4<double>(extent_scale);

			vec4 origin4(inv(mat) * vec4(control_origin, 1.f));
			vec3 origin(origin4 / origin4.w());

			temp_clipping_plane_idx = shader_clipping_planes.size();
			shader_clipping_planes.emplace_back(control_direction, -dot(origin, control_direction));
		}

		post_recreate_gui();
	}
	void set_clipping_plane()
	{
		// uncomment for temporary fix until grabbing the bag is fixed
		//if (temp_clipping_plane_idx > -1)
		//{
		//	clipping_planes_ctr->copy_clipping_plane(temp_clipping_plane_idx);

		//	shader_clipping_planes.emplace_back(shader_clipping_planes[temp_clipping_plane_idx]);
		//}

		temp_clipping_plane_idx = -1;

		if (li_clipping_plane_stats != -1) {
			vr::vr_scene* scene_ptr = get_scene_ptr();
			if (scene_ptr)
				scene_ptr->update_label_text(li_clipping_plane_stats, get_clipping_planes_stats());
		}

		// comment for temporary fix until grabbing the bag is fixed
		clipping_plane_grabbed = false;
	}
	void reset_clipping_plane()
	{
		if (temp_clipping_plane_idx == -1)
		{
			clipping_planes_ctr->clear_clipping_planes();

			shader_clipping_planes.clear();
		}
		else
		{
			clipping_planes_ctr->copy_clipping_plane(temp_clipping_plane_idx);
			clipping_planes_ctr->delete_clipping_plane(0, shader_clipping_planes.size());

			shader_clipping_planes.assign(shader_clipping_planes.begin() + temp_clipping_plane_idx, shader_clipping_planes.begin() + temp_clipping_plane_idx + 1);

			temp_clipping_plane_idx = 0;
		}
	}
	// listener for cell grab event inside box
	void on_cell_grabbed(size_t index)
	{
		if (li_cell_stats != -1) {
			vr::vr_scene* scene_ptr = get_scene_ptr();
			if (scene_ptr) {
				scene_ptr->update_label_text(li_cell_stats, "Cell with id " + std::to_string(ids[index]) + " and type " + std::to_string(visible_types[index]));
				scene_ptr->fix_label_size(li_cell_stats);
			}
		}

		li_cell_visible = true;
	}
	// listener for existing clipping plane grab event inside box
	void container_on_clipping_plane_grabbed(size_t index)
	{

	}
	// listener for clipping plane grab event inside bag
	void bag_on_clipping_plane_grabbed()
	{
		if (clipping_plane_grabbed || clipping_planes_ctr->get_num_clipping_planes() == clipped_box_renderer::MAX_CLIPPING_PLANES)
			return;

		clipping_plane_grabbed = true;
	}
};

cgv::base::object_registration<vr_ca_vis> or_vr_ca_vis("vr_ca_vis");