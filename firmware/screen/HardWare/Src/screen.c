#include "screen.h"
#include "screen_link.h"
#include "system_wire.h"
#include "system_state.h"
#include "ui.h"
#include "ui-myself.h"
#include "screens/ui_pressure_calibration.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
uint8_t EYE_status;
extern uint8_t work_mode;
extern TIM_HandleTypeDef htim14;
// char buffer[64];  // 假�?�数字加上百分号不会超过 64 �??字�??
extern lv_obj_t *ui_Label30; // 倒�?�时显示的标�??
extern lv_obj_t *ui_Label32; // 倒�?�时显示的标�??
extern lv_obj_t *ui_Label39; // 倒�?�时显示的标�??
extern lv_obj_t *ui_Label46; // 倒�?�时显示的标�??
extern lv_obj_t *ui_Slider1; // 电池电量
// extern lv_obj_t *ui_Label58; // 倒�?�时显示的标�??
extern lv_obj_t *ui_Slider2; // 电池电量
extern lv_obj_t *ui_Slider3; // 电池电量
extern lv_obj_t *ui_Slider9; // 电池电量
extern lv_obj_t *ui_Slider5; // 电池电量
extern lv_obj_t *ui_Slider6; // 电池电量
extern lv_obj_t *ui_Slider7; // 电池电量
extern lv_obj_t *ui_Slider8; // 电池电量
extern lv_obj_t *ui_Image53;
extern lv_obj_t *ui_Image56;
extern lv_obj_t *ui_Image57;
extern lv_obj_t *ui_Label5; // 电池电量数字
extern lv_obj_t *ui_Container10;
extern lv_obj_t *ui_Container4;
extern lv_obj_t *ui_Container7;

extern lv_obj_t *ui_Label2; // �??敷�?�数
extern lv_obj_t *ui_Label3; // 脉动次数
extern lv_obj_t *ui_Label4; // �??�?�??�数
extern lv_obj_t *ui_Label6; // 设置压力�??
extern lv_obj_t *ui_Label8; // 设置时间�??
extern lv_obj_t *ui_Image100;
extern lv_obj_t *ui_Image99;
extern lv_obj_t *ui_Image113;
extern lv_obj_t *ui_Image95;
extern lv_obj_t *ui_TextArea42;

uint16_t prepare_temprature, prepare_crimp, prepare_time, bee,
    prepare_set; // 准�?�温度，准�?�挤压�?�数，准备时间，蜂鸣�??，准备�?�置123
uint8_t work_finish = 0; // 0 没结束，1 工作完成
extern uint8_t usart1_tx;
extern UART_HandleTypeDef huart1;

static int16_t hot_temperature_tenths = 0;
static int16_t auto_temperature_tenths = 0;
static uint16_t pulsation_pressure = 0;
static uint16_t auto_pressure = 0;
static uint8_t battery_percent = 0;
static uint8_t battery_percent_valid = 0;
static uint8_t system_sample_flags;
static bool system_treatment_running;
static bool backlight_treatment_open;
static bool backlight_asleep, backlight_initialized, backlight_swallow_touch;
static uint32_t backlight_activity_ms;
extern TIM_HandleTypeDef htim1;

/* First touch wakes only; suppress the complete gesture to avoid blind clicks. */
bool screen_backlight_touch(bool pressed)
{
  if (pressed) {
    backlight_activity_ms = HAL_GetTick();
    if (backlight_asleep) {
      backlight_asleep = false;
      backlight_swallow_touch = true;
      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 60);
    }
  }
  bool consume = backlight_swallow_touch;
  if (!pressed) backlight_swallow_touch = false;
  return consume;
}

void screen_backlight_tick(void)
{
  uint32_t now = HAL_GetTick();
  if (!backlight_initialized || backlight_treatment_open) {
    backlight_initialized = true;
    backlight_activity_ms = now;
    if (backlight_asleep) {
      backlight_asleep = false;
      __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 60);
    }
  } else if (!backlight_asleep && now - backlight_activity_ms >= 120000U) {
    backlight_asleep = true;
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 0);
  }
}

static uint32_t software_version = 20261104UL;
static uint8_t screen_start_ack = 0;
static volatile uint8_t startup_language_received = 0U;
static volatile uint8_t startup_language_preload_enabled = 1U;

#define SCREEN_FAULT_STATUS_ID       0x2F01U
#define SCREEN_COMMUNICATION_FAULT   0x0501U

LV_FONT_DECLARE(ui_font_popup40);

typedef enum {
  SCREEN_POPUP_NONE = 0,
  SCREEN_POPUP_EYE_OFFLINE,
  SCREEN_POPUP_FAULT,
  SCREEN_POPUP_HOST_LOST
} screen_popup_kind_t;

static uint32_t active_fault_code = 0U;
static uint32_t last_battery_heartbeat_ms = 0U;
static uint8_t heartbeat_timer_started = 0U;
static uint8_t eye_state_received = 0U;
static screen_popup_kind_t displayed_popup = SCREEN_POPUP_NONE;
static uint32_t displayed_fault_code = 0U;
static uint8_t displayed_popup_language = 0xFFU;

