#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <WiFi.h>
#include "TouchDrvGT911.hpp"
#include <ArduinoJson.h>
#include "utilities.h"
#include <HTTPClient.h>
#include "credentials.c"
#include "mouse_pointer.h"
#include <TDECK_PINS.h>
#include <critical_low_bat.h>
#include <charging.h>
#include <full_battery.h>
#include <bat_two_bars.h>
#include <low_bat.h>
#include <wallpaper.h>
#include <keyboard.h>
#include <book.h>
#include <internet.h>
#include <no_wifi.h>
#include <setting.h>
#include <telephone.h>
#include <messages.h>
#include <calculator.h>

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

Weather *weather = new Weather;
char weather_buffer[7];
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
      *low_bat_img,
      *button_text,
      *charging_img,
      *messages,
      *calculator,
      *close_btn,
      *phone,
      *setting,
      *icons[10],
      *connection_status,
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

int16_t x[320 / 2], y[240 / 2];

/*Read the touchpad*/
void my_touch_read(lv_indev_t *indev_driver, lv_indev_data_t *data)
{

  data->state = LV_INDEV_STATE_REL;

  UP = digitalRead(TDECK_TRACKBALL_UP);
  DOWN = digitalRead(TDECK_TRACKBALL_DOWN);
  LEFT = digitalRead(TDECK_TRACKBALL_LEFT);
  RIGHT = digitalRead(TDECK_TRACKBALL_RIGHT);
  CLICKED = digitalRead(TDECK_TRACKBALL_CLICK);

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

static void close_window_cb(lv_event_t *e)
{

  lv_obj_clean(lv_obj_get_parent((lv_obj_t *)lv_event_get_target(e)));
  lv_obj_delete(lv_obj_get_parent((lv_obj_t *)lv_event_get_target(e)));
}
static void create_contact_page(lv_event_t *e)
{
  if (!lv_obj_is_valid(TdeckDisplayUI.contact))
  {
    TdeckDisplayUI.contact = lv_obj_create(lv_screen_active());

    lv_obj_t *label = lv_label_create(TdeckDisplayUI.contact);
    TdeckDisplayUI.close_btn = lv_button_create(TdeckDisplayUI.contact);
    lv_obj_t *label_close = lv_label_create(TdeckDisplayUI.close_btn);
    lv_label_set_text(label_close, "CLOSE");
    lv_label_set_text(label, "Contact Page");
    lv_obj_align(TdeckDisplayUI.close_btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
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
    TdeckDisplayUI.setting = lv_obj_create(lv_screen_active());
    lv_obj_t *label = lv_label_create(TdeckDisplayUI.setting);
    TdeckDisplayUI.close_btn = lv_button_create(TdeckDisplayUI.setting);
    lv_obj_t *label_close = lv_label_create(TdeckDisplayUI.close_btn);
    lv_label_set_text(label_close, "CLOSE");
    lv_label_set_text(label, "Settings Page");
    lv_obj_align(TdeckDisplayUI.close_btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_size(TdeckDisplayUI.setting, 200, 200);
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
    lv_obj_t *label_close = lv_label_create(TdeckDisplayUI.close_btn);
    lv_label_set_text(label_close, "CLOSE");
    lv_label_set_text(label, "Phone Page");
    lv_obj_align(TdeckDisplayUI.close_btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
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
    lv_obj_t *label_close = lv_label_create(TdeckDisplayUI.close_btn);
    lv_label_set_text(label_close, "CLOSE");
    lv_label_set_text(label, "Messages Page");
    lv_obj_align(TdeckDisplayUI.close_btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_size(TdeckDisplayUI.messages, 200, 200);
    lv_obj_center(TdeckDisplayUI.messages);
    Serial.println("I am clicked!!!");
    lv_obj_add_event_cb(TdeckDisplayUI.close_btn, close_window_cb, LV_EVENT_CLICKED, NULL);
  }
}

static void create_calculator_page(lv_event_t *e)
{
  if (!lv_obj_is_valid(TdeckDisplayUI.calculator))
  {
    TdeckDisplayUI.calculator = lv_obj_create(lv_screen_active());
    lv_obj_t *label = lv_label_create(TdeckDisplayUI.calculator);
    TdeckDisplayUI.close_btn = lv_button_create(TdeckDisplayUI.calculator);
    lv_obj_t *label_close = lv_label_create(TdeckDisplayUI.close_btn);
    lv_label_set_text(label_close, "CLOSE");
    lv_label_set_text(label, "Calculator Page");
    lv_obj_align(TdeckDisplayUI.close_btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_size(TdeckDisplayUI.calculator, 200, 200);
    lv_obj_center(TdeckDisplayUI.calculator);
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
  // lv_label_set_text(TdeckDisplayUI.connection_status, WiFi.status() == WL_CONNECTED ? "Connected..." : "Not Connected...");
  //  snprintf(weather_buffer, sizeof(weather_buffer), "%3.2f", (weather->temperature - 273.15) * 9 / 5 + 32);
  //  lv_label_set_text_fmt(TdeckDisplayUI.temperature_label, "Temp: %sF", weather_buffer);
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
  static lv_style_t button_click;
  lv_style_init(&button_click);
  lv_style_set_image_recolor_opa(&button_click, LV_OPA_30);
  TdeckDisplayUI.main_screen = lv_image_create(lv_screen_active());

  TdeckDisplayUI.nav_screen = lv_obj_create(TdeckDisplayUI.main_screen);
  TdeckDisplayUI.battery_label = lv_label_create(TdeckDisplayUI.nav_screen);
  // TdeckDisplayUI.datetime_label = lv_label_create(TdeckDisplayUI.nav_screen);
  TdeckDisplayUI.connection_status = lv_image_create(TdeckDisplayUI.nav_screen);
  // TdeckDisplayUI.temperature_label = lv_label_create(TdeckDisplayUI.main_screen);
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

  lv_image_set_src(TdeckDisplayUI.main_screen, &wallpaper);
  lv_imagebutton_set_src(TdeckDisplayUI.icons[0], LV_IMAGEBUTTON_STATE_RELEASED, &book, &book, &book);
  lv_imagebutton_set_src(TdeckDisplayUI.icons[1], LV_IMAGEBUTTON_STATE_RELEASED, &setting, &setting, &setting);
  lv_imagebutton_set_src(TdeckDisplayUI.icons[2], LV_IMAGEBUTTON_STATE_RELEASED, &telephone, &telephone, &telephone);
  lv_imagebutton_set_src(TdeckDisplayUI.icons[3], LV_IMAGEBUTTON_STATE_RELEASED, &messages, &messages, &messages);
  lv_imagebutton_set_src(TdeckDisplayUI.icons[4], LV_IMAGEBUTTON_STATE_RELEASED, &calculator, &calculator, &calculator);
  lv_obj_add_style(TdeckDisplayUI.icons[0], &button_click, LV_STATE_PRESSED);
  lv_obj_add_style(TdeckDisplayUI.icons[1], &button_click, LV_STATE_PRESSED);
  lv_obj_add_style(TdeckDisplayUI.icons[2], &button_click, LV_STATE_PRESSED);
  lv_obj_add_style(TdeckDisplayUI.icons[3], &button_click, LV_STATE_PRESSED);
  lv_obj_add_style(TdeckDisplayUI.icons[4], &button_click, LV_STATE_PRESSED);
  lv_obj_align(TdeckDisplayUI.bat_img, LV_ALIGN_RIGHT_MID, -10, 0);

  lv_obj_align(TdeckDisplayUI.connection_status, LV_ALIGN_LEFT_MID, 10, 0);

  lv_obj_set_size(TdeckDisplayUI.main_screen, 320, 240);

  lv_obj_center(TdeckDisplayUI.main_screen);

  lv_obj_set_style_margin_top(TdeckDisplayUI.main_screen, 40, LV_PART_MAIN);
  lv_obj_set_flex_flow(TdeckDisplayUI.main_screen, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_size(TdeckDisplayUI.nav_screen, 320, 30);
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
  lv_obj_set_size(TdeckDisplayUI.icons[0], 60, 60);
  lv_obj_set_size(TdeckDisplayUI.icons[1], 60, 60);
  lv_obj_set_size(TdeckDisplayUI.icons[2], 60, 60);
  lv_obj_set_size(TdeckDisplayUI.icons[3], 60, 60);
  lv_obj_set_size(TdeckDisplayUI.icons[4], 60, 60);

  lv_obj_add_event_cb(TdeckDisplayUI.icons[0], create_contact_page, LV_EVENT_CLICKED, TdeckDisplayUI.main_screen);
  lv_obj_add_event_cb(TdeckDisplayUI.icons[1], create_setting_page, LV_EVENT_CLICKED, TdeckDisplayUI.main_screen);
  lv_obj_add_event_cb(TdeckDisplayUI.icons[2], create_phone_page, LV_EVENT_CLICKED, TdeckDisplayUI.main_screen);
  lv_obj_add_event_cb(TdeckDisplayUI.icons[3], create_messages_page, LV_EVENT_CLICKED, TdeckDisplayUI.main_screen);
  lv_obj_add_event_cb(TdeckDisplayUI.icons[4], create_calculator_page, LV_EVENT_CLICKED, TdeckDisplayUI.main_screen);
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
        weather->temperature = doc["main"]["temp"];
        weather->humidity = doc["main"]["humidity"];
        weather->wind_speed = doc["wind"]["speed"];

        Serial.println(payload);
      }
      else
      {
        Serial.println(httpCode);
        WiFi.mode(WIFI_STA);
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

  static lv_color_t buf[320 * 10];
  lv_display_t *display = lv_display_create(320, 240);
/*Declare a buffer for 1/10 screen size*/
#define BYTE_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565)) /*will be 2 for RGB565 */
  static uint8_t buf1[320 * 240 / 10 * BYTE_PER_PIXEL];

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
  // lv_obj_add_style(mouse_cursor, &transparent_style, LV_PART_MAIN);
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
  touch.setMaxCoordinates(320, 240);

  // Set swap xy
  touch.setSwapXY(true);

  // Set mirror xy
  touch.setMirrorXY(false, true);
  pinMode(BOARD_BL_PIN, OUTPUT);
  setBrightness(16);

  xTaskCreatePinnedToCore(setupLVGL, "setupLVGL", 1024 * 10, NULL, 3, &lvglTaskHandler, 0);
  xTaskCreatePinnedToCore(wifiTask, "wifiTask", 1024 * 6, NULL, 2, &wifiTaskHandler, 1);
  xTaskCreatePinnedToCore(sensorsTask, "sensorsTask", 1024 * 6, NULL, 1, &sensorTaskHandler, 1);
}

void loop()
{
}