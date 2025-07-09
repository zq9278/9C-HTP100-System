#include "screen.h"
#include "ui.h"
#include "ui-myself.h"
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

uint16_t prepare_temprature, prepare_crimp, prepare_time, bee,
    prepare_set; // 准�?�温度，准�?�挤压�?�数，准备时间，蜂鸣�??，准备�?�置123
uint8_t work_finish = 0; // 0 没结束，1 工作完成
extern uint8_t usart1_tx;
extern UART_HandleTypeDef huart1;
void work_flinsh(void) {
  if (work_finish) {
	work_finish=0;
    HAL_TIM_Base_Stop_IT(&htim14);
    if (work_mode != 0) {
      static AXI_SRAM CTRL_MSG pData_time_finish;
      pData_time_finish.cmd_head_high = 0x5A;
      pData_time_finish.cmd_head_low = 0xA5;
      pData_time_finish.frame_length = 0x0d;
      pData_time_finish.cmd_type_high = 0x89;
      pData_time_finish.cmd_type_low = 0x00;
      pData_time_finish.data = 1.0;
      pData_time_finish.crc = Calculate_CRC((uint8_t *)&pData_time_finish,
                                            sizeof(pData_time_finish) - 4);
      pData_time_finish.end_high = 0xFF;
      pData_time_finish.end_low = 0xFF;
      UART_TransmitWithRetry(&huart1, (uint8_t *)&pData_time_finish,
                             sizeof(pData_time_finish));
      static AXI_SRAM CTRL_MSG pData;
      pData.cmd_head_high = 0x5A;
      pData.cmd_head_low = 0xA5;
      pData.frame_length = 0x0d;
      pData.cmd_type_high = 0x10;
      // 根据工作模式发送不同的命令
      switch (work_mode) {
      case 1:
        lv_obj_clear_flag(ui_Image53,
                          LV_OBJ_FLAG_HIDDEN); // 显示工作模式1相关UI元素
        lv_obj_clear_flag(ui_Container3, LV_OBJ_FLAG_HIDDEN);
        pData.cmd_type_low = 0x30;
        break;
      case 2:
        lv_obj_clear_flag(ui_Image56,
                          LV_OBJ_FLAG_HIDDEN); // 显示工作模式2相关UI元素
        lv_obj_clear_flag(ui_Container5, LV_OBJ_FLAG_HIDDEN);
        pData.cmd_type_low = 0x34;
        break;
      case 3:
        lv_obj_clear_flag(ui_Image57,
                          LV_OBJ_FLAG_HIDDEN); // 显示工作模式3相关UI元素
        lv_obj_clear_flag(ui_Container8, LV_OBJ_FLAG_HIDDEN);
        pData.cmd_type_low = 0x38;
        break;
      default:
        break;
      }
      pData.crc = Calculate_CRC((uint8_t *)&pData, sizeof(pData) - 4);
      pData.end_high = 0xFF;
      pData.end_low = 0xFF;
      UART_TransmitWithRetry(&huart1, (uint8_t *)&pData, sizeof(pData));
			UART_TransmitWithRetry(&huart1, (uint8_t *)&pData, sizeof(pData));
    }
  }
}

void command_parsing(
    UART_RX_TypeDef *received_data) { // 区分调试命令和屏幕工作命�??
  // �??认帧尾是否合�??
  // if (received_data->buffer[received_data->length - 2] != 0xFF ||
  //     received_data->buffer[received_data->length - 1] != 0xFF) {
  //   return; // 非法帧，直接返回
  // }
  // 提取命令类型（高字节和低字节�??
  uint16_t cmd_type =
      (received_data->buffer[0] << 8) |
      received_data->buffer[1]; // �??一位和�??二位判断�??不是调试参数
  // 解析命令
  switch (cmd_type) {
  case 0x5aa5: { // 命令类型 0x9000 - 处理 UART1 数据
    processData((PCTRL_MSG)received_data->buffer); // process data from uart
    break;
  }
  case 0x6aa6: { // 命令类型 0x6aa6 - 处理 UART1 数据
    processData_prepare(
        (prepare_data_p)received_data->buffer); // process data from uart
    HAL_GPIO_TogglePin(GPIOH, GPIO_PIN_15);
    break;
  }
  default:
    // �??知命令，忽略
    break;
  }
}