void screen_preload_language_from_uart(const uint8_t *data, uint16_t size) {
  const uint16_t frame_size = (uint16_t)sizeof(prepare_data);

  if (!startup_language_preload_enabled || data == NULL || size < frame_size) {
    return;
  }

  for (uint16_t i = 0U; i + frame_size <= size; ++i) {
    const uint8_t *frame = &data[i];
    uint16_t received_crc;
    uint16_t language_value;

    if (frame[0] != 0x6AU || frame[1] != 0xA6U ||
        frame[2] != frame_size || frame[3] != 0x00U || frame[4] != 0xABU ||
        frame[frame_size - 2U] != 0xFFU || frame[frame_size - 1U] != 0xFFU) {
      continue;
    }

    received_crc = (uint16_t)frame[frame_size - 4U] |
                   (uint16_t)((uint16_t)frame[frame_size - 3U] << 8U);
    if (Calculate_CRC((uint8_t *)frame, frame_size - 4U) != received_crc) {
      continue;
    }

    language_value = (uint16_t)frame[5] | (uint16_t)((uint16_t)frame[6] << 8U);
    if (language_value <= UI_LANG_EN) {
      ui_lang_preselect(language_value);
      startup_language_received = 1U;
    }
  }
}

void screen_finish_startup_language_preload(void) {
  startup_language_preload_enabled = 0U;
}

void screen_fault_popup_tick(void) {
  uint32_t now = HAL_GetTick();
  uint8_t language = (uint8_t)ui_lang_get();
  uint8_t host_lost;
  screen_popup_kind_t wanted;
  char text[64];

  if (!heartbeat_timer_started) {
    heartbeat_timer_started = 1U;
    last_battery_heartbeat_ms = now;
  }

  host_lost = !ScreenLink_Healthy();
  if (eye_state_received && EYE_status == 0U) {
    wanted = SCREEN_POPUP_EYE_OFFLINE;
  } else if (active_fault_code == SCREEN_COMMUNICATION_FAULT) {
    /* The controller's screen-timeout fault and the local heartbeat timeout
     * describe the same problem. Present one consistent popup. */
    wanted = SCREEN_POPUP_HOST_LOST;
  } else if (active_fault_code != 0U) {
    wanted = SCREEN_POPUP_FAULT;
  } else if (host_lost) {
    wanted = SCREEN_POPUP_HOST_LOST;
  } else {
    wanted = SCREEN_POPUP_NONE;
  }

  if (wanted == SCREEN_POPUP_NONE) {
    if (global_panel != NULL &&
        !lv_obj_has_flag(global_panel, LV_OBJ_FLAG_HIDDEN)) {
      lv_obj_add_flag(global_panel, LV_OBJ_FLAG_HIDDEN);
    }
    displayed_popup = SCREEN_POPUP_NONE;
    return;
  }

  /* Do not overlay a transient splash with a transparent fault mask. A real
   * startup fault goes straight to the stable home background. No link yet
   * during the normal splash is expected, not an immediate boot fault. */
  if (lv_scr_act() == ui_Power_on_interface) {
    if (wanted == SCREEN_POPUP_HOST_LOST && !eye_state_received) return;
    ui_startup_finish();
  }
  create_global_panel();
  if (global_panel == NULL) {
    return;
  }

  if (wanted != displayed_popup || language != displayed_popup_language ||
      (wanted == SCREEN_POPUP_FAULT && active_fault_code != displayed_fault_code)) {
    if (wanted == SCREEN_POPUP_EYE_OFFLINE) {
      global_panel_set_content(ui_lang_get() == UI_LANG_EN ?
                               "Shield invalid\nReplace shield" :
                               "检测眼盾无效\n请更换眼盾",
                               ui_lang_get() == UI_LANG_EN ?
                               "Return to home" : "返回首页");
    } else if (wanted == SCREEN_POPUP_FAULT) {
      if (ui_lang_get() == UI_LANG_EN) {
        snprintf(text, sizeof(text), "Fault code: 0x%04lX",
                 (unsigned long)active_fault_code);
      } else {
        snprintf(text, sizeof(text), "故障码: 0x%04lX",
                 (unsigned long)active_fault_code);
      }
      global_panel_set_content(text, ui_lang_get() == UI_LANG_EN ?
                               "Return to home" : "返回首页");
    } else {
      global_panel_set_content(ui_lang_get() == UI_LANG_EN ?
                               "Communication failed" : "通讯失败",
                               ui_lang_get() == UI_LANG_EN ?
                               "Return to home" : "返回首页");
    }
    displayed_popup = wanted;
    displayed_fault_code = active_fault_code;
    displayed_popup_language = language;
  }

  /* The Return Home button may hide the panel. Reassert it while the
   * corresponding condition is still active. */
  if (lv_obj_has_flag(global_panel, LV_OBJ_FLAG_HIDDEN)) {
    lv_obj_clear_flag(global_panel, LV_OBJ_FLAG_HIDDEN);
  }
}

typedef struct {
  lv_coord_t width;
  lv_coord_t height;
  lv_coord_t y;
  lv_coord_t pad_top;
  lv_coord_t pad_bottom;
} version_text_layout_t;

static version_text_layout_t version_text_layout(void) {
  version_text_layout_t layout;

  if (ui_lang_get() == UI_LANG_EN) {
    layout.width = 260;
    layout.height = 28;
    layout.y = 296;
    layout.pad_top = 1;
    layout.pad_bottom = 0;
  } else {
    layout.width = 124;
    layout.height = 27;
    layout.y = 282;
    layout.pad_top = 0;
    layout.pad_bottom = 0;
  }

  return layout;
}

static int16_t temp_to_tenths(float_t value) {
  float_t scaled = value * 10.0f;
  return (int16_t)(scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);
}

