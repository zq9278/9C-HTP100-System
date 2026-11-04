#include "screens/ui_pressure_calibration.h"
#include "system_state.h"
#include "screen_link.h"
#define SDL_MAIN_HANDLED

#include "lvgl.h"
#include "ui.h"
#include "ui-myself.h"
#include "screen.h"
#include <assert.h>
#include <stdio.h>

#include <SDL.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define SIM_HOR_RES 480
#define SIM_VER_RES 640

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static uint16_t framebuffer[SIM_HOR_RES * SIM_VER_RES];

static struct {
    int16_t x;
    int16_t y;
    bool pressed;
    bool quit;
} mouse_state;

static void sdl_display_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    int32_t x;
    int32_t y;

    for(y = area->y1; y <= area->y2; ++y) {
        for(x = area->x1; x <= area->x2; ++x) {
            framebuffer[y * SIM_HOR_RES + x] = color_p->full;
            color_p++;
        }
    }

    if(lv_disp_flush_is_last(disp_drv)) {
        SDL_UpdateTexture(texture, NULL, framebuffer, SIM_HOR_RES * (int)sizeof(uint16_t));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }

    lv_disp_flush_ready(disp_drv);
}

static void sdl_mouse_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;
    data->point.x = mouse_state.x;
    data->point.y = mouse_state.y;
    data->state = mouse_state.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static int init_sdl(void)
{
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        return -1;
    }

    window = SDL_CreateWindow(
        "SLK01 LVGL SDL Simulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SIM_HOR_RES,
        SIM_VER_RES,
        getenv("SLK_HEADLESS") ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN
    );
    if(window == NULL) {
        return -1;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if(renderer == NULL) {
        return -1;
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, SIM_HOR_RES, SIM_VER_RES);
    if(texture == NULL) {
        return -1;
    }

    memset(framebuffer, 0, sizeof(framebuffer));
    return 0;
}

static void init_lvgl_port(void)
{
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf1[SIM_HOR_RES * 40];
    static lv_color_t buf2[SIM_HOR_RES * 40];
    static lv_disp_drv_t disp_drv;
    static lv_indev_drv_t indev_drv;

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, SIM_HOR_RES * 40);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SIM_HOR_RES;
    disp_drv.ver_res = SIM_VER_RES;
    disp_drv.flush_cb = sdl_display_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = sdl_mouse_read;
    lv_indev_drv_register(&indev_drv);
}

