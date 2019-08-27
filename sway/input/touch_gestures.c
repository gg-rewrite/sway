#include <math.h>
#include "sway/config.h"
#include "sway/input/touch_gestures.h"

double measure_distance(double x1, double x2, double y1, double y2) {
	return sqrt(pow((x1 - x2), 2) + pow((y1 - y2), 2));
}

struct sway_touch_point *get_point_by_id(struct sway_touch_gesture *gesture,
		int32_t id) {
	struct sway_touch_point *point;
	wl_list_for_each(point, &gesture->touch_points, link) {
		if (point->touch_id == id) {
			break;
		}
	}
	return point;
}

bool is_touch_motion_hysteresis_unset(struct sway_touch_gesture *gesture) {
	if (gesture->motion_hysteresis == 0) {
		return true;
	}
	return false;
}

void set_touch_motion_hysteresis(struct sway_touch_gesture *gesture,
		int32_t phys_size,
		int32_t px_size) {
	gesture->motion_hysteresis = px_size / phys_size * HYSTERESIS_MM;
}

//initializing the touch points list
struct sway_touch_gesture *touch_gesture_create(struct sway_seat *seat) {

	struct sway_touch_gesture *gesture =
			calloc(1, sizeof(struct sway_touch_gesture));
	gesture->seat = seat;
	wl_list_init(&gesture->touch_points);
	gesture->maximum_touch_points = 0;
	gesture->motion_hysteresis = 0;
	gesture->gesture_state = GESTURE_TAP;
	return gesture;
}

bool process_touch_down(struct sway_touch_gesture *gesture,
		int32_t touch_id,
		double layout_x,
		double layout_y,
		uint32_t time_msec) {
	struct sway_touch_point *touch_point =
			calloc(1, sizeof(struct sway_touch_point));
	touch_point->x = layout_x;
	touch_point->y = layout_y;
	touch_point->dx = layout_x;
	touch_point->dy = layout_y;
	touch_point->touch_id = touch_id;
	touch_point->time = time_msec;

	if (gesture->maximum_touch_points == 0) {
		gesture->initial_touch_id = touch_id;
		touch_point->initial_distance = 0;
	} else {
		struct sway_touch_point *initial_point =
				get_point_by_id(gesture, gesture->initial_touch_id);
		touch_point->initial_distance = measure_distance(touch_point->x,
				initial_point->x,
				touch_point->y,
				initial_point->y);
	}

	wl_list_insert(&gesture->touch_points, &touch_point->link);

	uint32_t npoints = wl_list_length(&gesture->touch_points);
	if (npoints > gesture->maximum_touch_points) {
		gesture->maximum_touch_points = npoints;
	}

	if (npoints >= 3) {
		return false;
	} else {
		return true;
	}
}

void process_touch_up(struct sway_touch_gesture *gesture,
		int32_t touch_id,
		uint32_t time_msec) {

	//find the touch point with this id, remove it
	//and tell the resulting number of points

	struct sway_touch_point *point, *tmp;
	wl_list_for_each_safe(point, tmp, &gesture->touch_points, link) {
		if (touch_id == point->touch_id) {
			if (touch_id == gesture->initial_touch_id &&
					gesture->gesture_state == GESTURE_TAP &&
					(time_msec - point->time) > LONG_TAP_MS) {
				gesture->gesture_state = GESTURE_LONG_TAP;
			}
			wl_list_remove(&point->link);
			break;
		}
	}

	struct sway_binding *binding = calloc(1, sizeof(struct sway_binding));
	binding->type = BINDING_TOUCH;
	//bool is_assigned_cmd = false;

	uint32_t npoints = wl_list_length(&gesture->touch_points);
	if (npoints == 0) {

		list_t *mode_bindings = config->current_mode->touch_bindings;
		for (int i = 0; i < mode_bindings->length; i++) {
			struct sway_touch_binding *config_binding = mode_bindings->items[i];
			if (config_binding->npoints == gesture->maximum_touch_points &&
					config_binding->type == gesture->gesture_state) {
				binding->command = config_binding->command;
				seat_execute_command(gesture->seat, binding);
				break;
			}
		}



		switch(gesture->gesture_state) {
		case GESTURE_LONG_TAP:
		  printf("long tap");
		  break;
		case GESTURE_PINCH_IN:
		  printf("pinch in");
		  break;
		case GESTURE_PINCH_OUT:
		  printf("pinch out");
		  break;
		case GESTURE_SWIPE_DOWN:
		  printf("swipe down");
		  break;
		case GESTURE_SWIPE_LEFT:
		  printf("swipe left");
		  break;
		case GESTURE_SWIPE_RIGHT:
		  printf("swipe right");
		  break;
		case GESTURE_SWIPE_UP:
		  printf("swipe up");
		  break;
		case GESTURE_TAP:
		  printf("tap");
		  break;
		default:
		  break;
		}
		printf("\n");

		free(binding);
		gesture->maximum_touch_points = 0;
		gesture->gesture_state = GESTURE_TAP;
	}
}

bool process_touch_motion(struct sway_touch_gesture *gesture,
		double layout_x,
		double layout_y,
		int32_t touch_id) {
  
	//not sending touch events if three or more fingers touching
	bool touch_passthrough =
			(gesture->maximum_touch_points >= 3) ? false : true;

	struct sway_touch_point *point;
	bool found = false;
	wl_list_for_each(point, &gesture->touch_points, link) {
		if (point->touch_id == touch_id) {
			found = true;
			break;
		}
	}

	if (!found) {
		return touch_passthrough;
	}
	
	point->dx = layout_x;
	point->dy = layout_y;

	/*
	 * Detecting pinch
	 */

	struct sway_touch_point *initial_point =
			get_point_by_id(gesture, gesture->initial_touch_id);

	bool is_pinch = false;

	wl_list_for_each(point, &gesture->touch_points, link) {
		if (point->touch_id == gesture->initial_touch_id) {
			continue;
		}

		double current_distance = measure_distance(
				point->dx, initial_point->dx, point->dy, initial_point->dy);

		printf("point %d current distance %f\n",
		       point->touch_id,
		       current_distance);
		
		if (fabs(current_distance - point->initial_distance) >
				gesture->motion_hysteresis) {
			if (current_distance > point->initial_distance) {
				gesture->gesture_state = GESTURE_PINCH_OUT;
			} else {
				gesture->gesture_state = GESTURE_PINCH_IN;
			}
			is_pinch = true;
		} else {
			is_pinch = false;
			break;
		}
	}

	// detecting swipe
	if (!is_pinch) {
		double swipe_distance = measure_distance(initial_point->x,
				initial_point->dx,
				initial_point->y,
				initial_point->dy);
		if (swipe_distance > gesture->motion_hysteresis) {
			double dir_x = initial_point->dx - initial_point->x;
			double dir_y = initial_point->dy - initial_point->y;
			double dx = fabs(dir_x);
			double dy = fabs(dir_y);
			if (dx > dy) {
				//horizontal motion
				if (dir_x < 0) {
					//left
					gesture->gesture_state = GESTURE_SWIPE_LEFT;
				} else {
					//right
					gesture->gesture_state = GESTURE_SWIPE_RIGHT;
				}
			} else {
				//vertical motion
				if (dir_y < 0) {
					//up
					gesture->gesture_state = GESTURE_SWIPE_UP;
				} else {
					//down
					gesture->gesture_state = GESTURE_SWIPE_DOWN;
				}
			}
		}
	}
	return touch_passthrough;
}

void touch_gesture_destroy(struct sway_touch_gesture *gesture) {

	wl_list_remove(&gesture->touch_points);
	free(gesture);
}
