#include "declaration_reader.h"
#include <libs/plot/plot2d.h>
#include <libs/plot/plot3d.h>

namespace stream_vis {

	declaration_reader::declaration_reader(const std::string& _declarations) :
		declarations(_declarations)
	{
	}
	void declaration_reader::init(
		std::map<std::string, uint16_t>* _name2index_ptr,
		std::vector<stream_vis::streaming_time_series*>* _typed_time_series_ptr,
		std::vector<plot_info>* _plot_pool_ptr)
	{
		name2index_ptr = _name2index_ptr;
		typed_time_series_ptr = _typed_time_series_ptr;
		plot_pool_ptr = _plot_pool_ptr;
	}

	void declaration_reader::parse_mapped_rgba(const std::string& name_color, const std::string& name_opacity, cgv::plot::mapped_rgba& color)
	{
		parse_color(name_color, color.color);
		int idx;
		if (parse_int(name_color + "_idx", idx))
			if (idx >= -1 && idx < 2)
				color.color_idx = idx;
			else
				std::cerr << "error parsing mapped color: color index " << idx << " out of range [-1,2]." << std::endl;
		if (parse_int(name_opacity + "_idx", idx))
			if (idx >= -1 && idx < 2)
				color.opacity_idx = idx;
			else
				std::cerr << "error parsing mapped color: opacity index " << idx << " out of range [-1,2]." << std::endl;
	}
	void declaration_reader::parse_mapped_size(const std::string& name, cgv::plot::mapped_size& size)
	{
		parse_float(name, size.size);
		int idx;
		if (parse_int(name + "_idx", idx))
			if (idx >= -1 && idx < 2)
				size.size_idx = idx;
			else
				std::cerr << "error parsing mapped size: size index " << idx << " out of range [-1,2]." << std::endl;
	}
	bool declaration_reader::construct_time_series(const std::string& name, const std::string& type, const std::vector<std::string>& ts_names)
	{
		// for all declarations extract stream indices
		std::vector<uint16_t> stream_indices;
		for (auto ts_name : ts_names) {
			if (name2index_ptr->find(ts_name) != name2index_ptr->end())
				stream_indices.push_back(name2index_ptr->at(ts_name));
			else {
				std::cerr << "could not find name " << ts_name << " in inputs or outputs" << std::endl;
				stream_indices.push_back(0);
			}
		}
		// check if declaration extents input/output stream or defines a composed time series
		int ts_idx = -1;
		if (name2index_ptr->find(name) != name2index_ptr->end()) {
			ts_idx = name2index_ptr->at(name);
			streaming_time_series* ts_ptr = typed_time_series_ptr->at(ts_idx);
			if (stream_indices.size() == 0) {
			}
			else if (stream_indices.size() == 2) {
				if (ts_ptr->get_value_type_id() == cgv::type::info::TI_BOOL)
					std::cerr << "lower and upper bound declaration of boolean typed time series <" << name << "> not allowed and ignored." << std::endl;
				else {
					if (typed_time_series_ptr->at(stream_indices[0])->get_value_type_id() == ts_ptr->get_value_type_id()) 
						ts_ptr->lower_bound_index = stream_indices[0];
					else
						std::cerr << "lower bound time series <" << typed_time_series_ptr->at(stream_indices[0])->name << ":"
						<< typed_time_series_ptr->at(stream_indices[0])->get_value_type_name() << "> of different type as <"
						<< ts_ptr->name << ":" << ts_ptr->get_value_type_name() << "> and ignored" << std::endl;
					if (typed_time_series_ptr->at(stream_indices[1])->get_value_type_id() == ts_ptr->get_value_type_id())
						ts_ptr->upper_bound_index = stream_indices[1];
					else
						std::cerr << "upper bound time series <" << typed_time_series_ptr->at(stream_indices[1])->name << ":"
						<< typed_time_series_ptr->at(stream_indices[1])->get_value_type_name() << "> of different type as <"
						<< ts_ptr->name << ":" << ts_ptr->get_value_type_name() << "> and ignored" << std::endl;
					if (ts_ptr->lower_bound_index != uint16_t(-1) && ts_ptr->upper_bound_index != uint16_t(-1))
						ts_ptr->aabb_mode = AM_NONE;
				}
			}
			else {
				std::cerr << "for redeclaration of <" << ts_ptr->name << ":" << ts_ptr->get_value_type_name()
					<< "> only zero or two time series allowed in definition but found {";
				for (auto si : stream_indices)
					std::cerr << " " << typed_time_series_ptr->at(stream_indices[si])->name;
				std::cerr << " }" << std::endl;
			}
		}
		else {
			//
			if (type.substr(0, 3) == "vec") {
				if (type.length() != 4) {
					std::cerr << "unknown typed declaration <" << name << ":" << type << ">" << std::endl;
					return false;
				}
				int dim = type[3] - '0';
				if (stream_indices.size() != dim) {
					std::cerr << "composed stream of type <" << name << ":" << type << "> needs to be defined from " << dim << " streams, but found only " << stream_indices.size() << " for " << name << ":" << type << std::endl;
					return false;
				}
				if (dim < 2 || dim > 4) {
					std::cerr << "only support composed vec type of dim 2-4 but got " << dim << " for " << name << ":" << type << std::endl;
					return false;
				}
				switch (dim) {
				case 2: typed_time_series_ptr->push_back(new stream_vis::fvec_time_series<2>(stream_indices[0], stream_indices[1])); break;
				case 3: typed_time_series_ptr->push_back(new stream_vis::fvec_time_series<3>(stream_indices[0], stream_indices[1], stream_indices[2])); break;
				case 4: typed_time_series_ptr->push_back(new stream_vis::fvec_time_series<4>(stream_indices[0], stream_indices[1], stream_indices[2], stream_indices[3])); break;
				}
			}
			else if (type == "quat") {
				if (stream_indices.size() != 4) {
					std::cout << "composed stream of type <" << type << "> needs to be defined from 4 streams, but found only " << stream_indices.size() << " in " << name << ":" << type << std::endl;
					return false;
				}
				typed_time_series_ptr->push_back(new stream_vis::quat_time_series(stream_indices[0], stream_indices[1], stream_indices[2], stream_indices[3]));
			}
			typed_time_series_ptr->back()->name = name;
			typed_time_series_ptr->back()->series().set_ringbuffer_size(1024);
			ts_idx = (*name2index_ptr)[name] = uint16_t(typed_time_series_ptr->size() - 1);
		}
		if (ts_idx != -1) {
			streaming_time_series* ts_ptr = typed_time_series_ptr->at(ts_idx);
			parse_color("color", ts_ptr->default_color);
			parse_float("opacity", ts_ptr->default_opacity);
			parse_float("size", ts_ptr->default_size);
			parse_int("aabb_mode", (int&)ts_ptr->aabb_mode);
			int ring_buffer_size = 1024;
			if (parse_int("ring_buffer_size", ring_buffer_size))
				ts_ptr->series().set_ringbuffer_size(ring_buffer_size);
		}
		return true;
	}
	bool declaration_reader::construct_mark(const std::string& name, cgv::plot::plot_base_config& cfg, int dim)
	{
		if (name == "p" || name == "points") {
			cfg.show_points = true;
			parse_mapped_rgba("color", "opacity", cfg.point_color);
			parse_mapped_size("size", cfg.point_size);
			parse_mapped_rgba("halo_color", "halo_opacity", cfg.point_halo_color);
			parse_mapped_size("halo_width", cfg.point_halo_width);
		}
		else if (name == "s" || name == "sticks") {
			cfg.show_sticks = true;
			parse_mapped_rgba("color", "opacity", cfg.stick_color);
			parse_mapped_size("width", cfg.stick_width);
		}
		else if (name == "b" || name == "bars") {
			cfg.show_bars = true;
			parse_mapped_rgba("color", "opacity", cfg.bar_color);
			parse_mapped_rgba("outline_color", "outline_opacity", cfg.bar_outline_color);
			parse_mapped_size("outline_width", cfg.bar_outline_width);
			parse_mapped_size("percentual_width", cfg.bar_percentual_width);
			if (dim == 3)
				parse_mapped_size("percentual_depth", static_cast<cgv::plot::plot3d_config&>(cfg).bar_percentual_depth);
		}
		else if (name == "l" || name == "lines") {
			cfg.show_lines = true;
			parse_mapped_rgba("color", "opacity", cfg.line_color);
			parse_mapped_size("width", cfg.line_width);
			if (dim == 3)
				parse_bool("show_orientation", static_cast<cgv::plot::plot3d_config&>(cfg).show_line_orientation);
		}
		else {
			std::cerr << "unknown mark " << name << std::endl;
			return false;
		}
		return true;
	}
	bool declaration_reader::construct_attribute_definitions(std::vector<std::string>& defs, std::vector<attribute_definition>& ads)
	{
		for (auto ts_name : defs) {
			attribute_definition ad;
			ad.accessor = TSA_ALL;
			// check for swizzle and extract it
			std::string swizzle;
			size_t dot_pos = ts_name.find_first_of('.');
			if (dot_pos != std::string::npos) {
				swizzle = ts_name.substr(dot_pos + 1);
				ts_name = ts_name.substr(0, dot_pos);
			}
			// determine index of time series corresponding to name and check for existance
			auto iter = name2index_ptr->find(ts_name);
			if (iter == name2index_ptr->end()) {
				std::cout << "expected time series reference but found <" << ts_name << ">" << std::endl;
				return false;
			}
			ad.time_series_index = iter->second;
			// if no swizzle is given, add one accessor per dimension of time series
			if (swizzle.empty()) {
				size_t dim = typed_time_series_ptr->at(ad.time_series_index)->get_io_indices().size();
				if (dim == 1)
					ads.push_back(ad);
				else {
					ad.accessor = TimeSeriesAccessor(TSA_X);
					ads.push_back(ad);
					int delta = 1;
					for (size_t d = 1; d < dim; ++d) {
						delta *= 2;
						ad.accessor = TimeSeriesAccessor(TSA_X + delta);
						ads.push_back(ad);
					}
				}
			}
			// in case of swizzle add one accessor per swizzle entry
			else {
				for (auto s : swizzle) {
					switch (s) {
					case 't': ad.accessor = TSA_TIME; break;
					case 'x': ad.accessor = TSA_X; break;
					case 'y': ad.accessor = TSA_Y; break;
					case 'z': ad.accessor = TSA_Z; break;
					case 'l': ad.accessor = TSA_LENGTH; break;
					case 'd': {
						ad.accessor = TSA_DIRECTION_X;
						size_t dim = typed_time_series_ptr->at(ad.time_series_index)->get_io_indices().size();
						if (dim > 1) {
							int delta = 2;
							for (size_t d = 0; d + 1 < dim; ++d) {
								ads.push_back(ad);
								ad.accessor = TimeSeriesAccessor(ad.accessor * 2);
							}
						}
						break;
					}
					default:
						std::cerr << "encountered unknown swizzle <" << s << "> and ignoring it" << std::endl;
						continue;
					}
					ads.push_back(ad);
				}
			}
		}
		return true;
	}
	bool declaration_reader::construct_subplot(plot_info& pi, int dim, std::vector<attribute_definition>& ads)
	{
		// in case of missing component insert time component by default 
		if (ads.size() == dim - 1) {
			if (ads.front().accessor == TSA_TIME) {
				std::cerr << "subplot" << dim << "d declaration with only " << dim - 1 << " accessors and auto extension of time not possible." << std::endl;
				return false;
			}
			attribute_definition ad;
			ad.time_series_index = ads.front().time_series_index;
			ad.accessor = TSA_TIME;
			ads.insert(ads.begin(), ad);
		}
		if (ads.size() > 8) {
			std::cerr << "subplot" << dim << "d declaration found with " << ads.size() << " accessors out of valid range [2,8]." << std::endl;
			return false;
		}
		// todo add derivation of subplot name from attribute definitions
		std::string subplot_name = "<";
		uint16_t last_time_series_index = 65535;
		for (const auto& ad : ads) {
			if (ad.time_series_index != last_time_series_index) {
				if (last_time_series_index != 65535)
					subplot_name += ",";
				last_time_series_index = ad.time_series_index;
				subplot_name += typed_time_series_ptr->at(ad.time_series_index)->name;
				subplot_name += ".";
			}
			subplot_name += get_accessor_string(ad.accessor);			
		}
		subplot_name += ">";
		get_value("name", subplot_name);
		// add subplot info
		subplot_info spi;
		spi.attribute_definitions = ads;
		pi.subplot_infos.push_back(spi);
		auto& cfg = pi.plot_ptr->ref_sub_plot_config(pi.plot_ptr->add_sub_plot(subplot_name));

		// construct subplot config

		// by default show nothing
		cfg.show_points = cfg.show_sticks = cfg.show_bars = cfg.show_lines = false;

		// set reference color, opacity and size before specifying marks
		cfg.sub_plot_influence = cgv::plot::SPI_ALL;
		if (parse_color("color", cfg.ref_color.color))
			cfg.set_colors(cfg.ref_color.color);
		if (parse_float("size", cfg.ref_size.size))
			cfg.set_sizes(cfg.ref_size.size);

		cfg.sub_plot_influence = cgv::plot::SPI_POINT;
		parse_int("sub_plot_influence", (int&)cfg.sub_plot_influence);
		if (parse_int("color_idx", cfg.ref_color.color_idx))
			cfg.set_color_indices(cfg.ref_color.color_idx);
		if (parse_int("opacity_idx", cfg.ref_opacity.opacity_idx))
			cfg.set_opacity_indices(cfg.ref_opacity.opacity_idx);
		if (parse_int("size_idx", cfg.ref_size.size_idx))
			cfg.set_size_indices(cfg.ref_size.size_idx);

		parse_marks(pi, cfg, dim);

		return true;


	}
	void declaration_reader::construct_plot(const std::string& name, int dim)
	{
		plot_info pi;
		pi.dim = dim;
		pi.name = name;
		pi.fixed_domain = box3(vec3(0.0f), vec3(1.0f));
		int nr_attributes = 0;
		parse_int("nr_attributes", nr_attributes);
		for (int i = 0; i < 2; ++i) {
			for (int j = 0; j < dim+nr_attributes; ++j) {
				pi.domain_adjustment[i][j] = DA_COMPUTE;
				pi.domain_bound_ts_index[i][j] = uint16_t(-1);
			}
		}
		parse_bound_vecn("view_min", pi.domain_adjustment[0], pi.domain_bound_ts_index[0], &pi.fixed_domain.ref_min_pnt()[0], dim+nr_attributes);
		parse_bound_vecn("view_max", pi.domain_adjustment[1], pi.domain_bound_ts_index[1], &pi.fixed_domain.ref_max_pnt()[0], dim+nr_attributes);
		bool auto_color = false;
		parse_bool("auto_color", auto_color);
		pi.nr_axes = dim + nr_attributes;
		if (dim == 2) {
			auto* plot2d_ptr = new cgv::plot::plot2d(unsigned(nr_attributes));
			pi.plot_ptr = plot2d_ptr;
			vec2 ext = vec2(1.0f, 1.0f);
			parse_vec2("extent", ext);
			parse_float("dz", plot2d_ptr->dz);
			parse_bool("multi_y_axis_mode", plot2d_ptr->multi_y_axis_mode);
			pi.plot_ptr->set_extent(ext.to_vec());
			pi.plot_ptr->get_domain_config_ptr()->reference_size = 0.003f;
		}
		else {
			pi.plot_ptr = new cgv::plot::plot3d(unsigned(nr_attributes));
			vec3 ext;
			if (parse_vec3("extent", ext))
				pi.plot_ptr->set_extent(ext.to_vec());
		}
		vec3 center;
		if (parse_vec3("center", center))
			pi.plot_ptr->place_center(center);
		auto& dom_cfg = *pi.plot_ptr->get_domain_config_ptr();
		dom_cfg.fill = false;
		parse_bool("fill_domain", dom_cfg.fill);
		parse_bool("show_domain", dom_cfg.show_domain);
		parse_color("domain_color", dom_cfg.color);
		quat ori;
		if (parse_quat("orientation", ori))
			pi.plot_ptr->set_orientation(ori);
		parse_float("font_size", dom_cfg.label_font_size);
		//LegendComponent legend_components;
		//vec3 legend_location;
		//vec2 legend_extent;
		//rgba legend_color;
		if (parse_int("primary_color_mapping", pi.plot_ptr->color_mapping[0])) {
			if (pi.plot_ptr->color_mapping[0] >= (int)pi.nr_axes) {
				std::cerr << "primary_color_mapping of plot " << name << " with " 
					<< pi.plot_ptr->color_mapping[0] << " out of valid range [0," 
					<< pi.nr_axes - 1 << "]" << std::endl;
				pi.plot_ptr->color_mapping[0] = -1;
			}
		}
		std::string value;
		if (get_value("primary_color_scale", value)) {
			cgv::media::ColorScale cs;
			if (cgv::media::find_color_scale(value, cs))
				pi.plot_ptr->color_scale_index[0] = cs;
		}
		parse_float("primary_color_scale_gamma", pi.plot_ptr->color_scale_gamma[0]);
		parse_float("primary_window_zero_position", pi.plot_ptr->window_zero_position[0]);
		if (parse_int("primary_opacity_mapping", pi.plot_ptr->opacity_mapping[0])) {
			if (pi.plot_ptr->opacity_mapping[0] >= (int)pi.nr_axes) {
				std::cerr << "primary_opacity_mapping of plot " << name << " with "
					<< pi.plot_ptr->opacity_mapping[0] << " out of valid range [0,"
					<< pi.nr_axes - 1 << "]" << std::endl;
				pi.plot_ptr->opacity_mapping[0] = -1;
			}
		}
		parse_float("primary_opacity_gamma", pi.plot_ptr->opacity_gamma[0]);
		parse_bool("primary_opacity_is_bipolar", pi.plot_ptr->opacity_is_bipolar[0]);
		parse_float("primary_opacity_window_zero_position", pi.plot_ptr->opacity_window_zero_position[0]);
		parse_float("primary_opacity_min", pi.plot_ptr->opacity_min[0]);
		parse_float("primary_opacity_max", pi.plot_ptr->opacity_max[0]);
		if (parse_int("primary_size_mapping", pi.plot_ptr->size_mapping[0])) {
			if (pi.plot_ptr->size_mapping[0] >= (int)pi.nr_axes) {
				std::cerr << "primary_size_mapping of plot " << name << " with "
					<< pi.plot_ptr->size_mapping[0] << " out of valid range [0,"
					<< pi.nr_axes - 1 << "]" << std::endl;
				pi.plot_ptr->size_mapping[0] = -1;
			}
		}
		parse_float("primary_size_gamma", pi.plot_ptr->size_gamma[0]);
		parse_float("primary_size_min", pi.plot_ptr->size_min[0]);
		parse_float("primary_size_max", pi.plot_ptr->size_max[0]);

		if (parse_int("secondary_color_mapping", pi.plot_ptr->color_mapping[1])) {
			if (pi.plot_ptr->color_mapping[1] >= (int)pi.nr_axes) {
				std::cerr << "secondary_color_mapping of plot " << name << " with "
					<< pi.plot_ptr->color_mapping[1] << " out of valid range [0,"
					<< pi.nr_axes - 1 << "]" << std::endl;
				pi.plot_ptr->color_mapping[1] = -1;
			}
		}
		if (get_value("secondary_color_scale", value)) {
			cgv::media::ColorScale cs;
			if (cgv::media::find_color_scale(value, cs))
				pi.plot_ptr->color_scale_index[1] = cs;
		}
		parse_float("secondary_color_scale_gamma", pi.plot_ptr->color_scale_gamma[1]);
		parse_float("secondary_window_zero_position", pi.plot_ptr->window_zero_position[1]);
		if (parse_int("secondary_opacity_mapping", pi.plot_ptr->opacity_mapping[1])) {
			if (pi.plot_ptr->opacity_mapping[1] >= (int)pi.nr_axes) {
				std::cerr << "secondary_opacity_mapping of plot " << name << " with "
					<< pi.plot_ptr->opacity_mapping[1] << " out of valid range [0,"
					<< pi.nr_axes - 1 << "]" << std::endl;
				pi.plot_ptr->opacity_mapping[1] = -1;
			}
		}
		parse_float("secondary_opacity_gamma", pi.plot_ptr->opacity_gamma[1]);
		parse_bool("secondary_opacity_is_bipolar", pi.plot_ptr->opacity_is_bipolar[1]);
		parse_float("secondary_opacity_window_zero_position", pi.plot_ptr->opacity_window_zero_position[1]);
		parse_float("secondary_opacity_min", pi.plot_ptr->opacity_min[1]);
		parse_float("secondary_opacity_max", pi.plot_ptr->opacity_max[1]);
		if (parse_int("secondary_size_mapping", pi.plot_ptr->size_mapping[1])) {
			if (pi.plot_ptr->size_mapping[1] >= (int)pi.nr_axes) {
				std::cerr << "secondary_size_mapping of plot " << name << " with "
					<< pi.plot_ptr->size_mapping[1] << " out of valid range [0,"
					<< pi.nr_axes - 1 << "]" << std::endl;
				pi.plot_ptr->size_mapping[1] = -1;
			}
		}
		parse_float("secondary_size_gamma", pi.plot_ptr->size_gamma[1]);
		parse_float("secondary_size_min", pi.plot_ptr->size_min[1]);
		parse_float("secondary_size_max", pi.plot_ptr->size_max[1]);

		parse_subplots(pi, dim);
		if (auto_color) {
			unsigned n=pi.plot_ptr->get_nr_sub_plots();
			if (n > 1) {
				float scale = 1.0f / n;
				for (unsigned i = 0; i < n; ++i)
					pi.plot_ptr->ref_sub_plot_config(i).set_colors(cgv::media::color_scale(scale * i, cgv::media::CS_HUE));
			}
		}
		plot_pool_ptr->push_back(pi);
	}
}