static void set_temperature_label(lv_obj_t *label, int16_t tenths) {
  int16_t abs_value = tenths < 0 ? -tenths : tenths;
  int16_t whole = abs_value / 10;
  int16_t fraction = abs_value % 10;

  if (label == NULL) {
    return;
  }

  if (tenths < 0) {
    lv_label_set_text_fmt(label, "-%02d.%d", whole, fraction);
  } else {
    lv_label_set_text_fmt(label, "%02d.%d", whole, fraction);
  }
}

static void set_pressure_label(lv_obj_t *label, uint16_t value) {
  if (label == NULL) {
    return;
  }

  lv_label_set_text_fmt(label, "%03d", (int)value);
  if (value >= 247U) {
    lv_obj_set_style_text_color(label, lv_color_make(255, 0, 0), 0);
  } else {
    lv_obj_set_style_text_color(label, lv_color_make(255, 255, 255), 0);
  }
}

static void set_version_text(void) {
  char version_text[16];
  const char *current_text;
  version_text_layout_t layout;

  if (ui_TextArea42 == NULL) {
    return;
  }

  snprintf(version_text, sizeof(version_text), "%lu",
           (unsigned long)software_version);
  current_text = lv_textarea_get_text(ui_TextArea42);
  layout = version_text_layout();

  lv_textarea_set_one_line(ui_TextArea42, true);
  lv_textarea_set_cursor_click_pos(ui_TextArea42, false);
  lv_obj_clear_flag(ui_TextArea42, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(ui_TextArea42, layout.width);
  lv_obj_set_height(ui_TextArea42, layout.height);
  lv_obj_set_y(ui_TextArea42, layout.y);
  lv_obj_set_style_pad_top(ui_TextArea42, layout.pad_top,
                           LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_bottom(ui_TextArea42, layout.pad_bottom,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_line_space(ui_TextArea42, 0,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_letter_space(ui_TextArea42, 0,
                                     LV_PART_MAIN | LV_STATE_DEFAULT);
  if (current_text != NULL && strcmp(current_text, version_text) == 0) {
    return;
  }
  lv_textarea_set_text(ui_TextArea42, version_text);
  lv_textarea_set_cursor_pos(ui_TextArea42, 0);
  lv_obj_scroll_to_x(ui_TextArea42, 0, LV_ANIM_OFF);
  lv_obj_scroll_to_y(ui_TextArea42, 0, LV_ANIM_OFF);
}

void screen_refresh_prepare_ui(void) {
  ui_lang_update_preset_label(prepare_set);
}

void screen_refresh_runtime_ui(void) {
  screen_refresh_prepare_ui();
  if (system_sample_flags & 1U) set_temperature_label(ui_Label30, hot_temperature_tenths);
  else if (ui_Label30) lv_label_set_text(ui_Label30, "--");
  if (system_sample_flags & 1U) set_temperature_label(ui_Label39, auto_temperature_tenths);
  else if (ui_Label39) lv_label_set_text(ui_Label39, "--");
  set_pressure_label(ui_Label32, pulsation_pressure);
  set_pressure_label(ui_Label46, auto_pressure);
  set_version_text();
  ui_battery_warning_update(ScreenLink_Healthy() && (system_sample_flags & 32U),
                            ScreenLink_Healthy() && (system_sample_flags & 64U),
                            system_treatment_running || !ScreenLink_Healthy());

  if (!battery_percent_valid) {
    battery_percent = 0;
  }

  if (ui_Slider1 != NULL) lv_slider_set_value(ui_Slider1, battery_percent, LV_ANIM_ON);
  if (ui_Label5 != NULL) {
    char text[8];
    snprintf(text, sizeof(text), "%u%%", (unsigned)battery_percent);
    if (strcmp(lv_label_get_text(ui_Label5), text) != 0) {
      lv_label_set_text(ui_Label5, text);
    }
  }
  if (ui_Slider2 != NULL) lv_slider_set_value(ui_Slider2, battery_percent, LV_ANIM_ON);
  if (ui_Slider3 != NULL) lv_slider_set_value(ui_Slider3, battery_percent, LV_ANIM_ON);
  if (ui_Slider9 != NULL) lv_slider_set_value(ui_Slider9, battery_percent, LV_ANIM_ON);
  if (ui_Slider5 != NULL) lv_slider_set_value(ui_Slider5, battery_percent, LV_ANIM_ON);
  if (ui_Slider6 != NULL) lv_slider_set_value(ui_Slider6, battery_percent, LV_ANIM_ON);
  if (ui_Slider7 != NULL) lv_slider_set_value(ui_Slider7, battery_percent, LV_ANIM_ON);
  if (ui_Slider8 != NULL) lv_slider_set_value(ui_Slider8, battery_percent, LV_ANIM_ON);
}

void screen_reset_hotting_runtime_ui(void) {
  hot_temperature_tenths = 0;
  if (system_sample_flags & 1U) set_temperature_label(ui_Label30, hot_temperature_tenths);
  else if (ui_Label30) lv_label_set_text(ui_Label30, "--");
}

void screen_reset_pulsationning_runtime_ui(void) {
  pulsation_pressure = 0;
  set_pressure_label(ui_Label32, pulsation_pressure);
}

void screen_reset_autoing_runtime_ui(void) {
  auto_temperature_tenths = 0;
  auto_pressure = 0;
  if (system_sample_flags & 1U) set_temperature_label(ui_Label39, auto_temperature_tenths);
  else if (ui_Label39) lv_label_set_text(ui_Label39, "--");
  set_pressure_label(ui_Label46, auto_pressure);
}

void command_parsing(
    const uint8_t *received_data) { // 区分调试命令和屏幕工作命�??
  // �??认帧尾是否合�??
  // if (received_data->buffer[received_data->length - 2] != 0xFF ||
  //     received_data->buffer[received_data->length - 1] != 0xFF) {
  //   return; // 非法帧，直接返回
  // }
  // 提取命令类型（高字节和低字节�??
  uint16_t cmd_type =
      (received_data[0] << 8) |
      received_data[1]; // �??一位和�??二位判断�??不是调试参数
  // 解析命令
  switch (cmd_type) {
  case 0x5aa5: { // 命令类型 0x9000 - 处理 UART1 数据
    uint16_t message_type = ((uint16_t)received_data[3] << 8) | received_data[4];
    if (message_type == 0x2060U || message_type == SCREEN_FAULT_STATUS_ID) {
      processData_u32((PCTRL_MSG_U32)received_data);
    } else {
      processData((PCTRL_MSG)received_data); // process data from uart
    }
    break;
  }
  case 0x6aa6: { // 命令类型 0x6aa6 - 处理 UART1 数据
    processData_prepare(
        (prepare_data_p)received_data); // process data from uart
    HAL_GPIO_TogglePin(GPIOH, GPIO_PIN_15);
    break;
  }
  default:
    // �??知命令，忽略
    break;
  }
}

void processData(PCTRL_MSG msg) {
  uint16_t cmd_type = ((msg->cmd_type_high) << 8) |
                      (msg->cmd_type_low); // 将两�??8位值合并成一�??16位�?
  // uint16_t data = ((msg->data_high) << 8) | (msg->data_low); // 指令类型
  float_t data = msg->data;
  switch (cmd_type) {

  case 0x2041:                                       // 开始热�??
    hot_temperature_tenths = temp_to_tenths(data);
    screen_refresh_runtime_ui();
    break;
  case 0x2005: // 开始脉�??
    pulsation_pressure = (uint16_t)data;
    screen_refresh_runtime_ui();
    break;
  case 0x2037:                                       // �??动开始热�??
    auto_temperature_tenths = temp_to_tenths(data);
    screen_refresh_runtime_ui();
    break;
  case 0x2047: // �??动开始脉�??
    auto_pressure = (uint16_t)data;
    screen_refresh_runtime_ui();
    break;
  case 0x2050: // 跟新电池
    battery_percent = (uint8_t)data;
    battery_percent_valid = 1;
    last_battery_heartbeat_ms = HAL_GetTick();
    heartbeat_timer_started = 1U;
    screen_refresh_runtime_ui();
    break;
  case 0x2051: return; /* Versioned snapshot owns treatment UI. */
		case 0x2056: return;
  case 0x2057: return; /* Insertion is part of the snapshot. */
  case 0x2052: return;
  case 0x2053: // 在充�??
    lv_obj_set_style_bg_img_recolor(ui_Slider1, lv_color_hex(0x4FCD51),
                                    LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_recolor_opa(ui_Slider1, 255,
                                        LV_PART_INDICATOR | LV_STATE_DEFAULT);
    break;
  case 0x2054: // 不在充电
               // lv_obj_set_style_bg_img_recolor(ui_Slider1,
               // lv_color_hex(0x4FCD51), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_recolor_opa(ui_Slider1, 00,
                                        LV_PART_INDICATOR | LV_STATE_DEFAULT);
    break;
  case 0x2055: return; /* Eye validity is part of the snapshot. */
  default:
    break;
  }
}

void processData_u32(PCTRL_MSG_U32 msg) {
  uint16_t cmd_type = ((msg->cmd_type_high) << 8) | (msg->cmd_type_low);

  switch (cmd_type) {
  case 0x2060:
    software_version = msg->value;
    screen_refresh_runtime_ui();
    break;
  case SCREEN_FAULT_STATUS_ID:
    return; /* Versioned state snapshot owns fault status. */
  default:
    break;
  }
}

void processData_prepare(prepare_data_p msg) {
  uint16_t cmd_type = ((msg->cmd_type_high) << 8) |
                      (msg->cmd_type_low); // 将两�??8位值合并成一�??16位�?
  // uint16_t data = ((msg->data_high) << 8) | (msg->data_low); // 指令类型
  uint16_t data = msg->value;
  switch (cmd_type) {

  case 0x00A0:                                           // 加热次数
    lv_label_set_text_fmt(ui_Label2, "%05d", (int)data); // 显示加热次数
    break;
  case 0x00A1:                                           // 挤压次数
    lv_label_set_text_fmt(ui_Label3, "%05d", (int)data); //
    break;
  case 0x00A2:                                           // �??�?�??�数
    lv_label_set_text_fmt(ui_Label4, "%05d", (int)data); //
    break;
    case 0x00A3: // 选择预设
    prepare_set = data;
    switch (prepare_set) {
    case 0: {
      // 保持原有的面板透明度设置
      lv_obj_set_style_bg_opa(ui_Panel16, 220, LV_PART_MAIN); // 设置背景透明度
      lv_obj_set_style_bg_opa(ui_Panel17, 220, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(ui_Panel18, 220, LV_PART_MAIN);

      lv_obj_add_flag(ui_Image24, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_ADV_HITTEST); /// Flags
      prepare_time = 1;
      prepare_crimp = 150;
    } break;
    case 1: {
      lv_obj_set_style_bg_opa(ui_Panel16, 0, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(ui_Panel17, 220, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(ui_Panel18, 220, LV_PART_MAIN);

      lv_obj_clear_flag(ui_Image24, LV_OBJ_FLAG_HIDDEN); /// Flags
      screen_refresh_prepare_ui();
    } break;
    case 2: {
      lv_obj_set_style_bg_opa(ui_Panel16, 220, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(ui_Panel17, 0, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(ui_Panel18, 220, LV_PART_MAIN);

      lv_obj_clear_flag(ui_Image24, LV_OBJ_FLAG_HIDDEN); /// Flags
      screen_refresh_prepare_ui();
    } break;
    case 3: {
      lv_obj_set_style_bg_opa(ui_Panel16, 220, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(ui_Panel17, 220, LV_PART_MAIN);
      lv_obj_set_style_bg_opa(ui_Panel18, 0, LV_PART_MAIN);

      lv_obj_clear_flag(ui_Image24, LV_OBJ_FLAG_HIDDEN); /// Flags
      screen_refresh_prepare_ui();
    } break;
    }
    break;
  case 0x00A4: // 预�?�温�??
    prepare_temprature = data;

    break;
  case 0x00A5: // 预�?�压�??
    prepare_crimp = data;

    break;
  case 0x00A6: // 预�?�时�??
    prepare_time = data;
    break;
  case 0x00A8: // 预�?�温�??
               // prepare_temprature= data;

    break;
  case 0x00A9: // 预�?�压�??
               // prepare_crimp= data;

    lv_label_set_text_fmt(ui_Label6, "%03d", (int)data); // 显示加热温度次数
    break;
  case 0x00AA: // 预�?�时�??
    // prepare_time= data;
    lv_label_set_text_fmt(ui_Label8, "%02d", (int)data); // 显示加热温度次数
    break;
  case 0x00AC: // 型号区分
    if (data == 0U) {
      if (ui_Image24 != NULL) {
        lv_obj_add_flag(ui_Image24, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_ADV_HITTEST);
      }
      if (ui_Panel2 != NULL) {
        lv_obj_add_flag(ui_Panel2, LV_OBJ_FLAG_HIDDEN);
      }
    }
    break;
	case 0x00b0: // 
    lv_label_set_text_fmt(ui_Label1, "%d", (int)data); // 显示加热温度次数
    break;
  case 0x00A7: // bee
    bee = data;
    HAL_GPIO_WritePin(Bee_vcc_GPIO_Port, Bee_vcc_Pin,
                      bee == 1 ? GPIO_PIN_SET : GPIO_PIN_RESET);
    ui_lang_apply_bee_state();
    break;
  case 0x00AB: // language
    /* A startup frame may already have preselected the language before
     * ui_init(). Runtime frames reach here without ISR-side UI state changes,
     * so a changed value refreshes the current page immediately. */
    if ((data <= 1U) && (ui_lang_get() != data)) {
      ui_lang_set(data);
      screen_refresh_runtime_ui();
    }
    break;
  case 0x00AD: // screen power-on ACK
    if (data == 1U) {
      screen_start_ack = 1U;
    }
    break;
  default:
    break;
  }
}

uint8_t screen_start_ack_received(void) {
  return screen_start_ack;
}

uint8_t screen_start_language_received(void) {
  return startup_language_received;
}

/* 环形队列入队操作 */
#if 0
int RingQueue_Push(RingQueue *queue, UART_RX_TypeDef *data) {
  if (queue->count >= UART_QUEUE_SIZE) {
    return -1; // 队列已满，无法入队
  }
  queue->buffer[queue->head] = data;                 // 存入队列
  queue->head = (queue->head + 1) % UART_QUEUE_SIZE; // 头部索引循环递增
  queue->count++;                                    // 更新计数
  return 0;                                          // 入队成功
}

/* 环形队列出队操作 */
UART_RX_TypeDef *RingQueue_Pop(RingQueue *queue) {
  if (queue->count == 0) {
    return NULL; // 队列为空，返回NULL
  }
  UART_RX_TypeDef *data = queue->buffer[queue->tail]; // 取出数据
  queue->tail = (queue->tail + 1) % UART_QUEUE_SIZE;  // 尾部索引循环递增
  queue->count--;                                     // 更新计数
  return data;                                        // 返回数据指针
}
#endif
/* 计算 CRC 校验值 */
uint16_t Calculate_CRC(uint8_t *data, uint16_t length) {
  uint16_t crc = 0xFFFF; // 初始 CRC 值
  for (uint16_t i = 0; i < length; i++) {
    crc ^= data[i]; // 进行 CRC 计算
    for (uint8_t j = 0; j < 8; j++) {
      if (crc & 0x01) {
        crc = (crc >> 1) ^ 0xA001; // 右移并异或
      } else {
        crc >>= 1; // 右移
      }
    }
  }
  return crc; // 返回计算结果
}

#if 0
void Serial_data_stream_parsing(UART_RX_TypeDef *frameData) {
  if (frameData == NULL) {
    // printf("Error: Invalid frameData pointer or size\n");
    return;
  }
  for (uint16_t i = 0; i < frameData->size - 1; i++) {
    if ((frameData->buffer[i] == FRAME_HEADER_BYTE1 &&
         i + 1 < frameData->size &&
         frameData->buffer[i + 1] == FRAME_HEADER_BYTE2) ||
        (frameData->buffer[i] == 0x6a && i + 1 < frameData->size &&
         frameData->buffer[i + 1] == 0xa6)) {
      for (uint16_t j = i + 2; j < frameData->size - 1; j++) {
        if (frameData->buffer[j] == FRAME_TAIL_BYTE1 &&
            j + 1 < frameData->size &&
            frameData->buffer[j + 1] == FRAME_TAIL_BYTE2) {
          uint16_t frame_size = j - i + 2;
          if (frame_size > UART_RX_BUFFER_SIZE) {
            // printf("Error: Frame size exceeds buffer limit\n");
            break;
          }
          // 计算CRC并校验
          uint16_t received_crc =
              (frameData->buffer[j - 2] | (frameData->buffer[j - 1] << 8));
          uint16_t calculated_crc = Calculate_CRC(
              &frameData->buffer[i],
              frame_size - 4); // 不包含CRC和帧尾
                               // HAL_GPIO_TogglePin(GPIOH, GPIO_PIN_14);
          if (calculated_crc == received_crc) {
            // 													for
            // (uint16_t i = 0; i < frameData->size; i++) {
            //     printf("%02X ", frameData->buffer[i]);
            // }
            // printf("\n");
             //screenack();
            command_parsing(&frameData->buffer[i]);
          } else {
            // printf("Error: CRC mismatch\n");
          }
          // 跳过已解析的帧数据，避免重复解析
          i = j + 1;
          break;
        }
      }
    }
  }
}
#endif

#define UART_PROTOCOL_STREAM_SIZE 512U

static uint8_t protocol_rx_stream[UART_PROTOCOL_STREAM_SIZE];
static size_t protocol_rx_length;

static uint8_t protocol_header_valid(const uint8_t *frame)
{
  return (uint8_t)(((frame[0] == 0x5AU) && (frame[1] == 0xA5U)) ||
                   ((frame[0] == 0x6AU) && (frame[1] == 0xA6U)));
}

static size_t protocol_expected_frame_size(const uint8_t *frame)
{
  return (frame[0] == 0x5AU) ? sizeof(CTRL_MSG) : sizeof(prepare_data);
}

static uint8_t protocol_frame_valid(const uint8_t *frame, size_t frame_size)
{
  uint16_t received_crc;
  uint16_t calculated_crc;

  if (!protocol_header_valid(frame) || frame[2] != frame_size ||
      frame[frame_size - 2U] != FRAME_TAIL_BYTE1 ||
      frame[frame_size - 1U] != FRAME_TAIL_BYTE2) {
    return 0U;
  }

  received_crc = (uint16_t)frame[frame_size - 4U] |
                 (uint16_t)((uint16_t)frame[frame_size - 3U] << 8U);
  calculated_crc = Calculate_CRC((uint8_t *)frame,
                                 (uint16_t)(frame_size - 4U));
  return (uint8_t)(received_crc == calculated_crc);
}

void Serial_data_stream_parsing(const uint8_t *data, size_t length)
{
  if (data == NULL || length == 0U) {
    return;
  }

  if (length > sizeof(protocol_rx_stream) - protocol_rx_length) {
    protocol_rx_length = 0U;
    if (length > sizeof(protocol_rx_stream)) {
      data += length - sizeof(protocol_rx_stream);
      length = sizeof(protocol_rx_stream);
    }
  }
  memcpy(&protocol_rx_stream[protocol_rx_length], data, length);
  protocol_rx_length += length;

  while (protocol_rx_length >= 3U) {
    size_t start = 0U;
    size_t frame_size;

    while (start + 1U < protocol_rx_length &&
           !protocol_header_valid(&protocol_rx_stream[start])) {
      ++start;
    }
    if (start > 0U) {
      memmove(protocol_rx_stream, &protocol_rx_stream[start],
              protocol_rx_length - start);
      protocol_rx_length -= start;
    }
    if (protocol_rx_length < 3U) {
      return;
    }

    frame_size = protocol_expected_frame_size(protocol_rx_stream);
    if (protocol_rx_stream[2] != frame_size) {
      memmove(protocol_rx_stream, &protocol_rx_stream[1],
              protocol_rx_length - 1U);
      protocol_rx_length--;
      continue;
    }
    if (protocol_rx_length < frame_size) {
      return;
    }

    if (protocol_frame_valid(protocol_rx_stream, frame_size)) {
      command_parsing(protocol_rx_stream);
      memmove(protocol_rx_stream, &protocol_rx_stream[frame_size],
              protocol_rx_length - frame_size);
      protocol_rx_length -= frame_size;
    } else {
      /* Skip one byte after a damaged frame so the next valid header can be
       * recovered even if it began inside the declared frame length. */
      memmove(protocol_rx_stream, &protocol_rx_stream[1],
              protocol_rx_length - 1U);
      protocol_rx_length--;
    }
  }
}

#define MAX_RETRIES 3 // 最大重试次数
#define TIMEOUT 1000  // 发送超时时间，单位为毫秒
#if 0
volatile uint8_t uart_tx_done = 1;

// 封装的函数
HAL_StatusTypeDef UART_TransmitWithRetry(UART_HandleTypeDef *huart,
                                         uint8_t *data, uint16_t size) {
		uart_tx_done = 0;  // 标志：开始发送
    SCB_CleanInvalidateDCache();
    HAL_Delay(20);
    HAL_StatusTypeDef status=HAL_UART_Transmit_DMA(huart, data, size);
		if (status != HAL_OK) {
        return status;  // 发送启动失败
    }
		 uint32_t tickstart = HAL_GetTick();
    while (uart_tx_done == 0) {
        if ((HAL_GetTick() - tickstart) > TIMEOUT) {
            return HAL_TIMEOUT;  // 超时未完成
        }
    }

    return HAL_OK;

}
#endif

#define UART_TX_TIMEOUT_MS 20U

static HAL_StatusTypeDef uart_transmit_raw(UART_HandleTypeDef *huart,
                                           uint8_t *data, uint16_t size)
{
  HAL_StatusTypeDef status = HAL_ERROR;

  if (huart == NULL || data == NULL || size == 0U) {
    return HAL_ERROR;
  }

  /* Protocol frames are only 11 or 13 bytes. Polling transmission completes
   * in about 1.1 ms at 115200 baud and avoids DMA buffer lifetime/cache
   * problems. A short retry only applies when the peripheral is temporarily
   * busy or a real timeout/error is reported. */
  for (uint8_t attempt = 0U; attempt < MAX_RETRIES; ++attempt) {
    status = HAL_UART_Transmit(huart, data, size, UART_TX_TIMEOUT_MS);
    if (status == HAL_OK) {
      return HAL_OK;
    }
    if (attempt + 1U < MAX_RETRIES) {
      HAL_Delay(1U);
    }
  }
  return status;
}

HAL_StatusTypeDef UART_TransmitWithRetry(UART_HandleTypeDef *huart,
                                         uint8_t *data, uint16_t size)
{
  if (size == 13U && data[0] == 0x5a && data[1] == 0xa5) {
    uint16_t cmd = ((uint16_t)data[3] << 8) | data[4];
    if (ScreenLink_IsTreatmentCommand(cmd)) {
      extern int minutes;
      float value; memcpy(&value, data + 5, 4);
      return ScreenLink_Request(cmd, value, (uint16_t)(minutes * 60)) ? HAL_OK : HAL_ERROR;
    }
  }
  return uart_transmit_raw(huart, data, size);
}

void screenstart(void) {
  static AXI_SRAM CTRL_MSG pData;
  pData.cmd_head_high = 0x5A;
  pData.cmd_head_low = 0xA5;
  pData.frame_length = 0x0d;
  pData.cmd_type_high = 0x10;
  pData.cmd_type_low = 0x50;
  pData.data = 1.0;
  pData.crc = Calculate_CRC((uint8_t *)&pData, sizeof(pData) - 4);
  pData.end_high = 0xFF;
  pData.end_low = 0xFF;
  (void)UART_TransmitWithRetry(&huart1, (uint8_t *)&pData, sizeof(pData));
}
void screenack(void) {
  static AXI_SRAM CTRL_MSG pData;
  pData.cmd_head_high = 0x5A;
  pData.cmd_head_low = 0xA5;
  pData.frame_length = 0x0d;
  pData.cmd_type_high = 0x10;
  pData.cmd_type_low = 0x52;
  pData.data = 1.0;
  pData.crc = Calculate_CRC((uint8_t *)&pData, sizeof(pData) - 4);
  pData.end_high = 0xFF;
  pData.end_low = 0xFF;
  UART_TransmitWithRetry(&huart1, (uint8_t *)&pData, sizeof(pData));
}

/* State rendering is idempotent: a repeated snapshot cannot start a timer,
 * send a command, consume an eye shield or increment a treatment counter. */
void screen_apply_system_state(const uint8_t *p, bool pending, uint8_t result)
{
  extern int minutes, seconds;
  static lv_obj_t *request_label;
  static uint8_t last_mode;
  const uint8_t state = p[12], mode = p[13];
  system_sample_flags = p[15];
  system_treatment_running = state == APP_STATE_RUNNING;
  EYE_status = (p[14] == APP_EYE_NEW || p[14] == APP_EYE_IN_USE || p[14] == APP_EYE_SERVICE);
  eye_state_received = 1U;
  battery_percent = p[34]; battery_percent_valid = 1;
  hot_temperature_tenths = auto_temperature_tenths = (int16_t)Sw_Get16(p + 38) / 10;
  /* Keep the last valid display value during transient sample invalidity. */
  if (system_sample_flags & 2U)
    pulsation_pressure = auto_pressure = Sw_Get16(p + 26) / 10U;
  const bool open = state == APP_STATE_PREHEAT || state == APP_STATE_READY || state == APP_STATE_RUNNING;
  backlight_treatment_open = open;
  const bool running = state == APP_STATE_RUNNING;
  HAL_TIM_Base_Stop_IT(&htim14);
  active_fault_code = Sw_Get16(p + 16);
  if (request_label == NULL) {
    request_label = lv_label_create(lv_layer_top());
    lv_obj_set_style_text_font(request_label, &ui_font_popup40, 0);
    lv_obj_align(request_label, LV_ALIGN_BOTTOM_MID, 0, -8);
  }
  /* Use only glyphs included in popup40; the former arbitrary text rendered
   * missing-glyph boxes on the embedded build. */
  const char *text = pending || result == SW_BUSY ? "..." : result == SW_OK ? "" :
      result == SW_STALE ? "请返回首页" : result == SW_NO_LINK ? "请检查主机连接" : "失败 请检查主机";
  if (ui_lang_get() == UI_LANG_EN) {
    text = pending || result == SW_BUSY ? "Please wait..." : result == SW_OK ? "" :
        result == SW_STALE ? "State changed" : result == SW_NO_LINK ? "Check link" : "Request rejected";
  }
  if (!pending && result == SW_OK && lv_scr_act() == ui_Factorymode &&
      (ScreenLink_LastCommand() == 0x9202 || ScreenLink_LastCommand() == 0x9203)) text = "OK";
  lv_label_set_text(request_label, text);
  if (!pending && result == SW_OK && !open) {
    if ((p[15] & 128U) && (lv_scr_act() == ui_setting || lv_scr_act() == ui_home_page))
      _ui_screen_change(&ui_Factorymode, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Factorymode_screen_init);
    else if (!(p[15] & 128U) && lv_scr_act() == ui_Factorymode)
      _ui_screen_change(&ui_home_page, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_home_page_screen_init);
  }
  if(ui_pressure_calibration_active()) {
    ui_pressure_calibration_state(state,(p[15]&128U)!=0,pending,result,auto_pressure);
    screen_fault_popup_tick();return;
  }
  if (open && mode >= 1 && mode <= 3) {
    if (mode == 1 && lv_scr_act() != ui_hotting)
      _ui_screen_change(&ui_hotting, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_hotting_screen_init);
    if (mode == 2 && lv_scr_act() != ui_pulsationning)
      _ui_screen_change(&ui_pulsationning, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_pulsationning_screen_init);
    if (mode == 3 && lv_scr_act() != ui_autoing)
      _ui_screen_change(&ui_autoing, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_autoing_screen_init);
    work_mode = mode; last_mode = mode;
    lv_obj_t *start = mode == 1 ? ui_Image70 : mode == 2 ? ui_Image74 : ui_Image75;
    lv_obj_t *stop = mode == 1 ? ui_Image5 : mode == 2 ? ui_Image22 : ui_Image55;
    lv_obj_t *shade = mode == 1 ? ui_Image53 : mode == 2 ? ui_Image56 : ui_Image57;
    lv_obj_t *manual = mode == 1 ? ui_Container10 : mode == 2 ? ui_Container4 : ui_Container7;
    lv_obj_t *natural = mode == 1 ? ui_Container3 : mode == 2 ? ui_Container5 : ui_Container8;
    lv_obj_add_flag(shade, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(manual, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(natural, LV_OBJ_FLAG_HIDDEN);
    if (running) lv_obj_add_flag(start, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(start, LV_OBJ_FLAG_HIDDEN);
    if (pending) lv_obj_clear_flag(start, LV_OBJ_FLAG_CLICKABLE);
    else lv_obj_add_flag(start, LV_OBJ_FLAG_CLICKABLE);
    if (running) lv_obj_add_flag(stop, LV_OBJ_FLAG_CLICKABLE);
    else lv_obj_clear_flag(stop, LV_OBJ_FLAG_CLICKABLE);
    bool aging = running && Sw_Get32(p + 20) == UINT32_MAX;
    uint32_t total_s = aging ? 0 : (Sw_Get32(p + 20) + 999U) / 1000U;
    minutes = (int)(total_s / 60U); seconds = (int)(total_s % 60U);
    lv_obj_t *min_label = mode == 1 ? ui_cut_d_l11 : mode == 2 ? ui_cut_d_l21 : ui_cut_d_l31;
    lv_obj_t *sec_label = mode == 1 ? ui_cut_d_l12 : mode == 2 ? ui_cut_d_l22 : ui_cut_d_l32;
    if (aging) {
      lv_label_set_text(min_label, "--");
      lv_label_set_text(sec_label, "--");
    } else {
      lv_label_set_text_fmt(min_label, "%02d", minutes);
      lv_label_set_text_fmt(sec_label, "%02d", seconds);
    }
  } else {
    work_mode = 0;
    if (last_mode) {
      bool current = (last_mode == 1 && lv_scr_act() == ui_hotting) ||
                     (last_mode == 2 && lv_scr_act() == ui_pulsationning) ||
                     (last_mode == 3 && lv_scr_act() == ui_autoing);
      if (current && !pending && result == SW_OK) {
        minutes = seconds = 0;
        lv_label_set_text(last_mode == 1 ? ui_cut_d_l11 : last_mode == 2 ? ui_cut_d_l21 : ui_cut_d_l31, "00");
        lv_label_set_text(last_mode == 1 ? ui_cut_d_l12 : last_mode == 2 ? ui_cut_d_l22 : ui_cut_d_l32, "00");
        lv_obj_t *shade = last_mode == 1 ? ui_Image53 : last_mode == 2 ? ui_Image56 : ui_Image57;
        lv_obj_t *manual = last_mode == 1 ? ui_Container10 : last_mode == 2 ? ui_Container4 : ui_Container7;
        lv_obj_t *natural = last_mode == 1 ? ui_Container3 : last_mode == 2 ? ui_Container5 : ui_Container8;
        lv_obj_clear_flag(shade, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(manual, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(natural, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(p[18] == APP_STOP_NATURAL ? natural : manual, LV_OBJ_FLAG_HIDDEN);
      }
      /* Render the end of this session once. Later idle snapshots must not
       * resurrect its popup after the user has entered the next preparation. */
      if (!pending || !current) last_mode = 0U;
    }
    if (result != SW_OK && !pending && (lv_scr_act() == ui_hotting ||
               lv_scr_act() == ui_pulsationning || lv_scr_act() == ui_autoing)) {
      _ui_screen_change(&ui_home_page, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_home_page_screen_init);
    }
  }
  screen_fault_popup_tick();
}

void screen_config_saved(void)
{
  extern uint32_t save_picture_time;
  if (ui_Image107) lv_obj_clear_flag(ui_Image107, LV_OBJ_FLAG_HIDDEN);
  save_picture_time = HAL_GetTick();
}

void screen_system_link_lost(void)
{
  ui_pressure_calibration_state(APP_STATE_IDLE,false,false,SW_NO_LINK,0);
  system_sample_flags = 0;
  HAL_TIM_Base_Stop_IT(&htim14);
  screen_fault_popup_tick();
}
