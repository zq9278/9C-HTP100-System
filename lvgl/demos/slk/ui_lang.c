#include "ui.h"
#include "ui_lang.h"

#include <string.h>

typedef struct {
    lv_obj_t **ref;
    int16_t x;
    int16_t y;
    lv_coord_t w;
    lv_coord_t h;
    const lv_font_t *font;
    int16_t letter_space;
    int16_t line_space;
    uint8_t used;
} obj_snapshot_t;

static obj_snapshot_t snapshots[96];

static void snapshot_obj(lv_obj_t **ref)
{
    uint32_t i;
    lv_obj_t *obj;

    if(ref == NULL || *ref == NULL) return;
    obj = *ref;

    for(i = 0; i < sizeof(snapshots) / sizeof(snapshots[0]); ++i) {
        if(snapshots[i].used && snapshots[i].ref == ref) return;
    }

    for(i = 0; i < sizeof(snapshots) / sizeof(snapshots[0]); ++i) {
        if(!snapshots[i].used) {
            snapshots[i].used = 1;
            snapshots[i].ref = ref;
            snapshots[i].x = lv_obj_get_x(obj);
            snapshots[i].y = lv_obj_get_y(obj);
            snapshots[i].w = lv_obj_get_width(obj);
            snapshots[i].h = lv_obj_get_height(obj);
            snapshots[i].font = lv_obj_get_style_text_font(obj, LV_PART_MAIN | LV_STATE_DEFAULT);
            snapshots[i].letter_space = lv_obj_get_style_text_letter_space(obj, LV_PART_MAIN | LV_STATE_DEFAULT);
            snapshots[i].line_space = lv_obj_get_style_text_line_space(obj, LV_PART_MAIN | LV_STATE_DEFAULT);
            return;
        }
    }
}

