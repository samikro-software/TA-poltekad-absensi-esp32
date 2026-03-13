#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens

enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_HOME_PAGE = 1,
    SCREEN_ID_SECOND_PAGE = 2,
    _SCREEN_ID_LAST = 2
};

typedef struct _objects_t {
    lv_obj_t *home_page;
    lv_obj_t *second_page;
    lv_obj_t *lb_time;
    lv_obj_t *lb_date;
    lv_obj_t *lb_connection;
    lv_obj_t *lb_state;
    lv_obj_t *lb_mark;
    lv_obj_t *lb_total_offline;
    lv_obj_t *lb_scan;
    lv_obj_t *pl_modal;
    lv_obj_t *pl_success;
    lv_obj_t *lb_name;
    lv_obj_t *lb_report_state;
    lv_obj_t *lb_report_time;
    lv_obj_t *spn_load;
    lv_obj_t *label2;
    lv_obj_t *panel2;
    lv_obj_t *panel3;
    lv_obj_t *label3;
} objects_t;

extern objects_t objects;

void create_screen_home_page();
void tick_screen_home_page();

void create_screen_second_page();
void tick_screen_second_page();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/