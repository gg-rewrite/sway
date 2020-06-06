#ifndef _SWAY_INPUT_TOUCH_H
#define _SWAY_INPUT_TOUCH_H
#include <stdbool.h>
#include <stdint.h>
#include <wayland-util.h>
#include "sway/input/cursor.h"


#define LONG_TAP_MS 500

enum touch_gesture_states {
 TAP,
 LONG_TAP,
 MOTION_UP,
 MOTION_DOWN,
 MOTION_LEFT,
 MOTION_RIGHT,
 PINCH_IN,
 PINCH_OUT,
		     
};


/**
represents a single touch point to be stored by sway_cursor 
for processing multitouch gestures
 */
struct sway_touch_point {
  struct wl_list link;
  int32_t touch_id;
  double x;
  double y;
  uint32_t time;
  double initial_distance;
  double current_distance;
};


struct sway_touch_gesture {
    struct wl_list touch_points;
    struct wlr_surface *initial_surface;
    int32_t initial_touch_id;
    bool motion_gesture;
    uint32_t maximum_touch_points;
    double motion_hysteresis;
    enum touch_gesture_states gesture_state;
};

double measure_distance(double x1, double x2,
			double y1, double y2);

bool is_touch_motion_hysteresis_unset(struct sway_touch_gesture *gesture);

void set_touch_motion_hysteresis(struct sway_touch_gesture *gesture,
			   int32_t phys_size,
			   int32_t px_size);

struct sway_touch_gesture* touch_gesture_create();
void touch_gesture_destroy(struct sway_touch_gesture *gesture);

/**returns true if allowed to proceed with touch passthrough,
false otherwise
 */
bool process_touch_down(struct sway_touch_gesture *gesture,
			int32_t touch_id, double layout_x,
			double layout_y, uint32_t time_msec,
			struct wlr_surface *surface);

void process_touch_up(struct sway_touch_gesture *gesture,
			  int32_t touch_id, uint32_t time_msec);

bool process_touch_motion(struct sway_touch_gesture *gesture,
			  double layout_x, double layout_y,
			  int32_t touch_id);
#endif
