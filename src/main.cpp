#include <Arduino.h>
#include <TFT_eSPI.h>
#include <lvgl.h>
#include <WiFi.h>

#include <TDeck_Structs.h>
#include "TouchDrvGT911.hpp"
#include <ArduinoJson.h>

#include <RadioLib.h>
#include <HTTPClient.h>
#include "credentials.c"
#include "../assets/images/mouse_pointer.h"

#include <../assets/images/critical_low_bat.h>
#include <../assets/images/charging.h>
#include <../assets/images/full_battery.h>
#include <../assets/images/bat_two_bars.h>
#include <../assets/images/low_bat.h>
#include <../assets/images/wallpaper.h>
#include <../assets/images/antenna.h>

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

#include <applications/contact_app.cpp>
#include <applications/messages_app.cpp>
#include <applications/phone_app.cpp>
#include <applications/weather_app.cpp>
#include <applications/calendar_app.cpp>
#include <applications/calculator_app.cpp>
#include <applications/settings_app.cpp>
#define TFT_WIDTH 320
#define TFT_HEIGHT 240

TaskHandle_t lvglTaskHandler, sensorTaskHandler, wifiTaskHandler;

int direction_count_up = 0;
int direction_count_down = 0;
int direction_count_left = 0;
int direction_count_right = 0;
int pointer_speed = 5;

char kb_buf[256];

int UP, DOWN, LEFT, RIGHT, CLICKED;
TouchDrvGT911 touch;
unsigned long lastTickMillis = 0;
// LilyGo  T-Deck  control backlight chip has 16 levels of adjustment range
// The adjustable range is 0~15, 0 is the minimum brightness, 15 is the maximum brightness
int trackball_val = 0;

Weather *weather_vals = new Weather;

Settings *setting_values = new Settings;

// SX1262 has the following connections:
#define SCK 40
#define MISO 38
#define MOSI 41
#define CS 39
#define DIO1 45
#define RESET 17
#define BUSY 13

SX1262 radio = new Module(RADIO_CS_PIN, DIO1, RESET, BUSY);

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  lv_draw_sw_rgb565_swap(color_p, w * h);
  tdeckDisplayUI.tft.pushImage(area->x1, area->y1, w, h, (uint16_t *)color_p);
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

  data->state = LV_INDEV_STATE_REL;

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

