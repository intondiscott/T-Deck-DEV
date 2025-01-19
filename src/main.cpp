#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "TouchDrvGT911.hpp"
#include <ArduinoJson.h>
#include "utilities.h"
#include <HTTPClient.h>
#include "credentials.c"
#include "../assets/images/mouse_pointer.h"
#include <TDECK_PINS.h>
#include <../assets/images/critical_low_bat.h>
#include <../assets/images/charging.h>
#include <../assets/images/full_battery.h>
#include <../assets/images/bat_two_bars.h>
#include <../assets/images/low_bat.h>
#include <../assets/images/wallpaper.h>
#include <keyboard.h>
#include <../assets/images/book.h>
#include <../assets/images/internet.h>
#include <../assets/images/no_wifi.h>
#include <../assets/images/setting.h>
#include <../assets/images/telephone.h>
#include <../assets/images/messages.h>
#include <../assets/images/calculator.h>
#include <../assets/images/calendar.h>
#include <../assets/images/bluetooth.h>
#include <../assets/images/weather.h>

#define TFT_WIDTH 320
#define TFT_HEIGHT 240
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

TaskHandle_t lvglTaskHandler, sensorTaskHandler, wifiTaskHandler;

int direction_count_up = 0;
int direction_count_down = 0;
int direction_count_left = 0;
int direction_count_right = 0;
int pointer_speed = 5;

int UP, DOWN, LEFT, RIGHT, CLICKED;
TouchDrvGT911 touch;
unsigned long lastTickMillis = 0;
// LilyGo  T-Deck  control backlight chip has 16 levels of adjustment range
// The adjustable range is 0~15, 0 is the minimum brightness, 15 is the maximum brightness
int trackball_val = 0;
struct Weather
{
  float temperature = 255.372; // kelvin temp
  int humidity = 0;
  float wind_speed = 0.0;
  char icon[5];
};

struct Settings
{
  uint8_t brightness = 10;
  bool radio_communications = false;
  bool wifi_communications = false;
  bool bluetooth_communications = false;
};
Settings *setting_values = new Settings;
Weather *weather_vals = new Weather;
char weather_buffer[7];

typedef enum
{
  LV_MENU_ITEM_BUILDER_VARIANT_1,
  LV_MENU_ITEM_BUILDER_VARIANT_2
} lv_menu_builder_variant_t;

struct
{
  TFT_eSPI tft;

  lv_obj_t
      *main_screen,
      *nav_screen,
      *battery_label,
      *datetime_label,
      *bat_bar,
      *bat_img,
      *wallpaper,
      *root_page,
      *low_bat_img,
      *button_text,
      *charging_img,
      *messages,
      *calculator,
      *calendar,
      *weather,
      *close_btn,
      *phone,
      *setting,
      *icons[20],
      *connection_status,
      *bluetooth_status,
      *weather_conditions,
      *temperature_label,
      *wind_speed_label,
      *humidity_label,
      *contact;
  char bat[6];
} static TdeckDisplayUI;

void setBrightness(uint8_t value)
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

// !!! LVGL !!!
// !!! LVGL !!!
// !!! LVGL !!!
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  lv_draw_sw_rgb565_swap(color_p, w * h);
  TdeckDisplayUI.tft.pushImage(area->x1, area->y1, w, h, (uint16_t *)color_p);
  // lv_draw_sw_rgb565_swap(color_p, w * h);

  lv_disp_flush_ready(disp);
}

int16_t x[TFT_WIDTH / 2], y[TFT_HEIGHT / 2];

