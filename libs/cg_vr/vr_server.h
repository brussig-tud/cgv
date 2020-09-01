#pragma once

#include <cgv/gui/key_event.h>
#include <cgv/gui/throttle_event.h>
#include <cgv/gui/stick_event.h>
#include <cgv/gui/pose_event.h>
#include <cgv/gui/event_handler.h>
#include <vr/vr_kit.h>
#include <cgv/gui/window.h>
#include <cgv/signal/signal.h>
#include <cgv/signal/bool_signal.h>

#include "lib_begin.h"

///@ingroup VR
///@{

///
namespace cgv {
	///
	namespace gui {

		/// flags to define which events should be generated by server
		enum VREventTypeFlags
		{
			VRE_NONE = 0,      //!< no events
			VRE_DEVICE = 1,    //!< device change events
			VRE_STATUS = 2,    //!< status change events
			VRE_KEY = 4,       //!< key events
			VRE_ONE_AXIS = 8,  //!< trigger / throttle / pedal events
			VRE_TWO_AXES = 16, //!< pad / stick events
			VRE_ONE_AXIS_GENERATES_KEY =32,//!< whether one axis events should generate a key event when passing inputs threshold value
			VRE_TWO_AXES_GENERATES_DPAD = 64, //!< whether two axes input generates direction pad keys when presses
			VRE_POSE = 128,    //!< pose events
			VRE_ALL = 255      //!< all event types
		};

		/// different types of event focus grabbing
		enum VRFocus
		{
			VRF_RELEASED = 0,
			VRF_GRAB = 1,
			VRF_EXCLUSIVE = 2,
			VRF_GRAB_EXCLUSIVE = 3,
			VRF_PERMANENT = 4,
			VRF_GRAB_PERMANENT = 5,
			VRF_GRAB_PERMANENT_AND_EXCLUSIVE = 7
		};

