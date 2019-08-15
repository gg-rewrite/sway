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
		uint32_t time_msec,
		struct wlr_surface *surface) {
	struct sway_touch_point *touch_point =
			calloc(1, sizeof(struct sway_touch_point));
	touch_point->x = layout_x;
	touch_point->y = layout_y;
	touch_point->touch_id = touch_id;
	touch_point->time = time_msec;

	if (gesture->maximum_touch_points == 0) {
		gesture->initial_touch_id = touch_id;
		touch_point->initial_distance = 0;
	} else if (gesture->maximum_touch_points == 1) {
		//TODO make a less dumb way of just taking a point next to initial one
		gesture->next_touch_id = touch_id;
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
	bool is_assigned_cmd = false;

	uint32_t npoints = wl_list_length(&gesture->touch_points);
	if (npoints == 0) {
		switch (gesture->gesture_state) {
		case GESTURE_TAP:
			break;
		case GESTURE_LONG_TAP:
			switch (gesture->maximum_touch_points) {
			case 3:
				binding->command = "exec termite";
				is_assigned_cmd = true;
				break;
			case 4:
				break;
			case 5:
				break;
			default:
				break;
			}
			break;
		case GESTURE_SWIPE_UP:
			break;
		case GESTURE_SWIPE_DOWN:
			switch (gesture->maximum_touch_points) {
			case 3:
				break;
			case 4:
			  binding->command = "kill";
			  is_assigned_cmd = true;
				break;
			case 5:
				break;
			default:
				break;
			}

			break;
		case GESTURE_SWIPE_LEFT:
			break;
		case GESTURE_SWIPE_RIGHT:
			break;
		case GESTURE_PINCH_IN:
			break;
		case GESTURE_PINCH_OUT:
			break;
		default:
			break;
		}
		//TODO gesture processing goes here
		if (is_assigned_cmd) {
			seat_execute_command(gesture->seat, binding);
		}
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
			(wl_list_length(&gesture->touch_points) >= 3) ? true : false;

	struct sway_touch_point *point;
	wl_list_for_each(point, &gesture->touch_points, link) {
		if (point->touch_id == touch_id) {
			break;
		}
	}

	point->dx = layout_x;
	point->dy = layout_y;

	if (measure_distance(point->x, layout_x, point->y, layout_y) >=
			gesture->motion_hysteresis) {

		//swipe detection
		double dir_x = layout_x - point->x;
		double dir_y = layout_y - point->y;
		double dx = fabs(dir_x);
		double dy = fabs(dir_y);
		if (dx > dy) {
			//horizontal motion
			if (dir_x < 0) {
				//left
				gesture->gesture_state = GESTURE_SWIPE_LEFT;
			} else {
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

		if (wl_list_length(&gesture->touch_points) >= 2) {
			struct sway_touch_point *initial_point =
					get_point_by_id(gesture, gesture->initial_touch_id);
			struct sway_touch_point *next_point =
					get_point_by_id(gesture, gesture->next_touch_id);

			double initial_d = measure_distance(initial_point->x,
					next_point->x,
					initial_point->y,
					next_point->y); // sounds of eurobeat in the distance
			double current_d = measure_distance(initial_point->dx,
					next_point->dx,
					initial_point->dy,
					next_point->dy);

			if (fabs(current_d - initial_d) > gesture->motion_hysteresis) {
				if (current_d > initial_d) {
					gesture->gesture_state = GESTURE_PINCH_OUT;
				} else {
					gesture->gesture_state = GESTURE_PINCH_IN;
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
