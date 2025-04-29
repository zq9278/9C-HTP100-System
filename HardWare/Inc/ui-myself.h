#ifndef uiself
#define uiself
#include "ui.h"
extern lv_obj_t * global_panel;  // 确保只有一个 panel 实例
void create_global_panel();
#endif 