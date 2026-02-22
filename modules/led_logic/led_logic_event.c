#include "led_logic_event.h"

#include <stdio.h>
#include <stdlib.h>

led_strip_controller_t leds;  // Now visible to all files including the header
const wbgr_color_t     COLOR_RED   = {0x00, 0x00, 0x00, 0xFF};  // R
const wbgr_color_t     COLOR_GREEN = {0x00, 0x00, 0xFF, 0x00};  // G
const wbgr_color_t     COLOR_BLUE  = {0x00, 0xFF, 0x00, 0x00};  // B
const wbgr_color_t     COLOR_WHITE = {0xFF, 0xFF, 0xFF, 0xFF};  // W+B+G+R

led_strip_controller_t init_led_controller(uint8_t default_gpio1, uint8_t default_gpio2, int argc,
                                           char** argv)
{
    DEBUG_TRACE(MODULE_LED, "init_led_controller called");
    DEBUG_INFO(MODULE_LED, "Initializing LED controller with GPIOs: %d, %d", default_gpio1,
               default_gpio2);

    led_strip_controller_t controller = {0};

    controller.gpio1      = default_gpio1;
    controller.gpio2      = default_gpio2;
    controller.brightness = 10;  // Default brightness
    DEBUG_DEBUG(MODULE_LED, "Default brightness set to: %d", controller.brightness);

    if (argc == 2)
    {
        controller.gpio1 = (uint8_t)strtoul(argv[1], NULL, 0);
        DEBUG_INFO(MODULE_LED, "GPIO1 overridden from command line to: %d", controller.gpio1);
    }

    controller.pio = pio0;
    DEBUG_DEBUG(MODULE_LED, "Using PIO: pio0");

    controller.sm1 = pio_claim_unused_sm(controller.pio, true);
    controller.sm2 = pio_claim_unused_sm(controller.pio, true);
    DEBUG_DEBUG(MODULE_LED, "Claimed state machines: SM1=%d, SM2=%d", controller.sm1,
                controller.sm2);

    pio_sm_config_xfer(controller.pio, controller.sm1, PIO_DIR_TO_SM, 256, 1);
    pio_sm_config_xfer(controller.pio, controller.sm2, PIO_DIR_TO_SM, 256, 1);
    DEBUG_DEBUG(MODULE_LED, "State machines configured for data transfer");

    controller.offset = pio_add_program(controller.pio, &ws2812_program);
    DEBUG_INFO(MODULE_LED, "LED Controller Initialized: Program at %d, SMs %d & %d, GPIOs %d & %d",
               controller.offset, controller.sm1, controller.sm2, controller.gpio1,
               controller.gpio2);

    pio_sm_clear_fifos(controller.pio, controller.sm1);
    pio_sm_clear_fifos(controller.pio, controller.sm2);
    DEBUG_DEBUG(MODULE_LED, "Cleared state machine FIFOs");

    pio_sm_set_clkdiv(controller.pio, controller.sm1, 1.0);
    pio_sm_set_clkdiv(controller.pio, controller.sm2, 1.0);
    DEBUG_DEBUG(MODULE_LED, "Set clock divider to 1.0");

    ws2812_program_init(controller.pio, controller.sm1, controller.offset, controller.gpio1,
                        800000.0, false);
    ws2812_program_init(controller.pio, controller.sm2, controller.offset, controller.gpio2,
                        800000.0, false);
    DEBUG_DEBUG(MODULE_LED, "WS2812 program initialized on both state machines");

    initialize_train_positions(controller.train_positions, NUM_TRAINS, PIXELS);

    controller.enabled = true;
    DEBUG_INFO(MODULE_LED, "LED controller initialization completed successfully");

    return controller;
}

void set_brightness(led_strip_controller_t* controller, uint8_t value)
{
    DEBUG_TRACE(MODULE_LED, "set_brightness called");
    DEBUG_INFO(MODULE_LED, "Setting brightness from %d to %d", controller->brightness, value);
    controller->brightness = value;
}

uint8_t scale_brightness(uint8_t color, uint8_t brightness)
{
    uint8_t result = (color * brightness) / 255;
    DEBUG_TRACE(MODULE_LED, "scale_brightness: color=0x%02X, brightness=%d -> result=0x%02X", color,
                brightness, result);
    return result;
}

void initialize_train_positions(uint8_t train_positions[], uint8_t num_trains, uint8_t pixels)
{
    DEBUG_TRACE(MODULE_LED, "initialize_train_positions called");
    DEBUG_INFO(MODULE_LED, "Initializing %d trains across %d pixels", num_trains, pixels);

    for (uint8_t i = 0; i < num_trains; i++)
    {
        train_positions[i] = (i * pixels) / num_trains;
        DEBUG_DEBUG(MODULE_LED, "Train %d position: %d", i, train_positions[i]);
    }
    DEBUG_INFO(MODULE_LED, "Train positions initialized");
}