void processData(PCTRL_MSG msg) {
  uint16_t cmd_head = ((msg->cmd_head_high) << 8) |
                      (msg->cmd_head_low); // 将两�??8位值合并成一�??16位�?
  uint16_t cmd_type = ((msg->cmd_type_high) << 8) |
                      (msg->cmd_type_low); // 将两�??8位值合并成一�??16位�?
  // uint16_t data = ((msg->data_high) << 8) | (msg->data_low); // 指令类型
  float_t data = msg->data;
  switch (cmd_type) {

  case 0x2041:                                       // 开始热�??
    lv_label_set_text_fmt(ui_Label30, "%.1f", data); // 显示温度
    break;
  case 0x2005: // 开始脉�??
    // int data_int = data;
    lv_label_set_text_fmt(ui_Label32, "%03d", (int)data); // 显示压力
     // 根据压力值设置颜色
    if (data > 247.66) {
        lv_obj_set_style_text_color(ui_Label32, lv_color_make(255, 0, 0), 0); // 红色
    } else {
        lv_obj_set_style_text_color(ui_Label32, lv_color_make(255, 255, 255), 0); // 白色
    }
    break;
  case 0x2037:                                       // �??动开始热�??
    lv_label_set_text_fmt(ui_Label39, "%.1f", data); // 显示温度
    break;
  case 0x2047: // �??动开始脉�??
               // int data_int = data;
    lv_label_set_text_fmt(ui_Label46, "%03d", (int)data); // 显示压力
     // 根据压力值设置颜色
    if (data > 247.66) {
        lv_obj_set_style_text_color(ui_Label46, lv_color_make(255, 0, 0), 0); // 红色
    } else {
        lv_obj_set_style_text_color(ui_Label46, lv_color_make(255, 255, 255), 0); // 白色
    }
    break;
  case 0x2050: // 跟新电池
               // sprintf(buffer, "%d%%", (int)data);
    lv_slider_set_value(ui_Slider1, (int)data, LV_ANIM_ON);
    lv_label_set_text_fmt(ui_Label5, "%d%%", (int)data); // 电池图标数显
    // lv_label_ins_text(ui_Label5, 0, buffer);
    lv_slider_set_value(ui_Slider2, (int)data, LV_ANIM_ON);
    lv_slider_set_value(ui_Slider3, (int)data, LV_ANIM_ON);
    lv_slider_set_value(ui_Slider9, (int)data, LV_ANIM_ON);
    lv_slider_set_value(ui_Slider5, (int)data, LV_ANIM_ON);
    lv_slider_set_value(ui_Slider6, (int)data, LV_ANIM_ON);
    lv_slider_set_value(ui_Slider7, (int)data, LV_ANIM_ON);
    lv_slider_set_value(ui_Slider8, (int)data, LV_ANIM_ON);
    break;
  case 0x2051: // 主机发送停�??            
    HAL_TIM_Base_Stop_IT(&htim14);
    switch (work_mode) {
    case 1: // �??敷状态下点击的停�??
      lv_obj_clear_flag(ui_Image53, LV_OBJ_FLAG_HIDDEN);     // 打开阴影
      lv_obj_clear_flag(ui_Container10, LV_OBJ_FLAG_HIDDEN); // 打开阴影//3
      break;
    case 2: // 脉动状态下点击的停�??
      lv_obj_clear_flag(ui_Image56, LV_OBJ_FLAG_HIDDEN);    // 打开阴影
      lv_obj_clear_flag(ui_Container4, LV_OBJ_FLAG_HIDDEN); // 打开阴影//5
      break;
    case 3: // �??动状态下点击的停�??
      lv_obj_clear_flag(ui_Image57, LV_OBJ_FLAG_HIDDEN);    // 打开阴影
      lv_obj_clear_flag(ui_Container7, LV_OBJ_FLAG_HIDDEN); // 打开阴影//8
      break;

    default:
      break;
    }
    work_mode = 0;
    break;
		case 0x2056: // 眼盾拔出主机发送停          
    HAL_TIM_Base_Stop_IT(&htim14);
		break;
		case 0x2057: // 检测新眼盾 
		{lv_obj_t * current_screen1 = lv_scr_act();
    // 这些界面不创建 global_panel
    if (!(current_screen1 == ui_home_page ) )
		{
			_ui_screen_change(&ui_home_page, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_home_page_screen_init);
    }		}
		break;
  case 0x2052: // 主机发送开�??
    HAL_TIM_Base_Start_IT(&htim14);
    switch (work_mode) {
    case 1: // �??敷状态下点击的停�??
      lv_obj_add_flag(ui_Image70,
                      LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_ADV_HITTEST); /// 隐藏
      lv_obj_add_flag(ui_Image5, LV_OBJ_FLAG_CLICKABLE);
      break;
    case 2: // 脉动状态下点击的停�??
      lv_obj_add_flag(ui_Image74,
                      LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_ADV_HITTEST); /// 隐藏
      lv_obj_add_flag(ui_Image22, LV_OBJ_FLAG_CLICKABLE);
      break;
    case 3: // �??动状态下点击的停�??
      lv_obj_add_flag(ui_Image75,
                      LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_ADV_HITTEST); /// 隐藏
      lv_obj_add_flag(ui_Image55, LV_OBJ_FLAG_CLICKABLE);
      break;

    default:
      break;
    }

    break;
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
   case 0x2055:
        
  //if(global_panel != NULL){
	 if (data == 1.0) {
        // **只有当 global_panel 不是隐藏状态时，才执行隐藏**
     //   if (!lv_obj_has_flag(global_panel, LV_OBJ_FLAG_HIDDEN)) {
    //        lv_obj_add_flag(global_panel, LV_OBJ_FLAG_HIDDEN);
		//		
    //    }
        EYE_status=1;
    } else {
			work_mode=0;
			create_global_panel();
			EYE_status=0;
			 lv_obj_t * current_screen = lv_scr_act();

    // 这些界面不创建 global_panel
    if (!(current_screen == ui_Power_on_interface ||
        current_screen == ui_home_page ||
        current_screen == ui_test ||
        current_screen == ui_setting ||
        current_screen == ui_pre_setting ||
        current_screen == ui_set_value ||
        current_screen == ui_bee_setting ||
        current_screen == ui_count ||
		current_screen == ui_dev_model ||
        current_screen == ui_count_model ||
        current_screen == ui_Factorymode) )
		{
			//不在这些界面下时清除，打开显示
			if(lv_obj_has_flag(global_panel, LV_OBJ_FLAG_HIDDEN)) { // **只有当 global_panel 处于隐藏状态时，才执行显示**
            lv_obj_clear_flag(global_panel, LV_OBJ_FLAG_HIDDEN);
        }
    }
       
	}
	
//	}
    break;
  default:
    break;
  }
}
void processData_prepare(prepare_data_p msg) {
  uint16_t cmd_head = ((msg->cmd_head_high) << 8) |
                      (msg->cmd_head_low); // 将两�??8位值合并成一�??16位�?
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
  case 0x00A3: // 选择预�?��?
    prepare_set = data;
    switch (prepare_set) {
    case 0:
      // 				    lv_obj_set_style_img_recolor_opa(ui_Image97,
      // 170, LV_PART_MAIN | LV_STATE_DEFAULT);
      // lv_obj_set_style_img_recolor_opa(ui_Image100, 170, LV_PART_MAIN |
      // LV_STATE_DEFAULT); lv_obj_set_style_img_recolor_opa(ui_Image99, 170,
      // LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_bg_opa(
          ui_Panel16, 220,
          LV_PART_MAIN); // 设置背景透明度 (0~255或LV_OPA_0~LV_OPA_100)
      lv_obj_set_style_bg_opa(
          ui_Panel17, 220,
          LV_PART_MAIN); // 设置背景透明度 (0~255或LV_OPA_0~LV_OPA_100)
      lv_obj_set_style_bg_opa(
          ui_Panel18, 220,
          LV_PART_MAIN); // 设置背景透明度 (0~255或LV_OPA_0~LV_OPA_100)
      lv_obj_add_flag(ui_Image24,
                      LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_ADV_HITTEST); /// Flags
      prepare_time = 1;
      prepare_crimp = 150;

      break;
    case 1:
      // 				     lv_obj_set_style_img_recolor_opa(ui_Image97,
      // 0, LV_PART_MAIN | LV_STATE_DEFAULT);
      // lv_obj_set_style_img_recolor_opa(ui_Image100, 170, LV_PART_MAIN |
      // LV_STATE_DEFAULT); lv_obj_set_style_img_recolor_opa(ui_Image99, 170,
      // LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_bg_opa(
          ui_Panel16, 0,
          LV_PART_MAIN); // 设置背景透明度 (0~255或LV_OPA_0~LV_OPA_100)
      lv_obj_set_style_bg_opa(
          ui_Panel17, 220,
          LV_PART_MAIN); // 设置背景透明度 (0~255或LV_OPA_0~LV_OPA_100)
      lv_obj_set_style_bg_opa(
          ui_Panel18, 220,
          LV_PART_MAIN); // 设置背景透明度 (0~255或LV_OPA_0~LV_OPA_100)
      lv_obj_clear_flag(ui_Image24, LV_OBJ_FLAG_HIDDEN); /// Flags
      lv_textarea_set_text(ui_TextArea29, "设定一");
      break;
    case 2:
      // 				lv_obj_set_style_img_recolor_opa(ui_Image97,
      // 170, LV_PART_MAIN | LV_STATE_DEFAULT);
      // lv_obj_set_style_img_recolor_opa(ui_Image100, 0, LV_PART_MAIN |
      // LV_STATE_DEFAULT); lv_obj_set_style_img_recolor_opa(ui_Image99, 170,
      // LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_bg_opa(
          ui_Panel16, 220,
          LV_PART_MAIN); // 设置背景透明度 (0~255或LV_OPA_0~LV_OPA_100)
      lv_obj_set_style_bg_opa(
          ui_Panel17, 0,
          LV_PART_MAIN); // 设置背景透明度 (0~255或LV_OPA_0~LV_OPA_100)
      lv_obj_set_style_bg_opa(
          ui_Panel18, 220,
          LV_PART_MAIN); // 设置背景透明度 (0~255或LV_OPA_0~LV_OPA_100)
      lv_obj_clear_flag(ui_Image24, LV_OBJ_FLAG_HIDDEN); /// Flags
      lv_textarea_set_text(ui_TextArea29, "设定二");
      break;
    case 3:
      // 				lv_obj_set_style_img_recolor_opa(ui_Image97,
      // 170, LV_PART_MAIN | LV_STATE_DEFAULT);
      // lv_obj_set_style_img_recolor_opa(ui_Image100, 170, LV_PART_MAIN |
      // LV_STATE_DEFAULT); lv_obj_set_style_img_recolor_opa(ui_Image99, 0,
      // LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_bg_opa(
          ui_Panel16, 220,
          LV_PART_MAIN); // 设置背景透明度 (0~255或LV_OPA_0~LV_OPA_100)
      lv_obj_set_style_bg_opa(
          ui_Panel17, 220,
          LV_PART_MAIN); // 设置背景透明度 (0~255或LV_OPA_0~LV_OPA_100)
      lv_obj_set_style_bg_opa(
          ui_Panel18, 0,
          LV_PART_MAIN); // 设置背景透明度 (0~255或LV_OPA_0~LV_OPA_100)
      lv_obj_clear_flag(ui_Image24, LV_OBJ_FLAG_HIDDEN); /// Flags
      lv_textarea_set_text(ui_TextArea29, "设定三");
      break;
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
	case 0x00b0: // 
    lv_label_set_text_fmt(ui_Label1, "%d", (int)data); // 显示加热温度次数
    break;
  case 0x00A7: // bee
    bee = data;
    if (bee == 1) {
      lv_obj_add_flag(ui_Image113,
                      LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_ADV_HITTEST); /// Flags
      lv_obj_clear_flag(ui_Image95, LV_OBJ_FLAG_HIDDEN);
      HAL_GPIO_WritePin(Bee_vcc_GPIO_Port, Bee_vcc_Pin, GPIO_PIN_SET);

    } else {
      lv_obj_add_flag(ui_Image95,
                      LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_ADV_HITTEST); /// Flags
      lv_obj_clear_flag(ui_Image113, LV_OBJ_FLAG_HIDDEN);
      HAL_GPIO_WritePin(Bee_vcc_GPIO_Port, Bee_vcc_Pin, GPIO_PIN_RESET);
    }
    break;
  default:
    break;
  }
}

