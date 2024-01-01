#include <LGFX_T-Deck.h>
#include <T-Deck_Init_Disp.h>
#include <../lib/lvgl/lvgl.h>
#define TOUCH_MODULES_GT911
#include "TouchLib.h"
LGFX lcd;
lv_indev_t *touch_indev = NULL;
bool touchDected = false;
TouchLib *touch = NULL;

uint8_t touchAddress = GT911_SLAVE_ADDRESS1;
void createUI();
void setupLVGL();
static void disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);
static void eventHandle(lv_event_t *event);
static bool getTouch(int16_t &x, int16_t &y);
static void touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data);

void setup()
{

  Serial.begin(115200);
  pinMode(BOARD_TOUCH_INT, INPUT);
  Serial.println("Starting monitor");
  Init_Display();
  String greet = "Hello, Scotty";
  lcd.init();
  lcd.setRotation(1);
  lcd.fillScreen(TFT_BROWN);

  Wire.begin(BOARD_I2C_SDA, BOARD_I2C_SCL);
  delay(20);

  touch = new TouchLib(Wire, BOARD_I2C_SDA, BOARD_I2C_SCL, touchAddress);
  touch->init();
  bool ret = 0;
  Wire.beginTransmission(touchAddress);
  ret = Wire.endTransmission() == 0;
  touchDected = ret;
  setupLVGL();
  createUI();
}

void loop()
{
  lv_task_handler();
  delay(5);
}
void setupLVGL()
{
  static lv_disp_draw_buf_t draw_buf;

#define LVGL_BUFFER_SIZE (TFT_WIDTH * TFT_HEIGHT / 10)
  static lv_color_t buf[LVGL_BUFFER_SIZE];

  lv_init();

  lv_disp_draw_buf_init(&draw_buf, buf, NULL, LVGL_BUFFER_SIZE);

  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);

  /*Change the following line to your display resolution*/
  disp_drv.hor_res = TFT_HEIGHT;
  disp_drv.ver_res = TFT_WIDTH;
  disp_drv.flush_cb = disp_flush;
  disp_drv.draw_buf = &draw_buf;
  disp_drv.full_refresh = 1;

  lv_disp_drv_register(&disp_drv);

  /*Initialize the  input device driver*/
  /*Register a touchscreen input device*/
  if (touchDected)
  {
    static lv_indev_drv_t indev_touchpad;
    lv_indev_drv_init(&indev_touchpad);
    indev_touchpad.type = LV_INDEV_TYPE_POINTER;
    indev_touchpad.read_cb = touchpad_read;
    touch_indev = lv_indev_drv_register(&indev_touchpad);
  }
}

/*Read the touchpad*/
static void touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
  data->state = getTouch(data->point.x, data->point.y) ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
}

static void disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  lcd.startWrite();
  lcd.setAddrWindow(area->x1, area->y1, w, h);
  lcd.pushPixels((uint16_t *)&color_p->full, w * h, true);
  lcd.endWrite();
  lv_disp_flush_ready(disp);
}

static bool getTouch(int16_t &x, int16_t &y)
{
  uint8_t rotation = lcd.getRotation();
  if (!touch->read())
  {
    return false;
  }
  TP_Point t = touch->getPoint(0);
  switch (rotation)
  {
  case 1:
    x = t.y;
    y = lcd.height() - t.x;
    break;
  case 2:
    x = lcd.width() - t.x;
    y = lcd.height() - t.y;
    break;
  case 3:
    x = lcd.width() - t.y;
    y = t.x;
    break;
  case 0:
  default:
    x = t.x;
    y = t.y;
  }
  Serial.printf("R:%d X:%d Y:%d\n", rotation, x, y);
  return true;
}

void createUI()
{
  static lv_style_t style1;

  lv_style_init(&style1);
  lv_obj_t *btn = lv_btn_create(lv_scr_act());
  lv_obj_t *label = lv_label_create(btn);

  lv_obj_set_x(btn, 100);
  lv_obj_set_y(btn, 100);

  lv_style_set_bg_color(&style1, lv_color_make(128, 14, 118));
  lv_obj_add_event_cb(btn, eventHandle, LV_EVENT_ALL, NULL);
  lv_obj_add_style(btn, &style1, 0);
  lv_label_set_text(label, "My button");
}

static void eventHandle(lv_event_t *event)
{
  lv_event_code_t code = lv_event_get_code(event);
  lv_obj_t *obj = lv_event_get_target(event);
  // Serial.printf("something clicked at");
  if (code == LV_EVENT_CLICKED)
  {

    lv_obj_t *label = lv_obj_get_child(obj, 0);
    lv_label_set_text(label, "Clicked!!!");
  }
  if (code == LV_EVENT_LONG_PRESSED)
  {

    lv_obj_t *label = lv_obj_get_child(obj, 0);
    lv_label_set_text(label, "Clicked Long!!!");
  }
  if (code == LV_EVENT_LONG_PRESSED_REPEAT)
  {

    lv_obj_t *label = lv_obj_get_child(obj, 0);
    lv_label_set_text(label, "My Button!!!");
  }
}
