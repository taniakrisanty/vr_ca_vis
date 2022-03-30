// This source code is adapted from simple_object in vr_lab_test plugin

#include "tool_bag.h"
#include <cgv/math/proximity.h>
#include <cgv/math/intersection.h>

cgv::render::shader_program tool_bag::prog;

tool_bag::tool_bag(tool_bag_listener* _listener, const std::string& _name)
	: cgv::base::node(_name), listener(_listener)
{
	show = false;

	debug_point = vec3(0, 0.5f, 0);

	brs.rounding = true;
	brs.default_radius = 0.02f;

	srs.radius = 0.01f;
}
std::string tool_bag::get_type_name() const
{
	return "tool_bag";
}
void tool_bag::on_set(void* member_ptr)
{
	update_member(member_ptr);
	post_redraw();
}
bool tool_bag::focus_change(cgv::nui::focus_change_action action, cgv::nui::refocus_action rfa, const cgv::nui::focus_demand& demand, const cgv::gui::event& e, const cgv::nui::dispatch_info& dis_info)
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
		break;
	}
	return true;
}
void tool_bag::stream_help(std::ostream& os)
{
	os << "tool_bag: point at it" << std::endl;
}
bool tool_bag::handle(const cgv::gui::event& e, const cgv::nui::dispatch_info& dis_info, cgv::nui::focus_request& request)
{
	// ignore all events in idle mode
	if (state == state_enum::idle)
		return false;
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
			position_at_trigger = positions[prim_idx];
			grab_tool(prim_idx, dis_info.hid_id.kit_ptr);
		}
		else if (state == state_enum::triggered) {
			// if we still have an intersection point, use as debug point
			if (inter_info.ray_param != std::numeric_limits<float>::max())
				debug_point = inter_info.hit_point;
			// to be save even without new intersection, find closest point on ray to hit point at trigger
			vec3 q = cgv::math::closest_point_on_line_to_point(inter_info.ray_origin, inter_info.ray_direction, hit_point_at_trigger);
			//position = position_at_trigger + q - hit_point_at_trigger;
		}
		post_redraw();
		return true;
	}
	return false;
}
bool tool_bag::compute_intersection(const vec3& ray_start, const vec3& ray_direction, float& hit_param, vec3& hit_normal, size_t& primitive_idx)
{
	// point, prj_point, and prj_normal are in table coordinate
	// position is in head coordinate

	hit_param = std::numeric_limits<float>::max();
	for (size_t i = 0; i < positions.size(); ++i) {
		vec4 position_in_table4(inv_model_transform * head_transform * positions[i].lift());
		vec3 position_in_table(position_in_table4 / position_in_table4.w());

		vec3 rs = ray_start - position_in_table;
		vec3 rd = ray_direction;
		rotation.inverse_rotate(rs);
		rotation.inverse_rotate(rd);
		vec3 n;
		vec2 res;
		if (cgv::math::ray_box_intersection(rs, rd, 0.5f * extents[i], res, n) == 0)
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
bool tool_bag::init(cgv::render::context& ctx)
{
	cgv::render::ref_sphere_renderer(ctx, 1);

	auto& br = cgv::render::ref_box_renderer(ctx, 1);
	if (prog.is_linked())
		return true;
	return br.build_program(ctx, prog, brs);

	return true;
}
void tool_bag::clear(cgv::render::context& ctx)
{
	cgv::render::ref_box_renderer(ctx, -1);
	cgv::render::ref_sphere_renderer(ctx, -1);
}
void tool_bag::draw(cgv::render::context& ctx)
{
	// show clipping planes bag
	if (show) {
		ctx.push_modelview_matrix();
		ctx.mul_modelview_matrix(inv_model_transform * head_transform);

		auto& br = cgv::render::ref_box_renderer(ctx);
		br.set_render_style(brs);
		if (brs.rounding)
			br.set_prog(prog);
		br.set_position_array(ctx, positions);
		br.set_color_array(ctx, colors);
		br.set_extent_array(ctx, extents);
		//br.set_rotation_array(ctx, &rotation, 1);
		br.render(ctx, 0, positions.size());

		ctx.pop_modelview_matrix();
	}

	// show points
	if (state == state_enum::idle)
		return;

	auto& sr = cgv::render::ref_sphere_renderer(ctx);
	sr.set_render_style(srs);
	sr.set_position(ctx, debug_point);
	rgb color(0.5f, 0.5f, 0.5f);
	sr.set_color_array(ctx, &color, 1);
	sr.render(ctx, 0, 1);
}
void tool_bag::create_gui()
{
	if (begin_tree_node("Tool Bag", ids)) {
		align("\a");

		if (!ids.empty()) add_member_control(this, "show", reinterpret_cast<bool&>(show), "check");

		for (size_t i = 0; i < ids.size(); ++i)
		{
			if (begin_tree_node(names[i], ids[i])) {
				align("\a");
				add_member_control(this, "color", colors[i]);
				add_decorator("position", "heading", "level=3");
				add_member_control(this, "x", positions[i][0], "value_slider", "ticks=true;min=-1;max=1;log=true");
				add_member_control(this, "y", positions[i][1], "value_slider", "ticks=true;min=-1;max=1;log=true");
				add_member_control(this, "z", positions[i][2], "value_slider", "ticks=true;min=-1;max=1;log=true");
				add_decorator("extent", "heading", "level=3");
				add_member_control(this, "x", extents[i][0], "value_slider", "min=0.01;max=1;log=true");
				add_member_control(this, "y", extents[i][1], "value_slider", "min=0.01;max=1;log=true");
				add_member_control(this, "z", extents[i][2], "value_slider", "min=0.01;max=1;log=true");
				align("\b");
				end_tree_node(ids[i]);
			}
		}
		align("\b");
		end_tree_node(ids);
	}
}
void tool_bag::set_inverse_model_transform(const mat4& _inv_model_transform)
{
	inv_model_transform = _inv_model_transform;
}
void tool_bag::set_head_transform(const mat4& _head_transform)
{
	if (head_transform != _head_transform) {
		head_transform = _head_transform;

		mat3 rotation_matrix;

		rotation_matrix.set_col(0, normalize(head_transform.col(0)));
		rotation_matrix.set_col(1, normalize(head_transform.col(1)));
		rotation_matrix.set_col(2, normalize(head_transform.col(2)));

		rotation = quat(rotation_matrix);
	}
}
void tool_bag::add_tool(int id, const std::string& name, const vec3& position, const vec3& extent, const rgba& color)
{
	ids.push_back(id);
	names.push_back(name);
	positions.push_back(position);
	extents.push_back(extent);
	colors.push_back(color);
}
void tool_bag::grab_tool(int index, void* hid_kit) const
{
	if (listener)
		listener->on_tool_grabbed(ids[index], hid_kit);
}
