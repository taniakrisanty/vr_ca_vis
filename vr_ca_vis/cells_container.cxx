// This source code is adapted from simple_primitive_container in vr_lab_test plugin

#include "cells_container.h"
#include <cgv/math/ftransform.h>
#include <cgv/math/proximity.h>
#include <cgv/math/intersection.h>

#include <cgv_gl/line_renderer.h>

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

	scale_matrix.identity();
}
std::string cells_container::get_type_name() const
{
	return "cells_container";
}
void cells_container::on_set(void* member_ptr)
{
	update_member(member_ptr);
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
			grab_cell(prim_idx);
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
			position_at_grab = cells[prim_idx].node;
		}
		else if (state == state_enum::grabbed) {
			debug_point = prox_info.hit_point;
			//cells[prim_idx].node = position_at_grab + prox_info.query_point - query_point_at_grab;
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
			grab_cell(prim_idx);
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
			position_at_trigger = cells[prim_idx].node;
		}
		else if (state == state_enum::triggered) {
			// if we still have an intersection point, use as debug point
			if (inter_info.ray_param != std::numeric_limits<float>::max())
				debug_point = inter_info.hit_point;
			// to be save even without new intersection, find closest point on ray to hit point at trigger
			vec3 q = cgv::math::closest_point_on_line_to_point(inter_info.ray_origin, inter_info.ray_direction, hit_point_at_trigger);
			//cells[prim_idx].node = position_at_trigger + q - hit_point_at_trigger;
		}
		post_redraw();
		return true;
	}
	return false;
}
bool cells_container::compute_closest_point(const vec3& point, vec3& prj_point, vec3& prj_normal, size_t& primitive_idx)
{
	vec4 point_upscaled4(inv_scale_matrix * vec4(point, 1.f));
	vec3 point_upscaled = point_upscaled4 / point_upscaled4.w();

	float min_dist = std::numeric_limits<float>::max();
	//vec3 q, n;
	//for (size_t i = 0; i < positions.size(); ++i) {
	//	vec3 p = po - positions[i];
	//	rotation.inverse_rotate(p);
	//	for (int i = 0; i < 3; ++i)
	//		p[i] = std::max(-0.5f * extent[i], std::min(0.5f * extent[i], p[i]));
	//	rotation.rotate(p);
	//	prj_point = p + positions[i];
	//}
	//std::cout << "min_dist = " << positions[0] << " <-> " << point << " | " << radii[0] << " at " << min_dist << " for " << primitive_idx << std::endl;

	regular_grid<cell>::result_entry res = grid.closest_point(point_upscaled);

	if (min_dist > res.sqr_distance)
	{
		min_dist = sqrt(res.sqr_distance);

		primitive_idx = res.prim;

		vec4 position_downscaled4(scale_matrix * vec4(cells[primitive_idx].node, 1.f));
		vec3 position_downscaled = position_downscaled4 / position_downscaled4.w();

		vec4 extent_downscaled4(scale_matrix * vec4(extent, 1.f));
		vec3 extent_downscaled = extent_downscaled4 / extent_downscaled4.w();

		vec3 p = point - position_downscaled;
		rotation.inverse_rotate(p);
		for (int i = 0; i < 3; ++i)
			p[i] = std::max(-0.5f * extent_downscaled[i], std::min(0.5f * extent_downscaled[i], p[i]));
		rotation.rotate(p);
		prj_point = p + position_downscaled;

		std::cout << "Closest point from query point " << point << " = " << position_downscaled << std::endl;
	}

	return min_dist < std::numeric_limits<float>::max();
}
bool cells_container::compute_intersection(const vec3& ray_start, const vec3& ray_direction, float& hit_param, vec3& hit_normal, size_t& primitive_idx)
{
	vec4 ray_origin_upscaled4(inv_scale_matrix * vec4(ray_start, 1.f));
	vec3 ray_origin_upscaled = ray_origin_upscaled4 / ray_origin_upscaled4.w();

	hit_param = std::numeric_limits<float>::max();
	//for (size_t i = 0; i < positions.size(); ++i) {
	//	vec3 n;
	//	vec2 res;
	//	if (cgv::math::ray_box_intersection(ro - positions[i], rd, 0.5f * extent, res, n) == 0)
	//		continue;
	//	float param;
	//	if (res[0] < 0) {
	//		if (res[1] < 0)
	//			continue;
	//		param = res[1];
	//	}
	//	else
	//		param = res[0];
	//	if (param < hit_param) {
	//		primitive_idx = i;
	//		hit_param = param;
	//		hit_normal = n;
	//	}
	//}

	grid_traverser trav(ray_origin_upscaled, ray_direction, grid.get_cell_extents());
	for (int i = 0; ; ++i, trav++)
	{
		int index = grid.get_cell_index_to_grid_index(*trav);

		if (index < 0)
			break;
		//if (index < 0 || index >= 10 * 10 * 10)

		std::vector<int> indices;
		grid.get_closest_indices(index, indices);

		if (indices.empty())
			continue;

		for (int pi : indices)
		{
			vec3 n;
			vec2 res;
			if (cgv::math::ray_box_intersection(ray_origin_upscaled - cells[pi].node, ray_direction, 0.5f * extent, res, n) == 0)
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
				primitive_idx = pi;
				hit_param = param;
				hit_normal = n;
			}
		}

		break;
	}

	if (hit_param < std::numeric_limits<float>::max())
	{
		vec4 position_downscaled4(scale_matrix * vec4(cells[primitive_idx].node, 1.f));
		vec3 position_downscaled = position_downscaled4 / position_downscaled4.w();

		vec4 extent_downscaled4(scale_matrix * vec4(extent, 1.f));
		vec3 extent_downscaled = extent_downscaled4 / extent_downscaled4.w();

		vec3 n;
		vec2 res;
		cgv::math::ray_box_intersection(ray_start - position_downscaled, ray_direction, 0.5f * extent_downscaled, res, n);
		float param;
		if (res[0] < 0)
			param = res[1];
		else
			param = res[0];

		hit_param = param;
		hit_normal = n;

		std::cout << "Intersection from query ray " << ray_start << " = " << position_downscaled << std::endl;
		std::cout << "Hit param " << hit_param << " | hit normal " << hit_normal << std::endl;
	}

	return hit_param < std::numeric_limits<float>::max();
}
bool cells_container::init(cgv::render::context& ctx)
{
	ref_clipped_box_renderer(ctx, 1);
	cgv::render::ref_sphere_renderer(ctx, 1);
	cgv::render::ref_line_renderer(ctx, 1);
	return true;
}
void cells_container::clear(cgv::render::context& ctx)
{
	ref_clipped_box_renderer(ctx, -1);
	cgv::render::ref_sphere_renderer(ctx, -1);
	cgv::render::ref_line_renderer(ctx, -1);
}
void cells_container::draw(cgv::render::context& ctx)
{
	ctx.push_modelview_matrix();
	ctx.mul_modelview_matrix(scale_matrix);

	for (int i = 0; i < clipping_planes.size(); ++i)
		glEnable(GL_CLIP_DISTANCE0 + i);

	// show box
	if (!cells.empty())
	{
		auto& br = ref_clipped_box_renderer(ctx);
		br.set_render_style(brs);
		br.set_position_array(ctx, &cells.front().node, cells.size(), sizeof(cell));
		rgb tmp_color;
		if (prim_idx >= 0 && prim_idx < cells.size()) {
			tmp_color = cells[prim_idx].color;
			cells[prim_idx].color = get_modified_color(cells[prim_idx].color);
		}
		br.set_color_array(ctx, &cells.front().color, cells.size(), sizeof(cell));
		br.set_extent(ctx, extent);
		//br.set_rotation_array(ctx, &rotation, cells.size());
		br.set_clipping_planes(clipping_planes);
		br.render(ctx, 0, cells.size());
		if (prim_idx >= 0 && prim_idx < cells.size())
			cells[prim_idx].color = tmp_color;
	}

	for (int i = 0; i < clipping_planes.size(); ++i)
		glDisable(GL_CLIP_DISTANCE0 + i);

	ctx.pop_modelview_matrix();

	// show points
	auto& sr = cgv::render::ref_sphere_renderer(ctx);
	sr.set_render_style(srs);
	sr.set_position(ctx, debug_point);
	rgb color(1.f, 0.f, 0.f);
	sr.set_color_array(ctx, &color, 1);
	sr.render(ctx, 0, 1);
	if (state == state_enum::grabbed) {
		sr.set_position(ctx, query_point_at_grab);
		sr.set_color(ctx, rgb(0.f, 1.f, 0.f));
		sr.render(ctx, 0, 1);
	}
	if (state == state_enum::triggered) {
		sr.set_position(ctx, hit_point_at_trigger);
		sr.set_color(ctx, rgb(0.f, 0.f, 1.f));
		sr.render(ctx, 0, 1);
	}
}
void cells_container::create_gui()
{
	add_decorator(get_name(), "heading", "level=2");
	if (begin_tree_node("Cell Sphere Rendering", srs, false)) {
		align("\a");
		add_gui("sphere_style", srs);
		align("\b");
		end_tree_node(srs);
	}
	if (begin_tree_node("Cell Box Rendering", brs, false)) {
		align("\a");
		add_gui("box_style", brs);
		align("\b");
		end_tree_node(brs);
	}
}
void cells_container::set_scale_matrix(const mat4& _scale_matrix)
{
	scale_matrix = _scale_matrix;
	inv_scale_matrix = inv(_scale_matrix);
}
void cells_container::set_cells(size_t _offset, std::vector<cell>::const_iterator cells_begin, std::vector<cell>::const_iterator cells_end)
{
	offset = _offset;

	cells.assign(cells_begin, cells_end);

	grid.build_from_vertices(&cells);

	//grid.print();
}
void cells_container::set_clipping_planes(const std::vector<vec4>& _clipping_planes)
{
	clipping_planes = _clipping_planes;
}
void cells_container::grab_cell (size_t index)
{
	if (listener)
		listener->on_cell_grabbed(offset, index);
}
