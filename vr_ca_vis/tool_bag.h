// This source code is adapted from simple_object in vr_lab_test plugin

#pragma once

#include <cgv/base/node.h>
#include <cgv/render/drawable.h>
#include <cg_nui/focusable.h>
#include <cg_nui/pointable.h>
#include <cgv/gui/provider.h>
#include <cgv_gl/box_renderer.h>
#include <cgv_gl/sphere_renderer.h>

class tool_bag_listener
{
public:
	virtual void on_tool_grabbed(int id, void* kit_ptr) = 0;
};

class tool_bag :
	public cgv::base::node,
	public cgv::render::drawable,
	public cgv::nui::focusable,
	public cgv::nui::pointable,
	public cgv::gui::provider
{
	cgv::render::box_render_style brs;
	cgv::render::sphere_render_style srs;
	vec3 debug_point;
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
	tool_bag_listener* listener;

	// invert model transform applied by bag's parent
	mat4 inv_model_transform;
	mat4 head_transform;

	// geometry of tool bag
	int show;
	std::vector<int> ids;
	std::vector<std::string> names;
	std::vector<vec3> positions;
	std::vector<vec3> extents;
	std::vector<rgba> colors;
	quat rotation;

	// hid with focus on object
	cgv::nui::hid_identifier hid_id;
	// index of focused primitive
	int prim_idx = -1;
	// state of object
	state_enum state = state_enum::idle;
public:
	tool_bag(tool_bag_listener *_listener, const std::string& _name);
	/// return type name
	std::string get_type_name() const;
	/// callback on member updates to keep data structure consistent
	void on_set(void* member_ptr);

	//@name cgv::nui::focusable interface
	//@{
	bool focus_change(cgv::nui::focus_change_action action, cgv::nui::refocus_action rfa, const cgv::nui::focus_demand& demand, const cgv::gui::event& e, const cgv::nui::dispatch_info& dis_info);
	void stream_help(std::ostream& os);
	bool handle(const cgv::gui::event& e, const cgv::nui::dispatch_info& dis_info, cgv::nui::focus_request& request);
	//@}

	bool compute_intersection(const vec3& ray_start, const vec3& ray_direction, float& hit_param, vec3& hit_normal, size_t& primitive_idx);

	//@name cgv::render::drawable interface
	//@{
	/// initialization called once per context creation
	bool init(cgv::render::context& ctx);
	/// called before context destruction to clean up GPU objects
	void clear(cgv::render::context& ctx);
	/// draw scene here
	void draw(cgv::render::context& ctx);
	//@}
	/// cgv::gui::provider function to create classic UI
	void create_gui();

	void set_inverse_model_transform(const mat4& _inverse_model_transform);
	void set_head_transform(const mat4& _head_transform);

	void add_tool(int id, const std::string& name, const vec3& position, const vec3& extent = vec3(1.f), const rgba& color = rgba(0.f, 1.f, 1.f, 0.1f));

	void toggle_visibility();
private:
	void grab_tool(int index, void* hid_kit) const;
};

typedef cgv::data::ref_ptr<tool_bag> tool_bag_ptr;