/* 环形队列入队操作 */
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

// void Serial_data_stream_parsing(UART_RX_TypeDef *frameData) {
//     if (frameData == NULL) return;

//     uint16_t i = 0;
//     while (i < frameData->size - 1) {
//         if ((frameData->buffer[i] == FRAME_HEADER_BYTE1 &&
//         frameData->buffer[i + 1] == FRAME_HEADER_BYTE2) ||
//             (frameData->buffer[i] == 0x6A && frameData->buffer[i + 1] ==
//             0xA6)) {

//             uint16_t frame_start = i;
//             uint8_t frame_found = 0;

//             for (uint16_t j = i + 2; j < frameData->size - 1; j++) {
//                 if (frameData->buffer[j] == FRAME_TAIL_BYTE1 &&
//                 frameData->buffer[j + 1] == FRAME_TAIL_BYTE2) {
//                     uint16_t frame_size = j - frame_start + 2;

//                     if (frame_size > UART_RX_BUFFER_SIZE) break;

//                     uint16_t received_crc = (frameData->buffer[j - 2] |
//                     (frameData->buffer[j - 1] << 8)); uint16_t calculated_crc
//                     = Calculate_CRC(&frameData->buffer[frame_start],
//                     frame_size - 4);

//                     if (calculated_crc == received_crc) {

