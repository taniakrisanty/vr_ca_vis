// This source code is adapted from simple_object in vr_lab_test plugin

#include "clipping_planes_bag.h"
#include <cgv/math/proximity.h>
#include <cgv/math/intersection.h>

cgv::render::shader_program clipping_planes_bag::prog;

clipping_planes_bag::rgb clipping_planes_bag::get_modified_color(const rgb& color) const
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
clipping_planes_bag::clipping_planes_bag(clipping_planes_bag_listener* _listener, const std::string& _name, const vec3& _position, const rgba& _color, const vec3& _extent, const quat& _rotation)
	: cgv::base::node(_name), listener(_listener), position(_position), color(_color), extent(_extent), rotation(_rotation)
{
	debug_point = position + 0.5f * extent;
	
	brs.rounding = true;
	brs.default_radius = 0.02f;

	srs.radius = 0.01f;
}
std::string clipping_planes_bag::get_type_name() const
{
	return "clipping_planes_bag";
}
void clipping_planes_bag::on_set(void* member_ptr)
{
	update_member(member_ptr);
	post_redraw();
}
bool clipping_planes_bag::focus_change(cgv::nui::focus_change_action action, cgv::nui::refocus_action rfa, const cgv::nui::focus_demand& demand, const cgv::gui::event& e, const cgv::nui::dispatch_info& dis_info)
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
		// nothing to be done because with do not use indices
		break;
	}
	return true;
}
void clipping_planes_bag::stream_help(std::ostream& os)
{
	os << "clipping_planes_bag: grab and point at it" << std::endl;
}
bool clipping_planes_bag::handle(const cgv::gui::event& e, const cgv::nui::dispatch_info& dis_info, cgv::nui::focus_request& request)
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
			position_at_trigger = position;
			grab_clipping_plane(dis_info.hid_id.kit_ptr);
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
bool clipping_planes_bag::compute_intersection(const vec3& ray_start, const vec3& ray_direction, float& hit_param, vec3& hit_normal, size_t& primitive_idx)
{
	// point, prj_point, and prj_normal are in table coordinate
	// position is in head coordinate

	vec4 position_in_table4(inv_model_transform * head_transform * position.lift());
	vec3 position_in_table(position_in_table4 / position_in_table4.w());

	vec3 rs = ray_start - position_in_table;
	vec3 rd = ray_direction;
	rotation.inverse_rotate(rs);
	rotation.inverse_rotate(rd);
	vec3 n;
	vec2 res;
	if (cgv::math::ray_box_intersection(rs, rd, 0.5f * extent, res, n) == 0)
		return false;
	if (res[0] < 0) {
		if (res[1] < 0)
			return false;
		hit_param = res[1];
	}
	else {
		hit_param = res[0];
	}
	rotation.rotate(n);
	hit_normal = n;

	if (hit_param < std::numeric_limits<float>::max()) {
#ifdef DEBUG
		std::cout << "clipping_planes_bag::compute_intersection query " << ray_start << " = " << position_in_table << " | hit param " << hit_param << " | hit normal " << hit_normal << std::endl;
#endif
		return true;
	}
	else {
		return false;
	}
}
bool clipping_planes_bag::init(cgv::render::context& ctx)
{
	cgv::render::ref_sphere_renderer(ctx, 1);

	auto& br = cgv::render::ref_box_renderer(ctx, 1);
	if (prog.is_linked())
		return true;
	return br.build_program(ctx, prog, brs);
}
void clipping_planes_bag::clear(cgv::render::context& ctx)
{
	cgv::render::ref_box_renderer(ctx, -1);
	cgv::render::ref_sphere_renderer(ctx, -1);
}
void clipping_planes_bag::draw(cgv::render::context& ctx)
{
	// show clipping planes bag
	//ctx.push_modelview_matrix();
	//ctx.mul_modelview_matrix(inv_model_transform * head_transform);

	//auto& br = cgv::render::ref_box_renderer(ctx);
	//br.set_render_style(brs);
	//if (brs.rounding)
	//	br.set_prog(prog);
	//br.set_position(ctx, position);
	//br.set_color_array(ctx, &color, 1);
	//br.set_secondary_color(ctx, get_modified_color(color));
	//br.set_extent(ctx, extent);
	////br.set_rotation_array(ctx, &rotation, 1);
	//br.render(ctx, 0, 1);

	//ctx.pop_modelview_matrix();

	// show points
	if (state == state_enum::idle)
		return;

	auto& sr = cgv::render::ref_sphere_renderer(ctx);
	sr.set_render_style(srs);
	sr.set_position(ctx, debug_point);
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
void clipping_planes_bag::create_gui()
{
	add_decorator(get_name(), "heading", "level=2");
	add_member_control(this, "color", color);
	add_member_control(this, "width", extent[0], "value_slider", "min=0.01;max=1;log=true");
	add_member_control(this, "height", extent[1], "value_slider", "min=0.01;max=1;log=true");
	add_member_control(this, "depth", extent[2], "value_slider", "min=0.01;max=1;log=true");
	add_gui("rotation", rotation, "direction", "options='min=-1;max=1;ticks=true'");
	if (begin_tree_node("style", brs)) {
		align("\a");
		add_gui("brs", brs);
		align("\b");
		end_tree_node(brs);
	}
}
void clipping_planes_bag::set_inverse_model_transform(const mat4& _inv_model_transform)
{
	inv_model_transform = _inv_model_transform;
}
void clipping_planes_bag::set_head_transform(const mat4& _head_transform)
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
void clipping_planes_bag::grab_clipping_plane(void* hid_kit) const
{
	if (listener)
		listener->on_clipping_plane_grabbed(hid_kit);
}