static void restore_snapshots(void)
{
    uint32_t i;

    for(i = 0; i < sizeof(snapshots) / sizeof(snapshots[0]); ++i) {
        lv_obj_t *obj;

        if(!snapshots[i].used || snapshots[i].ref == NULL || *snapshots[i].ref == NULL) continue;
        obj = *snapshots[i].ref;
        lv_obj_set_x(obj, snapshots[i].x);
        lv_obj_set_y(obj, snapshots[i].y);
        lv_obj_set_width(obj, snapshots[i].w);
        lv_obj_set_height(obj, snapshots[i].h);
        if(snapshots[i].font != NULL) {
            lv_obj_set_style_text_font(obj, snapshots[i].font, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        lv_obj_set_style_text_letter_space(obj, snapshots[i].letter_space, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_line_space(obj, snapshots[i].line_space, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

static void set_textarea(lv_obj_t **ref, const char *text, lv_coord_t x, lv_coord_t y, lv_coord_t w,
                         lv_coord_t h, const lv_font_t *font)
{
    lv_obj_t *obj;

    if(ref == NULL || *ref == NULL) return;
    snapshot_obj(ref);
    obj = *ref;
    lv_textarea_set_text(obj, text);
    lv_textarea_set_cursor_pos(obj, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_scroll_to_x(obj, 0, LV_ANIM_OFF);
    lv_obj_scroll_to_y(obj, 0, LV_ANIM_OFF);
    lv_textarea_set_cursor_click_pos(obj, false);
    if(w >= 0) lv_obj_set_width(obj, w);
    if(h >= 0) lv_obj_set_height(obj, h);
    if(x != INT16_MIN) lv_obj_set_x(obj, x);
    if(y != INT16_MIN) lv_obj_set_y(obj, y);
    if(font != NULL) lv_obj_set_style_text_font(obj, font, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void set_label(lv_obj_t **ref, const char *text, lv_coord_t x, lv_coord_t y, lv_coord_t w,
                      const lv_font_t *font)
{
    lv_obj_t *obj;

    if(ref == NULL || *ref == NULL) return;
    snapshot_obj(ref);
    obj = *ref;
    lv_label_set_text(obj, text);
    if(w >= 0) lv_obj_set_width(obj, w);
    if(x != INT16_MIN) lv_obj_set_x(obj, x);
    if(y != INT16_MIN) lv_obj_set_y(obj, y);
    if(font != NULL) lv_obj_set_style_text_font(obj, font, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void restore_textarea(lv_obj_t **ref, const char *text)
{
    if(ref != NULL && *ref != NULL) {
        lv_textarea_set_text(*ref, text);
        lv_textarea_set_cursor_pos(*ref, 0);
        lv_obj_clear_flag(*ref, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_scroll_to_x(*ref, 0, LV_ANIM_OFF);
        lv_obj_scroll_to_y(*ref, 0, LV_ANIM_OFF);
        lv_textarea_set_cursor_click_pos(*ref, false);
    }
}

static void restore_label(lv_obj_t **ref, const char *text)
{
    if(ref != NULL && *ref != NULL) lv_label_set_text(*ref, text);
}

static void apply_common_state(void)
{
    ui_lang_apply_bee_setting_state();
}

static void apply_zh_texts(void)
{
    restore_textarea(&ui_TextArea33, "快 速治 疗");
    restore_textarea(&ui_TextArea32, "挤压");
    restore_textarea(&ui_TextArea31, "加热");
    restore_textarea(&ui_TextArea29, "设定一");
    restore_textarea(&ui_TextArea30, "检测眼盾无效\n请更换眼盾");
    restore_textarea(&ui_TextArea39, "返回首页");

    restore_textarea(&ui_TextArea43, "准  备");
    restore_textarea(&ui_TextArea46, "准  备");
    restore_textarea(&ui_TextArea54, "准  备");
    restore_textarea(&ui_TextArea55, "下 一 步");

    restore_textarea(&ui_TextArea34, "加热中");
    restore_textarea(&ui_TextArea45, "停 止");
    restore_textarea(&ui_TextArea51, "倒计时");
    restore_textarea(&ui_TextArea44, "开 始");
    restore_textarea(&ui_TextArea56, "返回首页");
    restore_textarea(&ui_TextArea18, "加热治疗已停止");
    restore_textarea(&ui_TextArea57, "返回首页");
    restore_textarea(&ui_TextArea19, "加热治疗已完成");

    restore_textarea(&ui_TextArea35, "挤压中");
    restore_textarea(&ui_TextArea52, "倒计时");
    restore_textarea(&ui_TextArea48, "停 止");
    restore_textarea(&ui_TextArea47, "开 始");
    restore_textarea(&ui_TextArea58, "返回首页");
    restore_textarea(&ui_TextArea20, "挤压治疗已停止");
    restore_textarea(&ui_TextArea21, "挤压治疗已完成");
    restore_textarea(&ui_TextArea62, "返回首页");

    restore_textarea(&ui_TextArea36, "快速治疗中");
    restore_textarea(&ui_TextArea53, "倒计时");
    restore_textarea(&ui_TextArea49, "停 止");
    restore_textarea(&ui_TextArea50, "开 始");
    restore_label(&ui_Label40, "压力:");
    restore_label(&ui_Label34, "温度:");
    restore_textarea(&ui_TextArea60, "返回首页");
    restore_textarea(&ui_TextArea23, "快速治疗已停止");
    restore_textarea(&ui_TextArea61, "返回首页");
    restore_textarea(&ui_TextArea22, "快速治疗已完成");

    restore_textarea(&ui_TextArea16, "眼盾更换次数");
    restore_textarea(&ui_TextArea17, "治疗计数");
    restore_textarea(&ui_TextArea25, "加热治疗次数:");
    restore_textarea(&ui_TextArea26, "挤压治疗次数:");
    restore_textarea(&ui_TextArea27, "快速治疗次数:");
    restore_textarea(&ui_TextArea28, "眼盾使用次数");
    restore_textarea(&ui_TextArea24, "返回首页");

    restore_textarea(&ui_TextArea1, "参数设定");
    restore_textarea(&ui_TextArea3, " 系统设置");
    restore_textarea(&ui_TextArea37, "发布版本号: V1");
    restore_textarea(&ui_TextArea38, "完整版本号: V1.0.2.");
    restore_textarea(&ui_TextArea42, "20241226");
    restore_textarea(&ui_TextArea13, "设定一");
    restore_textarea(&ui_TextArea15, "设定二");
    restore_textarea(&ui_TextArea14, "设定三");
    restore_textarea(&ui_TextArea5, "压力值:");
    restore_textarea(&ui_TextArea6, "治疗时间:");
    restore_textarea(&ui_TextArea10, "保存成功!");
    restore_textarea(&ui_TextArea7, "保存");

    restore_textarea(&ui_TextArea11, "按键音");
    restore_textarea(&ui_TextArea4, "开");
    restore_textarea(&ui_TextArea12, "关");
    restore_textarea(&ui_TextArea9, "语言");
    restore_textarea(&ui_TextArea40, "中");
    restore_textarea(&ui_TextArea41, "ENG");

    restore_label(&ui_Label13, "确认");
    restore_label(&ui_Label7, "老化模式");
    restore_label(&ui_Label11, "压力");
    restore_label(&ui_Label12, "无倒计时");
    restore_label(&ui_Label15, "眼盾清零");
    restore_label(&ui_Label14, "设置清零");
}

static void apply_en_texts(void)
{
    const lv_font_t *font30 = &ui_font_alibaba30;
    const lv_font_t *font40 = &ui_font_alibaba40;
    const lv_font_t *mid = &ui_font_alibaba10;
    const lv_font_t *small = LV_FONT_DEFAULT;

    set_textarea(&ui_TextArea33, "Quick\nTreatment", INT16_MIN, INT16_MIN, 190, 88, font30);
    set_textarea(&ui_TextArea32, "Compress", INT16_MIN, INT16_MIN, 180, 45, font30);
    set_textarea(&ui_TextArea31, "Heat", INT16_MIN, INT16_MIN, 86, 45, font30);
    set_textarea(&ui_TextArea29, "Setting 1", INT16_MIN, INT16_MIN, 105, 30, small);
    set_textarea(&ui_TextArea30, "Invalid shield\nPlease replace it", INT16_MIN, INT16_MIN, 230, 76, font30);
    set_textarea(&ui_TextArea39, "Return to Home", INT16_MIN, INT16_MIN, 148, 42, font30);

    set_textarea(&ui_TextArea43, "Prepare", INT16_MIN, INT16_MIN, 170, 56, font30);
    set_textarea(&ui_TextArea46, "Prepare", INT16_MIN, INT16_MIN, 170, 56, font30);
    set_textarea(&ui_TextArea54, "Prepare", INT16_MIN, INT16_MIN, 170, 56, font30);
    set_textarea(&ui_TextArea55, "Next step", INT16_MIN, INT16_MIN, 180, 56, font30);

    set_textarea(&ui_TextArea34, "Heating", INT16_MIN, INT16_MIN, 150, 34, mid);
    set_textarea(&ui_TextArea45, "Stop", INT16_MIN, INT16_MIN, 125, 52, font30);
    set_textarea(&ui_TextArea51, "Countdown", INT16_MIN, INT16_MIN, 153, 32, mid);
    if(ui_TextArea51 != NULL) lv_obj_set_style_text_letter_space(ui_TextArea51, -2, LV_PART_MAIN | LV_STATE_DEFAULT);
    set_textarea(&ui_TextArea44, "Start", INT16_MIN, INT16_MIN, 145, 58, font40);
    set_textarea(&ui_TextArea56, "Back to\nHome", INT16_MIN, INT16_MIN, 170, 56, small);
    set_textarea(&ui_TextArea18, "Heating\nstopped", INT16_MIN, INT16_MIN, 240, 82, font30);
    set_textarea(&ui_TextArea57, "Back to\nHome", INT16_MIN, INT16_MIN, 170, 56, small);
    set_textarea(&ui_TextArea19, "Heating\ncompleted", INT16_MIN, INT16_MIN, 250, 82, font30);

    set_textarea(&ui_TextArea35, "Compressing", INT16_MIN, INT16_MIN, 210, 34, mid);
    set_textarea(&ui_TextArea52, "Countdown", INT16_MIN, INT16_MIN, 153, 32, mid);
    if(ui_TextArea52 != NULL) lv_obj_set_style_text_letter_space(ui_TextArea52, -2, LV_PART_MAIN | LV_STATE_DEFAULT);
    set_textarea(&ui_TextArea48, "Stop", INT16_MIN, INT16_MIN, 125, 52, font30);
    set_textarea(&ui_TextArea47, "Start", INT16_MIN, INT16_MIN, 145, 58, font40);
    set_textarea(&ui_TextArea58, "Back to\nHome", INT16_MIN, INT16_MIN, 170, 56, small);
    set_textarea(&ui_TextArea20, "Compression\nstopped", INT16_MIN, INT16_MIN, 260, 82, font30);
    set_textarea(&ui_TextArea21, "Compression\ncompleted", INT16_MIN, INT16_MIN, 280, 82, font30);
    set_textarea(&ui_TextArea62, "Back to\nHome", INT16_MIN, INT16_MIN, 170, 56, small);

    set_textarea(&ui_TextArea36, "Quick\nTreatment", INT16_MIN, INT16_MIN, 150, 46, small);
    set_textarea(&ui_TextArea53, "Countdown", INT16_MIN, INT16_MIN, 153, 32, mid);
    if(ui_TextArea53 != NULL) lv_obj_set_style_text_letter_space(ui_TextArea53, -2, LV_PART_MAIN | LV_STATE_DEFAULT);
    set_textarea(&ui_TextArea49, "Stop", INT16_MIN, INT16_MIN, 125, 52, font30);
    set_textarea(&ui_TextArea50, "Start", INT16_MIN, INT16_MIN, 145, 58, font40);
    set_label(&ui_Label40, "Pressure:", INT16_MIN, INT16_MIN, 130, mid);
    set_label(&ui_Label34, "Temperature:", INT16_MIN, INT16_MIN, 155, mid);
    set_textarea(&ui_TextArea60, "Back to\nHome", INT16_MIN, INT16_MIN, 170, 56, small);
    set_textarea(&ui_TextArea23, "Quick treatment\nstopped", INT16_MIN, INT16_MIN, 290, 82, font30);
    set_textarea(&ui_TextArea61, "Back to\nHome", INT16_MIN, INT16_MIN, 170, 56, small);
    set_textarea(&ui_TextArea22, "Quick treatment\ncompleted", INT16_MIN, INT16_MIN, 290, 82, font30);

    set_textarea(&ui_TextArea16, "Shield Usage Count", INT16_MIN, INT16_MIN, 320, 58, font30);
    set_textarea(&ui_TextArea17, "Treatment Count", INT16_MIN, INT16_MIN, 300, 58, font30);
    set_textarea(&ui_TextArea25, "Heat Treatment Count:", 0, INT16_MIN, 360, 38, mid);
    set_textarea(&ui_TextArea26, "Compression Treatment Count:", 0, INT16_MIN, 360, 38, mid);
    set_textarea(&ui_TextArea27, "Quick Treatment Count:", 0, INT16_MIN, 360, 38, mid);
    if(ui_TextArea25 != NULL) lv_obj_set_style_text_letter_space(ui_TextArea25, -1, LV_PART_MAIN | LV_STATE_DEFAULT);
    if(ui_TextArea26 != NULL) lv_obj_set_style_text_letter_space(ui_TextArea26, -1, LV_PART_MAIN | LV_STATE_DEFAULT);
    if(ui_TextArea27 != NULL) lv_obj_set_style_text_letter_space(ui_TextArea27, -1, LV_PART_MAIN | LV_STATE_DEFAULT);
    set_textarea(&ui_TextArea28, "Shield Usage Count", INT16_MIN, INT16_MIN, 360, 58, font30);
    set_textarea(&ui_TextArea24, "Back to Home", INT16_MIN, INT16_MIN, 280, 50, font30);

    set_textarea(&ui_TextArea1, "Parameter Setting", INT16_MIN, INT16_MIN, 280, 54, font30);
    set_textarea(&ui_TextArea3, "System Setting", INT16_MIN, INT16_MIN, 250, 54, font30);
    set_textarea(&ui_TextArea37, "Version: V1", INT16_MIN, INT16_MIN, 170, 26, small);
    set_textarea(&ui_TextArea38, "Software Ver: V1.0.2.", -18, INT16_MIN, 200, 26, small);
    set_textarea(&ui_TextArea42, "20241226", 84, INT16_MIN, 82, 26, small);
    set_textarea(&ui_TextArea13, "Preset 1", INT16_MIN, INT16_MIN, 220, 58, font40);
    set_textarea(&ui_TextArea15, "Preset 2", INT16_MIN, INT16_MIN, 220, 58, font40);
    set_textarea(&ui_TextArea14, "Preset 3", INT16_MIN, INT16_MIN, 220, 58, font40);
    set_textarea(&ui_TextArea5, "Pressure Value:", INT16_MIN, INT16_MIN, 235, 38, mid);
    set_textarea(&ui_TextArea6, "Treatment Time:", INT16_MIN, INT16_MIN, 235, 38, mid);
    set_textarea(&ui_TextArea10, "Saved!", INT16_MIN, INT16_MIN, 100, 34, font30);
    set_textarea(&ui_TextArea7, "Save", INT16_MIN, INT16_MIN, 110, 36, font30);

    set_textarea(&ui_TextArea11, "Key Tone", INT16_MIN, INT16_MIN, 150, 42, font30);
    set_textarea(&ui_TextArea4, "ON", INT16_MIN, INT16_MIN, 80, 36, font30);
    set_textarea(&ui_TextArea12, "OFF", 176, INT16_MIN, 88, 36, font30);
    set_textarea(&ui_TextArea9, "Language", INT16_MIN, INT16_MIN, 170, 42, font30);
    set_textarea(&ui_TextArea40, "ZN", INT16_MIN, INT16_MIN, 70, 36, font30);
    set_textarea(&ui_TextArea41, "EN", INT16_MIN, INT16_MIN, 70, 36, font30);

    set_label(&ui_Label13, "Confirm", INT16_MIN, INT16_MIN, 118, small);
    set_label(&ui_Label7, "Aging\nMode", 0, -282, 150, font30);
    set_label(&ui_Label11, "Pressure", -188, -170, 95, small);
    set_label(&ui_Label12, "No Countdown", -65, -107, 155, small);
    set_label(&ui_Label15, "Clear Shield", INT16_MIN, INT16_MIN, 125, small);
    set_label(&ui_Label14, "Clear Settings", INT16_MIN, INT16_MIN, 125, small);
}

uint16_t ui_lang_get(void)
{
    return language == UI_LANG_EN ? UI_LANG_EN : UI_LANG_ZH;
}

void ui_lang_apply_bee_setting_state(void)
{
    if(ui_Image95 != NULL && ui_Image109 != NULL && ui_Image113 != NULL && ui_Image110 != NULL) {
        if(bee) {
            lv_obj_clear_flag(ui_Image95, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_Image109, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_Image113, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_Image110, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(ui_Image95, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_Image109, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_Image113, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_Image110, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if(ui_Image93 != NULL && ui_Image83 != NULL && ui_Image94 != NULL && ui_Image84 != NULL) {
        if(ui_lang_get() == UI_LANG_EN) {
            lv_obj_add_flag(ui_Image93, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_Image83, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_Image94, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_Image84, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(ui_Image93, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_Image83, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui_Image94, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(ui_Image84, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void ui_lang_apply_all(void)
{
    if(ui_lang_get() == UI_LANG_EN) {
        apply_en_texts();
    } else {
        restore_snapshots();
        apply_zh_texts();
    }
    apply_common_state();
}

void ui_lang_set(uint16_t lang)
{
    language = (lang == UI_LANG_EN) ? UI_LANG_EN : UI_LANG_ZH;
    ui_lang_apply_all();
}

static void init_theme_once(void)
{
    static uint8_t done = 0;
    lv_disp_t *dispp;
    lv_theme_t *theme;

    if(done) return;
    LV_EVENT_GET_COMP_CHILD = lv_event_register_id();
    dispp = lv_disp_get_default();
    theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED),
                                  false, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);
    done = 1;
}

void ui_debug_init_screen(const char *screen_name)
{
    init_theme_once();

    if(strcmp(screen_name, "ui_home_page") == 0) ui_home_page_screen_init();
    else if(strcmp(screen_name, "ui_hot_count") == 0) ui_hot_count_screen_init();
    else if(strcmp(screen_name, "ui_hotting") == 0) ui_hotting_screen_init();
    else if(strcmp(screen_name, "ui_pulsation_set") == 0) ui_pulsation_set_screen_init();
    else if(strcmp(screen_name, "ui_auto_count") == 0) ui_auto_count_screen_init();
    else if(strcmp(screen_name, "ui_pulsationning") == 0) ui_pulsationning_screen_init();
    else if(strcmp(screen_name, "ui_autoing") == 0) ui_autoing_screen_init();
    else if(strcmp(screen_name, "ui_pulsation_set1") == 0) ui_pulsation_set1_screen_init();
    else if(strcmp(screen_name, "ui_dev_model") == 0) ui_dev_model_screen_init();
    else if(strcmp(screen_name, "ui_count_model") == 0) ui_count_model_screen_init();
    else if(strcmp(screen_name, "ui_test") == 0) ui_test_screen_init();
    else if(strcmp(screen_name, "ui_setting") == 0) ui_setting_screen_init();
    else if(strcmp(screen_name, "ui_pre_setting") == 0) ui_pre_setting_screen_init();
    else if(strcmp(screen_name, "ui_set_value") == 0) ui_set_value_screen_init();
    else if(strcmp(screen_name, "ui_bee_setting") == 0) ui_bee_setting_screen_init();
    else if(strcmp(screen_name, "ui_count") == 0) ui_count_screen_init();
    else if(strcmp(screen_name, "ui_Factorymode") == 0) ui_Factorymode_screen_init();
    else ui_Power_on_interface_screen_init();

    ui____initial_actions0 = lv_obj_create(NULL);
    ui_lang_apply_all();
    ui_debug_load_screen(screen_name);
}

void ui_debug_load_screen(const char *screen_name)
{
    if(strcmp(screen_name, "ui_home_page") == 0) lv_disp_load_scr(ui_home_page);
    else if(strcmp(screen_name, "ui_hot_count") == 0) lv_disp_load_scr(ui_hot_count);
    else if(strcmp(screen_name, "ui_hotting") == 0) lv_disp_load_scr(ui_hotting);
    else if(strcmp(screen_name, "ui_pulsation_set") == 0) lv_disp_load_scr(ui_pulsation_set);
    else if(strcmp(screen_name, "ui_auto_count") == 0) lv_disp_load_scr(ui_auto_count);
    else if(strcmp(screen_name, "ui_pulsationning") == 0) lv_disp_load_scr(ui_pulsationning);
    else if(strcmp(screen_name, "ui_autoing") == 0) lv_disp_load_scr(ui_autoing);
    else if(strcmp(screen_name, "ui_pulsation_set1") == 0) lv_disp_load_scr(ui_pulsation_set1);
    else if(strcmp(screen_name, "ui_dev_model") == 0) lv_disp_load_scr(ui_dev_model);
    else if(strcmp(screen_name, "ui_count_model") == 0) lv_disp_load_scr(ui_count_model);
    else if(strcmp(screen_name, "ui_test") == 0) lv_disp_load_scr(ui_test);
    else if(strcmp(screen_name, "ui_setting") == 0) lv_disp_load_scr(ui_setting);
    else if(strcmp(screen_name, "ui_pre_setting") == 0) lv_disp_load_scr(ui_pre_setting);
    else if(strcmp(screen_name, "ui_set_value") == 0) lv_disp_load_scr(ui_set_value);
    else if(strcmp(screen_name, "ui_bee_setting") == 0) lv_disp_load_scr(ui_bee_setting);
    else if(strcmp(screen_name, "ui_count") == 0) lv_disp_load_scr(ui_count);
    else if(strcmp(screen_name, "ui_Factorymode") == 0) lv_disp_load_scr(ui_Factorymode);
    else lv_disp_load_scr(ui_Power_on_interface);
}