//                         // 打印帧数据
//                         printf("帧解析成功: ");
//                         for (uint16_t k = frame_start; k <= j + 1; k++) {
//                             printf("%02X ", frameData->buffer[k]);
//                         }
//                         printf("\n");

//                         // 命令解析
//                         command_parsing(&frameData->buffer[frame_start]);

//                         // 跳过当前帧
//                         i = j + 2; // 直接跳到帧尾之后
//                         frame_found = 1;
//                         break;
//                     } else {
//                         printf("Error: CRC mismatch\n");
//                         i = j + 2; // 跳过错误帧
//                         frame_found = 1;
//                         break;
//                     }
//                 }
//             }

//             // 若未找到帧尾，则退出避免死循环
//             if (!frame_found) break;

//         } else {
//             i++;  // 当前不是帧头，继续搜索
//         }
//     }
// }

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

#define MAX_RETRIES 3 // 最大重试次数
#define TIMEOUT 1000  // 发送超时时间，单位为毫秒
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
  UART_TransmitWithRetry(&huart1, (uint8_t *)&pData, sizeof(pData));
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

/*

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define FRAME_HEADER_BYTE1 0x5A
#define FRAME_HEADER_BYTE2 0xA5
#define FRAME_TAIL_BYTE1   0xFF
#define FRAME_TAIL_BYTE2   0xFF
#define MAX_FRAME_SIZE     256
#define MAX_RETRY          3
#define ACK_CMD            0xFFFF
#define NACK_CMD           0xFFFE
#define ACK_TIMEOUT_MS     1000  // 超时时间 (ms)

// 数据类型
typedef enum {
    DATA_TYPE_INT32 = 0x01,
    DATA_TYPE_FLOAT = 0x02,
    DATA_TYPE_STRING = 0x03,
    DATA_TYPE_UINT16 = 0x04
} DataType_t;

// 状态机状态
typedef enum {
    WAIT_HEADER,
    READ_HEADER,
    READ_DATA,
    VERIFY_CRC,
    FRAME_DONE
} FrameState_t;

// 接收解析结构体
typedef struct {
    uint8_t buffer[MAX_FRAME_SIZE];
    uint16_t index;
    uint8_t data_length;
    FrameState_t state;
} FrameParser_t;

// CRC16 (Modbus) 计算
uint16_t Calculate_CRC(uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            crc = (crc & 0x0001) ? (crc >> 1) ^ 0xA001 : (crc >> 1);
        }
    }
    return crc;
}

// 模拟串口发送
void UART_SendBytes(uint8_t *data, uint16_t length) {
    printf("🚀 发送: ");
    for (uint16_t i = 0; i < length; i++) printf("%02X ", data[i]);
    printf("\n");
}

// 生成ACK帧
void send_ack(uint16_t cmd) {
    uint8_t ack_frame[10] = {
        FRAME_HEADER_BYTE1, FRAME_HEADER_BYTE2,
        (uint8_t)(cmd & 0xFF), (uint8_t)((cmd >> 8) & 0xFF),
        0x00, 0x00,  // 无数据
        0x00, 0x00,  // CRC占位
        FRAME_TAIL_BYTE1, FRAME_TAIL_BYTE2
    };
    uint16_t crc = Calculate_CRC(ack_frame, 6);
    ack_frame[6] = crc & 0xFF;
    ack_frame[7] = (crc >> 8) & 0xFF;

    UART_SendBytes(ack_frame, sizeof(ack_frame));
}

// 模拟等待ACK (接收模拟)
int wait_for_ack(uint16_t expected_cmd) {
    clock_t start = clock();
    while ((clock() - start) * 1000 / CLOCKS_PER_SEC < ACK_TIMEOUT_MS) {
        // 模拟接收ACK成功
        return 1;
    }
    return 0;  // 超时
}

// 发送数据并等待ACK
void send_with_ack(uint8_t *frame, uint16_t length, uint16_t cmd) {
    int retry = 0;
    while (retry < MAX_RETRY) {
        UART_SendBytes(frame, length);
        printf("⏳ 等待ACK...\n");

        if (wait_for_ack(cmd)) {
            printf("✅ 收到ACK！\n");
            return;
        }

        retry++;
        printf("⚠️ 未收到ACK，重试 (%d/%d)\n", retry, MAX_RETRY);
    }
    printf("❌ 超过最大重试次数，发送失败！\n");
}

// 多帧粘连解析
void Serial_data_stream_parsing(FrameParser_t *parser, uint8_t *stream, uint16_t
length) { for (uint16_t i = 0; i < length; i++) { uint8_t byte = stream[i];
        switch (parser->state) {
            case WAIT_HEADER:
                if (byte == FRAME_HEADER_BYTE1) {
                    parser->buffer[0] = byte;
                    parser->index = 1;
                    parser->state = READ_HEADER;
                }
                break;

            case READ_HEADER:
                parser->buffer[parser->index++] = byte;
                if (parser->index == 2) {
                    parser->state = (byte == FRAME_HEADER_BYTE2) ? READ_DATA :
WAIT_HEADER;
                }
                break;

            case READ_DATA:
                parser->buffer[parser->index++] = byte;
                if (parser->index == 5) {
                    parser->data_length = parser->buffer[4];
                    if (parser->data_length > (MAX_FRAME_SIZE - 9))
parser->state = WAIT_HEADER;
                }
                if (parser->index >= (6 + parser->data_length + 4))
parser->state = VERIFY_CRC; break;

            case VERIFY_CRC: {
                uint16_t received_crc = parser->buffer[parser->index - 4] |
(parser->buffer[parser->index - 3] << 8); uint16_t calculated_crc =
Calculate_CRC(parser->buffer, parser->index - 4);

                if (received_crc == calculated_crc &&
                    parser->buffer[parser->index - 2] == FRAME_TAIL_BYTE1 &&
                    parser->buffer[parser->index - 1] == FRAME_TAIL_BYTE2) {
                    parser->state = FRAME_DONE;
                } else {
                    printf("❌ CRC错误，丢弃帧\n");
                    parser->state = WAIT_HEADER;
                }
                break;
            }

            case FRAME_DONE: {
                uint16_t cmd = parser->buffer[2] | (parser->buffer[3] << 8);
                printf("✅ 已接收命令: 0x%04X\n", cmd);
                send_ack(cmd);  // 发送ACK
                parser->state = WAIT_HEADER;
                parser->index = 0;
                break;
            }
        }
    }
}

// 构建数据帧
uint16_t build_frame(uint8_t *frame, uint16_t cmd, uint8_t data_type, uint8_t
*data, uint8_t data_len) { frame[0] = FRAME_HEADER_BYTE1; frame[1] =
FRAME_HEADER_BYTE2; frame[2] = cmd & 0xFF; frame[3] = (cmd >> 8) & 0xFF;
    frame[4] = data_len;
    frame[5] = data_type;
    memcpy(&frame[6], data, data_len);

    uint16_t crc = Calculate_CRC(frame, 6 + data_len);
    frame[6 + data_len] = crc & 0xFF;
    frame[7 + data_len] = (crc >> 8) & 0xFF;
    frame[8 + data_len] = FRAME_TAIL_BYTE1;
    frame[9 + data_len] = FRAME_TAIL_BYTE2;

    return 10 + data_len - 1;
}

// 主函数测试
int main() {
    FrameParser_t parser = {0};
    parser.state = WAIT_HEADER;

    uint8_t data1[] = {0x00, 0x01};  // uint16: 1
    uint8_t data2[] = {0x00, 0x02};  // uint16: 2

    uint8_t frame1[MAX_FRAME_SIZE], frame2[MAX_FRAME_SIZE];
    uint16_t len1 = build_frame(frame1, 0x0101, DATA_TYPE_UINT16, data1,
sizeof(data1)); uint16_t len2 = build_frame(frame2, 0x0102, DATA_TYPE_UINT16,
data2, sizeof(data2));

    // 发送并等待ACK
    send_with_ack(frame1, len1, 0x0101);
    send_with_ack(frame2, len2, 0x0102);

    printf("\n📥 模拟粘连接收:\n");
    uint8_t combined_stream[MAX_FRAME_SIZE * 2];
    memcpy(combined_stream, frame1, len1);
    memcpy(combined_stream + len1, frame2, len2);

    Serial_data_stream_parsing(&parser, combined_stream, len1 + len2);

    return 0;
}

*/