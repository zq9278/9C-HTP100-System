#include "ui.h"
#include "ui-myself.h"

lv_obj_t * global_panel;
lv_obj_t * global_panel = NULL;  // 确保只有一个 panel 实例

void create_global_panel() {
	   // 获取当前活动界面
    lv_obj_t * current_screen = lv_scr_act();


    if (global_panel != NULL) {
        return;  // 已经创建，不再重复创建
    }

    global_panel = lv_obj_create(lv_layer_top());  // 确保添加到最顶层
    lv_obj_set_width(global_panel, 480);
    lv_obj_set_height(global_panel, 640);
    lv_obj_set_align(global_panel, LV_ALIGN_CENTER);
    lv_obj_add_flag(global_panel, LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_clear_flag(global_panel, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(global_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(global_panel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(global_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(global_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Image82 = lv_img_create(global_panel);
    lv_img_set_src(ui_Image82, &ui_img_black_png);
    lv_obj_set_width(ui_Image82, LV_SIZE_CONTENT);   /// 480
    lv_obj_set_height(ui_Image82, LV_SIZE_CONTENT);    /// 640
    lv_obj_set_align(ui_Image82, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Image82, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_Image82, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_Image81 = lv_img_create(global_panel);
    lv_img_set_src(ui_Image81, &ui_img_woring_png);
    lv_obj_set_width(ui_Image81, LV_SIZE_CONTENT);   /// 424
    lv_obj_set_height(ui_Image81, LV_SIZE_CONTENT);    /// 360
    lv_obj_set_align(ui_Image81, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Image81, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_Image81, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_TextArea30 = lv_textarea_create(ui_Image81);
		
		// 创建后追加
lv_textarea_set_cursor_click_pos(ui_TextArea30, false);     // 点击不定位光标
lv_obj_clear_state(ui_TextArea30, LV_STATE_FOCUSED);        // 取消已获得的焦点
lv_obj_clear_flag(ui_TextArea30, LV_OBJ_FLAG_CLICK_FOCUSABLE); // 禁止通过点击获取焦点

// 隐藏“光标”这个部件（LV_PART_CURSOR）
lv_obj_set_style_bg_opa(ui_TextArea30, LV_OPA_TRANSP, LV_PART_CURSOR);
lv_obj_set_style_border_width(ui_TextArea30, 0, LV_PART_CURSOR);
lv_obj_set_style_opa(ui_TextArea30, LV_OPA_TRANSP, LV_PART_CURSOR); // 保险

    lv_obj_set_width(ui_TextArea30, 304);
    lv_obj_set_height(ui_TextArea30, 99);
    lv_obj_set_x(ui_TextArea30, 0);
    lv_obj_set_y(ui_TextArea30, -6);
    lv_obj_set_align(ui_TextArea30, LV_ALIGN_CENTER);
    lv_textarea_set_text(ui_TextArea30, "检测眼盾无效\n请更换眼盾");
    lv_textarea_set_placeholder_text(ui_TextArea30, "Placeholder...");
    lv_obj_clear_flag(ui_TextArea30, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_text_color(ui_TextArea30, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_TextArea30, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_TextArea30, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_TextArea30, &ui_font_alibaba40, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_TextArea30, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_TextArea30, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_TextArea30, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

 ui_Panel20 = lv_obj_create(ui_Image81);
    lv_obj_set_width(ui_Panel20, 306);
    lv_obj_set_height(ui_Panel20, 87);
    lv_obj_set_x(ui_Panel20, 0);
    lv_obj_set_y(ui_Panel20, 100);
    lv_obj_set_align(ui_Panel20, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_Panel20, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Panel20, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Panel20, lv_color_hex(0x363535), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Panel20, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_Panel20, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_TextArea39 = lv_textarea_create(ui_Panel20);
    lv_obj_set_width(ui_TextArea39, 193);
    lv_obj_set_height(ui_TextArea39, 70);
    lv_obj_set_x(ui_TextArea39, 0);
    lv_obj_set_y(ui_TextArea39, 6);
    lv_obj_set_align(ui_TextArea39, LV_ALIGN_CENTER);
    lv_textarea_set_text(ui_TextArea39, "返回首页");
    lv_textarea_set_placeholder_text(ui_TextArea39, "Placeholder...");
    lv_obj_set_style_text_color(ui_TextArea39, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_TextArea39, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_TextArea39, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_TextArea39, &ui_font_alibaba40, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_TextArea39, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_TextArea39, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_TextArea39, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_add_event_cb(ui_TextArea39, ui_event_TextArea39, LV_EVENT_ALL, NULL);
		lv_obj_set_style_opa(ui_TextArea39, 0, LV_PART_CURSOR | LV_STATE_DEFAULT);

    ui_lang_apply_invalid_shield_popup_state();
		
		lv_obj_clear_flag(global_panel, LV_OBJ_FLAG_HIDDEN);  // 让 panel 可见
}
//lv_obj_clear_flag(global_panel, LV_OBJ_FLAG_HIDDEN);  // 让 panel 可见
//lv_obj_add_flag(global_panel, LV_OBJ_FLAG_HIDDEN);
