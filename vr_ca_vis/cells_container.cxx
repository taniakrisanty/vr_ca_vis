// This source code is adapted from simple_primitive_container in vr_lab_test plugin

#include "cells_container.h"
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
	: cgv::base::node(_name), listener(_listener), extent(_extent), rotation(_rotation)//, vb_types(cgv::render::VBT_INDICES)
{
	debug_point = vec3(0, 0.5f, 0);
	
	srs.radius = 0.01f;
	
	brs.culling_mode = cgv::render::CullingMode::CM_BACKFACE;
	brs.use_group_color = true;

	show_gui = true;

	scale_matrix.identity();
	
	grid.set_clipping_planes(&clipping_planes);
}
std::string cells_container::get_type_name() const
{
	return "cells_container";
}
//bool cells_container::self_reflect(cgv::reflect::reflection_handler& rh)
//{
//	return rh.reflect_member("cell_type_visibilities", cell_type_visibilities);
//}
void cells_container::on_set(void* member_ptr)
{
	update_member(member_ptr);

	bool found = false;
	for (size_t i = 0; i < color_points_maps.size(); ++i) {
		for (size_t j = 0; j < color_points_maps[i].size(); ++j) {
			if (member_ptr == &color_points_maps[i][j]) {
				update_color_point(i, j, color_points_maps[i][j]);
				found = true;
				break;
			}
		}

		if (found) break;
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
			position_at_grab = cells->at(prim_idx).node;
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
			position_at_trigger = cells->at(prim_idx).node;
			grab_cell(prim_idx);
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
	grab_cell(SIZE_MAX);
	return false;
}
bool cells_container::compute_closest_point(const vec3& point, vec3& prj_point, vec3& prj_normal, size_t& primitive_idx)
{
	vec4 point_upscaled4(inv_scale_matrix * point.lift());
	vec3 point_upscaled(point_upscaled4 / point_upscaled4.w());

	float min_dist = std::numeric_limits<float>::max();

	regular_grid<cell>::result_entry res = grid.closest_point(point_upscaled);

	if (min_dist > res.sqr_distance)
	{
		min_dist = sqrt(res.sqr_distance);

		primitive_idx = res.prim;

		vec4 position_downscaled4(scale_matrix * cells->at(primitive_idx).node.lift());
		vec3 position_downscaled(position_downscaled4 / position_downscaled4.w());

		vec4 extent_downscaled4(scale_matrix * extent.lift());
		vec3 extent_downscaled(extent_downscaled4 / extent_downscaled4.w());

		vec3 p = point - position_downscaled;
		rotation.inverse_rotate(p);
		for (int i = 0; i < 3; ++i)
			p[i] = std::max(-0.5f * extent_downscaled[i], std::min(0.5f * extent_downscaled[i], p[i]));
		rotation.rotate(p);
		prj_point = p + position_downscaled;

		//std::cout << "Closest point from query point " << point << " = " << position_downscaled << std::endl;
	}

	return min_dist < std::numeric_limits<float>::max();
}
bool cells_container::compute_intersection(const vec3& ray_start, const vec3& ray_direction, float& hit_param, vec3& hit_normal, size_t& primitive_idx)
{
	vec4 ray_origin_upscaled4(inv_scale_matrix * ray_start.lift());
	vec3 ray_origin_upscaled(ray_origin_upscaled4 / ray_origin_upscaled4.w());

	hit_param = std::numeric_limits<float>::max();

	grid_traverser trav(ray_origin_upscaled, ray_direction, grid.get_cell_extents());
	for (int i = 0; ; ++i, trav++)
	{
		int index = grid.get_cell_index_to_grid_index(*trav);

		if (index < 0)
			break;

		std::vector<int> indices;
		grid.get_closest_indices(index, indices);

		if (indices.empty())
			continue;

		for (int pi : indices)
		{
			vec3 n;
			vec2 res;
			if (cgv::math::ray_box_intersection(ray_origin_upscaled - cells->at(pi).node, ray_direction, 0.5f * extent, res, n) == 0)
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
		vec4 position_downscaled4(scale_matrix * cells->at(primitive_idx).node.lift());
		vec3 position_downscaled(position_downscaled4 / position_downscaled4.w());

		vec4 extent_downscaled4(scale_matrix * extent.lift());
		vec3 extent_downscaled(extent_downscaled4 / extent_downscaled4.w());

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
void cells_container::init_frame(cgv::render::context& ctx)
{
	if (cells_out_of_date && use_vbo) {
		transmit_cells(ctx);
		cells_out_of_date = false;
	}
}
void cells_container::draw(cgv::render::context& ctx)
{
	// show box
	if (cells_end - cells_start > 0)
	{
		ctx.push_modelview_matrix();
		ctx.mul_modelview_matrix(scale_matrix);

		for (int i = 0; i < clipping_planes.size(); ++i)
			//for (int i = 0; clipping_plane_origins != NULL && i < clipping_plane_origins->size(); ++i)
			glEnable(GL_CLIP_DISTANCE0 + i);

		// save previous blend configuration
		GLboolean blend;
		glGetBooleanv(GL_BLEND, &blend);

		GLint blend_src, blend_dst;
		glGetIntegerv(GL_BLEND_SRC_ALPHA, &blend_src);
		glGetIntegerv(GL_BLEND_DST_ALPHA, &blend_dst);

		glDisable(GL_DEPTH_TEST);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		auto& br = ref_clipped_box_renderer(ctx);
		br.set_render_style(brs);
		// TODO copy to vertex_buffer
		// TODO ask about low frame rate during animation when using vertex buffer

		set_group_geometry(ctx, br);
		set_geometry(ctx, br);

		//rgb tmp_color;
		//if (prim_idx >= cells_start && prim_idx < cells_end) {
		//	tmp_color = cells->at(prim_idx).color;
		//	cells->at(prim_idx).color = get_modified_color(cells->at(prim_idx).color);
		//}
		br.set_extent(ctx, extent);
		//br.set_rotation_array(ctx, &rotation, cells.size());
		br.set_clipping_planes(clipping_planes);
		br.render(ctx, 0, cells_end - cells_start);
		//if (prim_idx >= cells_start && prim_idx < cells_end)
		//	cells->at(prim_idx).color = tmp_color;

		for (int i = 0; i < clipping_planes.size(); ++i)
			//for (int i = 0; clipping_plane_origins != NULL && i < clipping_plane_origins->size(); ++i)
			glDisable(GL_CLIP_DISTANCE0 + i);

		ctx.pop_modelview_matrix();

		// restore previous blend configuration
		if (blend)
			glEnable(GL_BLEND);
		else
			glDisable(GL_BLEND);

		glBlendFunc(blend_src, blend_dst);

		glEnable(GL_DEPTH_TEST);
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
		sr.set_color(ctx, rgb(0.3f, 0.3f, 0.3f));
		sr.render(ctx, 0, 1);
	}
}
void cells_container::create_gui()
{
	//if (begin_tree_node("Cell Sphere Rendering", srs, false)) {
	//	align("\a");
	//	add_gui("sphere_style", srs);
	//	align("\b");
	//	end_tree_node(srs);
	//}
	//if (begin_tree_node("Cell Box Rendering", brs, false)) {
	//	align("\a");
	//	add_gui("box_style", brs);
	//	align("\b");
	//	end_tree_node(brs);
	//}

	//if (begin_tree_node("Color Map", color_map, false)) {
	//	align("\a");
	//	for (std::map<float, rgb>::iterator i = color_points_map.begin(); i != color_points_map.end(); ++i) {
	//		add_member_control(this, "color", i->second);
	//	}
	//	align("\b");
	//	end_tree_node(color_map);
	//}

	for (size_t i = 0; i < cell_types.size(); ++i) {
		if (begin_tree_node(cell_types[i], color_points_maps[i])) {
			align("\a");
			//add_member_control(this, "show_cells", reinterpret_cast<bool&>(cell_types[i]), "check");
			for (auto& cp : color_points_maps[i])
				add_member_control(this, "color", cp.second);
			align("\b");
			end_tree_node(cell_types[i]);
		}
	}
}
void cells_container::set_scale_matrix(const mat4& _scale_matrix)
{
	if (scale_matrix != _scale_matrix)
	{
		scale_matrix = _scale_matrix;
		inv_scale_matrix = inv(_scale_matrix);

		// TODO update all clipping planes
	}
}
void cells_container::set_cell_types(const std::unordered_set<std::string>& _cell_types)
{
	cell_types.clear();

	size_t i = 0;
	for (const auto& ct : _cell_types) {
		cell_types.emplace_back(ct);

		add_color_point(i, 0.f, rgba(59.f / 255, 76.f / 255, 192.f / 255, 0.f));
		add_color_point(i, 1.f, rgba(180.f / 255, 4.f / 255, 38.f / 255, 0.f));
		
		++i;
	}
}
void cells_container::set_cells(const std::vector<cell>* _cells, size_t _cells_start, size_t _cells_end, const ivec3& extents)
{
	grid.cancel_build_from_vertices();

	if (cells != _cells || cells_end - cells_start != _cells_end - _cells_start) {
		recreate_vbo = true;
		cells_out_of_date = true;
	}

	cells = _cells;

	cells_start = _cells_start;
	cells_end = _cells_end;

	grid.build_from_vertices(cells, cells_start, cells_end, extents);
}
void cells_container::set_animate(bool animate)
{
	if (use_vbo) {
		if (animate)
			use_vbo = false;
	}
	else {
		if (!animate)
			use_vbo = true;
	}
}
void cells_container::add_color_point(size_t index, float t, rgba color)
{
	if (color_points_maps.size() <= index)
		color_points_maps.emplace_back();

	color_points_maps[index].emplace(t, color);

	if (color_maps.size() <= index)
		color_maps.emplace_back();

	color_maps[index].add_color_point(t, color);
	color_maps[index].add_opacity_point(t, color.alpha());

	update_color_points_vector();
}
void cells_container::update_color_point(size_t index, float t, rgba color)
{
	color_points_maps[index][t] = color;

	color_maps[index].clear();

	for (const auto& cp : color_points_maps[index]) {
		color_maps[index].add_color_point(cp.first, cp.second);
		color_maps[index].add_opacity_point(cp.first, cp.second.alpha());
	}

	update_color_points_vector();
}
void cells_container::remove_color_point(size_t index, float t)
{
	if (color_points_maps[index].erase(t) > 0)
	{
		color_maps[index].clear();

		for (const auto& cp : color_points_maps[index]) {
			color_maps[index].add_color_point(cp.first, cp.second);
			color_maps[index].add_opacity_point(cp.first, cp.second.alpha());
		}
	}

	update_color_points_vector();
}
void cells_container::update_color_points_vector()
{
	color_points_vector.clear();

	for (const auto& cm : color_maps) {
		std::vector<rgba> i = cm.interpolate(size_t(21));
		color_points_vector.insert(color_points_vector.end(), i.begin(), i.end());
	}
}
void cells_container::create_clipping_plane(const vec3& origin, const vec3& direction)
{
	vec4 scaled_origin4(inv_scale_matrix * origin.lift());
	vec3 scaled_origin(scaled_origin4 / scaled_origin4.w());

	clipping_planes.emplace_back(direction, -dot(scaled_origin, direction));
}
void cells_container::copy_clipping_plane(size_t index)
{
	clipping_planes.emplace_back(clipping_planes[index]);
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
//void cells_container::set_clipping_planes(const std::vector<vec4>& _clipping_planes)
//{
//	clipping_planes = _clipping_planes;
//
//	grid.set_clipping_planes(&clipping_planes);
//}
//void cells_container::set_clipping_planes(const std::vector<vec3>* _clipping_plane_origins, const std::vector<vec3>* _clipping_plane_directions)
//{
//	clipping_plane_origins = _clipping_plane_origins;
//	clipping_plane_directions = _clipping_plane_directions;
//
//	grid.set_clipping_planes(clipping_plane_origins, clipping_plane_directions);
//}
void cells_container::transmit_cells(cgv::render::context& ctx)
{
	std::vector<vec3> cell_positions;

	cell_ids.clear();

	for (size_t i = cells_start; i < cells_end; ++i) {
		const cell& c = cells->at(i);

		cell_positions.push_back(c.node);
		cell_ids.push_back(c.id);
	}

	if (recreate_vbo) {
		if (!vb_nodes.is_created())
			vb_nodes.create(ctx, cell_positions);
		
		//if (!vb_types.is_created())
		//	vb_types.create(ctx, cell_ids);
		
		recreate_vbo = false;
	}
	else {
		vb_nodes.replace(ctx, 0, &cell_positions[0], cell_positions.size());
		//vb_types.replace(ctx, 0, &cell_ids[0], cell_ids.size());
	}

	cells_out_of_date = false;
}
void cells_container::set_group_geometry(cgv::render::context& ctx, cgv::render::group_renderer& gr)
{
	if (!color_points_vector.empty())
		gr.set_group_colors(ctx, color_points_vector);
}
void cells_container::set_geometry(cgv::render::context& ctx, cgv::render::group_renderer& gr)
{
	if (cells_end - cells_start > 0) {
		if (use_vbo) {
			gr.set_position_array<vec3>(ctx, vb_nodes, 0, cells_end - cells_start);
		}
		else {
			gr.set_position_array(ctx, &cells->at(cells_start).node, cells_end - cells_start, sizeof(cell));
		}
		gr.set_color_array(ctx, &cells->at(cells_start).color, cells_end - cells_start, sizeof(cell));
		gr.set_group_index_array(ctx, cell_ids);
		//gr.set_group_index_array<unsigned int>(ctx, vb_types, 0, cells_end - cells_start);
	}
}
void cells_container::grab_cell(size_t index) const
{
	if (listener)
		listener->on_cell_grabbed(index);
}
