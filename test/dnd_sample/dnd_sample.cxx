#include <cgv/base/node.h>
#include <cgv/base/register.h>
#include <cgv/gui/event_handler.h>
#include <cgv/gui/provider.h>
#include <cgv/gui/key_event.h>
#include <cgv/gui/mouse_event.h>
#include <cgv/utils/file.h>
#include <cgv/utils/dir.h>
#include <cgv/utils/advanced_scan.h>
#include <cgv/render/drawable.h>
#include <cgv/render/attribute_array_binding.h>
#include <cgv/gui/dialog.h>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>

// renderer headers include self reflection helpers for render styles
#include <cgv_gl/box_renderer.h>
#include <cgv_gl/sphere_renderer.h>

// include self reflection helpers of used types (here vec3 & rgb)
#include <libs/cgv_reflect_types/math/fvec.h>
#include <libs/cgv_reflect_types/media/color.h>

std::string query_system_output(std::string cmd, bool cerr) {

	std::string data;
	FILE* stream;
	const int max_buffer = 256;
	char buffer[max_buffer];
	if (cerr)
		cmd.append(" 2>&1");

	stream = _popen(cmd.c_str(), "r");

	if (stream) {
		while (!feof(stream))
			if (fgets(buffer, max_buffer, stream) != NULL) data.append(buffer);
		_pclose(stream);
	}
	return data;
}


