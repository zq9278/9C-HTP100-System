#include "ui_pressure_calibration.h"
#include "screen_link.h"
#include "system_state.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
LV_FONT_DECLARE(ui_font_calibration22);
static lv_obj_t *page,*gear,*fields[4],*keyboard,*message,*actual,*buttons[6];
static bool loaded, running, start_after_apply;
static unsigned selected;
static bool en(void){return ui_lang_get()==UI_LANG_EN;}
static void text(const char *zh,const char *english){lv_label_set_text(message,en()?english:zh);}
bool ui_pressure_calibration_active(void){return page && lv_scr_act()==page;}
static void keyboard_event(lv_event_t *e){if(lv_event_get_code(e)==LV_EVENT_READY || lv_event_get_code(e)==LV_EVENT_CANCEL)lv_obj_add_flag(keyboard,LV_OBJ_FLAG_HIDDEN);}
static void edit(lv_event_t *e){BEE1(e);lv_keyboard_set_textarea(keyboard,lv_event_get_target(e));lv_obj_clear_flag(keyboard,LV_OBJ_FLAG_HIDDEN);lv_obj_move_foreground(keyboard);}
static bool values(float *v){
 for(unsigned i=0;i<4;i++){char *end;const char *s=lv_textarea_get_text(fields[i]);v[i]=strtof(s,&end);if(end==s||*end||!isfinite(v[i]))return false;}
 return v[0]>=1&&v[0]<=65535&&v[1]>=1&&v[1]<=65535&&v[2]>=.1f&&v[2]<=1000&&v[3]>=.01f&&v[3]<=1;
}
static void request(unsigned op){
 float v[4]={0};if(op && (!loaded||!values(v))){text("参数超出范围","Invalid parameters");start_after_apply=false;return;}
 if(ScreenLink_RequestCalibration(op,selected,v)){text("正在处理","Please wait");}
 else {start_after_apply=false;text("连接异常或正在处理","Link unavailable or busy");}
}
static void change_gear(lv_event_t *e){(void)e;selected=lv_dropdown_get_selected(gear);loaded=false;start_after_apply=false;for(unsigned i=0;i<4;i++)lv_textarea_set_text(fields[i],"");request(0);}
static void click(lv_event_t *e){
 BEE1(e);unsigned op=(unsigned)(uintptr_t)lv_event_get_user_data(e);
 lv_obj_add_flag(keyboard,LV_OBJ_FLAG_HIDDEN);
 if(op==0){if(!running&&!ScreenLink_Pending())lv_scr_load(ui_Factorymode);}
 else if(op==1)request(0);
 else if(op==2)request(1);
 else if(op==3)request(2);
 else if(op==4){start_after_apply=true;request(1);}
 else if(op==5){start_after_apply=false;(void)ScreenLink_Request(0x1038,0,0);}
}
static lv_obj_t *label(lv_obj_t *parent,const char *s,int x,int y,int width){lv_obj_t *l=lv_label_create(parent);lv_label_set_text(l,s);lv_obj_set_pos(l,x,y);lv_obj_set_width(l,width);return l;}
static lv_obj_t *button(const char *s,int x,int y,int width,unsigned action){lv_obj_t *b=lv_btn_create(page);lv_obj_set_pos(b,x,y);lv_obj_set_size(b,width,45);lv_obj_t *l=lv_label_create(b);lv_label_set_text(l,s);lv_obj_center(l);lv_obj_add_event_cb(b,click,LV_EVENT_CLICKED,(void*)(uintptr_t)action);return b;}
void ui_pressure_calibration_open(lv_event_t *e){
 BEE1(e);if(ScreenLink_Pending())return;
 if(page)lv_obj_del(page);
 page=lv_obj_create(NULL);lv_obj_clear_flag(page,LV_OBJ_FLAG_SCROLLABLE);
 lv_obj_set_style_bg_color(page,lv_color_hex(0x101c2d),0);lv_obj_set_style_text_color(page,lv_color_white(),0);
 lv_obj_set_style_text_font(page,&ui_font_calibration22,0);
 label(page,en()?"Pressure calibration":"压力校准",20,14,280);
 buttons[0]=button(en()?"Back":"返回",355,8,105,0);
 gear=lv_dropdown_create(page);lv_obj_set_pos(gear,20,62);lv_obj_set_size(gear,200,45);
 lv_dropdown_set_options(gear,"150 mmHg\n250 mmHg\n350 mmHg\n450 mmHg\n550 mmHg");
 lv_dropdown_set_symbol(gear,"v");
 lv_dropdown_set_selected(gear,0);selected=0;loaded=false;running=false;start_after_apply=false;
 lv_obj_add_event_cb(gear,change_gear,LV_EVENT_VALUE_CHANGED,NULL);
 buttons[1]=button(en()?"Read":"读取",235,62,105,1);
 const char *zh[]={"前进接近速度上限","回退速度上限","比例增益 Kp","减速比例"};
 const char *english[]={"Forward speed max","Retract speed max","Kp","Slowdown ratio"};
 for(unsigned i=0;i<4;i++){
   label(page,en()?english[i]:zh[i],20,127+57*i,245);
   fields[i]=lv_textarea_create(page);lv_obj_set_pos(fields[i],275,115+57*i);lv_obj_set_size(fields[i],185,46);
   lv_textarea_set_one_line(fields[i],true);lv_textarea_set_text(fields[i],"");lv_textarea_set_accepted_chars(fields[i],"0123456789.");lv_textarea_set_max_length(fields[i],9);
   lv_obj_add_event_cb(fields[i],edit,LV_EVENT_CLICKED,NULL);
 }
 label(page,en()?"Speed 1..65535; Kp 0.1..1000\nRatio 0.01..1; saves current profile":"速度1~65535  Kp 0.1~1000\n减速比0.01~1 保存仅覆盖当前挡",20,350,440);
 buttons[2]=button(en()?"Apply":"临时应用",20,412,210,2);
 buttons[3]=button(en()?"Save to host":"保存到主机",250,412,210,3);
 buttons[4]=button(en()?"Start squeeze":"开始挤压",20,470,210,4);
 buttons[5]=button(en()?"Stop":"停止",250,470,210,5);
 actual=label(page,"",20,531,440);message=label(page,"",20,567,440);
 keyboard=lv_keyboard_create(page);lv_obj_set_style_text_font(keyboard,LV_FONT_DEFAULT,LV_PART_ITEMS);lv_keyboard_set_mode(keyboard,LV_KEYBOARD_MODE_NUMBER);lv_obj_set_size(keyboard,480,250);lv_obj_align(keyboard,LV_ALIGN_BOTTOM_MID,0,0);
 lv_obj_add_event_cb(keyboard,keyboard_event,LV_EVENT_ALL,NULL);lv_obj_add_flag(keyboard,LV_OBJ_FLAG_HIDDEN);
 lv_scr_load(page);request(0);
}
void screen_calibration_result(uint8_t status,uint8_t op,uint8_t index,bool saved,const float *v){
 if(!ui_pressure_calibration_active()||index!=selected)return;
 if(status || !v){start_after_apply=false;if(op==0)loaded=false;text("操作失败 请检查连接或参数","Failed: check link / parameters");return;}
 loaded=true;
 /* nano libc does not link printf float support on the MCU. Format the
  * protocol's fixed precision using integer conversions instead. */
 for(unsigned i=0;i<4;i++){
   char s[24];unsigned scale=i<2?1U:i==2?10U:1000U;
   unsigned value=(unsigned)lroundf(v[i]*(float)scale);
   if(i<2)snprintf(s,sizeof(s),"%u",value);
   else snprintf(s,sizeof(s),i==2?"%u.%01u":"%u.%03u",value/scale,value%scale);
   lv_textarea_set_text(fields[i],s);
 }
 if(op==2)text("保存成功 已读回校验","Saved and verified");
 else if(op==1)text("临时生效 未保存","Applied temporarily; not saved");
 else if(saved)text("已读取当前参数 存在保存记录","Read current profile; saved table exists");
 else text("已读取默认参数 无保存记录","Read defaults; no saved record");
 if(start_after_apply&&op==1){start_after_apply=false;if(!ScreenLink_Request(0x9204,150+100*selected,0))text("启动失败","Start failed");}
}
void ui_pressure_calibration_state(uint8_t state,bool authorized,bool pending,uint8_t result,uint16_t pressure){
 if(!ui_pressure_calibration_active())return;
 running=state==APP_STATE_RUNNING;bool idle=state==APP_STATE_IDLE;bool ready=authorized&&ScreenLink_Healthy()&&!pending;
 bool enabled[]={!running&&!pending,ready,ready&&loaded&&(idle||running),ready&&loaded&&idle,ready&&loaded&&idle,running&&!pending};
 for(unsigned i=0;i<6;i++){if(enabled[i])lv_obj_clear_state(buttons[i],LV_STATE_DISABLED);else lv_obj_add_state(buttons[i],LV_STATE_DISABLED);}
 if(pending||running)lv_obj_add_state(gear,LV_STATE_DISABLED);else lv_obj_clear_state(gear,LV_STATE_DISABLED);
 lv_label_set_text_fmt(actual,en()?"Pressure: %u mmHg":"当前压力: %u mmHg",pressure);
 if(!authorized||!ScreenLink_Healthy()){loaded=false;text("工程权限或连接已失效","Engineering mode / link unavailable");}
 else if(!pending&&result && ScreenLink_LastCommand()==0x9204)text("启动失败 请检查主机","Start rejected; check host");
}
