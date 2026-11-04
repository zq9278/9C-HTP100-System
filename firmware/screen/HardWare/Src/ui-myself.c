#include "ui.h"
#include "ui-myself.h"
#include <string.h>

lv_obj_t * global_panel = NULL;  // 确保只有一个 panel 实例
static lv_obj_t *global_popup_message = NULL;
static lv_obj_t *global_popup_button_text = NULL;

LV_FONT_DECLARE(ui_font_popup40);
LV_FONT_DECLARE(ui_font_battery40);
static lv_obj_t *battery_popup, *battery_message, *battery_ack_text;
static uint8_t battery_notice, battery_dismissed, battery_deferred;
static lv_obj_t *create_notice_panel(lv_obj_t **, lv_obj_t **, lv_event_cb_t,
                                    lv_event_code_t, const lv_font_t *);
static void battery_ack(lv_event_t *e) {
    BEE1(e);
    battery_dismissed = battery_notice;
    battery_deferred = 0;
    lv_obj_add_flag(battery_popup, LV_OBJ_FLAG_HIDDEN);
}
void ui_battery_warning_update(bool low, bool shutdown, bool running) {
    battery_notice = shutdown ? 2 : low ? 1 : 0;
    if (running) {
        if (!battery_dismissed && battery_notice > battery_deferred) {
            battery_deferred = battery_notice;
        }
        if (battery_popup) lv_obj_add_flag(battery_popup, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (battery_deferred > battery_notice) battery_notice = battery_deferred;
    if (!battery_notice) battery_dismissed = 0;
    if (!battery_notice || battery_dismissed ||
        (global_panel && !lv_obj_has_flag(global_panel, LV_OBJ_FLAG_HIDDEN))) {
        if (battery_popup) lv_obj_add_flag(battery_popup, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    if (!battery_popup) {
        battery_popup = create_notice_panel(&battery_message, &battery_ack_text,
                                           battery_ack, LV_EVENT_CLICKED, &ui_font_battery40);
    }
    bool english = ui_lang_get() == UI_LANG_EN;
    lv_textarea_set_text(battery_message, english ? "Low battery\nPlease charge" : "电量低\n请充电");
    lv_label_set_text(battery_ack_text, english ? "OK" : "知道了");
    lv_obj_clear_flag(battery_popup, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *create_notice_panel(lv_obj_t **message_out, lv_obj_t **button_text_out,
                                    lv_event_cb_t callback, lv_event_code_t event_filter,
                                    const lv_font_t *font) {
    lv_obj_t *panel, *message, *button_text;
    panel = lv_obj_create(lv_layer_top());  // 确保添加到最顶层
    lv_obj_set_width(panel, 480);
    lv_obj_set_height(panel, 640);
    lv_obj_set_align(panel, LV_ALIGN_CENTER);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);     /// Flags
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *background = lv_img_create(panel);
    lv_img_set_src(background, &ui_img_black_png);
    lv_obj_set_width(background, LV_SIZE_CONTENT);   /// 480
    lv_obj_set_height(background, LV_SIZE_CONTENT);    /// 640
    lv_obj_set_align(background, LV_ALIGN_CENTER);
    lv_obj_add_flag(background, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(background, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    lv_obj_t *dialog = lv_img_create(panel);
    lv_img_set_src(dialog, &ui_img_woring_png);
    lv_obj_set_width(dialog, LV_SIZE_CONTENT);   /// 424
    lv_obj_set_height(dialog, LV_SIZE_CONTENT);    /// 360
    lv_obj_set_align(dialog, LV_ALIGN_CENTER);
    lv_obj_add_flag(dialog, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    message = lv_textarea_create(dialog);
		
		// 创建后追加
lv_textarea_set_cursor_click_pos(message, false);     // 点击不定位光标
lv_obj_clear_state(message, LV_STATE_FOCUSED);        // 取消已获得的焦点
lv_obj_clear_flag(message, LV_OBJ_FLAG_CLICK_FOCUSABLE); // 禁止通过点击获取焦点

// 隐藏“光标”这个部件（LV_PART_CURSOR）
lv_obj_set_style_bg_opa(message, LV_OPA_TRANSP, LV_PART_CURSOR);
lv_obj_set_style_border_width(message, 0, LV_PART_CURSOR);
lv_obj_set_style_opa(message, LV_OPA_TRANSP, LV_PART_CURSOR); // 保险

    lv_obj_set_width(message, 330);
    lv_obj_set_height(message, 100);
    lv_obj_set_x(message, 0);
    lv_obj_set_y(message, -6);
    lv_obj_set_align(message, LV_ALIGN_CENTER);
    lv_textarea_set_text(message, "");
    lv_obj_clear_flag(message, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_color(message, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(message, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(message, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(message, font, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(message, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(message, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *button_panel = lv_obj_create(dialog);
    lv_obj_set_width(button_panel, 306);
    lv_obj_set_height(button_panel, 87);
    lv_obj_set_x(button_panel, 0);
    lv_obj_set_y(button_panel, 100);
    lv_obj_set_align(button_panel, LV_ALIGN_CENTER);
    lv_obj_clear_flag(button_panel, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(button_panel, 30, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(button_panel, lv_color_hex(0x363535), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(button_panel, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(button_panel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(button_panel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(button_panel, callback, event_filter, NULL);

    /* A label has no edit cursor or scrollbar. The parent panel owns the
     * click event so the complete button area remains touchable. */
    button_text = lv_label_create(button_panel);
    lv_obj_set_width(button_text, 270);
    lv_obj_set_height(button_text, LV_SIZE_CONTENT);
    lv_obj_set_y(button_text, 0);
    lv_obj_set_align(button_text, LV_ALIGN_CENTER);
    lv_label_set_long_mode(button_text, LV_LABEL_LONG_CLIP);
    lv_label_set_text(button_text, "");
    lv_obj_clear_flag(button_text,
                      LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_color(button_text, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(button_text, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(button_text, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(button_text, font, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(button_text, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(button_text, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    *message_out = message;
    *button_text_out = button_text;
    return panel;
}
void create_global_panel(void) {
    if (!global_panel) global_panel = create_notice_panel(&global_popup_message,
        &global_popup_button_text, ui_event_TextArea39, LV_EVENT_ALL, &ui_font_popup40);
}

void global_panel_set_content(const char *message, const char *button_text) {
    create_global_panel();
    if (global_popup_message != NULL && message != NULL) {
        lv_textarea_set_text(global_popup_message, message);
    }
    if (global_popup_button_text != NULL && button_text != NULL) {
        lv_obj_set_style_text_font(global_popup_button_text,
                                   strcmp(button_text, "Return to home") == 0 ?
                                   &ui_font_alibaba30 : &ui_font_popup40,
                                   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_label_set_text(global_popup_button_text, button_text);
    }
}
//lv_obj_clear_flag(global_panel, LV_OBJ_FLAG_HIDDEN);  // 让 panel 可见
//lv_obj_add_flag(global_panel, LV_OBJ_FLAG_HIDDEN);
