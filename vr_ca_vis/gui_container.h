// This source code is adapted from simple_primitive_container in vr_lab_test plugin

#pragma once

#include <cgv/base/node.h>
#include <cgv/render/drawable.h>
#include <cg_nui/focusable.h>
#include <cg_nui/pointable.h>
#include <cgv/gui/provider.h>
#include <cgv_gl/sphere_renderer.h>
#include <cgv_gl/cone_renderer.h>
#include <cgv_glutil/color_map.h>

#include <unordered_map>

#include "cell_data.h"
#include "clipped_box_renderer.h"
#include "control_sphere_renderer.h"
#include "regular_grid.h"

class gui_container_listener
{
public:
	virtual uint32_t on_create_label_requested(const std::string& text, const cgv::render::render_types::rgba& bgclr, const vec3& position, const cgv::render::render_types::quat& rotation) = 0;
	virtual void on_label_pointed_at(size_t index) = 0;
};

class gui_container :
	public cgv::base::node,
	public cgv::render::drawable,
	public cgv::nui::focusable,
	public cgv::nui::pointable,
	public cgv::gui::provider
{
	clipped_box_render_style brs;
	control_sphere_render_style csrs;
	cgv::render::sphere_render_style srs;
	cgv::render::cone_render_style crs;
	vec3 debug_point;
	vec3 hit_point_at_trigger, position_at_trigger;
	cgv::nui::focus_configuration original_config;
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
	gui_container_listener* listener;

	// invert model transform applied by gui's parent
	mat4 inv_model_transform;
	mat4 left_controller_transform;

	std::vector<vec3> positions;
	vec3 extent;
	quat rotation;

	std::vector<uint32_t> menu_labels;

	// cell types (ct1, ct2, etc)
	std::unordered_map<std::string, cell_type> cell_types;

	// color map
	std::vector<cgv::glutil::color_map> color_maps;
	std::vector<std::map<unsigned int, rgba>> color_points_maps;

	// per group information
	std::vector<rgba> default_colors;

	std::vector<rgba> group_colors;
	std::vector<bool> group_colors_overrides;

	// visibility filter by id
	std::vector<int> visibilities;

	std::vector<int> show_all_checks;
	std::vector<int> hide_all_checks;

	std::vector<int> show_checks;

	// hid with focus on object
	cgv::nui::hid_identifier hid_id;
	// index of focused primitive
	int prim_idx = -1;
	// state of object
	state_enum state = state_enum::idle;
	/// return color modified based on state
	rgb get_modified_color(const rgb& color) const;

public:
	gui_container(gui_container_listener* _listener, const std::string& _name, const vec3& _extent = vec3(1.f, 1.f, 0.1f), const quat& _rotation = quat(1, 0, 0, 0));
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

	/// implement ray object intersection and return whether intersection was found and in this case set \c hit_param to ray parameter and optionally \c hit_normal to surface normal of intersection
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

	void set_cell_types(const std::unordered_map<std::string, cell_type>& _cell_types);
	//void set_cells(const std::vector<cell>* _cells, size_t _cells_start, size_t _cells_end, const ivec3& extents);
	//void unset_cells();

	void set_inverse_model_transform(const mat4& _inverse_model_transform);
	void set_left_controller_transform(const mat4& _head_transform);
private:
	/// color map
	//void add_color_points(const rgba& color0, const rgba& color1);
	//void update_color_point(size_t index, const rgba& color0, const rgba& color1);

	//void interpolate_colors(bool force = false);

	//void point_at_cell(size_t cell_index, size_t node_index = SIZE_MAX) const;

	void select_gui(int index);
};

typedef cgv::data::ref_ptr<gui_container> gui_container_ptr;