class dnd_sample :
	public cgv::base::node,          /// derive from node to integrate into global tree structure and to store a name
	public cgv::render::drawable,    /// derive from drawable for drawing the cube
	public cgv::base::argument_handler,
	public cgv::gui::event_handler,
	public cgv::gui::provider
{
protected:
	// declare some member variables
	std::string dnd_text;
	ivec2 dnd_pos;
	size_t insert_position;
	size_t cursor_position;
	std::vector<std::string> file_names;
	std::vector<std::string> durations;
	std::vector<bool> selection;
	std::string output_prefix;
	std::string input_dir;
	unsigned lecture;
	unsigned part;
	unsigned subpart;
public:
	dnd_sample() : node("dnd_sample_instance")
	{
		insert_position = 0;
		cursor_position = 0;
		output_prefix = "2020-CG1";
		lecture = 10;
		part = 1;
		subpart = 1;
	}
	std::string get_output_file_name(bool use_subpart = false) const
	{
		static const char* letters = "abcdefghijklmn";
		std::string output_name = output_prefix + "_" + cgv::utils::to_string(lecture, 2, '0') + "_" + cgv::utils::to_string(part);
		if (use_subpart)
			output_name += letters[subpart] - 1;
		return  output_name + ".mp4";
	}
	void insert_file(const std::string& s, bool selected = false)
	{
		if (insert_position == file_names.size()) {
			file_names.push_back(s);
			selection.push_back(selected);
			durations.push_back(get_file_duration(s));
		}
		else {
			file_names.insert(file_names.begin() + insert_position, s);
			selection.insert(selection.begin() + insert_position, selected);
			durations.insert(durations.begin() + insert_position, get_file_duration(s));
		}
	}
	std::string get_file_duration(const std::string& s)
	{
		std::string cmd = std::string("ffprobe ") + s;
		std::string output = query_system_output(cmd, true);
		size_t b = output.find("Duration: ");
		std::string duration = "00:00:00";
		if (b != std::string::npos) {
			b += 10;
			size_t e = output.find_first_of(',', b);
			if (e != std::string::npos) {
				duration = output.substr(b, e - b);
			}

		}
		return duration;
	}
	void trim_file()
	{
		if (cursor_position >= file_names.size())
			return;
		std::string output_path = cgv::utils::file::get_path(file_names[cursor_position]);
		if (!output_path.empty())
			output_path += "\\";
		std::string start = "00:00:00";
		std::string end = durations[cursor_position];
		std::string output_name = get_output_file_name(true);
		if (!cgv::gui::query("trim start:", start))
			return;
		if (!cgv::gui::query("trim end:", end))
			return;
		if (!cgv::gui::query("output:", output_name))
			return;
		std::string cmd = std::string("ffmpeg -ss ") + start + " -i " + file_names[cursor_position] + " -to " + end + " -c copy " + output_path+output_name;
		std::cout << cmd << std::endl; 
		system(cmd.c_str());		
	}
	void append_file(const std::string& s, bool selected = false)
	{
		file_names.push_back(s);
		selection.push_back(selected);
		durations.push_back(get_file_duration(s));
		insert_position = file_names.size();
	}
	void append_directory(const std::string& d, bool selected = false)
	{
		if (!cgv::utils::dir::exists(d))
			return;
		std::vector<std::string> fns;
		cgv::utils::dir::glob(d, fns, "*.mp4");
		for (auto fn : fns)
			append_file(fn, selected);
	}
	void insert_directory(const std::string& d, bool selected = false)
	{
		if (!cgv::utils::dir::exists(d))
			return;
		std::vector<std::string> fns;
		cgv::utils::dir::glob(d, fns, "*.mp4");
		for (size_t i = fns.size(); i > 0; ) {
			--i;
			insert_file(fns[i], selected);
		}
	}
	void handle_args(std::vector<std::string>& args)
	{
		for (auto a : args) {
			if (cgv::utils::file::exists(a))
				append_file(a);
			else if (cgv::utils::dir::exists(a))
				append_directory(a);
		}
		// clear args vector as all args have been processed
		args.clear();
		post_redraw();
	}
	void stream_help(std::ostream& os)
	{
		os << "dnd_sample" << std::endl;
	}
	void clear_files()
	{
		file_names.clear();
		selection.clear();
		durations.clear();
		cursor_position = 0;
		insert_position = 0;
		post_redraw();
	}
	void erase_file(size_t i)
	{
		file_names.erase(file_names.begin() + i);
		selection.erase(selection.begin() + i);
		durations.erase(durations.begin() + i);
		if (cursor_position > 0 && cursor_position >= i) {
			--cursor_position;
			update_member(&cursor_position);
		}
		if (insert_position > 0 && insert_position > i) {
			--insert_position;
			update_member(&insert_position);
		}
	}
	void discard_selection_or_current()
	{
		bool sel = false;
		for (size_t i = file_names.size(); i > 0; ) {
			--i;
			if (selection[i]) {
				erase_file(i);
				sel = true;
			}
		}
		if (!sel) {
			if (cursor_position < file_names.size()) {
				erase_file(cursor_position);
			}
		}
	}
	void remove_selection_or_current()
	{
		std::cerr << "not implemented" << std::endl;
	}
	void toggle_selection()
	{
		for (size_t i = 0; i < selection.size(); ++i)
			selection[i] = !selection[i];
	}
	void concat()
	{
		if (file_names.empty())
			return;
		std::vector<std::string> fns;
		for (size_t i = 0; i < file_names.size(); ++i) {
			if (selection[i])
				fns.push_back(file_names[i]);
		}
		if (fns.empty())
			fns = file_names;

		std::string output_path = cgv::utils::file::get_path(file_names[cursor_position]);
		cgv::utils::replace(output_path, "/", "\\");
		if (!output_path.empty())
			output_path += "\\";
		if (!cgv::gui::query("output path: ", output_path))
			return;
		std::ofstream os(output_path + "files.txt");
		for (auto fn : fns) {
			std::string fn1 = fn;
			cgv::utils::replace(fn1, "\\", "/");
			os << "file " << fn1 << std::endl;
		}
		os.close();
		std::string output_file_name = output_path + get_output_file_name();
		std::string cmd = std::string("ffmpeg -f concat -safe 0 -i ") + output_path + "files.txt -c copy " + output_file_name;
		if (!cgv::gui::query("cmd: ", cmd))
			return;
		system(cmd.c_str());
		cmd = std::string("ffplay -vf \"drawtext=text='%{pts\\:hms}':box=1:x=(w-tw)/2:y=lh\" ") + output_file_name;
		system(cmd.c_str());
		++part;
		on_set(&part);
	}
	void play(bool from_end = false)
	{
		if (file_names.empty())
			return;
		std::string cmd = std::string("ffplay -vf \"drawtext=text='%{pts\\:hms}':box=1:x=(w-tw)/2:y=lh\" ");
		if (from_end) {
			cmd += "-ss ";
			cmd += durations[cursor_position];
			cmd += " ";
		}
		cmd += file_names[cursor_position];
		system(cmd.c_str());
	}
	bool handle(cgv::gui::event& e)
	{
		if (e.get_kind() == cgv::gui::EID_KEY) {
			cgv::gui::key_event& ke = reinterpret_cast<cgv::gui::key_event&>(e);
			if (ke.get_action() == cgv::gui::KA_RELEASE)
				return false;
			switch (ke.get_key()) {
			case '-':
				if (ke.get_modifiers() == 0) {
					if (lecture > 0) {
						--lecture;
						on_set(&lecture);
					}
					return true;
				}
				if (ke.get_modifiers() == cgv::gui::EM_SHIFT) {
					if (part > 0) {
						--part;
						on_set(&part);
					}
					return true;
				}
				if (ke.get_modifiers() == cgv::gui::EM_CTRL) {
					if (subpart > 0) {
						--subpart;
						on_set(&subpart);
					}
					return true;
				}
				break;
			case '=':
				if (ke.get_modifiers() == 0) {
					++lecture;
					on_set(&lecture);
					return true;
				}
				if (ke.get_modifiers() == cgv::gui::EM_SHIFT) {
					++part;
					on_set(&part);
					return true;
				}
				if (ke.get_modifiers() == cgv::gui::EM_CTRL) {
					++subpart;
					on_set(&subpart);
					return true;
				}
				break;
			case 'I' :
				switch (ke.get_modifiers()) {
				case 0 :
					if (!input_dir.empty()) {
						clear_files();
						append_directory(input_dir);
						post_redraw();
					}
					break;
				case cgv::gui::EM_SHIFT :
					insert_position = cursor_position;
					post_redraw();
					break;
				}
				return true;
			case 'C': concat(); return true;
			case 'P': 
				if (ke.get_modifiers() == 0) {
					play(); 
					return true;
				}
				else if (ke.get_modifiers() == cgv::gui::EM_SHIFT) {
					play(true);
					return true;
				}
				break;
			case 'T': trim_file(); return true;
			case 'X': 
				if (ke.get_modifiers() == 0) {
					discard_selection_or_current();
					post_redraw();
					return true;
				}
				if (ke.get_modifiers() == cgv::gui::EM_ALT+cgv::gui::EM_CTRL) {
					remove_selection_or_current();
					post_redraw();
					return true;
				}
				break;
			case cgv::gui::KEY_Enter:
				if (ke.get_modifiers() == 0) {
					if (cursor_position < file_names.size()) {
						selection[cursor_position] = !selection[cursor_position];
						post_redraw();
						return true;
					}
				}
				if (ke.get_modifiers() == cgv::gui::EM_SHIFT) {
					toggle_selection();
					post_redraw();
					return true;
				}
				break;
			case cgv::gui::KEY_Up:
				if (ke.get_modifiers() == 0) {
					if (cursor_position > 0) {
						--cursor_position;
						on_set(&cursor_position);
					}
					return true;
				}
				if (ke.get_modifiers() == cgv::gui::EM_SHIFT) {
					if (cursor_position < file_names.size())
						selection[cursor_position] = true;
					if (cursor_position > 0) {
						--cursor_position;
						selection[cursor_position] = true;
						on_set(&cursor_position);
					}
					return true;
				}
				if (ke.get_modifiers() == cgv::gui::EM_CTRL) {
					if (cursor_position < file_names.size() && cursor_position > 0) {
						std::swap(file_names[cursor_position], file_names[cursor_position - 1]);
						std::swap(selection[cursor_position], selection[cursor_position - 1]);
						--cursor_position;
						on_set(&cursor_position);
					}
					return true;
				}
				break;
			case cgv::gui::KEY_Down:
				if (ke.get_modifiers() == 0) {
					if (cursor_position + 1 < file_names.size()) {
						++cursor_position;
						on_set(&cursor_position);
					}
					return true;
				}
				if (ke.get_modifiers() == cgv::gui::EM_SHIFT) {
					if (cursor_position < file_names.size())
						selection[cursor_position] = true;
					if (cursor_position + 1 < file_names.size()) {
						++cursor_position;
						selection[cursor_position] = true;
						on_set(&cursor_position);
					}
					return true;
				}
				if (ke.get_modifiers() == cgv::gui::EM_CTRL) {
					if (cursor_position + 1 < file_names.size()) {
						std::swap(file_names[cursor_position], file_names[cursor_position + 1]);
						std::swap(selection[cursor_position], selection[cursor_position + 1]);
						++cursor_position;
						on_set(&cursor_position);
					}
					return true;
				}
				break;
			case cgv::gui::KEY_Home:
				if (ke.get_modifiers() == 0) {
					if (cursor_position > 0) {
						cursor_position = 0;
						on_set(&cursor_position);
					}
					return true;
				}
				if (ke.get_modifiers() == cgv::gui::EM_SHIFT) {
					if (cursor_position < file_names.size())
						selection[cursor_position] = true;
					if (cursor_position > 0) {
						while (cursor_position > 0) {
							--cursor_position;
							selection[cursor_position] = true;
						}
						on_set(&cursor_position);
					}
					return true;
				}
				if (ke.get_modifiers() == cgv::gui::EM_CTRL) {
					if (cursor_position < file_names.size() && cursor_position > 0) {
						std::swap(file_names[cursor_position], file_names.front());
						std::swap(selection[cursor_position], selection.front());
						cursor_position = 0;
						on_set(&cursor_position);
					}
					return true;
				}
			case cgv::gui::KEY_End:
				if (ke.get_modifiers() == 0) {
					if (cursor_position + 1 < file_names.size()) {
						cursor_position = file_names.size() - 1;
						on_set(&cursor_position);
					}
					return true;
				}
				if (ke.get_modifiers() == cgv::gui::EM_SHIFT) {
					if (cursor_position < file_names.size())
						selection[cursor_position] = true;
					if (cursor_position + 1 > file_names.size()) {
						while (cursor_position + 1 > file_names.size()) {
							++cursor_position;
							selection[cursor_position] = true;
						}
						on_set(&cursor_position);
					}
					return true;
				}
				if (ke.get_modifiers() == cgv::gui::EM_CTRL) {
					if (cursor_position + 1 > file_names.size()) {
						std::swap(file_names[cursor_position], file_names.back());
						std::swap(selection[cursor_position], selection.back());
						cursor_position = 0;
						on_set(&cursor_position);
					}
					return true;
				}
				break;
			}
		}
		else if (e.get_kind() == cgv::gui::EID_MOUSE) {
			cgv::gui::mouse_event& me = static_cast<cgv::gui::mouse_event&>(e);
			if ((me.get_flags() & cgv::gui::EF_DND) != 0) { // select drag and drop events only
				switch (me.get_action()) {
				case cgv::gui::MA_ENTER: // store drag and drop text on dnd enter event (it is not available in drag events)
					dnd_text = me.get_dnd_text();
				case cgv::gui::MA_DRAG: // during dragging check for drop side and update dnd_mode
					dnd_pos = ivec2(me.get_x(), me.get_y());
					post_redraw(); // ensure to redraw in case dnd_mode changes
					return true;
				case cgv::gui::MA_LEAVE: // when mouse leaves window, we cancel drag and drop 
					dnd_text.clear();
					post_redraw(); // ensure to redraw to reflect change in dnd_mode 
					return true;
				case cgv::gui::MA_RELEASE: // release corresponds to drop
					dnd_pos = ivec2(me.get_x(), me.get_y());
					{
						std::vector<cgv::utils::line> lines;
						cgv::utils::split_to_lines(dnd_text, lines, true);
						for (size_t li = lines.size(); li > 0; ) {
							--li;
							std::string fn = cgv::utils::to_string(lines[li]);
							bool do_select = me.get_modifiers() == cgv::gui::EM_SHIFT;
							if (do_select)
								std::fill(selection.begin(), selection.end(), false);
							if (cgv::utils::dir::exists(fn))
								insert_directory(fn, do_select);
							else if (cgv::utils::file::exists(fn))
								insert_file(fn, do_select);
						}
					}
					dnd_text.clear();
					post_redraw();
					return true;
				}
				return false;
			}
		}
		return false;
	}
	std::string get_type_name() const { return "dnd_sample"; }
	bool self_reflect(cgv::reflect::reflection_handler& rh)
	{
		return
			rh.reflect_member("cursor_position", cursor_position) &&
			rh.reflect_member("input_dir", input_dir) &&
			rh.reflect_member("output_prefix", output_prefix) &&
			rh.reflect_member("lecture", lecture) &&
			rh.reflect_member("part", part);
	}
	bool init(cgv::render::context& ctx)
	{
		return true;
	}
	void clear(cgv::render::context& ctx)
	{
	}
	void draw(cgv::render::context& ctx)
	{
		ctx.push_pixel_coords();
		ivec2 pos(10, 24);
		for (size_t i = 0; i < file_names.size(); ++i) {
			rgb col(1, 1, 1);
			if (i == cursor_position)
				col[1] = 0.0f;
			if (selection[i])
				col[2] = 0.0f;
			ctx.set_color(col);
			ctx.set_cursor(pos[0], pos[1]);
			ctx.output_stream() << file_names[i] << " " << durations[i] << std::endl;
			pos[1] += 20;
		}
		if (!dnd_text.empty()) {
			rgb col(1, 0.5f, 0.5f);
			std::vector<cgv::utils::line> lines;
			cgv::utils::split_to_lines(dnd_text, lines);
			float w = 0;
			float s = ctx.get_current_font_size();
			for (auto l : lines)
				w = std::max(w, ctx.get_current_font_face()->measure_text_width(cgv::utils::to_string(l), s));
			float h = lines.size()*s;
			ivec2 ll_pos = dnd_pos + ivec2((int)w, (int)h);
			ivec2 draw_pos = dnd_pos;
			if (ll_pos[0] > (int)ctx.get_width())
				draw_pos[0] -= ll_pos[0] - ctx.get_width();
			if (ll_pos[1] - s > (int)ctx.get_height())
				draw_pos[1] -= int(ll_pos[1] - s - ctx.get_height());
			int ipos = int((ll_pos[1] -  24) / 20);
			if (ipos < 0)
				ipos = 0;
			if (ipos > file_names.size())
				ipos = int(file_names.size());
			insert_position = ipos;
			ctx.set_color(col);
			ctx.set_cursor(vecn(float(draw_pos[0]), float(draw_pos[1])), "", cgv::render::TA_TOP_LEFT);
			ctx.output_stream() << dnd_text << std::endl;
		}
		std::vector<vec3> P;
		P.push_back(vec3(0.0f, 20.0f * insert_position + 10.0f, 0));
		P.push_back(vec3(100.0f, 20.0f * insert_position + 10.0f, 0));
		auto& prog = ctx.ref_default_shader_program();
		cgv::render::attribute_array_binding::enable_global_array(ctx, prog.get_position_index());
		cgv::render::attribute_array_binding::set_global_attribute_array(ctx, prog.get_position_index(), P);
		ctx.set_color(rgb(1, 1, 1));
		prog.enable(ctx);
		glDrawArrays(GL_LINES, 0, 2);
		prog.disable(ctx);
		ctx.pop_pixel_coords();
	}
	void on_set(void* member_ptr)
	{
		if (member_ptr == &input_dir) {
			append_directory(input_dir);
		}
		// default implementation for all members
		update_member(member_ptr);
		post_redraw();
	}
	void create_gui()
	{
		add_decorator("dnd_sample", "heading", "level=1");
		connect_copy(add_button("concat")->click, cgv::signal::rebind(this, &dnd_sample::concat));
		add_gui("input_dir", input_dir, "directory");
		add_member_control(this, "lecture", lecture, "value_slider", "min=1;max=15;ticks=true");
		add_member_control(this, "part", part, "value_slider", "min=1;max=8;ticks=true");
		add_member_control(this, "subpart", subpart, "value_slider", "min=1;max=9;ticks=true");
		add_member_control(this, "output_prefix", output_prefix);
	}
};


#include <cgv/base/register.h>

cgv::base::object_registration<dnd_sample> reg_dnd_sample("");

#ifdef CGV_FORCE_STATIC
cgv::base::registration_order_definition ro_def("stereo_view_interactor;dnd_sample");
#endif