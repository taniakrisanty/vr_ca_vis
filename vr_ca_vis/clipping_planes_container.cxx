// This source code is adapted from simple_primitive_container in vr_lab_test plugin

#include "clipping_planes_container.h"
#include <cgv/math/proximity.h>
#include <cgv/math/intersection.h>

cgv::render::shader_program clipping_planes_container::prog;

clipping_planes_container::clipping_planes_container(clipping_planes_container_listener* _listener, const std::string& name)
	: cgv::base::node(name), listener(_listener)
{
	debug_point = vec3(0, 0.5f, 0);
	srs.radius = 0.01f;
}
std::string clipping_planes_container::get_type_name() const
{
	return "clipping_planes_container";
}
void clipping_planes_container::on_set(void* member_ptr)
{
	update_member(member_ptr);

	size_t clipping_plane_index = SIZE_MAX;
	if (clipping_plane_index == SIZE_MAX && !origins.empty()) {
		if (member_ptr >= &origins[0] && member_ptr < &origins[0] + origins.size()) {
			clipping_plane_index = static_cast<vec3*>(member_ptr) - &origins[0];

			for (size_t i = 0; i < origins[clipping_plane_index].size(); ++i)
				update_member(&origins[clipping_plane_index][i]);
		}
	}
	if (clipping_plane_index == SIZE_MAX && !directions.empty()) {
		if (member_ptr >= &directions[0] && member_ptr < &directions[0] + directions.size()) {
			clipping_plane_index = static_cast<vec3*>(member_ptr) - &directions[0];
		}
	}

	if (clipping_plane_index < SIZE_MAX && listener)
		listener->on_clipping_plane_updated(clipping_plane_index, origins[clipping_plane_index], directions[clipping_plane_index]);

	post_redraw();
}
bool clipping_planes_container::focus_change(cgv::nui::focus_change_action action, cgv::nui::refocus_action rfa, const cgv::nui::focus_demand& demand, const cgv::gui::event& e, const cgv::nui::dispatch_info& dis_info)
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
void clipping_planes_container::stream_help(std::ostream& os)
{
	os << "clipping_planes_container: grab and point at it" << std::endl;
}
bool clipping_planes_container::handle(const cgv::gui::event& e, const cgv::nui::dispatch_info& dis_info, cgv::nui::focus_request& request)
{
	// ignore all events in idle mode
	if (state == state_enum::idle) {
		point_at_clipping_plane(SIZE_MAX);
		return false;
	}
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
			prim_idx = int(prox_info.primitive_index);
			position_at_grab = origins[prim_idx];

			point_at_clipping_plane(prim_idx);
		}
		else if (state == state_enum::grabbed) {
			debug_point = prox_info.hit_point;
			end_drag_clipping_plane(prim_idx, position_at_grab + prox_info.query_point - query_point_at_grab);
		}
		post_redraw();
		return true;
	}
	return false;
}
bool clipping_planes_container::compute_closest_point(const vec3& point, vec3& prj_point, vec3& prj_normal, size_t& primitive_idx)
{
	float min_dist = std::numeric_limits<float>::max();
	
	vec3 q;
	for (size_t i = 0; i < origins.size(); ++i) {
		float dist = signed_distance_from_clipping_plane(i, point);
		if (abs(dist) < min_dist) {
			prj_point = point - (dist * directions[i]);
			
			// closest point is outside wireframe box
			bool outside = false;
			for (size_t j = 0; j < 3; ++j) {
				if (prj_point[j] < 0.f || prj_point[j] > 1.f) {
					outside = true;
					break;
				}
			}
			if (outside) continue;
			
			primitive_idx = i;
			prj_normal = directions[i];
			min_dist = abs(dist);
		}
	}

	return min_dist < std::numeric_limits<float>::max();
}
bool clipping_planes_container::init(cgv::render::context& ctx)
{
	cgv::render::ref_sphere_renderer(ctx, 1);
	return true;
}
void clipping_planes_container::clear(cgv::render::context& ctx)
{
	cgv::render::ref_sphere_renderer(ctx, -1);
}
void clipping_planes_container::draw(cgv::render::context& ctx)
{
	// show clipping planes
	if (!origins.empty())
	{
		// save previous blend configuration
		GLboolean blend;
		glGetBooleanv(GL_BLEND, &blend);

		GLint blend_src, blend_dst;
		glGetIntegerv(GL_BLEND_SRC_ALPHA, &blend_src);
		glGetIntegerv(GL_BLEND_DST_ALPHA, &blend_dst);

		for (size_t i = 0; i < origins.size(); ++i)
		{
			glDisable(GL_DEPTH_TEST);

			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			draw_clipping_plane(i, ctx);

			glDisable(GL_BLEND);

			glEnable(GL_DEPTH_TEST);
		}

		// restore previous blend configuration
		if (blend)
			glEnable(GL_BLEND);
		else
			glDisable(GL_BLEND);

		glBlendFunc(blend_src, blend_dst);
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
void clipping_planes_container::draw_clipping_plane(size_t index, cgv::render::context & ctx)
{
	std::vector<vec3> polygon;
	construct_clipping_plane(index, polygon);

	if (polygon.size() < 3)
		return;

	ctx.ref_default_shader_program().enable(ctx);
	ctx.set_color(colors[index]);

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
}
/// returns the 3D-texture coordinates of the polygon edges describing the cutting plane through
/// the volume
void clipping_planes_container::construct_clipping_plane(size_t index, std::vector<vec3>& polygon) const
{
	/************************************************************************************
	 Classify the volume box corners (vertices) as inside or outside vertices.
				   Use a unit cube for the volume box since the vertex coordinates of the unit cube
				   correspond to the texture coordinates for the volume.
				   You can use the signed_distance_from_clipping_plane()-method to get the
				   distance between each box corner and the slice. Assume that outside vertices
				   have a positive distance.*/

	static vec3 corners[8] = { vec3(0, 0, 0), vec3(1, 0, 0), vec3(1, 1, 0), vec3(0, 1, 0), vec3(0, 0, 1), vec3(1, 0, 1), vec3(1, 1, 1), vec3(0, 1, 1) };
	float values[8];
	bool corner_classifications[8]; // true = outside, false = inside

	for (int i = 0; i < 8; ++i)
	{
		float value = signed_distance_from_clipping_plane(index, corners[i]);

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

	static int a_corner_comparisons[12] = { 0, 3, 7, 4, 0, 4, 5, 1, 0, 1, 2, 3 };
	static int b_corner_comparisons[12] = { 1, 2, 6, 5, 3, 7, 6, 2, 4, 5, 6, 7 };
	static int comparison_to_edges[12] = { 0, 2, 6, 4, 3, 7, 5, 1, 8, 9, 10, 11 };

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
float clipping_planes_container::signed_distance_from_clipping_plane(size_t index, const vec3& p) const
{
	/************************************************************************************
	 The signed distance between the given point p and the slice which
				   is defined through clipping_plane_normal and clipping_plane_distance. */

	return dot(directions[index], p - origins[index]);
}
void clipping_planes_container::create_gui()
{
	if (begin_tree_node("Clipping Planes", origins)) {
		align("\a");

		for (size_t i = 0; i < origins.size(); ++i)
		{
			if (begin_tree_node("clipping plane " + std::to_string(i + 1), origins[i])) {
				align("\a");
				add_member_control(this, "color", colors[i]);
				add_decorator("origin", "heading", "level=3");
				add_member_control(this, "x", origins[i][0], "value_slider", "ticks=true;min=0;max=1;log=true");
				add_member_control(this, "y", origins[i][1], "value_slider", "ticks=true;min=0;max=1;log=true");
				add_member_control(this, "z", origins[i][2], "value_slider", "ticks=true;min=0;max=1;log=true");
				add_gui("direction", directions[i], "direction");
				align("\b");
				end_tree_node(origins[i]);
			}
		}
		align("\b");
		end_tree_node(origins);
	}
}
void clipping_planes_container::create_clipping_plane(const vec3& origin, const vec3& direction, const rgba& color)
{
	origins.emplace_back(origin);
	directions.emplace_back(direction);
	colors.emplace_back(color);

	post_recreate_gui();
}
void clipping_planes_container::delete_clipping_plane(size_t index, size_t count)
{
	origins.erase(origins.begin() + index, origins.begin() + index + count);
	directions.erase(directions.begin() + index, directions.begin() + index + count);
	colors.erase(colors.begin() + index, colors.begin() + index + count);

	post_recreate_gui();
}
void clipping_planes_container::clear_clipping_planes()
{
	origins.clear();
	directions.clear();
	colors.clear();

	post_recreate_gui();
}
size_t clipping_planes_container::get_num_clipping_planes() const
{
	return origins.size();
}
void clipping_planes_container::point_at_clipping_plane(size_t index)
{
	if (listener)
		listener->on_clipping_plane_pointed_at(index);
}
void clipping_planes_container::end_drag_clipping_plane(size_t index, vec3 p)
{
	for (size_t i = 0; i < 3; ++i)
		p[i] = std::max(0.f, std::min(p[i], 1.f));

	if (origins[index] != p) {
		origins[index] = p;

		on_set(origins[index]);
	}
}
