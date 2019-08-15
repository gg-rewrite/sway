#ifndef _SWAY_INPUT_TOUCH_H
#define _SWAY_INPUT_TOUCH_H
#include <stdbool.h>
#include <stdint.h>
#include <wayland-util.h>
#include "sway/input/cursor.h"

#define HYSTERESIS_MM 20
#define LONG_TAP_MS 500

/**
 * represents a single touch point to be stored by sway_cursor 
 * for processing multitouch gestures
 */
struct sway_touch_point {
	struct wl_list link;
	int32_t touch_id;
	double x;
	double y;
	double dx;
	double dy;
	uint32_t time;
	double initial_distance;
};

/**
 * represents a single touch gesture holding all the touch points, 
 * initial and neighboring points for pinch zoom detection and
 * calculated motion hysteresis based on the assigned output's DPM
 */
struct sway_touch_gesture {
	struct sway_seat *seat;
	struct wl_list touch_points;
	int32_t initial_touch_id;
	int32_t next_touch_id;
	uint32_t maximum_touch_points;
	double motion_hysteresis;
	enum touch_gesture_types gesture_state;
};

/**
 * returns true if hysteresis isn't calculated yet. Shouldn't be needed
 * once I understand how to get output's parameters in sway_cursor_create
 */
bool is_touch_motion_hysteresis_unset(struct sway_touch_gesture *gesture);

/**
 * calculates hysteresis in px based on DPM and hysteresis in mm
 */
void set_touch_motion_hysteresis(struct sway_touch_gesture *gesture,
		int32_t phys_size,
		int32_t px_size);

/**
 * creates and initializes a single gesture
 */
struct sway_touch_gesture *touch_gesture_create(struct sway_seat *seat);

/**
 * destroys the gesture 
 */
void touch_gesture_destroy(struct sway_touch_gesture *gesture);

/**
 * registers touch points, fills in the initial point.
 * returns true if allowed to proceed with touch passthrough,
 * false otherwise
 */
bool process_touch_down(struct sway_touch_gesture *gesture,
		int32_t touch_id,
		double layout_x,
		double layout_y,
		uint32_t time_msec,
		struct wlr_surface *surface);

/**
 * registers released points, calculates the final gesture to send to processing
 */
void process_touch_up(struct sway_touch_gesture *gesture,
		int32_t touch_id,
		uint32_t time_msec);

/**
 * registers touch motion, detects swipe and pinch zoom
 */
bool process_touch_motion(struct sway_touch_gesture *gesture,
		double layout_x,
		double layout_y,
		int32_t touch_id);
#endif
