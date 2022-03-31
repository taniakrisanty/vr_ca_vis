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
#include "tool_bag.h"
#include "cells_container.h"
#include "clipping_planes_container.h"
#include "model_parser.h"
#include "gzip_inflater.h"
#include <functional>
#include <limits>
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
	public tool_bag_listener,
	public clipping_planes_container_listener
{
public:
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

	enum class tool_enum {
		none,
		clipping_plane,
		gun,
		torch
	};

protected:
	// hid with focus on object
	cgv::nui::hid_identifier hid_id;
	// state of object
	state_enum state = state_enum::idle;
	// active tool 
	tool_enum tool = tool_enum::none;

	cells_container_ptr cells_ctr;
	tool_bag_ptr tool_b;
	clipping_planes_container_ptr clipping_planes_ctr;

	// keep reference to vr_view_interactor
	vr_view_interactor* vr_view_ptr;

	// cell data
	std::vector<cell> cells;

	//std::unordered_map<std::string, cell_type> cell_types;
	//std::unordered_set<std::string> types;
	//std::vector<uint32_t> ids;
	//std::vector<vec3> points;
	//std::vector<uint32_t> group_indices;
	//std::vector<float> attr_values;
	//std::vector<rgba8> colors;

	// previous inverse model transform
	// if it changes (e.g. the table is rotated) the quaternion for rotating control direction must be recalculated
	mat4 prev_inverse_model_transform;
	quat control_direction_rotation;

	// previous position and direction of the right controller
	vec3 prev_control_origin;
	vec3 prev_control_direction;

	// clipping plane
	// index of temporary clipping plane in clipping planes container
	int temp_clipping_plane_idx = -1;
	size_t selected_clipping_plane_idx = SIZE_MAX;

	// torch
	bool burn_outside = false;

	// extent
	ivec3 extent;
	dvec3 extent_scale;

	// counter how many times the controller not pointing to any cell
	const int max_cell_unselected_counter = 10;
	int cell_unselected_counter;
	size_t selected_cell_idx = SIZE_MAX;
	size_t selected_node_idx = SIZE_MAX;
	rgba selected_cell_color = rgba();

	size_t selected_cell_type = SIZE_MAX;

	// attributes
	uint32_t selected_attr;

	// scaling factor for dataset
	double scale;

	// current time step
	uint32_t current_time_step;
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
	float animation_speed;
	float opacity;
	float trigger[2];
	float time_delta;
	//cgv::media::ColorScale clr_scale;
	// render objects
	//std::vector<unsigned> indices;
	cgv::render::view* view_ptr;
	cgv::render::sphere_render_style sphere_style;
	cgv::render::sphere_renderer s_renderer;
	cgv::render::surface_render_style box_style;
	cgv::render::box_renderer b_renderer;
	cgv::render::surfel_render_style surf_rs;

	// Help GUI
	/// label index to show statistics
	uint32_t li_tool_stats, li_selected_cell_stats;
	/// visibility of statistics label
	bool li_tool_visible, li_selected_cell_visible;
	/// how long this label has been visible
	float li_tool_time_delta, li_selected_cell_time_delta;
	/// background color of statistics label
	rgba stats_bgclr;

	//void construct_group_information(unsigned nr_groups)
	//{
	//	group_colors.resize(nr_groups);
	//	group_translations.resize(nr_groups, vec3(0, 0, 0));
	//	group_rotations.resize(nr_groups, vec4(0, 0, 0, 1));
	//	// define colors from hls
	//	for (unsigned i = 0; i < nr_groups; ++i) {
	//		float hue = float(i) / nr_groups;
	//		group_colors[i] = cgv::media::color<float, cgv::media::HLS, cgv::media::OPACITY>(hue, 0.5f, 1.0f, 0.5f);
	//	}
	//}
	// update colors from attribute values
	//void update_colors()
	//{
	//	if (colors.size() != cells.size())
	//		colors.resize(cells.size());

	//	float offset = 0.0;
	//	float scale = 1.0f;
	//	if (((format.flags & cae::FF_ATTRIBUTE_RANGES) != 0) && attribute_ranges.size() == nr_attributes) {
	//		offset = attribute_ranges[selected_attr][0];
	//		scale = 1.0f / (attribute_ranges[selected_attr][1] - attribute_ranges[selected_attr][0]);
	//	}
	//	for (size_t i = 0; i < colors.size(); ++i) {
	//		float v = scale * (attr_values[i * nr_attributes + selected_attr] - offset);
	//		//float v = scale * (data.cells[i * nr_attributes + selected_attr].get_attr() - offset);
	//		colors[i] = cgv::media::color_scale(v, clr_scale);
	//	}
	//}

public:
	bool read_file(const std::string& file_name)
	{
		//if (!read(file_name, cells))
		//	return false;

		//construct_group_information(nr_groups);
		//update_colors();
		//return true;
		return false;
	}
	bool write_file(const std::string& file_name) const
	{
		//return write(file_name, points, group_indices, attr_values);
		return false;
	}
	void reset()
	{
		cell_unselected_counter = max_cell_unselected_counter;

		cells_ctr->unset_cells();

		cells.clear();

		cell::types.clear();

		cell::centers.clear();
		cell::nodes.clear();
	}
	bool read_xml_dir(const std::string& dir_name)
	{
		std::vector<std::string> file_names;
		if (cgv::utils::dir::glob(dir_name, file_names, "*.xml"))
		{
			for (const auto& file_name : file_names)
			{
				std::string time_str = cgv::utils::file::get_file_name(file_name);
				time_str = cgv::utils::file::drop_extension(time_str);
				time_str = time_str.substr(time_str.size() - 6);

				int time;
				if (!cgv::utils::is_integer(time_str, time))
					continue;

				if (times.empty() || times.back() != float(time)) {
					//std::cout << "t = " << t << " max = " << max_time_step << std::endl;
					time_step_start.push_back(cells.size());
					times.push_back(float(time));
				}

				size_t previous_cell_count = cells.size();

				model_parser parser(file_name, extent, cells);
				extent_scale = dvec3(1.0) / extent;

				std::cout << "read " << file_name << " with " << cells.size() - previous_cell_count << " cells" << std::endl;
			}
		}

		return file_names.size() > 0;
	}
	bool read_gz_dir(const std::string& dir_name)
	{
		std::vector<std::string> file_names;
		if (cgv::utils::dir::glob(dir_name, file_names, "*.xml.gz"))
		{
			for (const auto& file_name : file_names)
			{
				std::string xml_file_name = cgv::utils::file::drop_extension(file_name);

				gzip_inflater(file_name, xml_file_name);
			}
		}

		return file_names.size() > 0;
	}
	bool read_data_dir_ascii(const std::string& dir_name)
	{
		reset();

		std::string fn = dir_name + ".cae";
		//if (cgv::utils::file::exists(fn))
		//	if (read_file(fn))
		//		return true;

		//attr_names.push_back("b");
		//nr_attributes = uint32_t(attr_names.size());

		if (read_xml_dir(dir_name) || read_gz_dir(dir_name) && read_xml_dir(dir_name))
		{
			//for (auto id : group_indices)
			//{
			//	rgba col(0.f, 0.f, 0.f, 0.5f);
			//	colors.push_back(col);
			//	// cells of the same type should have the same color
			//	while (id >= int(group_colors.size())) {
			//		group_colors.push_back(rgba(1, 1, 1, 0.5f));
			//		group_translations.push_back(vec3(0, 0, 0));
			//		group_rotations.push_back(vec4(0, 0, 0, 1));
			//	}
			//}

			//// define colors from hls
			//for (unsigned i = 0; i < group_colors.size(); ++i) {
			//	float hue = float(i) / group_colors.size();
			//	group_colors[i] = cgv::media::color<float, cgv::media::HLS, cgv::media::OPACITY>(hue, 0.5f, 1.0f, 0.5f);
			//}
			//nr_points = cells.size();
			//nr_groups = uint32_t(group_colors.size());
			//nr_time_steps = uint32_t(times.size());
			//std::cout << "read " << file_name << " with "
			//	<< cells.size() << " points, " << times.size() << " time steps, " << group_colors.size() << " ids, and "
			//	<< nr_attributes << " attributes" << std::endl;
			// concatenate
			//write_file(fn);

			cells_ctr->set_cell_types(cell::types);
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
		//construct_group_information(nr_groups);
		ooc_file_name = file_name;
		return true;
	}
	bool read_ooc_time_step(const std::string& file_name, unsigned ti)
	{
		//if (!read_time_step(file_name, ti, data.points, group_indices, attr_values))
		//	return false;
		//update_colors();
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
			time_delta += animation_speed * (float)dt;
			if (time_delta >= 0.5f) {
				time_delta = 0;
				step();
			}
		}

		if (li_tool_visible) {
			li_tool_time_delta += (float)dt;
			if (li_tool_time_delta >= 5.f)
				li_tool_visible = false;
		}

		//if (li_selected_cell_visible) {
		//	li_cell_time_delta += (float)dt;
		//	if (li_cell_time_delta >= 5.f)
		//		li_selected_cell_visible = false;
		//}
	}
	/// define format and texture filters in constructor
	vr_ca_vis() : cgv::base::group("vr_ca_vis"), box_extent(1, 1, 1)
	{
		animate = false;
		animation_speed = 1.f;
		sort_points = false;
		use_boxes = true;
		blend = false;
		ooc_mode = false;
		opacity = 1.0f;
		scale = 1.0f;
		trigger[0] = trigger[1] = 0.0f; // TODO check
		time_delta = 0.0;
		//clr_scale = cgv::media::ColorScale::CS_TEMPERATURE;
		sphere_style.radius = 0.5f;
		sphere_style.use_group_color = true;
		box_style.map_color_to_material = cgv::render::CM_COLOR;
		box_style.use_group_color = true;
		time_step = 0;
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

		tool_b = new tool_bag(this, "Tool Bag");
		tool_b->add_tool(static_cast<int>(tool_enum::clipping_plane), "Clipping Plane", vec3(0.f, 0.f, 1.f), vec3(1.f, 1.f, 0.1f));
		tool_b->add_tool(static_cast<int>(tool_enum::gun), "Visibility Toggle", vec3(0.75f, -1.5f, 0.25f), vec3(0.5f, 1.f, 1.f));
		tool_b->add_tool(static_cast<int>(tool_enum::torch), "Torch", vec3(-0.75f, -1.5f, 0.25f), vec3(0.5f, 1.f, 1.f));
		append_child(tool_b);

		cells_ctr = new cells_container(this, "Cells");
		append_child(cells_ctr);

		clipping_planes_ctr = new clipping_planes_container(this, "Clipping Planes");
		append_child(clipping_planes_ctr);

		li_tool_stats = li_selected_cell_stats = -1;
		li_tool_visible = li_selected_cell_visible = false;
		stats_bgclr = rgba(0.8f, 0.6f, 0.0f, 0.8f);
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
		if (member_ptr == &dir_name) {
			read_data_dir_ascii(dir_name);
			time_step = 0;
			on_set(&time_step);
			post_recreate_gui();
		}
		//if (member_ptr == &opacity) {
		//	if (use_boxes ? box_style.use_group_color : sphere_style.use_group_color)
		//		for (auto& c : group_colors) {
		//			c.alpha() = opacity;
		//			update_member(&c);
		//		}
		//	else
		//		for (auto& c : colors)
		//			c.alpha() = cgv::type::uint8_type(255 * opacity);
		//}
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
		if (current_time_step != time_step) {
			current_time_step = time_step;

			compute_visible_points();
		}
	}
	void clear(cgv::render::context& ctx)
	{
		s_renderer.clear(ctx);
		b_renderer.clear(ctx);
		cgv::render::ref_surfel_renderer(ctx, -1);
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
		ctx.set_color(rgb(0.5f, 0.5f, 0.5f));
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

		// draw infinite clipping plane (as a disc) only when outside of wireframe box
		if (tool == tool_enum::clipping_plane && temp_clipping_plane_idx == -1 && scene_ptr->is_coordsystem_valid(coordinate_system::right_controller))
		{
			ctx.push_modelview_matrix();
			ctx.mul_modelview_matrix(pose4(scene_ptr->get_coordsystem(coordinate_system::right_controller)));
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			draw_circle(ctx, vec3(0.0f), vec3(0, 0, 1));
			glDisable(GL_BLEND);
			ctx.pop_modelview_matrix();
		}

		if (li_tool_stats != -1)
			if (li_tool_visible)
				scene_ptr->show_label(li_tool_stats);
			else
				scene_ptr->hide_label(li_tool_stats);

		if (li_selected_cell_stats != -1) {
			if (li_selected_cell_visible && cell_unselected_counter < max_cell_unselected_counter)
				scene_ptr->show_label(li_selected_cell_stats);
			else
				scene_ptr->hide_label(li_selected_cell_stats);
		}
	}
	void draw(cgv::render::context& ctx)
	{
		const vr::vr_scene* scene_ptr = get_scene_ptr();
		if (!scene_ptr)
			return;

		// draw wireframe box and cells on top of the table
		if (!scene_ptr->is_coordsystem_valid(coordinate_system::head))
			return;

		mat4 model_transform(pose4(scene_ptr->get_coordsystem(coordinate_system::table)));

		model_transform *= cgv::math::scale4<double>(dvec3(scale));
		model_transform *= cgv::math::translate4<double>(dvec3(-0.5, 0.0, -0.5));

		set_model_transform(model_transform);

		ctx.push_modelview_matrix();
		ctx.mul_modelview_matrix(model_transform);

		draw_box(ctx);

		if (tool == tool_enum::clipping_plane)
			compute_clipping_planes();
		
		cgv::render::shader_program& prog = ctx.ref_surface_shader_program(true);
		prog.set_uniform(ctx, "map_color_to_material", tool == tool_enum::torch ? 3 : 0); // material side front to back or none

		compute_burn();

		cells_ctr->set_inverse_model_transform(get_inverse_model_transform());

		mat4 c;
		c.identity();

		if (scene_ptr->is_coordsystem_valid(coordinate_system::left_controller))
			c *= pose4(scene_ptr->get_coordsystem(coordinate_system::left_controller));

		cells_ctr->set_left_controller_transform(c);

		cells_ctr->set_scale_matrix(cgv::math::scale4<double>(extent_scale));

		tool_b->set_inverse_model_transform(get_inverse_model_transform());

		mat4 h;
		h.identity();

		if (scene_ptr->is_coordsystem_valid(coordinate_system::head))
			h *= pose4(scene_ptr->get_coordsystem(coordinate_system::head));

		tool_b->set_head_transform(h);

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
		if ((e.get_flags() & cgv::gui::EF_VR) != 0) {
			switch (e.get_kind()) {
			case cgv::gui::EID_KEY:
			{
				// disable torch when interaction is detected
				//torch_grabbed = false;

				cgv::gui::vr_key_event& vrke = static_cast<cgv::gui::vr_key_event&>(e);
				//if (vrke.get_key() == vr::VR_MENU) {
				//	if (vrke.get_action() != cgv::gui::KA_RELEASE)
				//		animate = !animate;

				//	return true;
				//}

				// stop animation when interaction is detected
				//animate = false;

				if (vrke.get_controller_index() == 1) { // only right controller
					if (vrke.get_action() == cgv::gui::KA_RELEASE) {
						switch (vrke.get_key()) {
						case vr::VR_INPUT0_TOUCH:
							li_selected_cell_visible = false;
							return true;
						}
					}
					else {
						switch (vrke.get_key()) {
						case vr::VR_DPAD_LEFT:
							switch (tool) {
							case tool_enum::clipping_plane:
								set_clipping_plane(); // put current clipping plane permanently
								break;
							case tool_enum::gun:
								toggle_cell_visibility(); // toggle cell visibility
								break;
							default:
								break;
							}
							return true;
						case vr::VR_DPAD_RIGHT:
							switch (tool) {
							case tool_enum::clipping_plane:
								release_clipping_plane(); // release clipping plane disc
							case tool_enum::gun:
							case tool_enum::torch:
								tool = tool_enum::none;
								break;
							default:
								delete_clipping_plane();
								tool = tool_enum::none;
								break;
							}
							return true;
						case vr::VR_INPUT0_TOUCH:
							li_selected_cell_visible = true;
							return true;
						case vr::VR_INPUT1_TOUCH:
							//peel();
							if (tool == tool_enum::torch)
								burn_outside = !burn_outside;
							return true;
						}
					}
				}
				else if (vrke.get_controller_index() == 0) { // only left controller
					if (vrke.get_action() != cgv::gui::KA_RELEASE) {
						switch (vrke.get_key()) {
						case vr::VR_DPAD_LEFT:
							animate = false;
							on_set(&animate);
							step_back();
							return true;
						case vr::VR_DPAD_RIGHT:
							animate = false;
							on_set(&animate);
							step();
							return true; 
						case vr::VR_INPUT1_TOUCH:
							animate = !animate;
							on_set(&animate);
							return true;
						}
					}
				}
				break;
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
			time_step = uint32_t(times.size() - 1);
			on_set(&time_step);
			return true;
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
		//os << "vr_ca_vis: select draw <M>ode, press vr pad or trigger to draw, grip to change color" << std::endl;
	}
	void create_gui()
	{
		add_decorator("bio math", "heading");
		add_gui("data_dir", dir_name, "directory", "title='Data Directory'");
		add_member_control(this, "animate", animate, "toggle", "shortcut='A'");
		add_member_control(this, "animation_speed", animation_speed, "value_slider", "min=0.1;step=0.00001;max=5;log=true;ticks=true");
		add_member_control(this, "scale", scale, "value_slider", "min=0.0001;step=0.00001;max=10;log=true;ticks=true");
		add_member_control(this, "time_step", time_step, "value_slider", "min=0;ticks=true");
		if (find_control(time_step))
			find_control(time_step)->set("max", times.size() - 1);
		add_member_control(this, "blend", blend, "toggle", "shortcut='B'");
		//add_member_control(this, "sort_points", sort_points, "toggle", "shortcut='S'");
		//add_member_control(this, "opacity", opacity, "value_slider", "min=0;max=1;ticks=true");
		//add_member_control(this, "color_scale", clr_scale, "dropdown", "enums='Temperature,Hue,Hue+Luminance'");
		//rgba color;
		//add_member_control(this, "color", color);
		//add_member_control(this, "use_boxes", use_boxes, "toggle", "shortcut='M'");
		//add_gui("box_extent", box_extent, "", "gui_type='value_slider';options='min=0;max=1'");

		//if (begin_tree_node("geometry and groups", time_step, true)) {
		//	align("\a");
		//	if (begin_tree_node("group colors", group_colors, false)) {
		//		align("\a");
		//		for (unsigned i = 0; i < group_colors.size(); ++i) {
		//			add_member_control(this, std::string("C") + cgv::utils::to_string(i), reinterpret_cast<rgba&>(group_colors[i]));
		//		}
		//		align("\b");
		//		end_tree_node(group_colors);
		//	}
		//	if (begin_tree_node("group transformations", group_translations, false)) {
		//		align("\a");
		//		for (unsigned i = 0; i < group_translations.size(); ++i) {
		//			add_gui(std::string("T") + cgv::utils::to_string(i), group_translations[i]);
		//			add_gui(std::string("Q") + cgv::utils::to_string(i), group_rotations[i], "direction");
		//		}
		//		align("\b");
		//		end_tree_node(group_translations);
		//	}
		//	align("\b");
		//	end_tree_node(time_step);
		//}
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
		//if (begin_tree_node("Box Rendering", box_style, false)) {
		//	align("\a");
		//	add_gui("box_style", box_style);
		//	align("\b");
		//	end_tree_node(box_style);
		//}
		inline_object_gui(tool_b);
		inline_object_gui(cells_ctr);
		inline_object_gui(clipping_planes_ctr);
	}
	void compute_visible_points()
	{
		if (time_step_start.empty())
			return;

		size_t start = time_step_start[time_step];
		size_t end = (time_step + 1 == time_step_start.size() ? cells.size() : time_step_start[time_step + 1]);

		cells_ctr->set_cells(&cells, start, end, extent);
	}
	std::string get_clipping_planes_stats()
	{
		size_t num_clipping_planes = clipped_box_renderer::MAX_CLIPPING_PLANES - clipping_planes_ctr->get_num_clipping_planes();

		if (temp_clipping_plane_idx > -1)
			num_clipping_planes += 1;

		return "Clipping Planes active (" + std::to_string(num_clipping_planes) + " left)";
	}
	void compute_clipping_planes()
	{
		bool control_changed = true, create_clipping_plane = false;

		if (get_view_ptr() && get_view_ptr()->get_current_vr_state())
		{
			vec3 direction = -reinterpret_cast<const vec3&>(get_view_ptr()->get_current_vr_state()->controller[1].pose[6]);
			// control_origin have to be transformed to local space of the cells
			vec3 origin = reinterpret_cast<const vec3&>(get_view_ptr()->get_current_vr_state()->controller[1].pose[9]);

			if (prev_inverse_model_transform != get_inverse_model_transform()) {
				prev_inverse_model_transform = get_inverse_model_transform();

				mat3 rotation;

				for (size_t i = 0; i < 3; ++i) {
					vec3 col(get_inverse_model_transform().col(i));
					col.normalize();

					rotation.set_col(i, col);
				}

				control_direction_rotation = quat(rotation);
			}

			vec4 origin4(get_inverse_model_transform() * origin.lift());
			origin = origin4 / origin4.w();

			control_direction_rotation.rotate(direction);

			if (prev_control_direction != direction || prev_control_origin != origin)
			{
				prev_control_direction = direction;
				prev_control_origin = origin;

				// TODO: check if plane actually intersects the box, otherwise draw the infinite cutting plane
				create_clipping_plane = origin.x() >= 0.f && origin.x() <= 1.f &&
					origin.y() >= 0.f && origin.y() <= 1.f &&
					origin.z() >= 0.f && origin.z() <= 1.f;
			}
			else
			{
				control_changed = false;
			}
		}

		if (control_changed && temp_clipping_plane_idx > -1)
		{
			clipping_planes_ctr->delete_clipping_plane(temp_clipping_plane_idx);

			cells_ctr->delete_clipping_plane(temp_clipping_plane_idx);

			temp_clipping_plane_idx = -1;
		}

		if (create_clipping_plane)
		{
			temp_clipping_plane_idx = clipping_planes_ctr->get_num_clipping_planes();

			clipping_planes_ctr->create_clipping_plane(prev_control_origin, prev_control_direction);

			cells_ctr->create_clipping_plane(prev_control_origin, prev_control_direction);
		}
	}
	void set_clipping_plane()
	{
		temp_clipping_plane_idx = -1;

		tool = tool_enum::none;
	}
	void release_clipping_plane()
	{
		if (temp_clipping_plane_idx != -1) { // we are in the middle of placing a (temporary) clipping plane
			clipping_planes_ctr->delete_clipping_plane(temp_clipping_plane_idx);

			cells_ctr->delete_clipping_plane(temp_clipping_plane_idx);

			temp_clipping_plane_idx = -1;
		}
	}
	void delete_clipping_plane()
	{
		if (selected_clipping_plane_idx == SIZE_MAX) { // we are not grabbing a (permanent) clipping plane
			clipping_planes_ctr->clear_clipping_planes();

			cells_ctr->clear_clipping_planes();
		}
		else { // we are grabbing a (permanent) clipping plane
			size_t index = selected_clipping_plane_idx;
			selected_clipping_plane_idx = SIZE_MAX;

			clipping_planes_ctr->delete_clipping_plane(index);

			cells_ctr->delete_clipping_plane(index);
		}
	}
	void compute_burn()
	{
		if (tool == tool_enum::torch && get_scene_ptr() && get_scene_ptr()->is_coordsystem_valid(coordinate_system::right_controller))
		{
			const vr_view_interactor* vr_view_ptr = get_view_ptr();
			if (!vr_view_ptr) {
				cells_ctr->set_torch(false);
				return;
			}

			const vr::vr_kit_state* state_ptr = vr_view_ptr->get_current_vr_state();
			if (!state_ptr) {
				cells_ctr->set_torch(false);
				return;
			}

			// control_origin have to be transformed to local space of the cells
			vec3 control_origin = reinterpret_cast<const vec3&>(state_ptr->controller[1].pose[9]);

			vec4 origin4(get_inverse_model_transform() * control_origin.lift());
			vec3 origin(origin4 / origin4.w());

			// TODO: check if torch actually intersects the box
			if (origin.x() < 0.f || origin.x() > 1.f || origin.y() < 0.f || origin.y() > 1.f || origin.z() < 0.f || origin.z() > 1.f) {
				cells_ctr->set_torch(false);
				return;
			}

			cells_ctr->set_torch(true, burn_outside, origin, 20.f);
		}
		else
		{
			cells_ctr->set_torch(false);
		}
	}
	void toggle_cell_visibility()
	{
		if (selected_cell_type < SIZE_MAX)
			cells_ctr->toggle_cell_type_visibility(selected_cell_type);
		else if (selected_cell_idx < SIZE_MAX)
			cells_ctr->toggle_cell_visibility(selected_cell_idx);
	}
	void peel()
	{
		if (selected_cell_idx == SIZE_MAX)
			return;

		cells_ctr->peel(selected_cell_idx, selected_node_idx);
	}
	void update_selected_cell_stats()
	{
		vr::vr_scene* scene_ptr = get_scene_ptr();
		if (scene_ptr == NULL)
			return;

		const cell& c = cells[selected_cell_idx];

		const cell_type& ct = std::next(cell::types.begin(), c.type)->second;

		std::ostringstream oss;
		oss << "id " << c.id << "\ntype " << ct.name;
		size_t index = c.properties_start_index;
		for (const auto& p : ct.properties)
			oss << "\n" << p << " " << std::setprecision(1) << cell::properties[index];

		if (li_selected_cell_stats == -1) {
			li_selected_cell_stats = scene_ptr->add_label(oss.str(), stats_bgclr);
			scene_ptr->place_label(li_selected_cell_stats, vec3(0.f, 0.15f, -0.03f), quat(vec3(1, 0, 0), -1.5f), coordinate_system::right_controller, label_alignment::top);
			scene_ptr->fix_label_size(li_selected_cell_stats);
		}
		else {
			scene_ptr->update_label_text(li_selected_cell_stats, oss.str());
		}

		scene_ptr->update_label_background_color(li_selected_cell_stats, selected_cell_color);

		li_selected_cell_time_delta = 0;
	}
	void vibrate(void* hid_kit)
	{
		if (hid_kit) {
			vr::vr_kit* kit_ptr = vr::get_vr_kit(hid_kit);
			if (kit_ptr)
				kit_ptr->set_vibration(1, 0, 50000);
		}
	}

#pragma region cells_container_listener
	uint32_t on_create_label_requested(const std::string& text, const rgba& bgclr, const vec3& position, const quat& rotation)
	{
		vr::vr_scene* scene_ptr = get_scene_ptr();
		if (scene_ptr == NULL)
			return -1;

		uint32_t label = scene_ptr->add_label(text, bgclr);
		scene_ptr->fix_label_size(label);
		scene_ptr->place_label(label, position, rotation, coordinate_system::left_controller);
		scene_ptr->show_label(label);
		return label;
	}

	void on_update_label_requested(uint32_t id, const std::string& text)
	{
		vr::vr_scene* scene_ptr = get_scene_ptr();
		if (scene_ptr == NULL)
			return;

		scene_ptr->update_label_text(id, text);
	}

	void on_cell_type_pointed_at(size_t cell_type)
	{
		selected_cell_type = cell_type;
	}

	// listener for cell point at event
	void on_cell_pointed_at(size_t cell_index, size_t node_index, const rgb& color)
	{
		if (cell_index == SIZE_MAX) {
			if (cell_unselected_counter < max_cell_unselected_counter)
				cell_unselected_counter += 1;
		}
		else {
			cell_unselected_counter = 0;
		}

		if (selected_cell_idx != cell_index || selected_node_idx != node_index) {
			selected_cell_idx = cell_index;
			selected_node_idx = node_index;
			selected_cell_color = rgba(color.R(), color.G(), color.B(), 0.8f);

			if (selected_cell_idx < SIZE_MAX)
				update_selected_cell_stats();
		}
	}
#pragma endregion cells_container_listener

#pragma region tool_bag_listener
	// listener for tool grab event inside bag
	void on_tool_grabbed(int id, void* hid_kit)
	{
		tool_enum _tool = static_cast<tool_enum>(id);

		if (tool == _tool)
			return;

		switch (_tool) {
		case tool_enum::clipping_plane:
			// ignore if no clipping plane left (the maximum is 8)
			if (clipping_planes_ctr->get_num_clipping_planes() == clipped_box_renderer::MAX_CLIPPING_PLANES)
				return;

			tool = _tool;

			vibrate(hid_kit);

			{
				vr::vr_scene* scene_ptr = get_scene_ptr();
				if (scene_ptr) {
					// draw information about clipping planes
					if (li_tool_stats == -1) {
						li_tool_stats = scene_ptr->add_label(get_clipping_planes_stats(), stats_bgclr);
						scene_ptr->fix_label_size(li_tool_stats);
						scene_ptr->place_label(li_tool_stats, vec3(0.f, 0.f, -0.5f), quat(1, 0, 0, 0), coordinate_system::head, label_alignment::top);
					}
					else {
						scene_ptr->update_label_text(li_tool_stats, get_clipping_planes_stats());
					}
				}

				li_tool_time_delta = 0;
				li_tool_visible = true;
			}
			break;
		case tool_enum::gun:
			release_clipping_plane();

			tool = _tool;

			vibrate(hid_kit);

			{
				vr::vr_scene* scene_ptr = get_scene_ptr();
				if (scene_ptr) {
					// draw information about visibility toggle
					if (li_tool_stats == -1) {
						li_tool_stats = scene_ptr->add_label(get_clipping_planes_stats(), stats_bgclr);
						scene_ptr->fix_label_size(li_tool_stats);
						scene_ptr->place_label(li_tool_stats, vec3(0.f, 0.f, -0.5f), quat(1, 0, 0, 0), coordinate_system::head, label_alignment::top);
					}
					
					scene_ptr->update_label_text(li_tool_stats, "Visibility toggle active");
				}

				li_tool_time_delta = 0;
				li_tool_visible = true;
			}
			break;
		case tool_enum::torch:
			release_clipping_plane();

			tool = _tool;

			vibrate(hid_kit);

			{
				vr::vr_scene* scene_ptr = get_scene_ptr();
				if (scene_ptr) {
					// draw information about torch
					if (li_tool_stats == -1) {
						li_tool_stats = scene_ptr->add_label(get_clipping_planes_stats(), stats_bgclr);
						scene_ptr->fix_label_size(li_tool_stats);
						scene_ptr->place_label(li_tool_stats, vec3(0.f, 0.f, -0.5f), quat(1, 0, 0, 0), coordinate_system::head, label_alignment::top);
					}

					scene_ptr->update_label_text(li_tool_stats, "Torch active");
				}

				li_tool_time_delta = 0;
				li_tool_visible = true;
			}

			burn_outside = false;
			break;
		default:
			break;
		}
	}
#pragma endregion tool_bag_listener

#pragma region clipping_planes_container_listener
	// listener for installed clipping plane event inside box
	void on_clipping_plane_pointed_at(size_t index)
	{
		selected_clipping_plane_idx = index;
	}
	void on_clipping_plane_updated(size_t index, const vec3& origin, const vec3& direction)
	{
		cells_ctr->update_clipping_plane(index, origin, direction);
	}
	void on_clipping_plane_deleted(size_t index)
	{
		cells_ctr->delete_clipping_plane(index);
	}
#pragma endregion clipping_planes_container_listener
};

cgv::base::object_registration<vr_ca_vis> or_vr_ca_vis("vr_ca_vis");