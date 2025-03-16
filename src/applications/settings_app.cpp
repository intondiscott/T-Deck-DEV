#include <TDeck_Structs.h>
#include <RadioLib.h>
#include <close-windows.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <utilities.h>
#include <WiFi.h>
typedef enum
{
    LV_MENU_ITEM_BUILDER_VARIANT_1,
    LV_MENU_ITEM_BUILDER_VARIANT_2
} lv_menu_builder_variant_t;

extern Settings *setting_values;
extern SX1262 radio; // SX1262 radio object

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

static void setBrightness(uint8_t value)
{
    static uint8_t level = 0;
    static uint8_t steps = 16;
    if (value == 0)
    {
        digitalWrite(BOARD_BL_PIN, 0);
        delay(3);
        level = 0;
        return;
    }
    if (level == 0)
    {
        digitalWrite(BOARD_BL_PIN, 1);
        level = steps;
        delayMicroseconds(30);
    }
    int from = steps - level;
    int to = steps - value;
    int num = (steps + to - from) % steps;
    for (int i = 0; i < num; i++)
    {
        digitalWrite(BOARD_BL_PIN, 0);
        digitalWrite(BOARD_BL_PIN, 1);
    }
    level = value;
}

static void switch_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *menu = (lv_obj_t *)lv_event_get_user_data(e);
    lv_obj_t *obj = (lv_obj_t *)lv_event_get_target(e);
    if (code == LV_EVENT_VALUE_CHANGED)
    {
        if (lv_obj_has_state(obj, LV_STATE_CHECKED))
        {
            lv_menu_set_page(menu, NULL);
            lv_menu_set_sidebar_page(menu, tdeckDisplayUI.root_page);
            lv_obj_send_event(lv_obj_get_child(lv_obj_get_child(lv_menu_get_cur_sidebar_page(menu), 0), 0), LV_EVENT_CLICKED,
                              NULL);
        }
        else
        {
            lv_menu_set_sidebar_page(menu, NULL);
            lv_menu_clear_history(menu); /* Clear history because we will be showing the root page later */
            lv_menu_set_page(menu, tdeckDisplayUI.root_page);
        }
    }
}

static void back_event_handler(lv_event_t *e)
{
    lv_obj_t *obj = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t *menu = (lv_obj_t *)lv_event_get_user_data(e);

    if (lv_menu_back_button_is_root(menu, obj))
    {
        lv_obj_t *mbox1 = lv_msgbox_create(NULL);
        lv_msgbox_add_title(mbox1, "Hello");
        lv_msgbox_add_text(mbox1, "Root back btn click.");
        lv_msgbox_add_close_button(mbox1);
    }
}

static lv_obj_t *create_text(lv_obj_t *parent, const char *icon, const char *txt,
                             lv_menu_builder_variant_t builder_variant)
{
    lv_obj_t *obj = lv_menu_cont_create(parent);

    lv_obj_t *img = NULL;
    lv_obj_t *label = NULL;

    if (icon)
    {
        img = lv_image_create(obj);
        lv_image_set_src(img, icon);
    }

    if (txt)
    {
        label = lv_label_create(obj);
        lv_label_set_text(label, txt);
        lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_flex_grow(label, 1);
    }

    if (builder_variant == LV_MENU_ITEM_BUILDER_VARIANT_2 && icon && txt)
    {
        lv_obj_add_flag(img, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
        lv_obj_swap(img, label);
    }

    return obj;
}

static void brightness_control_cb(lv_event_t *e)
{
    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
    setting_values->brightness = (int)lv_slider_get_value(slider);
    setBrightness(setting_values->brightness);
}

static lv_obj_t *create_slider(lv_obj_t *parent, const char *icon, const char *txt, int32_t min, int32_t max,
                               int32_t val)
{
    lv_obj_t *obj = create_text(parent, icon, txt, LV_MENU_ITEM_BUILDER_VARIANT_2);

    lv_obj_t *slider = lv_slider_create(obj);
    lv_obj_set_flex_grow(slider, 1);
    lv_slider_set_range(slider, min, max);
    lv_slider_set_value(slider, val, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, brightness_control_cb, LV_EVENT_VALUE_CHANGED, NULL);

    if (icon == NULL)
    {
        lv_obj_add_flag(slider, LV_OBJ_FLAG_FLEX_IN_NEW_TRACK);
    }

    return obj;
}

static void switch_control_cb(lv_event_t *e)
{
    lv_obj_t *toggle_switch = (lv_obj_t *)lv_event_get_target(e);
    if (lv_event_get_user_data(e) == "WiFi")
    {
        setting_values->wifi_communications = lv_obj_has_state(toggle_switch, LV_STATE_CHECKED);
        if (!setting_values->wifi_communications)
            WiFi.disconnect();
        else
            WiFi.begin();
    }

    if (lv_event_get_user_data(e) == "Bluetooth")
    {
        setting_values->bluetooth_communications = lv_obj_has_state(toggle_switch, LV_STATE_CHECKED);
        if (!setting_values->bluetooth_communications)
        {
            BLEDevice::deinit();
        }

        else
        {
            BLEDevice::init("T-Deck");
            BLEServer *pServer = BLEDevice::createServer();
            BLEService *pService = pServer->createService(SERVICE_UUID);
            BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                CHARACTERISTIC_UUID,
                BLECharacteristic::PROPERTY_READ |
                    BLECharacteristic::PROPERTY_WRITE);

            pService->start();

            BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
            pAdvertising->addServiceUUID(SERVICE_UUID);
            pAdvertising->setScanResponse(true);
            pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
            pAdvertising->setMinPreferred(0x12);
            BLEDevice::startAdvertising();
        }
    }

    if (lv_event_get_user_data(e) == "Radio")
    {
        setting_values->radio_communications = lv_obj_has_state(toggle_switch, LV_STATE_CHECKED);
        if (setting_values->radio_communications)
        {
            // Initialize the radio module
            Serial.print(F("[SX1262] Initializing ... "));
            int state = radio.begin();
            if (state == RADIOLIB_ERR_NONE)
            {
                Serial.println(F("success!"));
            }
            else
            {
                Serial.print(F("failed, code "));
                Serial.println(state);
            }
        }
        else
        {
            // Deinitialize the radio module
            Serial.println(F("[SX1262] Deinitializing ... "));
            radio.sleep();
        }
    }
}

