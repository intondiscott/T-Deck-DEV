#include <TDeck_Structs.h>
#include <close-windows.h>

static void textarea_event_handler(lv_event_t *e)
{
    lv_obj_t *ta = (lv_obj_t *)lv_event_get_target(e);
    LV_UNUSED(ta);

    LV_LOG_USER("Enter was pressed. The current text is: %s", lv_textarea_get_text(ta));
}

static void btnm_event_handler(lv_event_t *e)
{
    lv_obj_t *obj = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t *ta = (lv_obj_t *)lv_event_get_user_data(e);
    const char *txt = lv_buttonmatrix_get_button_text(obj, lv_buttonmatrix_get_selected_button(obj));

    if (lv_strcmp(txt, LV_SYMBOL_BACKSPACE) == 0)
        lv_textarea_delete_char(ta);
    else if (lv_strcmp(txt, LV_SYMBOL_NEW_LINE) == 0)
        lv_obj_send_event(ta, LV_EVENT_READY, NULL);
    else
    {
        lv_textarea_add_text(ta, txt);
    }
}

static void create_calculator_page(lv_event_t *e)
{
    if (!lv_obj_is_valid(tdeckDisplayUI.calculator))
    {
        tdeckDisplayUI.calculator = lv_obj_create(lv_screen_active());
        lv_obj_t *ta = lv_textarea_create(tdeckDisplayUI.calculator);

        lv_group_t *keyboard_input_group = lv_group_create();
        lv_indev_t *indev = lv_indev_create();
        lv_indev_set_group(indev, keyboard_input_group);
        lv_group_add_obj(keyboard_input_group, ta);
        lv_textarea_set_one_line(ta, true);
        lv_obj_align(ta, LV_ALIGN_TOP_MID, -20, -5);
        lv_obj_set_size(ta, 200, 40);
        lv_obj_set_style_margin_bottom(ta, 5, LV_PART_MAIN);

        lv_obj_add_event_cb(ta, textarea_event_handler, LV_EVENT_READY, ta);
        lv_obj_add_state(ta, LV_STATE_FOCUSED); /*To be sure the cursor is visible*/

        static const char *btnm_map[] =
            {
                "AC", "+/-", "%", "/", "\n",
                "7", "8", "9", "*", "\n",
                "4", "5", "6", "-", "\n",
                "1", "2", "3", "+", "\n",
                LV_SYMBOL_BACKSPACE, "0", ".", "=", ""};

        lv_obj_t *btnm = lv_buttonmatrix_create(tdeckDisplayUI.calculator);
        lv_obj_set_size(btnm, 200, 150);
        lv_obj_align(btnm, LV_ALIGN_BOTTOM_MID, -20, 10);

        lv_obj_add_event_cb(btnm, btnm_event_handler, LV_EVENT_VALUE_CHANGED, ta);

        lv_obj_remove_flag(btnm, LV_OBJ_FLAG_CLICK_FOCUSABLE); /*To keep the text area focused on button clicks*/
        lv_buttonmatrix_set_map(btnm, btnm_map);

        tdeckDisplayUI.close_btn = lv_button_create(tdeckDisplayUI.calculator);
        lv_obj_set_style_bg_color(tdeckDisplayUI.close_btn, lv_color_hex(0xfc0303), LV_PART_MAIN);
        lv_obj_t *label_close = lv_label_create(tdeckDisplayUI.close_btn);
        lv_label_set_text(label_close, LV_SYMBOL_CLOSE);

        lv_obj_align(tdeckDisplayUI.close_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
        lv_obj_set_size(tdeckDisplayUI.calculator, 300, 220);
        lv_obj_center(tdeckDisplayUI.calculator);
        Serial.println("I am clicked!!!");
        lv_obj_add_event_cb(tdeckDisplayUI.close_btn, close_window_cb, LV_EVENT_CLICKED, NULL);
    }
}