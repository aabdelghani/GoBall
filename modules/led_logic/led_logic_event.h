#ifndef LED_LOGIC_EVENT_H
#define LED_LOGIC_EVENT_H

#include <stdint.h>

#include "../debug/debug.h"
#include "hardware/pio.h"
#include "lvgl/lvgl.h"
#include "utils/piolib/examples/ws2812.pio.h"

// Configuration Constants
#define NUM_TRAINS 5
#define TRAIN_LENGTH 10
#define PIXELS 144

typedef struct
{
    PIO     pio;
    uint8_t sm1;
    uint8_t sm2;
    uint8_t offset;
    uint8_t gpio1;
    uint8_t gpio2;
    uint8_t train_positions[NUM_TRAINS];
    uint8_t databuf1[PIXELS * 4];
    uint8_t databuf2[PIXELS * 4];
    uint8_t brightness;
    bool    enabled;
} led_strip_controller_t;

typedef struct
{
    uint8_t w;
    uint8_t b;
    uint8_t g;
    uint8_t r;
} wbgr_color_t;

typedef struct
{
    led_strip_controller_t* controller;
    wbgr_color_t            color;
} flash_context_t;

// Global color constants
extern const wbgr_color_t COLOR_RED;
extern const wbgr_color_t COLOR_GREEN;
extern const wbgr_color_t COLOR_BLUE;
extern const wbgr_color_t COLOR_WHITE;

// Global controller instance (extern declaration)
extern led_strip_controller_t leds;

// Initialization
led_strip_controller_t init_led_controller(uint8_t default_gpio1, uint8_t default_gpio2, int argc,
                                           char** argv);

// LED thread (runs on core 3, owns all PIO transfers)
void led_start_thread(led_strip_controller_t* controller);
void led_stop_thread(void);

// Brightness Control
void    set_brightness(led_strip_controller_t* controller, uint8_t value);
uint8_t scale_brightness(uint8_t color, uint8_t brightness);

// Animation Control
void update_led_animation(led_strip_controller_t* controller);

// Train Management
void initialize_train_positions(uint8_t train_positions[], uint8_t num_trains, uint8_t pixels);

// LED Strip Management
void update_led_strip(led_strip_controller_t* controller, uint8_t databuf[],
                      uint8_t train_positions[], uint8_t num_trains, uint8_t train_length,
                      uint8_t pixels);

// Flash Control
void trigger_flash_with_color(led_strip_controller_t* controller, uint32_t duration_ms,
                              wbgr_color_t color);
void set_all_leds(led_strip_controller_t* controller, uint32_t color);
void clear_all_leds(led_strip_controller_t* controller);

// LED kill switch (UI toggle — thread stays alive but strip goes dark)
void led_set_killed(bool killed);
void restore_animation(lv_timer_t* timer);
void flash_toggle(lv_timer_t* timer);
#endif  // LED_LOGIC_EVENT_H