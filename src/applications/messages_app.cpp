#include <lvgl.h>
#include <TDeck_Structs.h>
#include <keyboard.h>
#include <close-windows.h>
#include <RadioLib.h>
#include <utilities.h>



//TdeckDisplayUI tdeckDisplayUI;
extern Settings *setting_values;
extern SX1262 radio; // SX1262 radio object
#define TFT_WIDTH 320
#define TFT_HEIGHT 240
static void my_keypad_read(lv_indev_t *indev_driver, lv_indev_data_t *data)
{
    data->state = LV_INDEV_STATE_RELEASED;
    char key_press = keyboard_get_key();
    data->key = key_press;

    if (key_press)
    {
        data->state = LV_INDEV_STATE_PRESSED;
        data->btn_id = key_press;
    }
}

static void ta_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *ta = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t *kb = (lv_obj_t *)lv_event_get_user_data(e);
    if (code == LV_EVENT_FOCUSED)
    {
        lv_keyboard_set_textarea(kb, ta);
        lv_textarea_add_char(ta, keyboard_get_key());

        lv_obj_remove_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
    if (code == LV_EVENT_READY)
    {
        // lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        if (setting_values->radio_communications)
            radio.startTransmit(lv_textarea_get_text(ta));

        lv_textarea_set_text(ta, "");
    }

    if (code == LV_EVENT_DEFOCUSED)
    {
        lv_keyboard_set_textarea(kb, NULL);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
}

static void create_messages_page(lv_event_t *e)
{
    if (!lv_obj_is_valid(tdeckDisplayUI.messages))
    {
        tdeckDisplayUI.messages = lv_obj_create(lv_screen_active());

        lv_obj_set_size(tdeckDisplayUI.messages, TFT_WIDTH, TFT_HEIGHT);
        lv_obj_center(tdeckDisplayUI.messages);

        /*Create a keyboard to use it with an of the text areas*/

        /*Create a text area. The keyboard will write here*/
        lv_obj_t *ta1;
        ta1 = lv_textarea_create(tdeckDisplayUI.messages);
        lv_indev_t *indev = lv_indev_create(); /*Create an input device*/
        lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
        lv_indev_set_read_cb(indev, my_keypad_read);
        lv_group_t *keyboard_input_group = lv_group_create();

        lv_indev_set_group(indev, keyboard_input_group);
        lv_group_add_obj(keyboard_input_group, ta1);
        lv_obj_t *kb = lv_keyboard_create(tdeckDisplayUI.messages);

        lv_obj_align(ta1, LV_ALIGN_TOP_LEFT, 0, 0);
        lv_obj_add_event_cb(ta1, ta_event_cb, LV_EVENT_ALL, kb);
        lv_textarea_set_placeholder_text(ta1, "Put Message here...");
        lv_obj_set_size(ta1, 250, 80);

        lv_keyboard_set_textarea(kb, ta1);
        tdeckDisplayUI.close_btn = lv_button_create(tdeckDisplayUI.messages);
        lv_obj_set_style_bg_color(tdeckDisplayUI.close_btn, lv_color_hex(0xfc0303), LV_PART_MAIN);
        lv_obj_t *label_close = lv_label_create(tdeckDisplayUI.close_btn);
        lv_label_set_text(label_close, LV_SYMBOL_CLOSE);

        lv_obj_align(tdeckDisplayUI.close_btn, LV_ALIGN_TOP_RIGHT, 0, -5);

        lv_obj_add_event_cb(tdeckDisplayUI.close_btn, close_window_cb, LV_EVENT_CLICKED, NULL);
    }
}