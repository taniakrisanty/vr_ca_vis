// This source code is adapted from simple_primitive_container in vr_lab_test plugin

#pragma once

#include <cgv/base/node.h>
#include <cgv/render/drawable.h>
#include <cg_nui/focusable.h>
#include <cg_nui/pointable.h>
#include <cg_nui/grabable.h>
#include <cgv/gui/provider.h>
#include <cgv_gl/box_renderer.h>
#include <cgv_gl/sphere_renderer.h>

class clipping_planes_container :
	public cgv::base::node,
	public cgv::render::drawable,
	public cgv::nui::focusable,
	public cgv::nui::grabable,
	public cgv::nui::pointable,
	public cgv::gui::provider
{
	cgv::render::sphere_render_style srs;
	vec3 debug_point;
	vec3 query_point_at_grab, position_at_grab;
	vec3 hit_point_at_trigger, position_at_trigger;
	cgv::nui::focus_configuration original_config;
	static cgv::render::shader_program prog;
public:
	// different possible object states
	enum class state_enum {
		idle,
		close,
		pointed,
		grabbed,
		triggered
	};

protected:
	//mat4 modelview_matrix;

	// geometry of clipping planes with color

	std::vector<vec3> origins;
	std::vector<vec3> directions;
	std::vector<rgba> colors;
	quat rotation;
	// hid with focus on object
	cgv::nui::hid_identifier hid_id;
	// index of focused primitive
	int prim_idx = -1;
	// state of object
	state_enum state = state_enum::idle;
	/// return color modified based on state
	rgb get_modified_color(const rgb& color) const;
public:
	clipping_planes_container(const std::string& name);
	std::string get_type_name() const;
	void on_set(void* member_ptr);

	bool focus_change(cgv::nui::focus_change_action action, cgv::nui::refocus_action rfa, const cgv::nui::focus_demand& demand, const cgv::gui::event& e, const cgv::nui::dispatch_info& dis_info);
	void stream_help(std::ostream& os);
	bool handle(const cgv::gui::event& e, const cgv::nui::dispatch_info& dis_info, cgv::nui::focus_request& request);

	bool compute_closest_point(const vec3& point, vec3& prj_point, vec3& prj_normal, size_t& primitive_idx);
	bool compute_intersection(const vec3& ray_start, const vec3& ray_direction, float& hit_param, vec3& hit_normal, size_t& primitive_idx);

	bool init(cgv::render::context& ctx);
	void clear(cgv::render::context& ctx);
	void draw(cgv::render::context& ctx);

	void create_gui();

	//void set_modelview_matrix(const mat4& modelview_matrix);
	void create_clipping_plane(const vec3& origin, const vec3& direction, const rgba& color = rgba(0.f, 1.f, 1.f, 0.1f));
	void copy_clipping_plane(size_t index, const rgba& color = rgba(0.f, 1.f, 1.f, 0.1f));
	void delete_clipping_plane(size_t index, size_t count = 1);
	void clear_clipping_planes();
private:
	void draw_clipping_plane(size_t index, cgv::render::context& ctx);
	void construct_clipping_plane(size_t index, std::vector<vec3>& polygon);
	float signed_distance_from_clipping_plane(size_t index, const vec3& p);
};

typedef cgv::data::ref_ptr<clipping_planes_container> clipping_planes_container_ptr;