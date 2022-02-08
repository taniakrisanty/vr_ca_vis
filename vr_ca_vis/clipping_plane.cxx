// This source code is adapted from simple_object in vr_lab_test plugin

#include "clipping_plane.h"
#include <cgv/math/proximity.h>
#include <cgv/math/intersection.h>

cgv::render::shader_program clipping_plane::prog;

clipping_plane::rgb clipping_plane::get_modified_color(const rgb& color) const
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
clipping_plane::clipping_plane(const std::string& _name, const vec3& _origin, const vec3& _direction, const rgba& _color, const quat& _rotation)
	: cgv::base::node(_name), origin(_origin), direction(_direction), color(_color), rotation(_rotation)
{
	debug_point = origin;
	brs.rounding = true;
	brs.default_radius = 0.02f;
}
std::string clipping_plane::get_type_name() const
{
	return "clipping plane";
}
void clipping_plane::on_set(void* member_ptr)
{
	update_member(member_ptr);
	post_redraw();
}
bool clipping_plane::focus_change(cgv::nui::focus_change_action action, cgv::nui::refocus_action rfa, const cgv::nui::focus_demand& demand, const cgv::gui::event& e, const cgv::nui::dispatch_info& dis_info)
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
void clipping_plane::stream_help(std::ostream& os)
{
	os << "clipping_plane: grab and point at it" << std::endl;
}
bool clipping_plane::handle(const cgv::gui::event& e, const cgv::nui::dispatch_info& dis_info, cgv::nui::focus_request& request)
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
			drag_begin(request, false, original_config);
		}
		else {
			drag_end(request, original_config);
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
			position_at_grab = origin;
		}
		else if (state == state_enum::grabbed) {
			debug_point = prox_info.hit_point;
			origin = position_at_grab + prox_info.query_point - query_point_at_grab;
		}
		post_redraw();
		return true;
	}
	// hid independent check if object is triggered during pointing
	if (is_trigger_change(e, pressed)) {
		if (pressed) {
			state = state_enum::triggered;
			on_set(&state);
			drag_begin(request, true, original_config);
		}
		else {
			drag_end(request, original_config);
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
			position_at_trigger = origin;
		}
		else if (state == state_enum::triggered) {
			// if we still have an intersection point, use as debug point
			if (inter_info.ray_param != std::numeric_limits<float>::max())
				debug_point = inter_info.hit_point;
			// to be save even without new intersection, find closest point on ray to hit point at trigger
			vec3 q = cgv::math::closest_point_on_line_to_point(inter_info.ray_origin, inter_info.ray_direction, hit_point_at_trigger);
			origin = position_at_trigger + q - hit_point_at_trigger;
		}
		post_redraw();
		return true;
	}
	return false;
}
bool clipping_plane::compute_closest_point(const vec3& point, vec3& prj_point, vec3& prj_normal, size_t& primitive_idx)
{
	vec3 p = point - origin;
	rotation.inverse_rotate(p);
	//for (int i = 0; i < 3; ++i)
	//	p[i] = std::max(-0.5f * extent[i], std::min(0.5f * extent[i], p[i]));
	rotation.rotate(p);
	prj_point = p + origin;
	return true;
}
bool clipping_plane::compute_intersection(const vec3& ray_start, const vec3& ray_direction, float& hit_param, vec3& hit_normal, size_t& primitive_idx)
{
	vec3 ro = ray_start - origin;
	vec3 rd = ray_direction;
	rotation.inverse_rotate(ro);
	rotation.inverse_rotate(rd);
	vec3 n;
	vec2 res;
	if (cgv::math::ray_box_intersection(ro, rd, 0.5f * vec3(1.0), res, n) == 0)
		return false;
	if (res[0] < 0) {
		if (res[1] < 0)
			return false;
		hit_param = res[1];
	}
	else {
		hit_param = res[0];
	}
	hit_normal = n;
	rotation.rotate(n);
	return true;
}
bool clipping_plane::init(cgv::render::context& ctx)
{
	cgv::render::ref_sphere_renderer(ctx, 1);
	auto& br = cgv::render::ref_box_renderer(ctx, 1);
	if (prog.is_linked())
		return true;
	return br.build_program(ctx, prog, brs);
}
void clipping_plane::clear(cgv::render::context& ctx)
{
	cgv::render::ref_box_renderer(ctx, -1);
	cgv::render::ref_sphere_renderer(ctx, -1);
}
void clipping_plane::draw(cgv::render::context& ctx)
{
	// show clipping plane
	bool clipping_plane_drawn = false;

	ctx.push_modelview_matrix();
	ctx.mul_modelview_matrix(modelview_matrix);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	clipping_plane_drawn = draw_clipping_plane(ctx);
	glDisable(GL_BLEND);

	ctx.pop_modelview_matrix();

	// show points
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
bool clipping_plane::draw_clipping_plane(cgv::render::context & ctx)
{
	std::vector<vec3> polygon;
	construct_clipping_plane(polygon);

	if (polygon.size() < 3)
		return false;

	ctx.ref_default_shader_program().enable(ctx);
	ctx.set_color(rgba(0.0f, 1.0f, 1.0f, 0.1f));

	std::vector<float> N, V, T;
	std::vector<int> FN, F;

	vec3 a(polygon[0] - polygon[1]);
	vec3 b(polygon[0] - polygon[2]);
	vec3 n(cross(a, b));

	for (int i = 0; i < 3; ++i)
	{
		N.push_back(n[i]);
	}

	int i = 0;
	for (auto point : polygon)
	{
		V.push_back(point.x());
		V.push_back(point.y());
		V.push_back(point.z());

		FN.push_back(0);
		F.push_back(i++);
	}

	ctx.draw_faces(V.data(), N.data(), 0, F.data(), FN.data(), F.data(), 1, polygon.size());

	ctx.ref_default_shader_program().disable(ctx);

	return true;
}
/// returns the 3D-texture coordinates of the polygon edges describing the cutting plane through
/// the volume
void clipping_plane::construct_clipping_plane(std::vector<vec3>& polygon)
{
	/************************************************************************************
	 Classify the volume box corners (vertices) as inside or outside vertices.
				   Use a unit cube for the volume box since the vertex coordinates of the unit cube
				   correspond to the texture coordinates for the volume.
				   You can use the signed_distance_from_clipping_plane()-method to get the
				   distance between each box corner and the slice. Assume that outside vertices
				   have a positive distance.*/

	vec3 corners[8] = { vec3(0, 0, 0), vec3(1, 0, 0), vec3(1, 1, 0), vec3(0, 1, 0), vec3(0, 0, 1), vec3(1, 0, 1), vec3(1, 1, 1), vec3(0, 1, 1) };
	float values[8];
	bool corner_classifications[8]; // true = outside, false = inside

	for (int i = 0; i < 8; ++i)
	{
		float value = signed_distance_from_clipping_plane(corners[i]);

		values[i] = value;
		corner_classifications[i] = value >= 0;
	}

	/************************************************************************************
	 Construct the edge points on each edge connecting differently classified
				   corners. Remember that the edge point coordinates are in range [0,1] for
				   all dimensions since they are 3D-texture coordinates. These points are
				   stored in the polygon-vector.
	 Arrange the points along face adjacencies for easier tessellation of the
				   polygon. Store the ordered edge points in the polygon-vector. Create your own
				   helper structures for edge-face adjacenies etc.*/

	int a_corner_comparisons[12] = { 0, 3, 7, 4, 0, 4, 5, 1, 0, 1, 2, 3 };
	int b_corner_comparisons[12] = { 1, 2, 6, 5, 3, 7, 6, 2, 4, 5, 6, 7 };
	int  comparison_to_edges[12] = { 0, 2, 6, 4, 3, 7, 5, 1, 8, 9, 10, 11 };

	std::vector<vec3> p;

	for (int i = 0; i < 12; ++i)
	{
		int a_corner_index = a_corner_comparisons[i];
		int b_corner_index = b_corner_comparisons[i];

		if (corner_classifications[a_corner_index] != corner_classifications[b_corner_index])
		{
			vec3 coord = corners[a_corner_index];
			float a_value = abs(values[a_corner_index]);
			float b_value = abs(values[b_corner_index]);

			float new_value = a_value / (a_value + b_value);

			coord(i / 4) = new_value;

			p.push_back(coord);
		}
	}

	if (p.empty())
		return;

	vec3 p0 = p[0];
	p.erase(p.begin());

	polygon.push_back(p0);

	while (p.size() > 1)
	{
		bool found = false;

		for (unsigned int i = 0; i < 3; ++i)
		{
			if (p0(i) < std::numeric_limits<float>::epsilon() || p0(i) > 1 - std::numeric_limits<float>::epsilon())
			{
				for (unsigned int j = 0; j < p.size(); ++j)
				{
					vec3 f = p[j];

					if (fabs(f(i) - p0(i)) < std::numeric_limits<float>::epsilon())
					{
						p.erase(p.begin() + j);

						polygon.push_back(f);

						p0 = f;
						found = true;
						break;
					}
				}
			}

			if (found) break;
		}
	}

	polygon.push_back(p[0]);
}
float clipping_plane::signed_distance_from_clipping_plane(const vec3& p)
{
	/************************************************************************************
	 The signed distance between the given point p and the slice which
				   is defined through clipping_plane_normal and clipping_plane_distance. */

	return dot(direction, p - origin);
}
void clipping_plane::create_gui()
{
	add_decorator(get_name(), "heading", "level=2");
	add_member_control(this, "color", color);
	//add_member_control(this, "width", extent[0], "value_slider", "min=0.01;max=1;log=true");
	//add_member_control(this, "height", extent[1], "value_slider", "min=0.01;max=1;log=true");
	//add_member_control(this, "depth", extent[2], "value_slider", "min=0.01;max=1;log=true");
	add_gui("rotation", rotation, "direction", "options='min=-1;max=1;ticks=true'");
	if (begin_tree_node("style", brs)) {
		align("\a");
		add_gui("brs", brs);
		align("\b");
		end_tree_node(brs);
	}
}
void clipping_plane::set_modelview_matrix(const mat4& modelview_matrix)
{
	this->modelview_matrix = modelview_matrix;
}
void clipping_plane::set_origin_and_direction(const vec3& origin, const vec3& direction)
{
	this->origin = origin;
	this->direction = direction;
}
