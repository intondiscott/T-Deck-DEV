#include <lvgl.h>
#pragma once
static void close_window_cb(lv_event_t *e)
{

    // lv_obj_clean(lv_obj_get_parent((lv_obj_t *)lv_event_get_target(e)));
    lv_obj_delete(lv_obj_get_parent((lv_obj_t *)lv_event_get_target(e)));
}