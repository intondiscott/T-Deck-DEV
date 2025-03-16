#include <TDeck_Structs.h>
#include <close-windows.h>

static void create_calendar_page(lv_event_t *e)
{
    if (!lv_obj_is_valid(tdeckDisplayUI.calendar))
    {
        tdeckDisplayUI.calendar = lv_obj_create(lv_screen_active());
        lv_obj_t *calendar = lv_calendar_create(tdeckDisplayUI.calendar);
        tdeckDisplayUI.close_btn = lv_button_create(tdeckDisplayUI.calendar);
        lv_obj_set_style_bg_color(tdeckDisplayUI.close_btn, lv_color_hex(0xfc0303), LV_PART_MAIN);
        lv_obj_t *label_close = lv_label_create(tdeckDisplayUI.close_btn);
        lv_label_set_text(label_close, LV_SYMBOL_CLOSE);
        lv_calendar_set_today_date(calendar, 2025, 01, 15);

        lv_calendar_set_showed_date(calendar, 2025, 01);
        lv_obj_align(tdeckDisplayUI.close_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
        lv_obj_set_size(tdeckDisplayUI.calendar, TFT_WIDTH, TFT_HEIGHT);
        lv_obj_set_size(calendar, 220, TFT_HEIGHT - 30);
        lv_obj_center(tdeckDisplayUI.calendar);
        lv_obj_align(calendar, LV_ALIGN_CENTER, -20, 0);
        /*Highlight a few days*/
        static lv_calendar_date_t highlighted_days[3]; /*Only its pointer will be saved so should be static*/
        highlighted_days[0].year = 2025;
        highlighted_days[0].month = 01;
        highlighted_days[0].day = 6;

        highlighted_days[1].year = 2025;
        highlighted_days[1].month = 01;
        highlighted_days[1].day = 11;

        highlighted_days[2].year = 2025;
        highlighted_days[2].month = 01;
        highlighted_days[2].day = 22;

        lv_calendar_set_highlighted_dates(calendar, highlighted_days, 3);
        lv_calendar_header_dropdown_create(calendar);
        lv_calendar_header_arrow_create(calendar);

        Serial.println("I am clicked!!!");
        lv_obj_add_event_cb(tdeckDisplayUI.close_btn, close_window_cb, LV_EVENT_CLICKED, NULL);
    }
}