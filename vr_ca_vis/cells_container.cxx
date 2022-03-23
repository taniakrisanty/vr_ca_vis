// This source code is adapted from simple_primitive_container in vr_lab_test plugin

#include "cells_container.h"
#include <cgv/math/proximity.h>
#include <cgv/math/intersection.h>
#include <cgv/math/ftransform.h>

#include "grid_traverser.h"

cells_container::rgb cells_container::get_modified_color(const rgb& color) const
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
cells_container::cells_container(cells_container_listener* _listener, const std::string& _name, const vec3& _extent, const quat& _rotation)
	: cgv::base::node(_name), listener(_listener), extent(_extent), rotation(_rotation)
{
	debug_point = vec3(0, 0.5f, 0);
	
	srs.radius = 0.01f;
	
	brs.culling_mode = cgv::render::CullingMode::CM_BACKFACE;
	brs.use_group_color = true;
	brs.use_visibility = true;

	csrs.use_group_color = true;
	csrs.use_visibility = true;

	crs.radius = 0.005f;
	crs.rounded_caps = true;

	show_gui = true;

	cells_start = 0;
	cells_end = 0;

	scale_matrix.identity();
	
	grid.set_visibilities(&visibilities);
	
	grid.set_clipping_planes(&clipping_planes);
}
std::string cells_container::get_type_name() const
{
	return "cells_container";
}
void cells_container::on_set(void* member_ptr)
{
	update_member(member_ptr);

	// group colors
	bool found = false;
	if (!found && !group_colors.empty()) {
		if (member_ptr >= &group_colors[0] && member_ptr < &group_colors[0] + group_colors.size()) {
			size_t index = static_cast<rgba*>(member_ptr) - &group_colors[0];
			group_colors_overrides[index] = true;
			found = true;
		}
	}

	// show all & hide all checks
	if (!found && !show_all_checks.empty()) {
		if (member_ptr >= &show_all_checks[0] && member_ptr < &show_all_checks[0] + show_all_checks.size()) {
			size_t index = static_cast<int*>(member_ptr) - &show_all_checks[0];

			if (show_all_checks[index] == 1 && hide_all_checks[index] == 1) {
				hide_all_checks[index] = 0;
				update_member(&hide_all_checks[index]);
				post_recreate_gui();
			}

			for (size_t cell_index = cells_start; cell_index < cells_end; ++cell_index) {
				const cell& c = (*cells)[cell_index];

				if (c.type < index)
					continue;

				if (c.type > index)
					break;

				visibilities[cell_index - cells_start] = show_checks[cell_index - cells_start] | show_all_checks[index];
			}

			found = true;
		}
	}

	if (!found && !hide_all_checks.empty()) {
		if (member_ptr >= &hide_all_checks[0] && member_ptr < &hide_all_checks[0] + hide_all_checks.size()) {
			size_t index = static_cast<int*>(member_ptr) - &hide_all_checks[0];

			if (hide_all_checks[index] == 1 && show_all_checks[index] == 1) {
				show_all_checks[index] = 0;
				update_member(&show_all_checks[index]);
			}
			post_recreate_gui();

			for (size_t cell_index = cells_start; cell_index < cells_end; ++cell_index) {
				const cell& c = (*cells)[cell_index];

				if (c.type < index)
					continue;

				if (c.type > index)
					break;

				visibilities[cell_index - cells_start] = show_checks[cell_index - cells_start] & (hide_all_checks[index] == 0);
			}

			found = true;
		}
	}

	// show checks
	if (!found && !show_checks.empty()) {
		if (member_ptr >= &show_checks[0] && member_ptr < &show_checks[0] + show_checks.size()) {
			size_t index = static_cast<int*>(member_ptr) - &show_checks[0];

			const cell& c = (*cells)[index];

			if (show_checks[index] == 1) {
				hide_all_checks[c.type] = 0;
				update_member(&hide_all_checks[c.type]);

				visibilities[index] = 1;
			}
			else if (show_checks[index] == 0) {
				show_all_checks[c.type] = 1;
				update_member(&show_all_checks[c.type]);

				visibilities[index] = 0;
			}

			found = true;
		}
	}

	// color maps
	if (!found) {
		for (size_t i = 0; i < color_points_maps.size(); ++i) {
			for (size_t j = 0; j < color_points_maps[i].size(); ++j) {
				if (member_ptr == &color_points_maps[i][j]) {
					update_color_point(i, color_points_maps[i][0], color_points_maps[i][1]);
					interpolate_colors(true);
					found = true;
					break;
				}
			}

			if (found) break;
		}
	}

	post_redraw();
}
bool cells_container::focus_change(cgv::nui::focus_change_action action, cgv::nui::refocus_action rfa, const cgv::nui::focus_demand& demand, const cgv::gui::event& e, const cgv::nui::dispatch_info& dis_info)
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
void cells_container::stream_help(std::ostream& os)
{
	os << "cells_container: grab and point at it" << std::endl;
}
bool cells_container::handle(const cgv::gui::event& e, const cgv::nui::dispatch_info& dis_info, cgv::nui::focus_request& request)
{
	// ignore all events in idle mode
	if (state == state_enum::idle)
		return false;
	// ignore events from other hids
	if (!(dis_info.hid_id == hid_id))
		return false;
	bool pressed;
	// hid independent check if grabbing is activated or deactivated
	if (is_grab_change(e, pressed)) {
		if (pressed) {
			state = state_enum::grabbed;
			on_set(&state);
			//drag_begin(request, false, original_config);
		}
		else {
			//drag_end(request, original_config);
			state = state_enum::close;
			on_set(&state);
		}
		return true;
	}
	// check if event is for grabbing
	if (is_grabbing(e, dis_info)) {
		const auto& prox_info = get_proximity_info(dis_info);
		if (state == state_enum::close) {
			debug_point = prox_info.hit_point;
			query_point_at_grab = prox_info.query_point;
			prim_idx = int(prox_info.primitive_index);

			if (prim_idx & cell_sign_bit) {
				size_t center_index = prim_idx & cell_bitwise_and;

				point_at_cell(center_index);
			}
			else {
				size_t cell_index = (prim_idx >> cell_bitwise_shift) & cell_bitwise_and;
				size_t node_index = prim_idx & cell_bitwise_and;

				point_at_cell(cell_index, node_index);
			}
		}
		else if (state == state_enum::grabbed) {
			debug_point = prox_info.hit_point;
			//cells[prim_idx].node = position_at_grab + prox_info.query_point - query_point_at_grab;
			//vec4 translation4(inv_scale_matrix * (prox_info.query_point - query_point_at_grab).lift());
			//vec3 translation(translation4 / translation4.w());

			//size_t cell_index = prim_idx >> 10;

			//group_translations[(*cells)[cell_index].id] += translation;
		}
		post_redraw();
		return true;
	}
	// hid independent check if object is triggered during pointing
	if (is_trigger_change(e, pressed)) {
		if (pressed) {
			state = state_enum::triggered;
			on_set(&state);
			//drag_begin(request, true, original_config);
		}
		else {
			//drag_end(request, original_config);
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
			
			if (prim_idx & cell_sign_bit) {
				size_t center_index = prim_idx & cell_bitwise_and;

				point_at_cell(center_index);
			}
			else {
				size_t cell_index = (prim_idx >> cell_bitwise_shift) & cell_bitwise_and;
				size_t node_index = prim_idx & cell_bitwise_and;

				point_at_cell(cell_index, node_index);
			}
		}
		else if (state == state_enum::triggered) {
			// if we still have an intersection point, use as debug point
			if (inter_info.ray_param != std::numeric_limits<float>::max())
				debug_point = inter_info.hit_point;
			// to be save even without new intersection, find closest point on ray to hit point at trigger
			vec3 q = cgv::math::closest_point_on_line_to_point(inter_info.ray_origin, inter_info.ray_direction, hit_point_at_trigger);
			//cells[prim_idx].node = position_at_trigger + q - hit_point_at_trigger;
			//vec4 translation4(inv_scale_matrix * (q - hit_point_at_trigger).lift());
			//vec3 translation(translation4 / translation4.w());
		}
		post_redraw();
		return true;
	}
	//point_at_cell(SIZE_MAX, SIZE_MAX);
	return false;
}
bool cells_container::compute_closest_point(const vec3& point, vec3& prj_point, vec3& prj_normal, size_t& primitive_idx)
{
	if (burn) return false;

	vec4 point_upscaled4(inv_scale_matrix * point.lift());
	vec3 point_upscaled(point_upscaled4 / point_upscaled4.w());

	float min_dist, max_dist;
	min_dist = max_dist = std::numeric_limits<float>::max();

	// check closest point to centers
	vec3 q, n;
	for (size_t i = cells_start; i < cells_end; ++i) {
		if (visibilities[i - cells_start] > 0)
			continue;
		
		cgv::math::closest_point_on_sphere_to_point(cell::centers[i], 1.f, point_upscaled, q, n);
		float dist = (point_upscaled - q).sqr_length();
		if (dist < min_dist) {
			primitive_idx = i;
			min_dist = dist;
		}
	}
	
	if (min_dist < max_dist) {
		vec4 position_downscaled4(scale_matrix * cell::centers[primitive_idx].lift());
		vec3 position_downscaled(position_downscaled4 / position_downscaled4.w());

		vec4 radius_downscaled4(scale_matrix * vec4(1.f));
		vec3 radius_downscaled(radius_downscaled4 / radius_downscaled4.w());

		cgv::math::closest_point_on_sphere_to_point(position_downscaled, radius_downscaled.x(), point, q, n);

		prj_point = q;

		min_dist = (point - q).sqr_length();

		primitive_idx |= cell_sign_bit;
	}

	// check closest point to nodes
	regular_grid<cell>::result_entry res = grid.closest_point(point_upscaled);

	if (res.sqr_distance < min_dist)
	{
		primitive_idx = (res.cell_index << cell_bitwise_shift) | res.node_index;

		const auto& c = (*cells)[res.cell_index];

		vec4 position_downscaled4(scale_matrix * cell::nodes[c.nodes_start_index + res.node_index].lift());
		vec3 position_downscaled(position_downscaled4 / position_downscaled4.w());

		vec4 extent_downscaled4(scale_matrix * extent.lift());
		vec3 extent_downscaled(extent_downscaled4 / extent_downscaled4.w());

		vec3 p = point - position_downscaled;
		rotation.inverse_rotate(p);
		for (int i = 0; i < 3; ++i)
			p[i] = std::max(-0.5f * extent_downscaled[i], std::min(0.5f * extent_downscaled[i], p[i]));
		rotation.rotate(p);
		prj_point = p + position_downscaled;

#ifdef DEBUG
		std::cout << "cells_container::compute_closest_point query " << point << " = " << position_downscaled << std::endl;
#endif
	}
	else {
		prj_normal = n;
	}

	return min_dist < max_dist;
}
bool cells_container::compute_intersection(const vec3& ray_start, const vec3& ray_direction, float& hit_param, vec3& hit_normal, size_t& primitive_idx)
{
	if (burn) return false;

	vec4 ray_origin_upscaled4(inv_scale_matrix * ray_start.lift());
	vec3 ray_origin_upscaled(ray_origin_upscaled4 / ray_origin_upscaled4.w());

	float max_hit_param = std::numeric_limits<float>::max();
	hit_param = max_hit_param;

	for (size_t i = cells_start; i < cells_end; ++i) {
		if (visibilities[i - cells_start] > 0)
			continue;

		vec2 res;
		if (cgv::math::ray_sphere_intersection(ray_origin_upscaled, ray_direction, cell::centers[i], 1.f, res) == 0)
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
		}
	}

	if (hit_param < max_hit_param) {
		vec4 position_downscaled4(scale_matrix * cell::centers[primitive_idx].lift());
		vec3 position_downscaled(position_downscaled4 / position_downscaled4.w());

		vec4 radius_downscaled4(scale_matrix * vec4(1.f));
		vec3 radius_downscaled(radius_downscaled4 / radius_downscaled4.w());

		vec2 res;
		cgv::math::ray_sphere_intersection(ray_start, ray_direction, position_downscaled, radius_downscaled.x(), res);

		float param;
		if (res[0] < 0)
			param = res[1];
		else
			param = res[0];

		hit_param = param;
		hit_normal = normalize(ray_start + param * ray_direction - position_downscaled);

		primitive_idx |= cell_sign_bit;
	}

	grid_traverser trav(ray_origin_upscaled, ray_direction, grid.get_cell_extents());
	for (int i = 0; ; ++i, trav++)
	{
		int index = grid.get_cell_index_to_grid_index(*trav);
		if (index < 0)
			break;

		size_t cell_index, node_index;
		if (!grid.get_closest_index(index, cell_index, node_index))
			continue;

		const auto& c = (*cells)[cell_index];

		vec4 position_downscaled4(scale_matrix * cell::nodes[c.nodes_start_index + node_index].lift());
		vec3 position_downscaled(position_downscaled4 / position_downscaled4.w());

		vec4 extent_downscaled4(scale_matrix * extent.lift());
		vec3 extent_downscaled(extent_downscaled4 / extent_downscaled4.w());

		vec3 n;
		vec2 res;
		if (cgv::math::ray_box_intersection(ray_start - position_downscaled, ray_direction, 0.5f * extent_downscaled, res, n) == 0)
			break;
		float param;
		if (res[0] < 0) {
			if (res[1] < 0)
				break;
			param = res[1];
		}
		else
			param = res[0];

		if (param < hit_param) {
			primitive_idx = (cell_index << cell_bitwise_shift) | node_index;
			hit_param = param;
			hit_normal = n;
		}

#ifdef DEBUG
		std::cout << "cells_container::compute_intersection query " << ray_start << " = " << position_downscaled << " | hit param " << hit_param << " | hit normal " << hit_normal << std::endl;
#endif
		break;
	}

	return hit_param < max_hit_param;
}
bool cells_container::init(cgv::render::context& ctx)
{
	ref_clipped_box_renderer(ctx, 1);
	ref_control_sphere_renderer(ctx, 1);
	cgv::render::ref_cone_renderer(ctx, 1);
	cgv::render::ref_sphere_renderer(ctx, 1);
	return true;
}
void cells_container::clear(cgv::render::context& ctx)
{
	ref_clipped_box_renderer(ctx, -1);
	ref_control_sphere_renderer(ctx, -1);
	cgv::render::ref_cone_renderer(ctx, -1);
	cgv::render::ref_sphere_renderer(ctx, -1);
}
void cells_container::init_frame(cgv::render::context& ctx)
{
	if (cells_out_of_date) {
		transmit_cells(ctx);
		cells_out_of_date = false;
	}
}
void cells_container::draw(cgv::render::context& ctx)
{
	// show cell centers
	if (cells_count > 0)
	{
		ctx.push_modelview_matrix();
		ctx.mul_modelview_matrix(scale_matrix);

		auto& csr = ref_control_sphere_renderer(ctx);
		csr.set_render_style(csrs);

		set_centers_group_geometry(ctx, csr);
		set_centers_geometry(ctx, csr);

		csr.render(ctx, 0, cells_count);

		ctx.pop_modelview_matrix();
	}

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

	// show nodes
	if (nodes_count > 0)
	{
		ctx.push_modelview_matrix();
		ctx.mul_modelview_matrix(scale_matrix);

		for (size_t i = 0; i < clipping_planes.size(); ++i)
			glEnable(GL_CLIP_DISTANCE0 + i);

		// save previous blend configuration
		GLboolean blend;
		glGetBooleanv(GL_BLEND, &blend);

		GLint blend_src, blend_dst;
		glGetIntegerv(GL_BLEND_SRC_ALPHA, &blend_src);
		glGetIntegerv(GL_BLEND_DST_ALPHA, &blend_dst);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		auto& br = ref_clipped_box_renderer(ctx);
		br.set_render_style(brs);

		set_nodes_group_geometry(ctx, br);
		set_nodes_geometry(ctx, br);

		br.set_extent(ctx, extent);
		//br.set_rotation_array(ctx, &rotation, cells.size());
		br.set_clipping_planes(clipping_planes);
		br.set_torch(burn, burn_center, burn_distance);
		br.render(ctx, 0, nodes_count);

		for (size_t i = 0; i < clipping_planes.size(); ++i)
			glDisable(GL_CLIP_DISTANCE0 + i);

		// restore previous blend configuration
		if (blend)
			glEnable(GL_BLEND);
		else
			glDisable(GL_BLEND);

		glBlendFunc(blend_src, blend_dst);

		ctx.pop_modelview_matrix();
	}

	// show points
	auto& sr = cgv::render::ref_sphere_renderer(ctx);
	sr.set_render_style(srs);
	sr.set_position(ctx, debug_point);
	rgb color(0.5f, 0.5f, 0.5f);
	sr.set_color_array(ctx, &color, 1);
	sr.render(ctx, 0, 1);
	if (state == state_enum::grabbed) {
		sr.set_position(ctx, query_point_at_grab);
		sr.set_color(ctx, rgb(0.5f, 0.5f, 0.5f));
		sr.render(ctx, 0, 1);
	}
	if (state == state_enum::triggered) {
		sr.set_position(ctx, hit_point_at_trigger);
		sr.set_color(ctx, rgb(0.5f, 0.3f, 0.3f));
		sr.render(ctx, 0, 1);
	}
}
void cells_container::create_gui()
{
	if (begin_tree_node("Cells", cells)) {
		align("\a");
		add_member_control(this, "culling_mode", brs.culling_mode, "dropdown", "enums='off,backface,frontface'");

		size_t type_index = 0, cell_index = cells_start;
		for (const auto& ct : cell_types) {
			if (begin_tree_node(ct.first, ct)) {
				align("\a");
				add_member_control(this, "show_all", reinterpret_cast<bool&>(show_all_checks[type_index]), "check");
				add_member_control(this, "hide_all", reinterpret_cast<bool&>(hide_all_checks[type_index]), "check");

				if (hide_all_checks[type_index] == 0) {
					std::map<unsigned int, rgba>& cm = color_points_maps[type_index];

					for (size_t i = 0; i < cm.size(); ++i)
						add_member_control(this, "color", cm[i]);

					for (; cell_index < cells_end; ++cell_index) {
						const auto& c = (*cells)[cell_index];

						if (c.type < type_index)
							continue;

						if (c.type > type_index)
							break;

						add_member_control(this, " cell_" + std::to_string(c.id), reinterpret_cast<bool&>(show_checks[cell_index - cells_start]), "check");
						add_member_control(this, "color", group_colors[cell_index - cells_start]);
					}
				}

				align("\b");
				end_tree_node(ct);
			}
			++type_index;
		}
		align("\b");
		end_tree_node(cells);
	}
}
void cells_container::set_scale_matrix(const mat4& _scale_matrix)
{
	if (scale_matrix != _scale_matrix)
	{
		scale_matrix = _scale_matrix;
		inv_scale_matrix = inv(_scale_matrix);

		// TODO update all clipping planes

#ifdef DEBUG
		if (clipping_planes.empty())
			create_clipping_plane(vec3(0.558930457f, 0.278098106f, 0.932542086f), vec3(-0.902267516f, 0, -0.431176722f));
#endif
	}
}
void cells_container::set_cell_types(const std::unordered_map<std::string, cell_type>& _cell_types)
{
	cell_types = _cell_types;

	color_points_maps.clear();
	color_maps.clear();

	for (size_t i = 0; i < cell_types.size(); ++i) {
		float hue = float(i) / cell_types.size();
		add_color_points(cgv::media::color<float, cgv::media::HLS, cgv::media::OPACITY>(hue, 0.5f, 1.0f, 0.5f), cgv::media::color<float, cgv::media::HLS, cgv::media::OPACITY>(hue, 0.5f, 1.0f, 0.5f));
	}

	//for (size_t i = 0; i < cell_types.size(); ++i)
	//	add_color_points(rgba(59.f / 255, 76.f / 255, 192.f / 255, 1.f), rgba(180.f / 255, 4.f / 255, 38.f / 255, 1.f));

	show_all_checks.resize(cell_types.size(), 1);
	hide_all_checks.resize(cell_types.size(), 0);
}
void cells_container::set_cells(const std::vector<cell>* _cells, size_t _cells_start, size_t _cells_end, const ivec3& extents)
{
	grid.cancel_build_from_vertices();

	cells = _cells;

	cells_start = _cells_start;
	cells_end = _cells_end;

	cells_out_of_date = true;

	grid.build_from_vertices(cells, cells_start, cells_end, extents);

	visibilities.resize(cells_end - cells_start, 1);

	show_checks.resize(cells_end - cells_start, 1);

	interpolate_colors();
}
void cells_container::unset_cells()
{
	grid.cancel_build_from_vertices();

	grid.build_from_vertices(NULL, 0, 0);
}
void cells_container::add_color_points(rgba color0, rgba color1)
{
	color_points_maps.push_back({{ 0, color0 }, { 1, color1 }});
	
	color_maps.emplace_back();

	for (const auto& cp : color_points_maps.back()) {
		color_maps.back().add_color_point(float(cp.first), cp.second);
		color_maps.back().add_opacity_point(float(cp.first), cp.second.alpha());
	}
}
void cells_container::update_color_point(size_t index, rgba color0, rgba color1)
{
	color_maps[index].clear();

	for (const auto& cp : color_points_maps[index]) {
		color_maps[index].add_color_point(float(cp.first), cp.second);
		color_maps[index].add_opacity_point(float(cp.first), cp.second.alpha());
	}
}
void cells_container::interpolate_colors(bool force)
{
	if (force || group_colors.size() != cells_end - cells_start || group_colors_overrides.size() != cells_end - cells_start) {
		group_colors.resize(cells_end - cells_start);
		group_colors_overrides.resize(cells_end - cells_start);

		size_t type_index = 0, cell_index = cells_start;
		for (const auto& ct : cell_types) {
			size_t cell_count = 0;
			for (; cell_index < cells_end; ++cell_index) {
				const auto& c = (*cells)[cell_index];

				if (c.type != type_index)
					break;

				cell_count += 1;
			}

			if (cell_count == 1) {
				if (!group_colors_overrides[cell_index - cells_start - cell_count]) {
					group_colors[cell_index - cells_start - cell_count] = color_points_maps[type_index][0];
					update_member(&group_colors[cell_index - cells_start - cell_count]);
				}
			}
			else {
				std::vector<rgba> colors = color_maps[type_index].interpolate(cell_count);
				for (size_t color_index = 0; color_index < colors.size(); ++color_index) {
					if (!group_colors_overrides[cell_index - cells_start - cell_count + color_index]) {
						group_colors[cell_index - cells_start - cell_count + color_index] = colors[color_index];
						update_member(&group_colors[cell_index - cells_start - cell_count + color_index]);
					}
				}
			}

			++type_index;
		}
	}
}
void cells_container::create_clipping_plane(const vec3& origin, const vec3& direction)
{
	vec4 scaled_origin4(inv_scale_matrix * origin.lift());
	vec3 scaled_origin(scaled_origin4 / scaled_origin4.w());

	clipping_planes.emplace_back(direction, -dot(scaled_origin, direction));
}
void cells_container::delete_clipping_plane(size_t index, size_t count)
{
	clipping_planes.erase(clipping_planes.begin() + index, clipping_planes.begin() + index + count);
}
void cells_container::clear_clipping_planes()
{
	clipping_planes.clear();
}
void cells_container::update_clipping_plane(size_t index, const vec3& origin, const vec3& direction)
{
	vec4 scaled_origin4(inv_scale_matrix * origin.lift());
	vec3 scaled_origin(scaled_origin4 / scaled_origin4.w());

	clipping_planes[index] = vec4(direction, -dot(scaled_origin, direction));
}
void cells_container::set_torch(bool _burn, const vec3& _unscaled_burn_center, float _burn_distance)
{
	burn = _burn;

	if (burn) {
		if (unscaled_burn_center != _unscaled_burn_center) {
			unscaled_burn_center = _unscaled_burn_center;

			vec4 scaled_center4(inv_scale_matrix * unscaled_burn_center.lift());
			burn_center = scaled_center4 / scaled_center4.w();
		}

		// TODO should distance be in cells local coordinate?
		burn_distance = _burn_distance;
	}
}
void cells_container::toggle_cell_visibility(size_t cell_index)
{
	const auto& c = (*cells)[cell_index];

	show_checks[c.id] = !show_checks[c.id];
	on_set(&show_checks[c.id]);
}
void cells_container::transmit_cells(cgv::render::context& ctx)
{
	std::vector<unsigned int> center_ids, node_ids;
	center_ids.resize(cells_end - cells_start);

	size_t nodes_start_index = (*cells)[cells_start].nodes_start_index;
	size_t nodes_end_index = (*cells)[cells_end - 1].nodes_end_index;

	for (size_t i = cells_start; i < cells_end; ++i) {
		const auto& c = (*cells)[i];

		center_ids[i - cells_start] = c.id;

		for (size_t j = c.nodes_start_index; j < c.nodes_end_index; ++j)
			node_ids.push_back(c.id);
	}

	if (nodes_count != nodes_end_index - nodes_start_index) {
		nodes_count = nodes_end_index - nodes_start_index;

		vb_node_indices.destruct(ctx);
		vb_nodes.destruct(ctx);
		vb_colors.destruct(ctx);
	}

	default_colors.resize(nodes_count, rgba(1.f));

	if (nodes_count > 0) {
		if (!vb_node_indices.is_created())
			vb_node_indices.create(ctx, node_ids);
		else
			vb_node_indices.replace(ctx, 0, &node_ids[0], nodes_count);

		if (!vb_nodes.is_created())
			vb_nodes.create(ctx, &cell::nodes[nodes_start_index], nodes_count);
		else
			vb_nodes.replace(ctx, 0, &cell::nodes[nodes_start_index], nodes_count);

		if (!vb_colors.is_created())
			vb_colors.create(ctx, default_colors);
		else
			vb_colors.replace(ctx, 0, &default_colors[0], nodes_count);
	}

	if (cells_count != cells_end - cells_start) {
		cells_count = cells_end - cells_start;

		vb_center_indices.destruct(ctx);
		vb_centers.destruct(ctx);
	}

	if (cells_count > 0) {
		if (!vb_center_indices.is_created())
			vb_center_indices.create(ctx, center_ids);
		else
			vb_center_indices.replace(ctx, 0, &center_ids[0], cells_count);

		if (!vb_centers.is_created())
			vb_centers.create(ctx, &cell::centers[cells_start], cells_count);
		else
			vb_centers.replace(ctx, 0, &cell::centers[cells_start], cells_count);
	}

	cells_out_of_date = false;
}
void cells_container::set_nodes_group_geometry(cgv::render::context& ctx, clipped_box_renderer& br)
{
	if (!group_colors.empty())
		br.set_group_colors(ctx, group_colors);
	if (!visibilities.empty())
		br.set_visibilities(ctx, visibilities);
}
void cells_container::set_nodes_geometry(cgv::render::context& ctx, clipped_box_renderer& br)
{
	if (nodes_count > 0) {
		br.set_visibilities_index_array<unsigned int>(ctx, vb_node_indices, 0, nodes_count);
		br.set_group_index_array<unsigned int>(ctx, vb_node_indices, 0, nodes_count);
		br.set_position_array<vec3>(ctx, vb_nodes, 0, nodes_count);
		br.set_color_array<rgba>(ctx, vb_colors, 0, nodes_count);
	}
}
void cells_container::set_centers_group_geometry(cgv::render::context& ctx, control_sphere_renderer& csr)
{
	if (!group_colors.empty())
		csr.set_group_colors(ctx, group_colors);
	if (!visibilities.empty())
		csr.set_visibilities(ctx, visibilities);
}
void cells_container::set_centers_geometry(cgv::render::context& ctx, control_sphere_renderer& csr)
{
	if (cells_count > 0) {
		csr.set_visibilities_index_array<unsigned int>(ctx, vb_center_indices, 0, cells_count);
		csr.set_group_index_array<unsigned int>(ctx, vb_center_indices, 0, cells_count);
		csr.set_position_array<vec3>(ctx, vb_centers, 0, cells_count);
		csr.set_color_array<rgba>(ctx, vb_colors, 0, cells_count);
	}
}
void cells_container::point_at_cell(size_t cell_index, size_t node_index) const
{
	if (listener)
		listener->on_cell_pointed_at(cell_index, node_index);
}
