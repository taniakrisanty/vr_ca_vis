// This source code is adapted from simple_primitive_container in vr_lab_test plugin

#include "gui_container.h"
#include <cgv/math/proximity.h>
#include <cgv/math/intersection.h>
#include <cgv/math/ftransform.h>

gui_container::rgb gui_container::get_modified_color(const rgb& color) const
{
	rgb mod_col(color);
	switch (state) {
	case state_enum::grabbed:
		mod_col[1] = std::min(1.0f, mod_col[0] + 0.2f);
	case state_enum::close:
		mod_col[0] = std::min(1.0f, mod_col[0] + 0.2f);
		break;
	case state_enum::triggered:
		mod_col[1] = std::min(1.0f, mod_col[0] + 0.2f);
	case state_enum::pointed:
		mod_col[2] = std::min(1.0f, mod_col[2] + 0.2f);
		break;
	}
	return mod_col;
}
gui_container::gui_container(gui_container_listener* _listener, const std::string& _name, const vec3& _extent, const quat& _rotation)
	: cgv::base::node(_name), listener(_listener), extent(_extent), rotation(_rotation)
{
	debug_point = vec3(0, 0.5f, 0);
	
	srs.radius = 0.01f;
}
std::string gui_container::get_type_name() const
{
	return "cells_container";
}
void gui_container::on_set(void* member_ptr)
{
	update_member(member_ptr);
	post_redraw();
}
bool gui_container::focus_change(cgv::nui::focus_change_action action, cgv::nui::refocus_action rfa, const cgv::nui::focus_demand& demand, const cgv::gui::event& e, const cgv::nui::dispatch_info& dis_info)
{
	switch (action) {
	case cgv::nui::focus_change_action::attach:
		if (state == state_enum::idle) {
			// set state based on dispatch mode
			state = dis_info.mode == cgv::nui::dispatch_mode::pointing ? state_enum::pointed : state_enum::close;
			on_set(&state);
			// store hid to filter handled events
			hid_id = dis_info.hid_id;
			return true;
		}
		// if focus is given to other hid, refuse attachment to new hid
		return false;
	case cgv::nui::focus_change_action::detach:
		// check whether detach corresponds to stored hid
		if (state != state_enum::idle && hid_id == dis_info.hid_id) {
			state = state_enum::idle;
			on_set(&state);
			return true;
		}
		return false;
	case cgv::nui::focus_change_action::index_change:
		prim_idx = int(reinterpret_cast<const cgv::nui::hit_dispatch_info&>(dis_info).get_hit_info()->primitive_index);
		on_set(&prim_idx);
		return true;
	}
	return true;
}
void gui_container::stream_help(std::ostream& os)
{
	os << "gui_container: point at it" << std::endl;
}
bool gui_container::handle(const cgv::gui::event& e, const cgv::nui::dispatch_info& dis_info, cgv::nui::focus_request& request)
{
	// ignore all events in idle mode
	if (state == state_enum::idle) {
		prim_idx = -1;
		return false;
	}
	// ignore events from other hids
	if (!(dis_info.hid_id == hid_id))
		return false;
	bool pressed;
	// hid independent check if object is triggered during pointing
	if (is_trigger_change(e, pressed)) {
		if (pressed) {
			state = state_enum::triggered;
			on_set(&state);
		}
		else {
			state = state_enum::pointed;
			on_set(&state);
		}
		return true;
	}
	// check if event is for pointing
	if (is_pointing(e, dis_info)) {
		const auto& inter_info = get_intersection_info(dis_info);
		if (state == state_enum::pointed) {
			debug_point = inter_info.hit_point;
			hit_point_at_trigger = inter_info.hit_point;
			prim_idx = int(inter_info.primitive_index);
		}
		else if (state == state_enum::triggered) {
			// if we still have an intersection point, use as debug point
			if (inter_info.ray_param != std::numeric_limits<float>::max())
				debug_point = inter_info.hit_point;
			// to be save even without new intersection, find closest point on ray to hit point at trigger
			vec3 q = cgv::math::closest_point_on_line_to_point(inter_info.ray_origin, inter_info.ray_direction, hit_point_at_trigger);
			select_gui(prim_idx);
		}
		post_redraw();
		return true;
	}
	return false;
}
bool gui_container::compute_intersection(const vec3& ray_start, const vec3& ray_direction, float& hit_param, vec3& hit_normal, size_t& primitive_idx)
{
	// point, prj_point, and prj_normal are in table coordinate
	// positions are in left controller coordinate

	hit_param = std::numeric_limits<float>::max();

	for (size_t i = 0; i < positions.size(); ++i) {
		vec4 position_in_table4(inv_model_transform * left_controller_transform * positions[i].lift());
		vec3 position_in_table(position_in_table4 / position_in_table4.w());

		vec3 rs = ray_start - position_in_table;
		vec3 rd = ray_direction;
		rotation.inverse_rotate(rs);
		rotation.inverse_rotate(rd);
		vec3 n;
		vec2 res;
		if (cgv::math::ray_box_intersection(rs, rd, 0.5f * extent, res, n) == 0)
			continue;
		float param;
		if (res[0] < 0) {
			if (res[1] < 0)
				continue;
			param = res[1];
		}
		else
			param = res[0];
		if (param < hit_param) {
			primitive_idx = i;
			hit_param = param;
			rotation.rotate(n);
			hit_normal = n;
		}
	}

	return hit_param < std::numeric_limits<float>::max();
}
bool gui_container::init(cgv::render::context& ctx)
{
	ref_clipped_box_renderer(ctx, 1);
	ref_control_sphere_renderer(ctx, 1);
	cgv::render::ref_cone_renderer(ctx, 1);
	cgv::render::ref_sphere_renderer(ctx, 1);
	return true;
}
void gui_container::clear(cgv::render::context& ctx)
{
	ref_clipped_box_renderer(ctx, -1);
	ref_control_sphere_renderer(ctx, -1);
	cgv::render::ref_cone_renderer(ctx, -1);
	cgv::render::ref_sphere_renderer(ctx, -1);
}
void gui_container::draw(cgv::render::context& ctx)
{
	// show labels
	//if (cells_count > 0)
	//{
	//	ctx.push_modelview_matrix();
	//	ctx.mul_modelview_matrix(scale_matrix);

	//	crs.radius_scale = 0.25f * std::max(inv_scale_matrix.x(), std::max(inv_scale_matrix.y(), inv_scale_matrix.z()));

	//	auto& cr = cgv::render::ref_cone_renderer(ctx);
	//	cr.set_render_style(crs);

	//	std::vector<vec3> cone_positions;
	//	std::vector<rgb> cone_colors;

	//	cone_positions.resize(2 * cells_count);
	//	cone_colors.resize(2 * cells_count);

	//	size_t nodes_start_index = (*cells)[cells_start].nodes_start_index;
	//	size_t nodes_end_index = (*cells)[cells_end - 1].nodes_end_index;

	//	for (size_t i = cells_start; i < cells_end; ++i) {
	//		const auto& center = cell::centers[i - cells_start];

	//		cone_positions[2 * (i - cells_start)] = center;
	//		cone_positions[2 * (i - cells_start) + 1] = vec3(center.x(), inv_scale_matrix[5], center.z());

	//		rgb color = rgb(1, 1, 1);
	//		cone_colors.push_back(color);
	//		cone_colors.push_back(color);
	//	}

	//	cr.set_position_array(ctx, cone_positions);
	//	cr.set_color_array(ctx, cone_colors);

	//	cr.render(ctx, 0, cone_positions.size());

	//	ctx.pop_modelview_matrix();
	//}

	// show points
	if (state == state_enum::idle)
		return;

	//auto& sr = cgv::render::ref_sphere_renderer(ctx);
	//sr.set_render_style(srs);
	//sr.set_position(ctx, debug_point);
	//rgb color(0.5f, 0.5f, 0.5f);
	//sr.set_color_array(ctx, &color, 1);
	//sr.render(ctx, 0, 1);
	//if (state == state_enum::triggered) {
	//	sr.set_position(ctx, hit_point_at_trigger);
	//	sr.set_color(ctx, rgb(0.5f, 0.3f, 0.3f));
	//	sr.render(ctx, 0, 1);
	//}
}
void gui_container::create_gui()
{}
void gui_container::set_cell_types(const std::unordered_map<std::string, cell_type>& _cell_types)
{
	cell_types = _cell_types;

	// TODO delete menu labels
	//if (menu_labels.size() != cell_types.size()) {
	//	menu_labels.resize(cell_types.size(), -1);
	//	positions.resize(cell_types.size());

	//	size_t type_index = 0;
	//	for (const auto& type : cell_types) {
	//		if (menu_labels[type_index] == -1) {
	//			if (listener) {
	//				vec3 position(0.f, 0.1f, -0.1f);
	//				uint32_t label = listener->on_create_label_requested(type.second.name, rgba(0.5f, 0.5f, 0.5f, 1.f), position, quat(1, 0, 0, 0));

	//				menu_labels[type_index] = label;
	//				positions[type_index] = position;
	//			}
	//		}
	//		else {

	//		}
	//		++type_index;
	//	}
	//}
}
void gui_container::set_inverse_model_transform(const mat4& _inv_model_transform)
{
	inv_model_transform = _inv_model_transform;
}
void gui_container::set_left_controller_transform(const mat4& _left_controller_transform)
{
	if (left_controller_transform != _left_controller_transform) {
		left_controller_transform = _left_controller_transform;

		mat3 rotation_matrix;

		rotation_matrix.set_col(0, normalize(left_controller_transform.col(0)));
		rotation_matrix.set_col(1, normalize(left_controller_transform.col(1)));
		rotation_matrix.set_col(2, normalize(left_controller_transform.col(2)));

		rotation = quat(rotation_matrix);
	}
}
void gui_container::select_gui(int index)
{

}
//void cells_container::add_color_points(const rgba& color0, const rgba& color1)
//{
//	color_points_maps.push_back({{ 0, color0 }, { 1, color1 }});
//	
//	color_maps.emplace_back();
//
//	for (const auto& cp : color_points_maps.back()) {
//		color_maps.back().add_color_point(float(cp.first), cp.second);
//		color_maps.back().add_opacity_point(float(cp.first), cp.second.alpha());
//	}
//}
//void cells_container::update_color_point(size_t index, const rgba& color0, const rgba& color1)
//{
//	color_maps[index].clear();
//
//	for (const auto& cp : color_points_maps[index]) {
//		color_maps[index].add_color_point(float(cp.first), cp.second);
//		color_maps[index].add_opacity_point(float(cp.first), cp.second.alpha());
//	}
//}
//void cells_container::interpolate_colors(bool force)
//{
//	if (force || group_colors.size() != cells_end - cells_start || group_colors_overrides.size() != cells_end - cells_start) {
//		group_colors.resize(cells_end - cells_start);
//		group_colors_overrides.resize(cells_end - cells_start);
//
//		size_t type_index = 0, cell_index = cells_start;
//		for (const auto& ct : cell_types) {
//			size_t cell_count = 0;
//			for (; cell_index < cells_end; ++cell_index) {
//				const auto& c = (*cells)[cell_index];
//
//				if (c.type != type_index)
//					break;
//
//				cell_count += 1;
//			}
//
//			if (cell_count == 1) {
//				if (!group_colors_overrides[cell_index - cells_start - cell_count]) {
//					group_colors[cell_index - cells_start - cell_count] = color_points_maps[type_index][0];
//					update_member(&group_colors[cell_index - cells_start - cell_count]);
//				}
//			}
//			else {
//				std::vector<rgba> colors = color_maps[type_index].interpolate(cell_count);
//				for (size_t color_index = 0; color_index < colors.size(); ++color_index) {
//					if (!group_colors_overrides[cell_index - cells_start - cell_count + color_index]) {
//						group_colors[cell_index - cells_start - cell_count + color_index] = colors[color_index];
//						update_member(&group_colors[cell_index - cells_start - cell_count + color_index]);
//					}
//				}
//			}
//
//			++type_index;
//		}
//	}
//}