static void process_events(void)
{
    SDL_Event event;

    while(SDL_PollEvent(&event)) {
        switch(event.type) {
        case SDL_QUIT:
            mouse_state.quit = true;
            break;
        case SDL_MOUSEMOTION:
            mouse_state.x = (int16_t)event.motion.x;
            mouse_state.y = (int16_t)event.motion.y;
            break;
        case SDL_MOUSEBUTTONDOWN:
            if(event.button.button == SDL_BUTTON_LEFT) {
                mouse_state.pressed = true;
                mouse_state.x = (int16_t)event.button.x;
                mouse_state.y = (int16_t)event.button.y;
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if(event.button.button == SDL_BUTTON_LEFT) {
                mouse_state.pressed = false;
                mouse_state.x = (int16_t)event.button.x;
                mouse_state.y = (int16_t)event.button.y;
            }
            break;
        default:
            break;
        }
    }
}

static void save_framebuffer_bmp(const char *path)
{
    SDL_Surface *surface;

    if(path == NULL || path[0] == '\0') {
        return;
    }

    surface = SDL_CreateRGBSurfaceWithFormatFrom(
        framebuffer,
        SIM_HOR_RES,
        SIM_VER_RES,
        16,
        SIM_HOR_RES * (int)sizeof(uint16_t),
        SDL_PIXELFORMAT_RGB565
    );
    if(surface != NULL) {
        SDL_SaveBMP(surface, path);
        SDL_FreeSurface(surface);
    }
}

static void test_static_ui(void)
{
    extern unsigned sim_beep_count, sim_request_count;
    extern volatile uint8_t work_mode;
    extern uint16_t sim_last_command;
    lv_obj_t *stop_buttons[] = {ui_Image5, ui_Image22, ui_Image55};
    const uint16_t commands[] = {0x1030, 0x1034, 0x1038};
    for (unsigned i = 0; i < 3; i++) {
        work_mode = i + 1;
        unsigned beeps = sim_beep_count, requests = sim_request_count;
        bee = 1;
        lv_event_send(stop_buttons[i], LV_EVENT_RELEASED, NULL);
        assert(sim_beep_count == beeps + 1);
        assert(sim_request_count == requests + 1 && sim_last_command == commands[i]);
        bee = 0;
        lv_event_send(stop_buttons[i], LV_EVENT_RELEASED, NULL);
        assert(sim_beep_count == beeps + 1);
        assert(sim_request_count == requests + 2);
    }
    work_mode = 0; bee = 1;
    puts("PASS: three stop buttons beep once, mute still sends STOP");
    lv_obj_t *engineering_before = lv_scr_act();
    for (unsigned i=0;i<6;i++) formattee(NULL);
    assert(sim_last_command == 0x9200);
    assert(lv_scr_act() == engineering_before);
    lv_textarea_set_text(ui_TextArea2, "250");
    lv_event_send(ui_Panel6, LV_EVENT_RELEASED, NULL);
    assert(sim_last_command == 0x9201 && lv_scr_act() == engineering_before);
    lv_event_send(ui_Panel7, LV_EVENT_RELEASED, NULL);
    assert(sim_last_command == 0x9202);
    lv_event_send(ui_Panel8, LV_EVENT_RELEASED, NULL);
    assert(sim_last_command == 0x9203);
    lv_event_send(ui_Button17, LV_EVENT_RELEASED, NULL);
    assert(sim_last_command == 0x9200 && lv_scr_act() == engineering_before);
    puts("PASS: engineering buttons send versioned requests without speculative navigation");
    ui_lang_set(UI_LANG_EN);
    assert(lv_scr_act() == ui_Power_on_interface);
    ui_startup_finish();
    assert(lv_scr_act() == ui_home_page);
    for (unsigned preset = 1; preset <= 3; ++preset) {
        char expected[20]; snprintf(expected, sizeof(expected), "Setting %u", preset);
        prepare_set = preset;
        for (unsigned repeat = 0; repeat < 6; ++repeat) {
            ui_lang_apply_all();
            assert(strcmp(lv_label_get_text(ui_TextArea29), expected) == 0);
            _ui_screen_change(&ui_setting, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_setting_screen_init);
            _ui_screen_change(&ui_home_page, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_home_page_screen_init);
            lv_obj_update_layout(ui_home_page);
            assert(lv_obj_get_width(ui_TextArea29) == 127);
            assert(lv_obj_get_height(ui_TextArea29) == 43);
            assert(lv_obj_get_x_aligned(ui_TextArea29) == 3);
            assert(lv_obj_get_y_aligned(ui_TextArea29) == 1);
            assert(strcmp(lv_label_get_text(ui_TextArea29), expected) == 0);
        }
    }
    _ui_screen_change(&ui_setting, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_setting_screen_init);
    lv_tick_inc(1700); lv_timer_handler();
    assert(lv_scr_act() == ui_setting); /* Cancelled boot timer cannot jump home later. */
    ui_lang_set(UI_LANG_ZH); ui_lang_set(UI_LANG_EN);
    assert(strcmp(lv_label_get_text(ui_TextArea29), "Setting 3") == 0);
    _ui_screen_change(&ui_home_page, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_home_page_screen_init);
    puts("PASS: preset label geometry/text, language roundtrip, startup cancellation");
    ui_battery_warning_update(true, false, false);
    lv_obj_t *notice = lv_obj_get_child(lv_layer_top(), -1);
    lv_obj_t *page_before = lv_scr_act();
    unsigned battery_beeps = sim_beep_count;
    bee = 1;
    assert(!lv_obj_has_flag(notice, LV_OBJ_FLAG_HIDDEN));
    lv_event_send(lv_obj_get_child(lv_obj_get_child(notice, 1), 1), LV_EVENT_CLICKED, NULL);
    assert(lv_obj_has_flag(notice, LV_OBJ_FLAG_HIDDEN));
    assert(lv_scr_act() == page_before);
    assert(sim_beep_count == battery_beeps + 1);
    ui_battery_warning_update(true, false, false);
    assert(lv_obj_has_flag(notice, LV_OBJ_FLAG_HIDDEN));
    ui_battery_warning_update(true, false, true);
    ui_battery_warning_update(true, true, false);
    assert(lv_obj_has_flag(notice, LV_OBJ_FLAG_HIDDEN));
    /* Recovery re-arms a later low-battery episode. */
    ui_battery_warning_update(false, false, false);
    ui_battery_warning_update(true, false, false);
    assert(!lv_obj_has_flag(notice, LV_OBJ_FLAG_HIDDEN));
    bee = 0;
    lv_event_send(lv_obj_get_child(lv_obj_get_child(notice, 1), 1), LV_EVENT_CLICKED, NULL);
    assert(sim_beep_count == battery_beeps + 1);
    ui_battery_warning_update(true, false, false);
    assert(lv_obj_has_flag(notice, LV_OBJ_FLAG_HIDDEN));
    bee = 1;
    ui_battery_warning_update(false, false, false);
    assert(lv_obj_has_flag(notice, LV_OBJ_FLAG_HIDDEN));
    ui_battery_warning_update(true, false, true);
    assert(lv_obj_has_flag(notice, LV_OBJ_FLAG_HIDDEN));
    ui_battery_warning_update(true, true, true);
    assert(lv_obj_has_flag(notice, LV_OBJ_FLAG_HIDDEN));
    /* Even if voltage recovers, deliver the deferred notice after RUNNING. */
    ui_battery_warning_update(false, false, false);
    assert(!lv_obj_has_flag(notice, LV_OBJ_FLAG_HIDDEN));
    lv_event_send(lv_obj_get_child(lv_obj_get_child(notice, 1), 1), LV_EVENT_CLICKED, NULL);
    ui_battery_warning_update(false, false, false);
    assert(lv_obj_has_flag(notice, LV_OBJ_FLAG_HIDDEN));
    puts("PASS: battery acknowledgement beeps respecting mute, acknowledged low stays dismissed across treatment and escalation, recovered deferred notice dismisses");
    ui_pressure_calibration_open(NULL);
    assert(ui_pressure_calibration_active());
    float calibration_values[4]={20000,60000,200,.9f};
    screen_calibration_result(0,0,0,false,calibration_values);
    ui_pressure_calibration_state(APP_STATE_IDLE,true,false,0,0);
    lv_obj_t *cal_page=lv_scr_act();
    assert(lv_obj_get_child_cnt(cal_page)>15);
    lv_obj_t *save_button=NULL,*apply_button=NULL;
    for(unsigned i=0;i<lv_obj_get_child_cnt(cal_page);i++){
        lv_obj_t *child=lv_obj_get_child(cal_page,i);
        if(!lv_obj_check_type(child,&lv_btn_class))continue;
        lv_obj_t *caption=lv_obj_get_child(child,0);
        if(!strcmp(lv_label_get_text(caption),"Save to host"))save_button=child;
        if(!strcmp(lv_label_get_text(caption),"Apply"))apply_button=child;
    }
    assert(save_button && apply_button);
    const char *expected_values[]={"20000","60000","200.0","0.900"};
    unsigned value_index=0;
    for(unsigned i=0;i<lv_obj_get_child_cnt(cal_page);i++){
        lv_obj_t *child=lv_obj_get_child(cal_page,i);
        if(lv_obj_check_type(child,&lv_textarea_class)){
            assert(value_index<4);
            assert(!strcmp(lv_textarea_get_text(child),expected_values[value_index++]));
        }
    }
    assert(value_index==4);
    assert(!lv_obj_has_state(save_button,LV_STATE_DISABLED));
    ui_pressure_calibration_state(APP_STATE_RUNNING,true,false,0,250);
    assert(lv_obj_has_state(save_button,LV_STATE_DISABLED));
    assert(!lv_obj_has_state(apply_button,LV_STATE_DISABLED));
    assert(ui_pressure_calibration_active());
    _ui_screen_change(&ui_home_page,LV_SCR_LOAD_ANIM_NONE,0,0,&ui_home_page_screen_init);
    puts("PASS: calibration page constructs, loads values and survives running snapshots");

}

int main(int argc, char **argv)
{
    uint32_t last_tick;
    const char *screen_name;
    const char *lang_name;
    const char *capture_path;
    uint32_t capture_after_ms = 0;
    uint32_t runtime_ms = 0;
    (void)argc;
    (void)argv;

    if(init_sdl() != 0) {
        return 1;
    }

    lv_init();
    init_lvgl_port();
    screen_name = getenv("SLK_SCREEN");
    if(screen_name != NULL && screen_name[0] != '\0') {
        ui_debug_init_screen(screen_name);
    } else {
        ui_init();
    }
    lang_name = getenv("SLK_LANG");
    if(lang_name != NULL) {
        if(strcmp(lang_name, "en") == 0 || strcmp(lang_name, "EN") == 0) {
            ui_lang_set(UI_LANG_EN);
        } else if(strcmp(lang_name, "zh") == 0 || strcmp(lang_name, "ZH") == 0) {
            ui_lang_set(UI_LANG_ZH);
        }
    }
    if (getenv("SLK_SELF_TEST")) test_static_ui();
    if (getenv("SLK_CALIBRATION")) {
        ui_pressure_calibration_open(NULL);
        float v[4]={20000,60000,200,.9f};
        screen_calibration_result(0,0,0,false,v);
        ui_pressure_calibration_state(APP_STATE_IDLE,true,false,0,0);
    }
    if (getenv("SLK_LOW_BATTERY")) {
        ui_startup_finish();
        ui_battery_warning_update(true, true, false);
    }
    if (getenv("SLK_INVALID_SHIELD")) {
        ui_startup_finish();
        create_global_panel();
        global_panel_set_content("Shield invalid\nReplace shield", "Return to home");
        lv_obj_clear_flag(global_panel, LV_OBJ_FLAG_HIDDEN);
    }
    capture_path = getenv("SLK_CAPTURE_BMP");
    if(capture_path != NULL && capture_path[0] != '\0') {
        const char *capture_ms = getenv("SLK_CAPTURE_MS");
        capture_after_ms = capture_ms != NULL ? (uint32_t)atoi(capture_ms) : 500;
    }

    last_tick = SDL_GetTicks();
    while(!mouse_state.quit) {
        uint32_t now = SDL_GetTicks();
        uint32_t elapsed = now - last_tick;

        last_tick = now;
        process_events();
        lv_tick_inc(elapsed);
        lv_timer_handler();
        runtime_ms += elapsed;
        if(capture_after_ms > 0 && runtime_ms >= capture_after_ms) {
            save_framebuffer_bmp(capture_path);
            break;
        }
        SDL_Delay(5);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