		//! server for vr events
		/*! It keeps
  the previous state of each vr kit and compares it with current state. In case of state
  changes, the server can emit device change, status change, key, throttle, stick and pose events.
  For key, throttle, stick and pose events, the classes cgv::gui::vr_key_event, cgv::gui::vr_throttle_event,
  cgv::gui::vr_stick_event, and cgv::gui::vr_pose_event are used and the event flag cgv::gui::EF_VR
  is set to mark it as vr event. To process for example a stick event in the
  cgv::gui::event_handler::handle() function, the following shows a code sample:
~~~~~~~~~~~~~~~~~~~~~~~~
	#include <cg_vr/vr_server.h>
	#include <cgv/gui/event_handler.h>
	
	bool handle(cgv::gui::event& e)
	{
		// check if vr event flag is not set and don't process events in this case
		if ((e.get_flags() & cgv::gui::EF_VR) == 0)
			return false;
		// check event id
		if (e.get_kind() == cgv::gui::EID_STICK) {
			cgv::gui::vr_stick_event& vrse = static_cast<cgv::gui::vr_stick_event&>(e);
			switch (vrse.get_action()) {
			case cgv::gui::SA_TOUCH:
			case cgv::gui::SA_PRESS:
			case cgv::gui::SA_UNPRESS:
			case cgv::gui::SA_RELEASE:
				std::cout << "stick " << vrse.get_stick_index()
					<< " of controller " << vrse.get_controller_index()
					<< " " << cgv::gui::get_stick_action_string(vrse.get_action())
					<< " at " << vrse.get_x() << ", " << vrse.get_y() << std::endl;
				return true;
			case cgv::gui::SA_MOVE:
			case cgv::gui::SA_DRAG:
				std::cout << "stick " << vrse.get_stick_index()
					<< " of controller " << vrse.get_controller_index()
					<< " " << cgv::gui::get_stick_action_string(vrse.get_action())
					<< " from " << vrse.get_last_x() << ", " << vrse.get_last_y()
					<< " to " << vrse.get_x() << ", " << vrse.get_y() << std::endl;
				return true;
			}
			return true;
		}
		return false;
	}
~~~~~~~~~~~~~~~~~~~~~~~~
  Which events are emitted by the server can be configured with the
  set_event_type_flags() function.
  Device change events are emitted with the vr_server::on_device_change signal, status change
  events with the vr_server::on_status_change signal and all others via the vr_server::on_event
  signal.

  There is a signelton vr_server instance provided with cgv::gui::ref_vr_server().
  The current vr_kit states are polled with	the check_and_emit_events() or in
  case some other class has queried the states with the check_new_state() functions for each
  vr_kit separately. In an interval of get_device_scan_interval() seconds the vr_kits are
  scanned again to detect device changes.

  The vr_server can be used in three ways. In all cases you have to connect directly to the 
  vr_server::on_device_change or vr_server::on_status_change signals in order to receive
  device change or status change events.

  1. call the cgv::gui::connect_vr_server() function in the contructor of one of your
     plugin classes with *false* as function paramter. Then a predefined callback function
	 is attached to the animation trigger singelton (accessed through the header <cgv/gui/trigger.h>)
	 that calls the vr_server singelton's check_and_emit_events() function with 60Hz.
	 The vr_server::on_event signal is attached to a callback that displatches the events
	 with the standard event processing of the framework. All registered classes derived
	 from cgv::gui::event_handler will see the events and can process them. 
	
  2. call the cgv::gui::connect_vr_server() function in the contructor of one of your
     plugin classes with *true* as function paramter. Again a callback function
	 is attached to the animation trigger singelton but it calls the 
	 vr_server singelton's check_device_changes() such that only device change events
	 are generated. In this case you or the crg_vr_view plugin have to poll the vr_kit 
	 states and call the vr_server's check_new_state() function. Again the 
	 vr_server::on_event signal is attached to a callback that displatches the events
	 to all registered cgv::gui::event_handler instances.

  3. connect directly to the vr_server::on_event signal and a timer event function to
     the animation trigger. Then you need to call the vr_server's check_and_emit_events() 
	 or check_new_state() function in your timer event function. In this approach all
	 vr events are dispatched only to the callback function that you attach to the 
	 vr_server::on_event signal of the vr_server singelton.
  */
		class CGV_API vr_server
		{
		public:
			typedef stick_event::vec2 vec2;
		protected:
			double last_device_scan;
			double device_scan_interval;
			std::vector<void*> vr_kit_handles;
			std::vector<vr::vr_kit_state> last_states;
			std::vector<unsigned> last_time_stamps;
			VREventTypeFlags event_type_flags;
			VRFocus focus_type;
			event_handler* focus;
			///
			void emit_events_and_update_state(void* kit_handle, const vr::vr_kit_state& new_state, int kit_index, VREventTypeFlags flags, double time);
			///
			float correct_deadzone_and_precision(float value, const vr::controller_input_config& IC);
			///
			vec2 correct_deadzone_and_precision(const vec2& position, const vr::controller_input_config& IC);
		public:
			/// construct server with default configuration
			vr_server();
			/// query the currently set event type flags
			VREventTypeFlags get_event_type_flags() const;
			/// set the event type flags of to be emitted events
			void set_event_type_flags(VREventTypeFlags flags);
			/// set time interval in seconds to check for device connection changes
			void set_device_scan_interval(double duration);
			/// return the time interval in seconds to check for device connection changes
			double get_device_scan_interval() const { return device_scan_interval; }
			/// check which vr_kits are present and emit on_device_change events
			void check_device_changes(double time);
			/// check which vr_kits are present, query their current states and dispatch events through on_event, on_status_change and on_device_change signals
			void check_and_emit_events(double time);
			/// in case the current vr state of a kit had been queried somewhere else, use this function to communicate the new state to the server; return whether kit_handle had been seen by server before
			bool check_new_state(void* kit_handle, const vr::vr_kit_state& new_state, double time);
			/// same as previous function but with overwrite of flags
			bool check_new_state(void* kit_handle, const vr::vr_kit_state& new_state, double time, VREventTypeFlags flags);
			/// grab the event focus to the given event handler and return whether this was possible
			bool grab_focus(VRFocus focus, event_handler* handler);
			/// release focus of handler and return whether handler had the focus
			bool release_focus(event_handler* handler);
			/// dispatch an event to focus handler and or signal attachments
			bool dispatch(cgv::gui::event& e);
			/// signal emitted to dispatch events
			cgv::signal::bool_signal<cgv::gui::event&> on_event;
			/// signal emitted to notify about device changes, first argument is handle and second a flag telling whether device got connected or if false disconnected
			cgv::signal::signal<void*, bool> on_device_change;
			/// signal emitted to notify about status changes of trackables, first argument is handle, second -1 for hmd + 0|1 for left|right controller, third is old status and fourth new status
			cgv::signal::signal<void*, int, vr::VRStatus, vr::VRStatus> on_status_change;
		};

		/// return a reference to vr server singleton
		extern CGV_API vr_server& ref_vr_server();

		/// connect the vr server to the given window or the first window of the application, if window is not provided
		extern CGV_API void connect_vr_server(bool connect_device_change_only_to_animation_trigger = true, cgv::gui::window_ptr w = cgv::gui::window_ptr());
	}
}

///@}

#include <cgv/config/lib_end.h>
