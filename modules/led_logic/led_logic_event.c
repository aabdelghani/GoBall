#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>

#include "led_logic_event.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

led_strip_controller_t leds;  // Now visible to all files including the header
const wbgr_color_t     COLOR_RED   = {0x00, 0x00, 0x00, 0xFF};  // R
const wbgr_color_t     COLOR_GREEN = {0x00, 0x00, 0xFF, 0x00};  // G
const wbgr_color_t     COLOR_BLUE  = {0x00, 0xFF, 0x00, 0x00};  // B
const wbgr_color_t     COLOR_WHITE = {0xFF, 0xFF, 0xFF, 0xFF};  // W+B+G+R

/* ── LED thread state (shared with main thread via atomics/volatiles) ── */
static pthread_t       led_thread;
static volatile bool   led_thread_running = false;
static volatile bool   animation_paused   = false;

/* Flash request: main thread writes, LED thread reads and executes */
static volatile bool         flash_pending  = false;
static volatile uint32_t     flash_duration = 0;
static volatile wbgr_color_t flash_color    = {0};

/* ── Helper: send both buffers to PIO (only called from LED thread) ── */
static void led_xfer(led_strip_controller_t* c)
{
    pio_sm_xfer_data(c->pio, c->sm1, PIO_DIR_TO_SM, sizeof(c->databuf1), c->databuf1);
    pio_sm_xfer_data(c->pio, c->sm2, PIO_DIR_TO_SM, sizeof(c->databuf2), c->databuf2);
}

/* ── LED thread: runs on core 3, owns all PIO transfers ── */
static void* led_thread_func(void* arg)
{
    led_strip_controller_t* controller = (led_strip_controller_t*)arg;

    /* Pin to core 3 */
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(3, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

    DEBUG_INFO(MODULE_LED, "LED thread started on core 3");

    while (led_thread_running)
    {
        if (!controller->enabled)
        {
            usleep(100000);  /* 100ms idle when disabled */
            continue;
        }

        /* Handle flash request from main thread */
        if (flash_pending)
        {
            flash_pending  = false;
            animation_paused = true;

            uint32_t     dur   = flash_duration;
            wbgr_color_t color = flash_color;
            uint8_t      brightness = controller->brightness;
            uint8_t      cycles = dur / 250;
            bool         flash_on = false;

            DEBUG_INFO(MODULE_LED, "Flash: duration=%ums, cycles=%u", dur, cycles);

            /* Clear first */
            memset(controller->databuf1, 0, sizeof(controller->databuf1));
            memset(controller->databuf2, 0, sizeof(controller->databuf2));
            led_xfer(controller);

            /* Flash loop */
            for (uint8_t i = 0; i < cycles && led_thread_running; i++)
            {
                flash_on = !flash_on;
                if (flash_on)
                {
                    for (uint8_t p = 0; p < PIXELS; p++)
                    {
                        controller->databuf1[4 * p + 0] = scale_brightness(color.w, brightness);
                        controller->databuf1[4 * p + 1] = scale_brightness(color.b, brightness);
                        controller->databuf1[4 * p + 2] = scale_brightness(color.r, brightness);
                        controller->databuf1[4 * p + 3] = scale_brightness(color.g, brightness);
                        controller->databuf2[4 * p + 0] = controller->databuf1[4 * p + 0];
                        controller->databuf2[4 * p + 1] = controller->databuf1[4 * p + 1];
                        controller->databuf2[4 * p + 2] = controller->databuf1[4 * p + 2];
                        controller->databuf2[4 * p + 3] = controller->databuf1[4 * p + 3];
                    }
                }
                else
                {
                    memset(controller->databuf1, 0, sizeof(controller->databuf1));
                    memset(controller->databuf2, 0, sizeof(controller->databuf2));
                }
                led_xfer(controller);
                usleep(50000);  /* 50ms per toggle, matches original timer */
            }

            animation_paused = false;
            DEBUG_INFO(MODULE_LED, "Flash complete, animation resumed");
            continue;
        }

        /* Normal animation — same speed as original (1 position per frame) */
        if (!animation_paused)
        {
            update_led_strip(controller, controller->databuf1, controller->train_positions,
                             NUM_TRAINS, TRAIN_LENGTH, PIXELS);
            update_led_strip(controller, controller->databuf2, controller->train_positions,
                             NUM_TRAINS, TRAIN_LENGTH, PIXELS);

            led_xfer(controller);

            for (uint8_t t = 0; t < NUM_TRAINS; t++)
            {
                controller->train_positions[t] = (controller->train_positions[t] + 1) % PIXELS;
            }
        }

        usleep(5000);  /* 5ms — matches original main loop timing */
    }

    DEBUG_INFO(MODULE_LED, "LED thread exiting");
    return NULL;
}

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

    controller.pio = pio_open(0);
    if (PIO_IS_ERR(controller.pio))
    {
        DEBUG_WARN(MODULE_LED, "PIO hardware not available (error %d) - LEDs disabled",
                   PIO_ERR_VAL(controller.pio));
        controller.pio     = NULL;
        controller.enabled = false;
        return controller;
    }
    DEBUG_DEBUG(MODULE_LED, "Using PIO: pio0");

    controller.sm1 = pio_claim_unused_sm(controller.pio, false);
    controller.sm2 = pio_claim_unused_sm(controller.pio, false);
    if (controller.sm1 < 0 || controller.sm2 < 0)
    {
        DEBUG_WARN(MODULE_LED, "Failed to claim PIO state machines - LEDs disabled");
        controller.enabled = false;
        return controller;
    }
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

    DEBUG_INFO(MODULE_LED, "LED controller initialization completed (enabled=%d)", controller.enabled);

    return controller;
}