/*Read the touchpad*/
void my_touch_read(lv_indev_t *indev_driver, lv_indev_data_t *data)
{

  data->state = LV_INDEV_STATE_REL;

  UP = digitalRead(TDECK_TRACKBALL_UP);
  DOWN = digitalRead(TDECK_TRACKBALL_DOWN);
  LEFT = digitalRead(TDECK_TRACKBALL_LEFT);
  RIGHT = digitalRead(TDECK_TRACKBALL_RIGHT);
  CLICKED = digitalRead(TDECK_TRACKBALL_CLICK);
  char key_press = keyboard_get_key();
  if (key_press)
    data->key = key_press;
  if (DOWN == 1)
  {
    direction_count_down = 1;
  }

  if (DOWN == 0 && direction_count_down == 1)
  {
    data->point.y += pointer_speed;
    direction_count_down = 0;
    Serial.printf("direction down: %dpx\n", pointer_speed);
    return;
  }
  if (UP == 1)
  {
    direction_count_up = 1;
  }
  if (UP == 0 && direction_count_up == 1)
  {
    data->point.y -= pointer_speed;
    direction_count_up = 0;
    Serial.printf("direction up: %dpx\n", pointer_speed);
    return;
  }
  if (LEFT == 1)
  {
    direction_count_left = 1;
  }
  if (LEFT == 0 && direction_count_left == 1)
  {
    data->point.x -= pointer_speed;
    direction_count_left = 0;
    Serial.printf("direction left: %dpx\n", pointer_speed);
    return;
  }
  if (RIGHT == 1)
  {
    direction_count_right = 1;
  }
  if (RIGHT == 0 && direction_count_right == 1)
  {
    data->point.x += pointer_speed;
    direction_count_right = 0;
    Serial.printf("direction right: %dpx\n", pointer_speed);
    return;
  }
  if (CLICKED == 0)
  {
    data->state = LV_INDEV_STATE_PR;
  }
  if (touch.isPressed())
  {
    Serial.println("Pressed!");
    uint8_t touched = touch.getPoint(x, y, touch.getSupportTouchPoint());
    if (touched > 0)
    {
      data->state = LV_INDEV_STATE_PR;
      data->point.x = x[0];
      data->point.y = y[0];

      Serial.print(millis());
      Serial.print("ms ");
      for (int i = 0; i < touched; ++i)
      {
        Serial.print("X[");
        Serial.print(i);
        Serial.print("]:");
        Serial.print(x[i]);
        Serial.print(" ");
        Serial.print(" Y[");
        Serial.print(i);
        Serial.print("]:");
        Serial.print(y[i]);
        Serial.print(" ");
      }
      Serial.println();
    }
  }
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
      lv_menu_set_sidebar_page(menu, TdeckDisplayUI.root_page);
      lv_obj_send_event(lv_obj_get_child(lv_obj_get_child(lv_menu_get_cur_sidebar_page(menu), 0), 0), LV_EVENT_CLICKED,
                        NULL);
    }
    else
    {
      lv_menu_set_sidebar_page(menu, NULL);
      lv_menu_clear_history(menu); /* Clear history because we will be showing the root page later */
      lv_menu_set_page(menu, TdeckDisplayUI.root_page);
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

static void close_window_cb(lv_event_t *e)
{

  // lv_obj_clean(lv_obj_get_parent((lv_obj_t *)lv_event_get_target(e)));
  lv_obj_delete(lv_obj_get_parent((lv_obj_t *)lv_event_get_target(e)));
}
static void create_contact_page(lv_event_t *e)
{
  if (!lv_obj_is_valid(TdeckDisplayUI.contact))
  {
    TdeckDisplayUI.contact = lv_obj_create(lv_screen_active());

    lv_obj_t *label = lv_label_create(TdeckDisplayUI.contact);
    TdeckDisplayUI.close_btn = lv_button_create(TdeckDisplayUI.contact);
    lv_obj_set_style_bg_color(TdeckDisplayUI.close_btn, lv_color_hex(0xfc0303), LV_PART_MAIN);
    lv_obj_t *label_close = lv_label_create(TdeckDisplayUI.close_btn);
    lv_label_set_text(label_close, LV_SYMBOL_CLOSE);
    lv_label_set_text(label, "Contact Page");
    lv_obj_align(TdeckDisplayUI.close_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_size(TdeckDisplayUI.contact, 200, 200);
    lv_obj_center(TdeckDisplayUI.contact);
    Serial.println("I am clicked!!!");
    lv_obj_add_event_cb(TdeckDisplayUI.close_btn, close_window_cb, LV_EVENT_CLICKED, NULL);
  }
}
static void create_setting_page(lv_event_t *e)
{
  if (!lv_obj_is_valid(TdeckDisplayUI.setting))
  {
    TdeckDisplayUI.setting = lv_menu_create(lv_screen_active());
    // lv_obj_t *label = lv_label_create(TdeckDisplayUI.setting);
    TdeckDisplayUI.close_btn = lv_button_create(TdeckDisplayUI.setting);
    lv_obj_set_style_bg_color(TdeckDisplayUI.close_btn, lv_color_hex(0xfc0303), LV_PART_MAIN);
    lv_obj_t *label_close = lv_label_create(TdeckDisplayUI.close_btn);
    lv_label_set_text(label_close, LV_SYMBOL_CLOSE);
    // lv_label_set_text(label, "Settings Page");

    lv_color_t bg_color = lv_obj_get_style_bg_color(TdeckDisplayUI.setting, 0);
    if (lv_color_brightness(bg_color) > 127)
    {
      lv_obj_set_style_bg_color(TdeckDisplayUI.setting, lv_color_darken(lv_obj_get_style_bg_color(TdeckDisplayUI.setting, 0), 10), 0);
    }
    else
    {
      lv_obj_set_style_bg_color(TdeckDisplayUI.setting, lv_color_darken(lv_obj_get_style_bg_color(TdeckDisplayUI.setting, 0), 50), 0);
    }
    lv_menu_set_mode_root_back_button(TdeckDisplayUI.setting, LV_MENU_ROOT_BACK_BUTTON_ENABLED);
    lv_obj_add_event_cb(TdeckDisplayUI.setting, back_event_handler, LV_EVENT_CLICKED, TdeckDisplayUI.setting);
    lv_obj_set_size(TdeckDisplayUI.setting, lv_display_get_horizontal_resolution(NULL), lv_display_get_vertical_resolution(NULL));
    lv_obj_center(TdeckDisplayUI.setting);

    lv_obj_t *cont;
    lv_obj_t *section;

    lv_obj_t *sub_display_page = lv_menu_page_create(TdeckDisplayUI.setting, NULL);
    lv_obj_set_style_pad_hor(sub_display_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(TdeckDisplayUI.setting), 0), 0);
    lv_menu_separator_create(sub_display_page);
    section = lv_menu_section_create(sub_display_page);
    create_slider(section, LV_SYMBOL_SETTINGS, "Brightness", 3, 16, setting_values->brightness);

    lv_obj_t *sub_communications_page = lv_menu_page_create(TdeckDisplayUI.setting, NULL);
    lv_obj_set_style_pad_hor(sub_communications_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(TdeckDisplayUI.setting), 0), 0);
    lv_menu_separator_create(sub_communications_page);
    section = lv_menu_section_create(sub_communications_page);
    create_switch(section, LV_SYMBOL_WIFI, "WiFi", setting_values->wifi_communications);
    create_switch(section, LV_SYMBOL_BLUETOOTH, "Bluetooth", setting_values->bluetooth_communications);
    create_switch(section, LV_SYMBOL_GPS, "Radio", setting_values->radio_communications);

    lv_obj_t *sub_software_info_page = lv_menu_page_create(TdeckDisplayUI.setting, NULL);
    lv_obj_set_style_pad_hor(sub_software_info_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(TdeckDisplayUI.setting), 0), 0);
    section = lv_menu_section_create(sub_software_info_page);
    create_text(section, NULL, "Version 1.0", LV_MENU_ITEM_BUILDER_VARIANT_1);

    lv_obj_t *sub_legal_info_page = lv_menu_page_create(TdeckDisplayUI.setting, NULL);
    lv_obj_set_style_pad_hor(sub_legal_info_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(TdeckDisplayUI.setting), 0), 0);
    section = lv_menu_section_create(sub_legal_info_page);
    for (uint32_t i = 0; i < 15; i++)
    {
      create_text(section, NULL,
                  "This is a long long long long long long long long long text, if it is long enough it may scroll.",
                  LV_MENU_ITEM_BUILDER_VARIANT_1);
    }

    lv_obj_t *sub_about_page = lv_menu_page_create(TdeckDisplayUI.setting, NULL);
    lv_obj_set_style_pad_hor(sub_about_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(TdeckDisplayUI.setting), 0), 0);
    lv_menu_separator_create(sub_about_page);
    section = lv_menu_section_create(sub_about_page);
    cont = create_text(section, NULL, "Software information", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_menu_set_load_page_event(TdeckDisplayUI.setting, cont, sub_software_info_page);
    cont = create_text(section, NULL, "Legal information", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_menu_set_load_page_event(TdeckDisplayUI.setting, cont, sub_legal_info_page);

    lv_obj_t *sub_menu_mode_page = lv_menu_page_create(TdeckDisplayUI.setting, NULL);
    lv_obj_set_style_pad_hor(sub_menu_mode_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(TdeckDisplayUI.setting), 0), 0);
    lv_menu_separator_create(sub_menu_mode_page);
    section = lv_menu_section_create(sub_menu_mode_page);
    cont = create_switch(section, LV_SYMBOL_AUDIO, "Sidebar enable", true);
    lv_obj_add_event_cb(lv_obj_get_child(cont, 2), switch_handler, LV_EVENT_VALUE_CHANGED, TdeckDisplayUI.setting);

    /*Create a root page*/
    TdeckDisplayUI.root_page = lv_menu_page_create(TdeckDisplayUI.setting, "Settings");
    lv_obj_set_style_pad_hor(TdeckDisplayUI.root_page, lv_obj_get_style_pad_left(lv_menu_get_main_header(TdeckDisplayUI.setting), 0), 0);
    section = lv_menu_section_create(TdeckDisplayUI.root_page);

    cont = create_text(section, LV_SYMBOL_SETTINGS, "Display", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_menu_set_load_page_event(TdeckDisplayUI.setting, cont, sub_display_page);
    cont = create_text(section, LV_SYMBOL_WIFI, "Wireless", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_menu_set_load_page_event(TdeckDisplayUI.setting, cont, sub_communications_page);
    create_text(TdeckDisplayUI.root_page, NULL, "Others", LV_MENU_ITEM_BUILDER_VARIANT_1);
    section = lv_menu_section_create(TdeckDisplayUI.root_page);
    cont = create_text(section, NULL, "About", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_menu_set_load_page_event(TdeckDisplayUI.setting, cont, sub_about_page);
    cont = create_text(section, LV_SYMBOL_SETTINGS, "Menu mode", LV_MENU_ITEM_BUILDER_VARIANT_1);
    lv_menu_set_load_page_event(TdeckDisplayUI.setting, cont, sub_menu_mode_page);

    lv_menu_set_sidebar_page(TdeckDisplayUI.setting, TdeckDisplayUI.root_page);
    lv_obj_align(TdeckDisplayUI.close_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_size(TdeckDisplayUI.setting, TFT_WIDTH, TFT_HEIGHT);
    lv_obj_center(TdeckDisplayUI.setting);
    Serial.println("I am clicked!!!");
    lv_obj_add_event_cb(TdeckDisplayUI.close_btn, close_window_cb, LV_EVENT_CLICKED, NULL);
  }
}

static void create_phone_page(lv_event_t *e)
{
  if (!lv_obj_is_valid(TdeckDisplayUI.phone))
  {
    TdeckDisplayUI.phone = lv_obj_create(lv_screen_active());
    lv_obj_t *label = lv_label_create(TdeckDisplayUI.phone);
    TdeckDisplayUI.close_btn = lv_button_create(TdeckDisplayUI.phone);
    lv_obj_set_style_bg_color(TdeckDisplayUI.close_btn, lv_color_hex(0xfc0303), LV_PART_MAIN);
    lv_obj_t *label_close = lv_label_create(TdeckDisplayUI.close_btn);
    lv_label_set_text(label_close, LV_SYMBOL_CLOSE);
    lv_label_set_text(label, "Phone Page");
    lv_obj_align(TdeckDisplayUI.close_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_size(TdeckDisplayUI.phone, 200, 200);
    lv_obj_center(TdeckDisplayUI.phone);
    Serial.println("I am clicked!!!");
    lv_obj_add_event_cb(TdeckDisplayUI.close_btn, close_window_cb, LV_EVENT_CLICKED, NULL);
  }
}

static void create_messages_page(lv_event_t *e)
{
  if (!lv_obj_is_valid(TdeckDisplayUI.messages))
  {
    TdeckDisplayUI.messages = lv_obj_create(lv_screen_active());
    lv_obj_t *label = lv_label_create(TdeckDisplayUI.messages);
    TdeckDisplayUI.close_btn = lv_button_create(TdeckDisplayUI.messages);
    lv_obj_set_style_bg_color(TdeckDisplayUI.close_btn, lv_color_hex(0xfc0303), LV_PART_MAIN);
    lv_obj_t *label_close = lv_label_create(TdeckDisplayUI.close_btn);
    lv_label_set_text(label_close, LV_SYMBOL_CLOSE);
    lv_label_set_text(label, "Messages Page");
    lv_obj_align(TdeckDisplayUI.close_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_size(TdeckDisplayUI.messages, 200, 200);
    lv_obj_center(TdeckDisplayUI.messages);
    Serial.println("I am clicked!!!");
    lv_obj_add_event_cb(TdeckDisplayUI.close_btn, close_window_cb, LV_EVENT_CLICKED, NULL);
  }
}

static void textarea_event_handler(lv_event_t *e)
{
  lv_obj_t *ta = (lv_obj_t *)lv_event_get_target(e);
  LV_UNUSED(ta);
  Serial.println(keyboard_get_key());
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
    lv_textarea_add_text(ta, txt);
}

static void create_calculator_page(lv_event_t *e)
{
  if (!lv_obj_is_valid(TdeckDisplayUI.calculator))
  {
    TdeckDisplayUI.calculator = lv_obj_create(lv_screen_active());
    lv_obj_t *ta = lv_textarea_create(TdeckDisplayUI.calculator);

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

    lv_obj_t *btnm = lv_buttonmatrix_create(TdeckDisplayUI.calculator);
    lv_obj_set_size(btnm, 200, 150);
    lv_obj_align(btnm, LV_ALIGN_BOTTOM_MID, -20, 10);
    lv_obj_add_event_cb(btnm, btnm_event_handler, LV_EVENT_VALUE_CHANGED, ta);
    lv_obj_remove_flag(btnm, LV_OBJ_FLAG_CLICK_FOCUSABLE); /*To keep the text area focused on button clicks*/
    lv_buttonmatrix_set_map(btnm, btnm_map);

    TdeckDisplayUI.close_btn = lv_button_create(TdeckDisplayUI.calculator);
    lv_obj_set_style_bg_color(TdeckDisplayUI.close_btn, lv_color_hex(0xfc0303), LV_PART_MAIN);
    lv_obj_t *label_close = lv_label_create(TdeckDisplayUI.close_btn);
    lv_label_set_text(label_close, LV_SYMBOL_CLOSE);

    lv_obj_align(TdeckDisplayUI.close_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_size(TdeckDisplayUI.calculator, 300, 220);
    lv_obj_center(TdeckDisplayUI.calculator);
    Serial.println("I am clicked!!!");
    lv_obj_add_event_cb(TdeckDisplayUI.close_btn, close_window_cb, LV_EVENT_CLICKED, NULL);
  }
}

static void create_calendar_page(lv_event_t *e)
{
  if (!lv_obj_is_valid(TdeckDisplayUI.calendar))
  {
    TdeckDisplayUI.calendar = lv_obj_create(lv_screen_active());
    lv_obj_t *calendar = lv_calendar_create(TdeckDisplayUI.calendar);
    TdeckDisplayUI.close_btn = lv_button_create(TdeckDisplayUI.calendar);
    lv_obj_set_style_bg_color(TdeckDisplayUI.close_btn, lv_color_hex(0xfc0303), LV_PART_MAIN);
    lv_obj_t *label_close = lv_label_create(TdeckDisplayUI.close_btn);
    lv_label_set_text(label_close, LV_SYMBOL_CLOSE);
    lv_calendar_set_today_date(calendar, 2025, 01, 15);

    lv_calendar_set_showed_date(calendar, 2025, 01);
    lv_obj_align(TdeckDisplayUI.close_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_size(TdeckDisplayUI.calendar, TFT_WIDTH, TFT_HEIGHT);
    lv_obj_set_size(calendar, 220, TFT_HEIGHT-30);
    lv_obj_center(TdeckDisplayUI.calendar);
    lv_obj_align(calendar,LV_ALIGN_CENTER,-20,0);
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
    lv_obj_add_event_cb(TdeckDisplayUI.close_btn, close_window_cb, LV_EVENT_CLICKED, NULL);
  }
}

static void create_weather_page(lv_event_t *e)
{
  if (!lv_obj_is_valid(TdeckDisplayUI.weather))
  {
    TdeckDisplayUI.weather = lv_obj_create(lv_screen_active());
    lv_obj_t *label = lv_label_create(TdeckDisplayUI.weather);
    TdeckDisplayUI.close_btn = lv_button_create(TdeckDisplayUI.weather);
    lv_obj_set_style_bg_color(TdeckDisplayUI.close_btn, lv_color_hex(0xfc0303), LV_PART_MAIN);
    lv_obj_t *label_close = lv_label_create(TdeckDisplayUI.close_btn);
    lv_label_set_text(label_close, LV_SYMBOL_CLOSE);
    lv_label_set_text(label, "Weather Page");
    lv_obj_align(TdeckDisplayUI.close_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_size(TdeckDisplayUI.weather, 200, 200);
    lv_obj_center(TdeckDisplayUI.weather);
    Serial.println("I am clicked!!!");
    lv_obj_add_event_cb(TdeckDisplayUI.close_btn, close_window_cb, LV_EVENT_CLICKED, NULL);
  }
}

void screen_update()
{
  // make time struct

  uint32_t battery_percentage = (analogRead(BOARD_BAT_ADC) * 4.6) * 8 / 1024;

  snprintf(TdeckDisplayUI.bat, sizeof(TdeckDisplayUI.bat), "%d", battery_percentage);
  if (WiFi.status() == WL_CONNECTED)
  {
    LV_IMAGE_DECLARE(internet);
    lv_image_set_src(TdeckDisplayUI.connection_status, &internet);
  }
  else
  {
    LV_IMAGE_DECLARE(no_wifi);
    lv_image_set_src(TdeckDisplayUI.connection_status, &no_wifi);
  }
  if (setting_values->bluetooth_communications)
  {
    LV_IMAGE_DECLARE(bluetooth);
    lv_image_set_src(TdeckDisplayUI.bluetooth_status, &bluetooth);
  }
  else
  {
    lv_image_set_src(TdeckDisplayUI.bluetooth_status, NULL);
  }
  // lv_label_set_text(TdeckDisplayUI.connection_status, WiFi.status() == WL_CONNECTED ? "Connected..." : "Not Connected...");
  snprintf(weather_buffer, sizeof(weather_buffer), "%3.2f", (weather_vals->temperature - 273.15) * 9 / 5 + 32);
  lv_label_set_text_fmt(TdeckDisplayUI.temperature_label, "%s°F", weather_buffer);
  //  lv_label_set_text_fmt(TdeckDisplayUI.humidity_label, "Hum: %d%%", weather->humidity);
  //  snprintf(weather_buffer, sizeof(weather_buffer), "%3.2f", weather->wind_speed);
  //  lv_label_set_text_fmt(TdeckDisplayUI.wind_speed_label, "Wind Speed: %s MPH", weather_buffer);
  lv_label_set_text_fmt(TdeckDisplayUI.battery_label, "%s%%", TdeckDisplayUI.bat);
  // lv_label_set_text(TdeckDisplayUI.bat_bar, weather->icon);
  //  M5.Rtc.GetTime(&TimeStruct);
  /*lv_label_set_text_fmt(
      TdeckDisplayUI.datetime_label, "%02d : %02d : %02d %s",
      TimeStruct.Hours > 12 ? TimeStruct.Hours - 12 : TimeStruct.Hours,
      TimeStruct.Minutes,
      TimeStruct.Seconds,
      TimeStruct.Hours >= 12 ? "PM" : "AM");*/
  if (analogRead(BOARD_BAT_ADC) > 2700)
  {
    LV_IMAGE_DECLARE(charging);
    lv_image_set_src(TdeckDisplayUI.bat_img, &charging);
  }
  else
  {
    switch (battery_percentage)
    {
    case 80 ... 100:
      LV_IMAGE_DECLARE(full_battery);
      lv_image_set_src(TdeckDisplayUI.bat_img, &full_battery);
      break;
    case 40 ... 79:
      LV_IMAGE_DECLARE(bat_two_bars);
      lv_image_set_src(TdeckDisplayUI.bat_img, &bat_two_bars);
      break;
    case 15 ... 39:
      LV_IMAGE_DECLARE(low_bat);
      lv_image_set_src(TdeckDisplayUI.bat_img, &low_bat);
      break;
    case 0 ... 14:
      LV_IMAGE_DECLARE(critical_low_bat);
      lv_image_set_src(TdeckDisplayUI.bat_img, &critical_low_bat);
      break;
    default:
      break;
    }
  }
  // if (lv_obj_is_valid(TdeckDisplayUI.contact))
  // lv_obj_add_event_cb(TdeckDisplayUI.close_btn, close_window_cb, LV_EVENT_CLICKED, NULL);
}
void sensorsTask(void *pvParams)
{
  // lv_label_set_text(TdeckDisplayUI.wifi_list,"wifi");
  while (1)
  {

    while (1)
    {

      if (WiFi.status() == WL_CONNECTED)
      {
        HTTPClient http2, http3;
        float x, y, z;
        String update = "\"device_status\":\"ONLINE\",";
        String battery = "\"pin1\":\"" + (String)TdeckDisplayUI.bat + "\",";
        // String charging = "\"pin2\":\"" + (String)M5.Axp.GetAPSVoltage() + "\",";
        // String chip_temp = "\"pin3\":\"" + (String)M5.Axp.GetTempInAXP192() + "\"";

        http2.begin("http://192.168.0.114:8080/api/v1/devices/2");
        http3.begin("http://192.168.0.223:8080/api/v1/devices/2");
        http2.addHeader("Content-type", "application/json");
        http3.addHeader("Content-type", "application/json");
        http2.PUT(
            "{" +
            update +
            battery +
            // charging +
            // chip_temp +
            "}");
        http3.PUT(
            "{" +
            update +
            battery +
            // charging +
            // chip_temp +
            "}");

        http2.end();
        http3.end();
      }
      else
      {
        WiFi.mode(WIFI_STA);
        if (setting_values->wifi_communications)
          WiFi.begin(MY_SECRET_SSID, MY_SECRET_PASSWORD);
        Serial.print("Connecting to WiFi ..");
        while (WiFi.status() != WL_CONNECTED)
        {
          Serial.print('.');
          delay(1000);
        }
        Serial.println(WiFi.localIP());
      }
      vTaskDelay(2);
    }
  }
}
void drawUI()
{
  LV_IMAGE_DECLARE(wallpaper);
  LV_IMAGE_DECLARE(book);
  LV_IMAGE_DECLARE(setting);
  LV_IMAGE_DECLARE(telephone);
  LV_IMAGE_DECLARE(messages);
  LV_IMAGE_DECLARE(calculator);
  LV_IMAGE_DECLARE(calendar);
  LV_IMAGE_DECLARE(weather);
  static lv_style_t button_click;
  lv_style_init(&button_click);
  lv_style_set_image_recolor_opa(&button_click, LV_OPA_30);
  TdeckDisplayUI.main_screen = lv_image_create(lv_screen_active());

  TdeckDisplayUI.nav_screen = lv_obj_create(TdeckDisplayUI.main_screen);
  TdeckDisplayUI.battery_label = lv_label_create(TdeckDisplayUI.nav_screen);
  // TdeckDisplayUI.datetime_label = lv_label_create(TdeckDisplayUI.nav_screen);
  TdeckDisplayUI.connection_status = lv_image_create(TdeckDisplayUI.nav_screen);
  TdeckDisplayUI.bluetooth_status = lv_image_create(TdeckDisplayUI.nav_screen);
  TdeckDisplayUI.temperature_label = lv_label_create(TdeckDisplayUI.nav_screen);
  // TdeckDisplayUI.wind_speed_label = lv_label_create(TdeckDisplayUI.main_screen);
  // TdeckDisplayUI.humidity_label = lv_label_create(TdeckDisplayUI.main_screen);
  TdeckDisplayUI.bat_img = lv_image_create(TdeckDisplayUI.nav_screen);
  // TdeckDisplayUI.weather_conditions = lv_image_create(TdeckDisplayUI.main_screen);
  // TdeckDisplayUI.bat_bar = lv_label_create(TdeckDisplayUI.main_screen);
  TdeckDisplayUI.icons[0] = lv_imagebutton_create(TdeckDisplayUI.main_screen);
  TdeckDisplayUI.icons[1] = lv_imagebutton_create(TdeckDisplayUI.main_screen);
  TdeckDisplayUI.icons[2] = lv_imagebutton_create(TdeckDisplayUI.main_screen);
  TdeckDisplayUI.icons[3] = lv_imagebutton_create(TdeckDisplayUI.main_screen);
  TdeckDisplayUI.icons[4] = lv_imagebutton_create(TdeckDisplayUI.main_screen);
  TdeckDisplayUI.icons[5] = lv_imagebutton_create(TdeckDisplayUI.main_screen);
  TdeckDisplayUI.icons[6] = lv_imagebutton_create(TdeckDisplayUI.main_screen);

  lv_image_set_src(TdeckDisplayUI.main_screen, &wallpaper);
  lv_imagebutton_set_src(TdeckDisplayUI.icons[0], LV_IMAGEBUTTON_STATE_RELEASED, &book, &book, &book);
  lv_imagebutton_set_src(TdeckDisplayUI.icons[1], LV_IMAGEBUTTON_STATE_RELEASED, &setting, &setting, &setting);
  lv_imagebutton_set_src(TdeckDisplayUI.icons[2], LV_IMAGEBUTTON_STATE_RELEASED, &telephone, &telephone, &telephone);
  lv_imagebutton_set_src(TdeckDisplayUI.icons[3], LV_IMAGEBUTTON_STATE_RELEASED, &messages, &messages, &messages);
  lv_imagebutton_set_src(TdeckDisplayUI.icons[4], LV_IMAGEBUTTON_STATE_RELEASED, &calculator, &calculator, &calculator);
  lv_imagebutton_set_src(TdeckDisplayUI.icons[5], LV_IMAGEBUTTON_STATE_RELEASED, &calendar, &calendar, &calendar);
  lv_imagebutton_set_src(TdeckDisplayUI.icons[6], LV_IMAGEBUTTON_STATE_RELEASED, &weather, &weather, &weather);
  lv_obj_add_style(TdeckDisplayUI.icons[0], &button_click, LV_STATE_PRESSED);
  lv_obj_add_style(TdeckDisplayUI.icons[1], &button_click, LV_STATE_PRESSED);
  lv_obj_add_style(TdeckDisplayUI.icons[2], &button_click, LV_STATE_PRESSED);
  lv_obj_add_style(TdeckDisplayUI.icons[3], &button_click, LV_STATE_PRESSED);
  lv_obj_add_style(TdeckDisplayUI.icons[4], &button_click, LV_STATE_PRESSED);
  lv_obj_add_style(TdeckDisplayUI.icons[5], &button_click, LV_STATE_PRESSED);
  lv_obj_add_style(TdeckDisplayUI.icons[6], &button_click, LV_STATE_PRESSED);
  lv_obj_align(TdeckDisplayUI.bat_img, LV_ALIGN_RIGHT_MID, -10, 0);
  lv_obj_align(TdeckDisplayUI.temperature_label, LV_ALIGN_CENTER, 0, 0);
  lv_obj_align(TdeckDisplayUI.connection_status, LV_ALIGN_LEFT_MID, 10, 0);
  lv_obj_align(TdeckDisplayUI.bluetooth_status, LV_ALIGN_LEFT_MID, 30, 0);

  lv_obj_set_size(TdeckDisplayUI.main_screen, TFT_WIDTH, TFT_HEIGHT);

  lv_obj_center(TdeckDisplayUI.main_screen);

  lv_obj_set_style_margin_top(TdeckDisplayUI.main_screen, 40, LV_PART_MAIN);
  lv_obj_set_flex_flow(TdeckDisplayUI.main_screen, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_size(TdeckDisplayUI.nav_screen, TFT_WIDTH, 30);
  lv_obj_align(TdeckDisplayUI.battery_label, LV_ALIGN_RIGHT_MID, -60, 0);
  // lv_obj_align(TdeckDisplayUI.datetime_label, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_set_style_pad_all(TdeckDisplayUI.nav_screen, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(TdeckDisplayUI.main_screen, 0, LV_PART_MAIN);
  lv_obj_set_style_margin_all(TdeckDisplayUI.nav_screen, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(TdeckDisplayUI.main_screen, lv_color_hex(0x98a3a2), LV_PART_MAIN);
  lv_obj_set_style_bg_color(TdeckDisplayUI.nav_screen, lv_color_hex(0x948d8d), LV_PART_MAIN);

  lv_obj_set_style_text_color(TdeckDisplayUI.nav_screen, lv_color_hex(0x000000), LV_PART_MAIN);
  // lv_obj_align(TdeckDisplayUI.icons[0], LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_style_margin_all(TdeckDisplayUI.icons[0], 10, LV_PART_MAIN);
  lv_obj_set_style_margin_all(TdeckDisplayUI.icons[1], 10, LV_PART_MAIN);
  lv_obj_set_style_margin_all(TdeckDisplayUI.icons[2], 10, LV_PART_MAIN);
  lv_obj_set_style_margin_all(TdeckDisplayUI.icons[3], 10, LV_PART_MAIN);
  lv_obj_set_style_margin_all(TdeckDisplayUI.icons[4], 10, LV_PART_MAIN);
  lv_obj_set_style_margin_all(TdeckDisplayUI.icons[5], 10, LV_PART_MAIN);
  lv_obj_set_style_margin_all(TdeckDisplayUI.icons[6], 10, LV_PART_MAIN);
  lv_obj_set_size(TdeckDisplayUI.icons[0], 60, 60);
  lv_obj_set_size(TdeckDisplayUI.icons[1], 60, 60);
  lv_obj_set_size(TdeckDisplayUI.icons[2], 60, 60);
  lv_obj_set_size(TdeckDisplayUI.icons[3], 60, 60);
  lv_obj_set_size(TdeckDisplayUI.icons[4], 60, 60);
  lv_obj_set_size(TdeckDisplayUI.icons[5], 60, 60);
  lv_obj_set_size(TdeckDisplayUI.icons[6], 60, 60);

  lv_obj_add_event_cb(TdeckDisplayUI.icons[0], create_contact_page, LV_EVENT_CLICKED, TdeckDisplayUI.main_screen);
  lv_obj_add_event_cb(TdeckDisplayUI.icons[1], create_setting_page, LV_EVENT_CLICKED, TdeckDisplayUI.main_screen);
  lv_obj_add_event_cb(TdeckDisplayUI.icons[2], create_phone_page, LV_EVENT_CLICKED, TdeckDisplayUI.main_screen);
  lv_obj_add_event_cb(TdeckDisplayUI.icons[3], create_messages_page, LV_EVENT_CLICKED, TdeckDisplayUI.main_screen);
  lv_obj_add_event_cb(TdeckDisplayUI.icons[4], create_calculator_page, LV_EVENT_CLICKED, TdeckDisplayUI.main_screen);
  lv_obj_add_event_cb(TdeckDisplayUI.icons[5], create_calendar_page, LV_EVENT_CLICKED, TdeckDisplayUI.main_screen);
  lv_obj_add_event_cb(TdeckDisplayUI.icons[6], create_weather_page, LV_EVENT_CLICKED, TdeckDisplayUI.main_screen);
}
void wifiTask(void *pvParams)
{

  while (1)
  {

    if (WiFi.status() == WL_CONNECTED)
    {
      HTTPClient http1;

      String server_path1 = "https://api.openweathermap.org/data/2.5/weather?lat=41.3165&lon=-73.0932&appid=";

      http1.begin(server_path1 + MY_SECRET_API_KEY);

      int httpCode = http1.GET();

      if (httpCode > 0)
      {
        String payload = http1.getString();

        JsonDocument doc;
        deserializeJson(doc, payload);
        weather_vals->temperature = doc["main"]["temp"];
        weather_vals->humidity = doc["main"]["humidity"];
        weather_vals->wind_speed = doc["wind"]["speed"];

        Serial.println(payload);
      }
      else
      {
        Serial.println(httpCode);
        WiFi.mode(WIFI_STA);
        if (setting_values->wifi_communications)
          WiFi.begin(MY_SECRET_SSID, MY_SECRET_PASSWORD);
        Serial.print("Connecting to WiFi ..");
        while (WiFi.status() != WL_CONNECTED)
        {
          Serial.print('.');
          delay(1000);
        }
        Serial.println(WiFi.localIP());
      }
      http1.end();
    }
    vTaskDelay(10000);
  }
}

void setupLVGL(void *pvParams)
{

  lv_init();

  static lv_color_t buf[TFT_WIDTH * 10];
  lv_display_t *display = lv_display_create(TFT_WIDTH, TFT_HEIGHT);
/*Declare a buffer for 1/10 screen size*/
#define BYTE_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565)) /*will be 2 for RGB565 */
  static uint8_t buf1[TFT_WIDTH * TFT_HEIGHT / 10 * BYTE_PER_PIXEL];

  lv_display_set_buffers(display, buf1, NULL, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL); /*Initialize the display buffer.*/
  lv_display_set_flush_cb(display, my_disp_flush);
  // static lv_style_t transparent_style;
  // lv_style_init(&transparent_style);
  // lv_style_set_bg_opa(&transparent_style, LV_OPA_TRANSP);
  // lv_style_set_bg_color(&transparent_style, lv_color_black());
  lv_indev_t *indev = lv_indev_create();           /*Create an input device*/
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER); /*Touch pad is a pointer-like device*/
  lv_indev_set_read_cb(indev, my_touch_read);
  LV_IMAGE_DECLARE(mouse_pointer);
  lv_obj_t *mouse_cursor = lv_img_create(lv_screen_active());

  lv_image_set_src(mouse_cursor, &mouse_pointer);
  lv_indev_set_cursor(indev, mouse_cursor);

  drawUI();
  while (1)
  {
    screen_update();
    u_int8_t tickPeriod = millis() - lastTickMillis;
    lv_tick_inc(tickPeriod);
    lastTickMillis = millis();
    lv_timer_handler();
    vTaskDelay(1);
  }
}

void setup()
{
  Serial.begin(115200);

  Serial.println("lvgl example");

  //! The board peripheral power control pin needs to be set to HIGH when using the peripheral
  pinMode(BOARD_POWERON, OUTPUT);
  digitalWrite(BOARD_POWERON, HIGH);

  //! Set CS on all SPI buses to high level during initialization
  pinMode(BOARD_SDCARD_CS, OUTPUT);
  pinMode(RADIO_CS_PIN, OUTPUT);
  pinMode(BOARD_TFT_CS, OUTPUT);

  digitalWrite(BOARD_SDCARD_CS, HIGH);
  digitalWrite(RADIO_CS_PIN, HIGH);
  digitalWrite(BOARD_TFT_CS, HIGH);

  pinMode(BOARD_SPI_MISO, INPUT_PULLUP);
  SPI.begin(BOARD_SPI_SCK, BOARD_SPI_MISO, BOARD_SPI_MOSI); // SD

  pinMode(BOARD_BOOT_PIN, INPUT_PULLUP);
  pinMode(BOARD_TBOX_G02, INPUT_PULLUP);
  pinMode(BOARD_TBOX_G01, INPUT_PULLUP);
  pinMode(BOARD_TBOX_G04, INPUT_PULLUP);
  pinMode(BOARD_TBOX_G03, INPUT_PULLUP);

  // trackball setup
  pinMode(TDECK_TRACKBALL_UP, INPUT_PULLUP);
  pinMode(TDECK_TRACKBALL_DOWN, INPUT_PULLUP);
  pinMode(TDECK_TRACKBALL_LEFT, INPUT_PULLUP);
  pinMode(TDECK_TRACKBALL_RIGHT, INPUT_PULLUP);
  pinMode(TDECK_TRACKBALL_CLICK, INPUT_PULLUP);
  attachInterrupt(TDECK_TRACKBALL_DOWN, nullptr, FALLING);

  pinMode(BOARD_BAT_ADC, INPUT);
  adcAttachPin(BOARD_BAT_ADC);

  Serial.print("Init display id:");
  Serial.println(USER_SETUP_ID);

  TdeckDisplayUI.tft.begin();
  TdeckDisplayUI.tft.setRotation(1);
  TdeckDisplayUI.tft.fillScreen(TFT_BLACK);

  // Set touch int input
  pinMode(BOARD_TOUCH_INT, INPUT);
  delay(20);

  Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);

  touch.setPins(-1, BOARD_TOUCH_INT);
  if (!touch.begin(Wire, GT911_SLAVE_ADDRESS_L))
  {
    while (1)
    {
      Serial.println("Failed to find GT911 - check your wiring!");
      delay(1000);
    }
  }

  Serial.println("Init GT911 Sensor success!");

  // Set touch max xy
  touch.setMaxCoordinates(TFT_WIDTH, TFT_HEIGHT);

  // Set swap xy
  touch.setSwapXY(true);

  // Set mirror xy
  touch.setMirrorXY(false, true);
  pinMode(BOARD_BL_PIN, OUTPUT);
  setBrightness(setting_values->brightness);

  xTaskCreatePinnedToCore(setupLVGL, "setupLVGL", 1024 * 10, NULL, 3, &lvglTaskHandler, 0);
  xTaskCreatePinnedToCore(wifiTask, "wifiTask", 1024 * 6, NULL, 2, &wifiTaskHandler, 1);
  xTaskCreatePinnedToCore(sensorsTask, "sensorsTask", 1024 * 6, NULL, 1, &sensorTaskHandler, 1);
}

void loop()
{
}