// Global to track animation state
static bool     animation_paused    = false;
static uint32_t last_animation_time = 0;

// Set all LEDs to a specific color
void set_all_leds(led_strip_controller_t* controller, uint32_t color)
{
    DEBUG_TRACE(MODULE_LED, "set_all_leds called");
    if (!controller->enabled)
    {
        return;
    }
    DEBUG_INFO(MODULE_LED, "Setting all LEDs to color: 0x%08X", color);

    for (int i = 0; i < PIXELS; i++)
    {
        controller->databuf1[i] = color;
        controller->databuf2[i] = color;
    }
    pio_sm_xfer_data(controller->pio, controller->sm1, PIO_DIR_TO_SM, sizeof(controller->databuf1),
                     controller->databuf1);
    pio_sm_xfer_data(controller->pio, controller->sm2, PIO_DIR_TO_SM, sizeof(controller->databuf2),
                     controller->databuf2);
    DEBUG_DEBUG(MODULE_LED, "All LEDs set to specified color");
}
void update_led_animation(led_strip_controller_t* controller)
{
    DEBUG_TRACE(MODULE_LED, "update_led_animation called");

    if (!controller->enabled)
    {
        return;
    }

    if (animation_paused)
    {
        DEBUG_TRACE(MODULE_LED, "Animation paused, skipping update");
        return;
    }

    DEBUG_TRACE(MODULE_LED, "Updating LED animation");

    update_led_strip(controller, controller->databuf1, controller->train_positions, NUM_TRAINS,
                     TRAIN_LENGTH, PIXELS);
    update_led_strip(controller, controller->databuf2, controller->train_positions, NUM_TRAINS,
                     TRAIN_LENGTH, PIXELS);

    uint32_t t_start = lv_tick_get();

    pio_sm_xfer_data(controller->pio, controller->sm1, PIO_DIR_TO_SM, sizeof(controller->databuf1),
                     controller->databuf1);

    uint32_t elapsed_ms = lv_tick_elaps(t_start);
    if (elapsed_ms > 500)
    {
        controller->enabled = false;
        DEBUG_WARN(MODULE_LED, "LED DMA transfer timed out (%ums) - LEDs not connected, disabling LED output", elapsed_ms);
        fprintf(stderr, "Warning: LED strips not connected, disabling LED output\n");
        return;
    }

    pio_sm_xfer_data(controller->pio, controller->sm2, PIO_DIR_TO_SM, sizeof(controller->databuf2),
                     controller->databuf2);

    for (uint8_t t = 0; t < NUM_TRAINS; t++)
    {
        controller->train_positions[t] = (controller->train_positions[t] + 1) % PIXELS;
        DEBUG_TRACE(MODULE_LED, "Train %d moved to position: %d", t,
                    controller->train_positions[t]);
    }

    last_animation_time = lv_tick_get();
    DEBUG_TRACE(MODULE_LED, "LED animation update completed");
}

void update_led_strip(led_strip_controller_t* controller, uint8_t databuf[],
                      uint8_t train_positions[], uint8_t num_trains, uint8_t train_length,
                      uint8_t pixels)
{
    DEBUG_TRACE(MODULE_LED, "update_led_strip called");
    uint8_t brightness = controller->brightness;
    // Change brightness logging to TRACE
    DEBUG_TRACE(MODULE_LED, "Updating LED strip with brightness: %d", brightness);

    for (uint8_t i = 0; i < pixels; i++)
    {
        databuf[4 * i + 0] = scale_brightness(0xFF, brightness);
        databuf[4 * i + 1] = scale_brightness(0xFF, brightness);
        databuf[4 * i + 2] = scale_brightness(0xFF, brightness);
        databuf[4 * i + 3] = scale_brightness(0xFF, brightness);
    }
    // Change background logging to TRACE
    DEBUG_TRACE(MODULE_LED, "Base background set to white");

    for (uint8_t t = 0; t < num_trains; t++)
    {
        for (uint8_t l = 0; l < train_length; l++)
        {
            int led_pos              = (train_positions[t] + l) % pixels;
            databuf[4 * led_pos + 0] = scale_brightness(0x00, brightness);
            databuf[4 * led_pos + 1] = scale_brightness(0x00, brightness);
            databuf[4 * led_pos + 2] = scale_brightness(0x08, brightness);
            databuf[4 * led_pos + 3] = scale_brightness(0x65, brightness);
        }
        // Change individual train drawing logging to TRACE
        DEBUG_TRACE(MODULE_LED, "Train %d drawn at position %d", t, train_positions[t]);
    }
    // Change completion logging to TRACE
    DEBUG_TRACE(MODULE_LED, "LED strip update completed");
}
// Flash control functions
void restore_animation(lv_timer_t* timer)
{
    DEBUG_TRACE(MODULE_LED, "restore_animation called");
    led_strip_controller_t* controller = (led_strip_controller_t*)timer->user_data;
    animation_paused                   = false;
    DEBUG_INFO(MODULE_LED, "Animation resumed after flash");
    lv_timer_del(timer);
    DEBUG_DEBUG(MODULE_LED, "Restore timer deleted");
}

