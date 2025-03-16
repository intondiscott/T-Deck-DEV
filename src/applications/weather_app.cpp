#include <TDeck_Structs.h>
#include <close-windows.h>

static char weather_buffer[7];
extern Weather *weather_vals;
static void create_weather_page(lv_event_t *e)
{
    if (!lv_obj_is_valid(tdeckDisplayUI.weather))
    {
        tdeckDisplayUI.weather = lv_obj_create(lv_screen_active());
        lv_obj_t *weather_win = lv_obj_create(tdeckDisplayUI.weather);
        lv_obj_set_size(weather_win, 200, 200);
        lv_obj_set_flex_flow(weather_win, LV_FLEX_FLOW_COLUMN);
        tdeckDisplayUI.wind_speed_label = lv_label_create(weather_win);
        tdeckDisplayUI.humidity_label = lv_label_create(weather_win);
        tdeckDisplayUI.temperature_label = lv_label_create(weather_win);
        tdeckDisplayUI.close_btn = lv_button_create(tdeckDisplayUI.weather);
        lv_obj_set_style_bg_color(tdeckDisplayUI.close_btn, lv_color_hex(0xfc0303), LV_PART_MAIN);
        lv_obj_t *label_close = lv_label_create(tdeckDisplayUI.close_btn);
        lv_label_set_text(label_close, LV_SYMBOL_CLOSE);
        snprintf(weather_buffer, sizeof(weather_buffer), "%3.2f", (weather_vals->temperature - 273.15) * 9 / 5 + 32);
        lv_label_set_text_fmt(tdeckDisplayUI.temperature_label, "%s°F", weather_buffer);
        snprintf(weather_buffer, sizeof(weather_buffer), "%3.2f", weather_vals->wind_speed);
        lv_label_set_text_fmt(tdeckDisplayUI.wind_speed_label, "Wind Speed: %s MPH", weather_buffer);
        lv_label_set_text_fmt(tdeckDisplayUI.humidity_label, "Hum: %d%%", weather_vals->humidity);
        lv_obj_align(tdeckDisplayUI.close_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
        lv_obj_set_size(tdeckDisplayUI.weather, 300, 250);
        lv_obj_center(tdeckDisplayUI.weather);
        Serial.println("I am clicked!!!");
        lv_obj_add_event_cb(tdeckDisplayUI.close_btn, close_window_cb, LV_EVENT_CLICKED, NULL);
    }
}