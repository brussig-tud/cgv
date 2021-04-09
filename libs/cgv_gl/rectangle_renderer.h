#pragma once

#include "surface_renderer.h"

#include "gl/lib_begin.h"

namespace cgv {
	namespace render {
		class CGV_API rectangle_renderer;

		//! reference to a singleton plane renderer that can be shared among drawables
		/*! the second parameter is used for reference counting. Use +1 in your init method,
			-1 in your clear method and default 0 argument otherwise. If internal reference
			counter decreases to 0, singelton renderer is destructed. */
		extern CGV_API rectangle_renderer& ref_rectangle_renderer(context& ctx, int ref_count_change = 0);

		struct CGV_API rectangle_render_style : public surface_render_style
		{
			int border_mode;
			rgba border_color;
			float pixel_blend;
			float percentual_border_width;
			float border_width_in_pixel;
			float default_depth_offset;
			int texture_mode;
			// influence on opengl state
			bool blend_rectangles;
		protected:
			friend class rectangle_renderer;
			mutable GLboolean is_blend;
			mutable GLint blend_src, blend_dst;
		public:
			rectangle_render_style();
		};

		/// renderer that supports plane rendering
		class CGV_API rectangle_renderer : public surface_renderer
		{
		protected:
			/// whether extent array has been specified
			bool has_extents;
			/// whether translation array has been specified
			bool has_translations;
			/// whether rotation array has been specified
			bool has_rotations;
			/// whether position is rectangle center, if not it is lower left corner
			bool position_is_center;
			/// whether depth offset array has been specified
			bool has_depth_offsets;
			float y_view_angle;
			/// overload to allow instantiation of rectangle_renderer
			render_style* create_render_style() const;
		public:
			///
			rectangle_renderer();
			///
			void set_y_view_angle(float y_view_angle);
			/// call this before setting attribute arrays to manage attribute array in given manager
			void enable_attribute_array_manager(const context& ctx, attribute_array_manager& aam);
			/// call this after last render/draw call to ensure that no other users of renderer change attribute arrays of given manager
			void disable_attribute_array_manager(const context& ctx, attribute_array_manager& aam);
			///
			bool init(context& ctx);
			/// set the flag, whether the position is interpreted as the box center, true by default
			void set_position_is_center(bool _position_is_center);
			/// specify a single extent for all boxes
			template <typename T>
			void set_extent(const context& ctx, const cgv::math::fvec<T, 2U>& extent) { has_extents = true;  ref_prog().set_attribute(ctx, ref_prog().get_attribute_location(ctx, "extent"), extent); }
			/// extent array specifies plane side lengths from origin to edge
			template <typename T>
			void set_extent_array(const context& ctx, const std::vector<cgv::math::fvec<T, 2U>>& extents) { has_extents = true;  set_attribute_array(ctx, ref_prog().get_attribute_location(ctx, "extent"), extents); }
			/// extent array specifies plane side lengths from origin to edge
			template <typename T>
			void set_extent_array(const context& ctx, const cgv::math::fvec<T, 2U>* extents, size_t nr_elements, unsigned stride_in_bytes = 0) { has_extents = true;  set_attribute_array(ctx, ref_prog().get_attribute_location(ctx, "extent"), extents, nr_elements, stride_in_bytes); }
			/// specify a single rectangle without array. This sets position_is_center to false as well as position and extent array
			template <typename T>
			void set_rectangle(const context& ctx, const cgv::media::axis_aligned_box<T, 2>& box) {
				has_positions = true;
				has_extents = true;
				set_position_is_center(false);
				ref_prog().set_attribute(ctx, ref_prog().get_position_index(), box.get_min_pnt());
				ref_prog().set_attribute(ctx, ref_prog().get_attribute_location(ctx, "extent"), box.get_max_pnt());
			}
			/// specify rectangle array directly. This sets position_is_center to false as well as position and extent array
			template <typename T>
			void set_rectangle_array(const context& ctx, const std::vector<cgv::media::axis_aligned_box<T, 2> >& boxes) {
				set_composed_attribute_array(ctx, ref_prog().get_attribute_location(ctx, "position"),
					&boxes.front(), boxes.size(), boxes[0].get_min_pnt());
				ref_composed_attribute_array(ctx, ref_prog().get_attribute_location(ctx, "extent"),
					ref_prog().get_attribute_location(ctx, "position"), &boxes.front(), boxes.size(), boxes[0].get_max_pnt());
				has_positions = true;
				has_extents = true;
				set_position_is_center(false);
			}
			/// specify ractangle array directly. This sets position_is_center to false as well as position and extent array
			template <typename T>
			void set_rectangle_array(const context& ctx, const cgv::media::axis_aligned_box<T, 2>* boxes, size_t count) {
				set_composed_attribute_array(ctx, ref_prog().get_attribute_location(ctx, "position"),
					boxes, count, boxes[0].get_min_pnt());
				ref_composed_attribute_array(ctx, ref_prog().get_attribute_location(ctx, "extent"),
					ref_prog().get_attribute_location(ctx, "position"), boxes, count, boxes[0].get_max_pnt());
				has_positions = true;
				has_extents = true;
				set_position_is_center(false);
			}
			/// specify rectangle without array. This sets position_is_center to false as well as position and extent array
			void set_textured_rectangle(const context& ctx, const textured_rectangle& tcr);
			/// specify rectangle array directly. This sets position_is_center to false as well as position and extent array
			void set_textured_rectangle_array(const context& ctx, const std::vector<textured_rectangle>& tc_rects) {
				set_composed_attribute_array(ctx, ref_prog().get_attribute_location(ctx, "position"),
					&tc_rects.front(), tc_rects.size(), tc_rects[0].rectangle.get_min_pnt());
				ref_composed_attribute_array(ctx, ref_prog().get_attribute_location(ctx, "extent"),
					ref_prog().get_attribute_location(ctx, "position"), &tc_rects.front(), tc_rects.size(), tc_rects[0].rectangle.get_max_pnt());
				ref_composed_attribute_array(ctx, ref_prog().get_attribute_location(ctx, "texcoord"),
					ref_prog().get_attribute_location(ctx, "position"), &tc_rects.front(), tc_rects.size(), tc_rects[0].texcoords);
				has_positions = true;
				has_extents = true;
				has_texcoords = true;
				set_position_is_center(false);
			}
			/// specify ractangle array directly. This sets position_is_center to false as well as position and extent array
			void set_textured_rectangle_array(const context& ctx, const textured_rectangle* tc_rects, size_t count) {
				set_composed_attribute_array(ctx, ref_prog().get_attribute_location(ctx, "position"),
					tc_rects, count, tc_rects[0].rectangle.get_min_pnt());
				ref_composed_attribute_array(ctx, ref_prog().get_attribute_location(ctx, "extent"),
					ref_prog().get_attribute_location(ctx, "position"), tc_rects, count, tc_rects[0].rectangle.get_max_pnt());
				ref_composed_attribute_array(ctx, ref_prog().get_attribute_location(ctx, "texcoord"),
					ref_prog().get_attribute_location(ctx, "position"), tc_rects, count, tc_rects[0].texcoords);
				has_positions = true;
				has_extents = true;
				has_texcoords = true;
				set_position_is_center(false);
			}
			/// specify a single depth_offset for all lines
			template <typename T>
			void set_depth_offset(const context& ctx, const T& depth_offset) { has_depth_offsets = true;  ref_prog().set_attribute(ctx, ref_prog().get_attribute_location(ctx, "depth_offset"), depth_offset); }
			/// set per rectangle depth offsets
			template <typename T = float>
			void set_depth_offset_array(const context& ctx, const std::vector<T>& depth_offsets) { has_depth_offsets = true; set_attribute_array(ctx, ref_prog().get_attribute_location(ctx, "depth_offset"), depth_offsets); }
			/// template method to set the translations from a vector of vectors of type T, which should have 3 components
			template <typename T>
			void set_translation_array(const context& ctx, const std::vector<T>& translations) { has_translations = true; set_attribute_array(ctx, ref_prog().get_attribute_location(ctx, "translation"), translations); }
			/// template method to set the translations from a vector of vectors of type T, which should have 3 components
			template <typename T>
			void set_translation_array(const context& ctx, const T* translations, size_t nr_elements, unsigned stride) { has_translations = true; set_attribute_array(ctx, ref_prog().get_attribute_location(ctx, "translation"), translations, nr_elements, stride); }
			/// template method to set the rotation from a vector of quaternions of type T, which should have 4 components
			template <typename T>
			void set_rotation_array(const context& ctx, const std::vector<T>& rotations) { has_rotations = true; set_attribute_array(ctx, ref_prog().get_attribute_location(ctx, "rotation"), rotations); }
			/// template method to set the rotation from a vector of quaternions of type T, which should have 4 components
			template <typename T>
			void set_rotation_array(const context& ctx, const T* rotations, size_t nr_elements, unsigned stride = 0) { has_rotations = true; set_attribute_array(ctx, ref_prog().get_attribute_location(ctx, "rotation"), rotations, nr_elements, stride); }
			///
			bool validate_attributes(const context& ctx) const;
			///
			bool enable(context& ctx);
			///
			bool disable(context& ctx);
			/// convenience function to render with default settings
			void draw(context& ctx, size_t start, size_t count,
				bool use_strips = false, bool use_adjacency = false, uint32_t strip_restart_index = -1);
		};
		struct CGV_API rectangle_render_style_reflect : public rectangle_render_style
		{
			bool self_reflect(cgv::reflect::reflection_handler& rh);
		};
		extern CGV_API cgv::reflect::extern_reflection_traits<rectangle_render_style, rectangle_render_style_reflect> get_reflection_traits(const rectangle_render_style&);
	}
}

#include <cgv/config/lib_end.h>