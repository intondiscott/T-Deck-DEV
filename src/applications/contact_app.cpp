#include <lvgl.h>
#include <Arduino.h>
#include <close-windows.h>
#include <TDeck_Structs.h>
//extern TdeckDisplayUI tdeckDisplayUI;

static void create_contact_page(lv_event_t *e)
{
    if (!lv_obj_is_valid(tdeckDisplayUI.contact))
    {
        tdeckDisplayUI.contact = lv_obj_create(lv_screen_active());

        lv_obj_t *label = lv_label_create(tdeckDisplayUI.contact);
        tdeckDisplayUI.close_btn = lv_button_create(tdeckDisplayUI.contact);
        lv_obj_set_style_bg_color(tdeckDisplayUI.close_btn, lv_color_hex(0xfc0303), LV_PART_MAIN);
        lv_obj_t *label_close = lv_label_create(tdeckDisplayUI.close_btn);
        lv_label_set_text(label_close, LV_SYMBOL_CLOSE);
        lv_label_set_text(label, "Contact Page");
        lv_obj_align(tdeckDisplayUI.close_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
        lv_obj_set_size(tdeckDisplayUI.contact, 200, 200);
        lv_obj_center(tdeckDisplayUI.contact);
        Serial.println("I am clicked!!!");
        lv_obj_add_event_cb(tdeckDisplayUI.close_btn, close_window_cb, LV_EVENT_CLICKED, NULL);
    }
}