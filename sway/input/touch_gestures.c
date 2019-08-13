#include <math.h>
#include "sway/input/touch_gestures.h"
#include "sway/input/cursor.h"

double measure_distance(double x1, double x2, double y1, double y2) {
	return sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
}

bool is_touch_motion_hysteresis_unset(struct sway_touch_gesture *gesture) {
	if (gesture->motion_hysteresis == 0) {
		return true;
	}
	return false;
}

void set_touch_motion_hysteresis(struct sway_touch_gesture *gesture,
		int32_t phys_size,
		int32_t ps_size) {
	gesture->motion_hysteresis = phys_size / ps_size * 10;
}

//initializing the touch points list
struct sway_touch_gesture *touch_gesture_create() {

	struct sway_touch_gesture *gesture =
			calloc(1, sizeof(struct sway_touch_gesture));
	wl_list_init(&gesture->touch_points);
	gesture->initial_surface = NULL;
	gesture->maximum_touch_points = 0;
	gesture->motion_hysteresis = 0;
	gesture->gesture_state = TAP;
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
		gesture->initial_surface = surface;
		gesture->initial_touch_id = touch_id;
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
					gesture->gesture_state == TAP &&
					(time_msec - point->time) > LONG_TAP_MS) {
				gesture->gesture_state = LONG_TAP;
			}
			wl_list_remove(&point->link);
			break;
		}
	}

	uint32_t npoints = wl_list_length(&gesture->touch_points);
	if (npoints == 0) {
		printf("Points for this gesture: %d\n", gesture->maximum_touch_points);
		//printf("is motion gesture: %s\n",
		//	 (cursor->touch_gestures.motion_gesture) ? "yes" : "no");
		printf("resulting gesture: ");
		switch (gesture->gesture_state) {
		case TAP:
			printf("Tap");
			break;
		case LONG_TAP:
			printf("Long Tap");
			break;
		case MOTION_UP:
			printf("Swipe Up");
			break;
		case MOTION_DOWN:
			printf("Swipe Down");
			break;
		case MOTION_LEFT:
			printf("Swipe Left");
			break;
		case MOTION_RIGHT:
			printf("Swipe right");
			break;
		case PINCH_IN:
			printf("Pinch In");
			break;
		case PINCH_OUT:
			printf("Pinch Out");
			break;
		default:
			break;
		}
		printf("\n");
		printf("all touch points freed, starting gesture processing\n");
		//TODO gesture processing goes here
		gesture->maximum_touch_points = 0;
		gesture->initial_surface = NULL;
		gesture->motion_gesture = false;
		gesture->gesture_state = TAP;
	}
}

bool process_touch_motion(struct sway_touch_gesture *gesture,
		double layout_x,
		double layout_y,
		int32_t touch_id) {

	struct sway_touch_point *point;
	wl_list_for_each(point, &gesture->touch_points, link) {
		if (point->touch_id == touch_id) {
			break;
		}
	}

	if (measure_distance(point->x, layout_x, point->y, layout_y) >=
			gesture->motion_hysteresis) {
		gesture->motion_gesture = true;
		double dir_x = layout_x - point->x;
		double dir_y = layout_y - point->y;
		double dx = fabs(dir_x);
		double dy = fabs(dir_y);
		if (dx > dy) {
		  //horizontal motion
		  if (dir_x < 0) {
		    //left
		    gesture->gesture_state = MOTION_LEFT;
		  } else {
		    gesture->gesture_state = MOTION_RIGHT;
		  }
		} else {
		  //vertical motion
		  if (dir_y < 0) {
		    //up
		    gesture->gesture_state = MOTION_UP;
		  } else {
		    //down
		    gesture->gesture_state = MOTION_DOWN;
		  }
		}
	}

	//not sending touch motion if three or more fingers present
	if (wl_list_length(&gesture->touch_points) >= 3) {
		return false;
	}
	return true;
}

void touch_gesture_destroy(struct sway_touch_gesture *gesture) {

	gesture->initial_surface = NULL;
	wl_list_remove(&gesture->touch_points);
	free(gesture);
}