void flash_toggle(lv_timer_t* timer)
{
    DEBUG_TRACE(MODULE_LED, "flash_toggle called");
    flash_context_t*        context    = (flash_context_t*)timer->user_data;
    led_strip_controller_t* controller = context->controller;
    if (!controller->enabled)
    {
        return;
    }
    static bool flash_state = false;
    uint8_t                 brightness  = controller->brightness;

    DEBUG_DEBUG(MODULE_LED, "Flash state: %s, brightness: %d", flash_state ? "ON" : "OFF",
                brightness);

    if (flash_state)
    {
        DEBUG_DEBUG(MODULE_LED, "Setting flash color: W=0x%02X, B=0x%02X, R=0x%02X, G=0x%02X",
                    context->color.w, context->color.b, context->color.r, context->color.g);

        for (uint8_t i = 0; i < PIXELS; i++)
        {
            controller->databuf1[4 * i + 0] = scale_brightness(context->color.w, brightness);  // W
            controller->databuf1[4 * i + 1] = scale_brightness(context->color.b, brightness);  // B
            controller->databuf1[4 * i + 2] = scale_brightness(context->color.r, brightness);  // R
            controller->databuf1[4 * i + 3] = scale_brightness(context->color.g, brightness);  // G

            controller->databuf2[4 * i + 0] = controller->databuf1[4 * i + 0];
            controller->databuf2[4 * i + 1] = controller->databuf1[4 * i + 1];
            controller->databuf2[4 * i + 2] = controller->databuf1[4 * i + 2];
            controller->databuf2[4 * i + 3] = controller->databuf1[4 * i + 3];
        }
    }
    else
    {
        DEBUG_DEBUG(MODULE_LED, "Clearing LEDs for flash off state");
        memset(controller->databuf1, 0, sizeof(controller->databuf1));
        memset(controller->databuf2, 0, sizeof(controller->databuf2));
    }

    pio_sm_xfer_data(controller->pio, controller->sm1, PIO_DIR_TO_SM, sizeof(controller->databuf1),
                     controller->databuf1);
    pio_sm_xfer_data(controller->pio, controller->sm2, PIO_DIR_TO_SM, sizeof(controller->databuf2),
                     controller->databuf2);

    flash_state = !flash_state;
    DEBUG_DEBUG(MODULE_LED, "Flash toggle completed, new state: %s", flash_state ? "ON" : "OFF");
}

// Turn off all LEDs
void clear_all_leds(led_strip_controller_t* controller)
{
    DEBUG_TRACE(MODULE_LED, "clear_all_leds called");
    DEBUG_INFO(MODULE_LED, "Turning off all LEDs");
    set_all_leds(controller, 0x000000);
}

void trigger_flash_with_color(led_strip_controller_t* controller, uint32_t duration_ms,
                              wbgr_color_t color)
{
    DEBUG_TRACE(MODULE_LED, "trigger_flash_with_color called");
    if (!controller->enabled)
    {
        return;
    }
    DEBUG_INFO(MODULE_LED,
               "Triggering flash: duration=%dms, color=W:0x%02X B:0x%02X R:0x%02X G:0x%02X",
               duration_ms, color.w, color.b, color.r, color.g);

    animation_paused = true;
    DEBUG_DEBUG(MODULE_LED, "Animation paused for flash");

    clear_all_leds(controller);

    flash_context_t* context = malloc(sizeof(flash_context_t));
    context->controller      = controller;
    context->color           = color;
    DEBUG_DEBUG(MODULE_LED, "Flash context allocated and initialized");

    lv_timer_t* flash_timer  = lv_timer_create(flash_toggle, 50, context);
    uint8_t     flash_cycles = duration_ms / 250;
    lv_timer_set_repeat_count(flash_timer, flash_cycles);
    DEBUG_DEBUG(MODULE_LED, "Flash timer created: interval=50ms, cycles=%d", flash_cycles);

    lv_timer_t* restore_timer = lv_timer_create(restore_animation, duration_ms, controller);
    lv_timer_set_repeat_count(restore_timer, 1);
    DEBUG_DEBUG(MODULE_LED, "Restore timer created: delay=%dms", duration_ms);

    DEBUG_INFO(MODULE_LED, "Flash sequence started successfully");
}