void screen_update()
{

  uint32_t battery_percentage = (analogRead(BOARD_BAT_ADC) * 4.6) * 8 / 1024;

  snprintf(tdeckDisplayUI.bat, sizeof(tdeckDisplayUI.bat), "%d", battery_percentage);
  if (WiFi.status() == WL_CONNECTED)
  {
    LV_IMAGE_DECLARE(internet);
    lv_image_set_src(tdeckDisplayUI.connection_status, &internet);
  }
  else
  {
    LV_IMAGE_DECLARE(no_wifi);
    lv_image_set_src(tdeckDisplayUI.connection_status, &no_wifi);
  }
  if (setting_values->bluetooth_communications)
  {
    LV_IMAGE_DECLARE(bluetooth);
    lv_image_set_src(tdeckDisplayUI.bluetooth_status, &bluetooth);
  }
  else
  {
    lv_image_set_src(tdeckDisplayUI.bluetooth_status, NULL);
  }
  if (setting_values->radio_communications)
  {
    LV_IMAGE_DECLARE(antenna);
    lv_image_set_src(tdeckDisplayUI.lora_status, &antenna);
  }
  else
  {
    lv_image_set_src(tdeckDisplayUI.lora_status, NULL);
  }

  lv_label_set_text_fmt(tdeckDisplayUI.battery_label, "%s%%", tdeckDisplayUI.bat);

  if (analogRead(BOARD_BAT_ADC) > 2700)
  {
    LV_IMAGE_DECLARE(charging);
    lv_image_set_src(tdeckDisplayUI.bat_img, &charging);
  }
  else
  {
    switch (battery_percentage)
    {
    case 80 ... 100:
      LV_IMAGE_DECLARE(full_battery);
      lv_image_set_src(tdeckDisplayUI.bat_img, &full_battery);
      break;
    case 40 ... 79:
      LV_IMAGE_DECLARE(bat_two_bars);
      lv_image_set_src(tdeckDisplayUI.bat_img, &bat_two_bars);
      break;
    case 15 ... 39:
      LV_IMAGE_DECLARE(low_bat);
      lv_image_set_src(tdeckDisplayUI.bat_img, &low_bat);
      break;
    case 0 ... 14:
      LV_IMAGE_DECLARE(critical_low_bat);
      lv_image_set_src(tdeckDisplayUI.bat_img, &critical_low_bat);
      break;
    default:
      break;
    }
  }
}
void sensorsTask(void *pvParams)
{

  while (1)
  {

    if (WiFi.status() == WL_CONNECTED)
    {
      HTTPClient http2, http3;
      int httpCode2, httpCode3;
      float x, y, z;

      String battery = "\"pin1\":\"" + (String)tdeckDisplayUI.bat + "\",";
      String charging = "\"pin2\":\"" + (String)analogRead(BOARD_BAT_ADC) + "\",";
      String chip_temp = "\"pin3\":\"84\"";

      http2.begin("http://192.168.0.114:8080/api/v1/devices/1");
      http3.begin("http://192.168.0.223:8080/api/v1/devices/1");
      http2.addHeader("Content-type", "application/json");
      http3.addHeader("Content-type", "application/json");
      http2.PUT(
          "{" +

          battery +
          charging +
          chip_temp +
          "}");
      http3.PUT(
          "{" +

          battery +
          // charging +
          // chip_temp +
          "}");
      http2.end();
      http3.end();
    }
    else
    {

      if (setting_values->wifi_communications)
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
    }
    vTaskDelay(2);
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
  tdeckDisplayUI.main_screen = lv_image_create(lv_screen_active());

  tdeckDisplayUI.nav_screen = lv_obj_create(tdeckDisplayUI.main_screen);
  tdeckDisplayUI.battery_label = lv_label_create(tdeckDisplayUI.nav_screen);

  tdeckDisplayUI.connection_status = lv_image_create(tdeckDisplayUI.nav_screen);
  tdeckDisplayUI.bluetooth_status = lv_image_create(tdeckDisplayUI.nav_screen);
  tdeckDisplayUI.lora_status = lv_image_create(tdeckDisplayUI.nav_screen);
  tdeckDisplayUI.bat_img = lv_image_create(tdeckDisplayUI.nav_screen);

  tdeckDisplayUI.icons[0] = lv_imagebutton_create(tdeckDisplayUI.main_screen);
  tdeckDisplayUI.icons[1] = lv_imagebutton_create(tdeckDisplayUI.main_screen);
  tdeckDisplayUI.icons[2] = lv_imagebutton_create(tdeckDisplayUI.main_screen);
  tdeckDisplayUI.icons[3] = lv_imagebutton_create(tdeckDisplayUI.main_screen);
  tdeckDisplayUI.icons[4] = lv_imagebutton_create(tdeckDisplayUI.main_screen);
  tdeckDisplayUI.icons[5] = lv_imagebutton_create(tdeckDisplayUI.main_screen);
  tdeckDisplayUI.icons[6] = lv_imagebutton_create(tdeckDisplayUI.main_screen);

  lv_image_set_src(tdeckDisplayUI.main_screen, &wallpaper);
  lv_imagebutton_set_src(tdeckDisplayUI.icons[0], LV_IMAGEBUTTON_STATE_RELEASED, &book, &book, &book);
  lv_imagebutton_set_src(tdeckDisplayUI.icons[1], LV_IMAGEBUTTON_STATE_RELEASED, &setting, &setting, &setting);
  lv_imagebutton_set_src(tdeckDisplayUI.icons[2], LV_IMAGEBUTTON_STATE_RELEASED, &telephone, &telephone, &telephone);
  lv_imagebutton_set_src(tdeckDisplayUI.icons[3], LV_IMAGEBUTTON_STATE_RELEASED, &messages, &messages, &messages);
  lv_imagebutton_set_src(tdeckDisplayUI.icons[4], LV_IMAGEBUTTON_STATE_RELEASED, &calculator, &calculator, &calculator);
  lv_imagebutton_set_src(tdeckDisplayUI.icons[5], LV_IMAGEBUTTON_STATE_RELEASED, &calendar, &calendar, &calendar);
  lv_imagebutton_set_src(tdeckDisplayUI.icons[6], LV_IMAGEBUTTON_STATE_RELEASED, &weather, &weather, &weather);
  lv_obj_add_style(tdeckDisplayUI.icons[0], &button_click, LV_STATE_PRESSED);
  lv_obj_add_style(tdeckDisplayUI.icons[1], &button_click, LV_STATE_PRESSED);
  lv_obj_add_style(tdeckDisplayUI.icons[2], &button_click, LV_STATE_PRESSED);
  lv_obj_add_style(tdeckDisplayUI.icons[3], &button_click, LV_STATE_PRESSED);
  lv_obj_add_style(tdeckDisplayUI.icons[4], &button_click, LV_STATE_PRESSED);
  lv_obj_add_style(tdeckDisplayUI.icons[5], &button_click, LV_STATE_PRESSED);
  lv_obj_add_style(tdeckDisplayUI.icons[6], &button_click, LV_STATE_PRESSED);
  lv_obj_align(tdeckDisplayUI.bat_img, LV_ALIGN_RIGHT_MID, -10, 0);

  lv_obj_align(tdeckDisplayUI.connection_status, LV_ALIGN_LEFT_MID, 10, 0);
  lv_obj_align(tdeckDisplayUI.bluetooth_status, LV_ALIGN_LEFT_MID, 30, 0);
  lv_obj_align(tdeckDisplayUI.lora_status, LV_ALIGN_LEFT_MID, 50, 0);

  lv_obj_set_size(tdeckDisplayUI.main_screen, TFT_WIDTH, TFT_HEIGHT);

  lv_obj_center(tdeckDisplayUI.main_screen);

  lv_obj_set_style_margin_top(tdeckDisplayUI.main_screen, 40, LV_PART_MAIN);
  lv_obj_set_flex_flow(tdeckDisplayUI.main_screen, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_size(tdeckDisplayUI.nav_screen, TFT_WIDTH, 30);
  lv_obj_align(tdeckDisplayUI.battery_label, LV_ALIGN_RIGHT_MID, -60, 0);

  lv_obj_set_style_pad_all(tdeckDisplayUI.nav_screen, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(tdeckDisplayUI.main_screen, 0, LV_PART_MAIN);
  lv_obj_set_style_margin_all(tdeckDisplayUI.nav_screen, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(tdeckDisplayUI.main_screen, lv_color_hex(0x98a3a2), LV_PART_MAIN);
  lv_obj_set_style_bg_color(tdeckDisplayUI.nav_screen, lv_color_hex(0x948d8d), LV_PART_MAIN);

  lv_obj_set_style_text_color(tdeckDisplayUI.nav_screen, lv_color_hex(0x000000), LV_PART_MAIN);

  lv_obj_set_style_margin_all(tdeckDisplayUI.icons[0], 10, LV_PART_MAIN);
  lv_obj_set_style_margin_all(tdeckDisplayUI.icons[1], 10, LV_PART_MAIN);
  lv_obj_set_style_margin_all(tdeckDisplayUI.icons[2], 10, LV_PART_MAIN);
  lv_obj_set_style_margin_all(tdeckDisplayUI.icons[3], 10, LV_PART_MAIN);
  lv_obj_set_style_margin_all(tdeckDisplayUI.icons[4], 10, LV_PART_MAIN);
  lv_obj_set_style_margin_all(tdeckDisplayUI.icons[5], 10, LV_PART_MAIN);
  lv_obj_set_style_margin_all(tdeckDisplayUI.icons[6], 10, LV_PART_MAIN);
  lv_obj_set_size(tdeckDisplayUI.icons[0], 60, 60);
  lv_obj_set_size(tdeckDisplayUI.icons[1], 60, 60);
  lv_obj_set_size(tdeckDisplayUI.icons[2], 60, 60);
  lv_obj_set_size(tdeckDisplayUI.icons[3], 60, 60);
  lv_obj_set_size(tdeckDisplayUI.icons[4], 60, 60);
  lv_obj_set_size(tdeckDisplayUI.icons[5], 60, 60);
  lv_obj_set_size(tdeckDisplayUI.icons[6], 60, 60);

  lv_obj_add_event_cb(tdeckDisplayUI.icons[0], create_contact_page, LV_EVENT_CLICKED, tdeckDisplayUI.main_screen);
  lv_obj_add_event_cb(tdeckDisplayUI.icons[1], create_setting_page, LV_EVENT_CLICKED, tdeckDisplayUI.main_screen);
  lv_obj_add_event_cb(tdeckDisplayUI.icons[2], create_phone_page, LV_EVENT_CLICKED, tdeckDisplayUI.main_screen);
  lv_obj_add_event_cb(tdeckDisplayUI.icons[3], create_messages_page, LV_EVENT_CLICKED, tdeckDisplayUI.main_screen);
  lv_obj_add_event_cb(tdeckDisplayUI.icons[4], create_calculator_page, LV_EVENT_CLICKED, tdeckDisplayUI.main_screen);
  lv_obj_add_event_cb(tdeckDisplayUI.icons[5], create_calendar_page, LV_EVENT_CLICKED, tdeckDisplayUI.main_screen);
  lv_obj_add_event_cb(tdeckDisplayUI.icons[6], create_weather_page, LV_EVENT_CLICKED, tdeckDisplayUI.main_screen);
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

        if (setting_values->wifi_communications)
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
        http1.end();
      }
    }
    vTaskDelay(10000);
  }
}

#define BYTE_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565)) /*will be 2 for RGB565 */

void setupLVGL(void *pvParams)
{

  lv_init();

  static lv_color_t buf[TFT_WIDTH * 10];
  lv_display_t *display = lv_display_create(TFT_WIDTH, TFT_HEIGHT);

  /*Declare a buffer for 1/10 screen size*/
  static uint8_t buf1[TFT_WIDTH * TFT_HEIGHT / 10 * BYTE_PER_PIXEL];

  lv_display_set_buffers(display, buf1, NULL, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL); /*Initialize the display buffer.*/
  lv_display_set_flush_cb(display, my_disp_flush);

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

  tdeckDisplayUI.tft.begin();
  tdeckDisplayUI.tft.setRotation(1);
  tdeckDisplayUI.tft.fillScreen(TFT_BLACK);
  tdeckDisplayUI.tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tdeckDisplayUI.tft.setTextSize(2);
  tdeckDisplayUI.tft.setCursor(0, TFT_HEIGHT / 2);

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
  tdeckDisplayUI.tft.print("Loading");
  for (int i = 0; i < 5; i++)
  {
    tdeckDisplayUI.tft.print(".");
    delay(500);
  }
  keyboard_init();
  xTaskCreatePinnedToCore(setupLVGL, "setupLVGL", 1024 * 10, NULL, 3, &lvglTaskHandler, 0);
  xTaskCreatePinnedToCore(wifiTask, "wifiTask", 1024 * 6, NULL, 2, &wifiTaskHandler, 1);
  xTaskCreatePinnedToCore(sensorsTask, "sensorsTask", 1024 * 6, NULL, 1, &sensorTaskHandler, 1);
}

void loop()
{
}