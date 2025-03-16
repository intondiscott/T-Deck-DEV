#include <TFT_eSPI.h>
#include <lvgl.h>
#pragma once
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
        *lora_status,
        *weather_conditions,
        *temperature_label,
        *wind_speed_label,
        *humidity_label,
        *contact;
    char bat[6];
} static tdeckDisplayUI;

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
