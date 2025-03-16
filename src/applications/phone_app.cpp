#include <TDeck_Structs.h>
#include <close-windows.h>

static void create_phone_page(lv_event_t *e)
{
  if (!lv_obj_is_valid(tdeckDisplayUI.phone))
  {
    tdeckDisplayUI.phone = lv_obj_create(lv_screen_active());
    lv_obj_t *label = lv_label_create(tdeckDisplayUI.phone);
    tdeckDisplayUI.close_btn = lv_button_create(tdeckDisplayUI.phone);
    lv_obj_set_style_bg_color(tdeckDisplayUI.close_btn, lv_color_hex(0xfc0303), LV_PART_MAIN);
    lv_obj_t *label_close = lv_label_create(tdeckDisplayUI.close_btn);
    lv_label_set_text(label_close, LV_SYMBOL_CLOSE);
    lv_label_set_text(label, "Phone Page");
    lv_obj_align(tdeckDisplayUI.close_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_size(tdeckDisplayUI.phone, 200, 200);
    lv_obj_center(tdeckDisplayUI.phone);
    Serial.println("I am clicked!!!");
    lv_obj_add_event_cb(tdeckDisplayUI.close_btn, close_window_cb, LV_EVENT_CLICKED, NULL);
  }
}