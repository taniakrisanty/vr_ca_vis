// This source code is adapted from simple_object in vr_lab_test plugin

#pragma once

#include <cgv/base/node.h>
#include <cgv/render/drawable.h>
#include <cg_nui/focusable.h>
#include <cg_nui/pointable.h>
#include <cgv/gui/provider.h>
#include <cgv_gl/box_renderer.h>
#include <cgv_gl/sphere_renderer.h>

class clipping_planes_bag_listener
{
public:
	virtual void on_clipping_plane_grabbed(void* kit_ptr) = 0;
};

class clipping_planes_bag :
	public cgv::base::node,
	public cgv::render::drawable,
	public cgv::nui::focusable,
	public cgv::nui::pointable,
	public cgv::gui::provider
{
	cgv::render::box_render_style brs;
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
	clipping_planes_bag_listener* listener;

	// invert model transform applied by bag's parent
	mat4 inv_model_transform;
	mat4 head_transform;

	// geometry of bag with color
	vec3 position;
	vec3 extent;
	quat rotation;
	rgba color;
	// hid with focus on object
	cgv::nui::hid_identifier hid_id;
	// state of object
	state_enum state = state_enum::idle;
	/// return color modified based on state
	rgb get_modified_color(const rgb& color) const;
public:
	clipping_planes_bag(clipping_planes_bag_listener *_listener, const std::string& _name, const vec3& _position, const rgba& _color = rgba(0.f, 1.f, 1.f, 0.1f), const vec3& _extent = vec3(1.f, 1.f, 0.1f), const quat& _rotation = quat(1, 0, 0, 0));
	std::string get_type_name() const;
	void on_set(void* member_ptr);

	bool focus_change(cgv::nui::focus_change_action action, cgv::nui::refocus_action rfa, const cgv::nui::focus_demand& demand, const cgv::gui::event& e, const cgv::nui::dispatch_info& dis_info);
	void stream_help(std::ostream& os);
	bool handle(const cgv::gui::event& e, const cgv::nui::dispatch_info& dis_info, cgv::nui::focus_request& request);

	bool compute_intersection(const vec3& ray_start, const vec3& ray_direction, float& hit_param, vec3& hit_normal, size_t& primitive_idx);

	bool init(cgv::render::context& ctx);
	void clear(cgv::render::context& ctx);
	void draw(cgv::render::context& ctx);

	void create_gui();

	void set_inverse_model_transform(const mat4& _inverse_model_transform);
	void set_head_transform(const mat4& _head_transform);
private:
	void grab_clipping_plane(void* hid_kit) const;
};

typedef cgv::data::ref_ptr<clipping_planes_bag> clipping_planes_bag_ptr;