static lv_obj_t *create_switch(lv_obj_t *parent, const char *icon, const char *txt, bool chk)
{
    lv_obj_t *obj = create_text(parent, icon, txt, LV_MENU_ITEM_BUILDER_VARIANT_1);

    lv_obj_t *sw = lv_switch_create(obj);
    lv_obj_add_state(sw, chk);
    String *user_data = (String *)txt;
    lv_obj_add_event_cb(sw, switch_control_cb, LV_EVENT_VALUE_CHANGED, user_data);
    return obj;
}

static void create_setting_page(lv_event_t *e)
{
    if (!lv_obj_is_valid(tdeckDisplayUI.setting))
    {
        tdeckDisplayUI.setting = lv_menu_create(lv_screen_active());
        // lv_obj_t *label = lv_label_create(TdeckDisplayUI.setting);
        tdeckDisplayUI.close_btn = lv_button_create(tdeckDisplayUI.setting);
        lv_obj_set_style_bg_color(tdeckDisplayUI.close_btn, lv_color_hex(0xfc0303), LV_PART_MAIN);
        lv_obj_t *label_close = lv_label_create(tdeckDisplayUI.close_btn);
        lv_label_set_text(label_close, LV_SYMBOL_CLOSE);
        // lv_label_set_text(label, "Settings Page");

        lv_color_t bg_color = lv_obj_get_style_bg_color(tdeckDisplayUI.setting, 0);
        if (lv_color_brightness(bg_color) > 127)
        {
            lv_obj_set_style_bg_color(tdeckDisplayUI.setting, lv_color_darken(lv_obj_get_style_bg_color(tdeckDisplayUI.setting, 0), 10), 0);
        }
        else
        {
            lv_obj_set_style_bg_color(tdeckDisplayUI.setting, lv_color_darken(lv_obj_get_style_bg_color(tdeckDisplayUI.setting, 0), 50), 0);
        }
        lv_menu_set_mode_root_back_button(tdeckDisplayUI.setting, LV_MENU_ROOT_BACK_BUTTON_ENABLED);
        lv_obj_add_event_cb(tdeckDisplayUI.setting, back_event_handler, LV_EVENT_CLICKED, tdeckDisplayUI.setting);
        lv_obj_set_size(tdeckDisplayUI.setting, lv_display_get_horizontal_resolution(NULL), lv_display_get_vertical_resolution(NULL));
        lv_obj_center(tdeckDisplayUI.setting);

        lv_obj_t *cont;
        lv_obj_t *section;

        lv_obj_t *sub_display_page = lv_menu_page_create(tdeckDisplayUI.setting, NULL);
        lv_obj_set_style_pad_hor(sub_display_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(tdeckDisplayUI.setting), 0), 0);
        lv_menu_separator_create(sub_display_page);
        section = lv_menu_section_create(sub_display_page);
        create_slider(section, LV_SYMBOL_SETTINGS, "Brightness", 3, 16, setting_values->brightness);

        lv_obj_t *sub_communications_page = lv_menu_page_create(tdeckDisplayUI.setting, NULL);
        lv_obj_set_style_pad_hor(sub_communications_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(tdeckDisplayUI.setting), 0), 0);
        lv_menu_separator_create(sub_communications_page);
        section = lv_menu_section_create(sub_communications_page);
        create_switch(section, LV_SYMBOL_WIFI, "WiFi", setting_values->wifi_communications);
        create_switch(section, LV_SYMBOL_BLUETOOTH, "Bluetooth", setting_values->bluetooth_communications);
        create_switch(section, LV_SYMBOL_GPS, "Radio", setting_values->radio_communications);

        lv_obj_t *sub_software_info_page = lv_menu_page_create(tdeckDisplayUI.setting, NULL);
        lv_obj_set_style_pad_hor(sub_software_info_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(tdeckDisplayUI.setting), 0), 0);
        section = lv_menu_section_create(sub_software_info_page);
        create_text(section, NULL, "Version 1.0", LV_MENU_ITEM_BUILDER_VARIANT_1);

        lv_obj_t *sub_legal_info_page = lv_menu_page_create(tdeckDisplayUI.setting, NULL);
        lv_obj_set_style_pad_hor(sub_legal_info_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(tdeckDisplayUI.setting), 0), 0);
        section = lv_menu_section_create(sub_legal_info_page);
        for (uint32_t i = 0; i < 15; i++)
        {
            create_text(section, NULL,
                        "This is a long long long long long long long long long text, if it is long enough it may scroll.",
                        LV_MENU_ITEM_BUILDER_VARIANT_1);
        }

        lv_obj_t *sub_about_page = lv_menu_page_create(tdeckDisplayUI.setting, NULL);
        lv_obj_set_style_pad_hor(sub_about_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(tdeckDisplayUI.setting), 0), 0);
        lv_menu_separator_create(sub_about_page);
        section = lv_menu_section_create(sub_about_page);
        cont = create_text(section, NULL, "Software information", LV_MENU_ITEM_BUILDER_VARIANT_1);
        lv_menu_set_load_page_event(tdeckDisplayUI.setting, cont, sub_software_info_page);
        cont = create_text(section, NULL, "Legal information", LV_MENU_ITEM_BUILDER_VARIANT_1);
        lv_menu_set_load_page_event(tdeckDisplayUI.setting, cont, sub_legal_info_page);

        lv_obj_t *sub_menu_mode_page = lv_menu_page_create(tdeckDisplayUI.setting, NULL);
        lv_obj_set_style_pad_hor(sub_menu_mode_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(tdeckDisplayUI.setting), 0), 0);
        lv_menu_separator_create(sub_menu_mode_page);
        section = lv_menu_section_create(sub_menu_mode_page);
        cont = create_switch(section, LV_SYMBOL_AUDIO, "Sidebar enable", true);
        lv_obj_add_event_cb(lv_obj_get_child(cont, 2), switch_handler, LV_EVENT_VALUE_CHANGED, tdeckDisplayUI.setting);

        /*Create a root page*/
        tdeckDisplayUI.root_page = lv_menu_page_create(tdeckDisplayUI.setting, "Settings");
        lv_obj_set_style_pad_hor(tdeckDisplayUI.root_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(tdeckDisplayUI.setting), 0), 0);
        section = lv_menu_section_create(tdeckDisplayUI.root_page);

        cont = create_text(section, LV_SYMBOL_SETTINGS, "Display", LV_MENU_ITEM_BUILDER_VARIANT_1);
        lv_menu_set_load_page_event(tdeckDisplayUI.setting, cont, sub_display_page);
        cont = create_text(section, LV_SYMBOL_WIFI, "Wireless", LV_MENU_ITEM_BUILDER_VARIANT_1);
        lv_menu_set_load_page_event(tdeckDisplayUI.setting, cont, sub_communications_page);
        create_text(tdeckDisplayUI.root_page, NULL, "Others", LV_MENU_ITEM_BUILDER_VARIANT_1);
        section = lv_menu_section_create(tdeckDisplayUI.root_page);
        cont = create_text(section, NULL, "About", LV_MENU_ITEM_BUILDER_VARIANT_1);
        lv_menu_set_load_page_event(tdeckDisplayUI.setting, cont, sub_about_page);
        cont = create_text(section, LV_SYMBOL_SETTINGS, "Menu mode", LV_MENU_ITEM_BUILDER_VARIANT_1);
        lv_menu_set_load_page_event(tdeckDisplayUI.setting, cont, sub_menu_mode_page);

        lv_menu_set_sidebar_page(tdeckDisplayUI.setting, tdeckDisplayUI.root_page);
        lv_obj_align(tdeckDisplayUI.close_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
        lv_obj_set_size(tdeckDisplayUI.setting, TFT_WIDTH, TFT_HEIGHT);
        lv_obj_center(tdeckDisplayUI.setting);
        Serial.println("I am clicked!!!");
        lv_obj_add_event_cb(tdeckDisplayUI.close_btn, close_window_cb, LV_EVENT_CLICKED, NULL);
    }
}