void led_start_thread(led_strip_controller_t* controller)
{
    if (!controller->enabled)
    {
        DEBUG_WARN(MODULE_LED, "LEDs disabled, not starting thread");
        return;
    }
    led_thread_running = true;
    pthread_create(&led_thread, NULL, led_thread_func, controller);
    DEBUG_INFO(MODULE_LED, "LED thread launched (pinned to core 3)");
}

void led_stop_thread(void)
{
    if (led_thread_running)
    {
        led_thread_running = false;
        pthread_join(led_thread, NULL);
        DEBUG_INFO(MODULE_LED, "LED thread stopped");
    }
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

// Set all LEDs to a specific color (safe to call from main thread — just writes buffers)
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
    /* PIO transfer happens on LED thread's next cycle */
    DEBUG_DEBUG(MODULE_LED, "All LEDs set to specified color");
}

/* update_led_animation: now a no-op, animation runs on LED thread */
void update_led_animation(led_strip_controller_t* controller)
{
    (void)controller;
}

void update_led_strip(led_strip_controller_t* controller, uint8_t databuf[],
                      uint8_t train_positions[], uint8_t num_trains, uint8_t train_length,
                      uint8_t pixels)
{
    uint8_t brightness = controller->brightness;

    for (uint8_t i = 0; i < pixels; i++)
    {
        databuf[4 * i + 0] = scale_brightness(0xFF, brightness);
        databuf[4 * i + 1] = scale_brightness(0xFF, brightness);
        databuf[4 * i + 2] = scale_brightness(0xFF, brightness);
        databuf[4 * i + 3] = scale_brightness(0xFF, brightness);
    }

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
    }
}

// Flash control — now just signals the LED thread
void restore_animation(lv_timer_t* timer)
{
    (void)timer;
    /* No-op: flash is handled entirely on LED thread now */
}

void flash_toggle(lv_timer_t* timer)
{
    (void)timer;
    /* No-op: flash is handled entirely on LED thread now */
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
    if (!controller->enabled)
    {
        return;
    }
    DEBUG_INFO(MODULE_LED,
               "Triggering flash: duration=%dms, color=W:0x%02X B:0x%02X R:0x%02X G:0x%02X",
               duration_ms, color.w, color.b, color.r, color.g);

    /* Signal LED thread to execute the flash */
    flash_color    = color;
    flash_duration = duration_ms;
    __sync_synchronize();  /* memory barrier */
    flash_pending  = true;

    DEBUG_INFO(MODULE_LED, "Flash request queued for LED thread");
}
