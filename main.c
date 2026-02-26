/**
 * @file main
 */

/*********************
 *      INCLUDES
 *********************/
#define _DEFAULT_SOURCE /* needed for usleep() */
#include <stdlib.h>
#include <unistd.h>

#include "lvgl/lvgl.h"
#include "modules/debug/debug.h"
#include "modules/event_bus/event_bus.h"
#include "modules/game_state/game_state.h"
#include "modules/gpio_driver/gpio_driver.h"
#include "modules/game_controller/game_controller.h"
#include "modules/led_logic/led_logic_event.h"
#include "modules/sound_logic/sound_logic_event.h"
#include "modules/ui_logic/ui_logic.event.h"
#include "ui/ui.h"

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_display_t *hal_init(int32_t w, int32_t h);

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

int main(int argc, char **argv)
{
    DEBUG_INFO(MODULE_MAIN, "#0 Application starting");

    /* Initialize debug system */
    DEBUG_TRACE(MODULE_MAIN, "#1 Initializing debug system");
    debug_init();

    DEBUG_INFO(MODULE_MAIN, "SquareLine Project starting");
    DEBUG_DEBUG(MODULE_MAIN, "Build type: %s", debug_get_build_type());

    /* Initialize event bus (must be first - other modules subscribe to it) */
    DEBUG_TRACE(MODULE_MAIN, "#2 Initializing event bus");
    event_bus_init();
    DEBUG_INFO(MODULE_MAIN, "Event bus initialized");

    /* Initialize LVGL */
    DEBUG_TRACE(MODULE_LVGL, "#3 Initializing LVGL");
    (void)argc; /*Unused*/
    (void)argv; /*Unused*/
    lv_init();
    DEBUG_INFO(MODULE_LVGL, "LVGL initialized successfully");

    /* Initialize the display and input devices */
    DEBUG_TRACE(MODULE_MAIN, "#4 Initializing HAL");
    hal_init(2560, 720);
    DEBUG_INFO(MODULE_MAIN, "HAL initialized with resolution 2560x720");

    /* Initialize UI */
    DEBUG_TRACE(MODULE_UI, "#5 Initializing UI");
    ui_init();
    DEBUG_INFO(MODULE_UI, "UI initialized successfully");

    /* Reset game state to defaults */
    DEBUG_TRACE(MODULE_MAIN, "#6 Initializing game state");
    game_state_reset();
    DEBUG_INFO(MODULE_MAIN, "Game state initialized");

    /* Subscribe game controller to events */
    DEBUG_TRACE(MODULE_MAIN, "#7 Initializing game controller");
    game_controller_init();
    DEBUG_INFO(MODULE_MAIN, "Game controller initialized");

    /* Subscribe UI controller to events */
    DEBUG_TRACE(MODULE_MAIN, "#8 Initializing UI controller");
    ui_controller_init();
    DEBUG_INFO(MODULE_MAIN, "UI controller initialized");

    /* Initialize GPIO driver */
    DEBUG_TRACE(MODULE_MAIN, "#9 Initializing GPIO driver");
    if (gpio_driver_init() < 0)
    {
        DEBUG_ERROR(MODULE_MAIN, "GPIO driver initialization failed!");
        fprintf(stderr, "Warning: GPIO not available, continuing without sensors\n");
    }
    else
    {
        DEBUG_INFO(MODULE_MAIN, "GPIO driver initialized");
    }

    /* Initialize Sound System */
    DEBUG_TRACE(MODULE_SOUND, "#10 Initializing audio system");
    if (!init_audio_system())
    {
        DEBUG_ERROR(MODULE_SOUND, "Audio system initialization failed!");
        fprintf(stderr, "Warning: Audio not available, continuing without sound\n");
    }
    else
    {
        DEBUG_INFO(MODULE_SOUND, "Audio system initialized successfully");
    }
    print_audio_driver_info();
    print_current_working_dir();

    /* Initialize LED System */
    DEBUG_TRACE(MODULE_LED, "#11 Initializing LED controller");
    leds = init_led_controller(3, 2, argc, argv);
    DEBUG_INFO(MODULE_LED, "LED controller initialized");

    set_brightness(&leds, 50);
    DEBUG_INFO(MODULE_LED, "LED brightness set to 50");

    DEBUG_INFO(MODULE_MAIN, "#12 Entering main loop");

    /* Main loop */
    while (1)
    {
        /* Update LED animation */
        update_led_animation(&leds);

        /* Poll GPIO sensors - publishes events on valid triggers */
        gpio_driver_poll();

        /* Run LVGL timer handler */
        lv_timer_handler();

        usleep(5 * 1000);
    }

    /* Shutdown */
    DEBUG_INFO(MODULE_MAIN, "Application shutdown started");

    gpio_driver_cleanup(0);
    lv_deinit();
    cleanup_audio_system();

    DEBUG_INFO(MODULE_MAIN, "Application shutdown completed");

    return 0;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * Initialize the Hardware Abstraction Layer (HAL) for the LVGL graphics
 * library
 */
static lv_display_t *hal_init(int32_t w, int32_t h)
{
    DEBUG_TRACE(MODULE_HAL, "#hal.1 Creating default group");
    lv_group_set_default(lv_group_create());
    DEBUG_DEBUG(MODULE_HAL, "Default group created");

    DEBUG_TRACE(MODULE_HAL, "#hal.2 Creating SDL window");
    lv_display_t *disp = lv_sdl_window_create(w, h);
    DEBUG_DEBUG(MODULE_HAL, "SDL window created: %dx%d", w, h);

    DEBUG_TRACE(MODULE_HAL, "#hal.3 Creating mouse input device");
    lv_indev_t *mouse = lv_sdl_mouse_create();
    lv_indev_set_group(mouse, lv_group_get_default());
    lv_indev_set_display(mouse, disp);
    lv_display_set_default(disp);
    DEBUG_DEBUG(MODULE_HAL, "Mouse input device configured");

    DEBUG_TRACE(MODULE_HAL, "#hal.4 Creating mousewheel input device");
    lv_indev_t *mousewheel = lv_sdl_mousewheel_create();
    lv_indev_set_display(mousewheel, disp);
    DEBUG_DEBUG(MODULE_HAL, "Mousewheel input device configured");

    DEBUG_TRACE(MODULE_HAL, "#hal.5 Creating keyboard input device");
    lv_indev_t *keyboard = lv_sdl_keyboard_create();
    lv_indev_set_display(keyboard, disp);
    lv_indev_set_group(keyboard, lv_group_get_default());
    DEBUG_DEBUG(MODULE_HAL, "Keyboard input device configured");

    DEBUG_INFO(MODULE_HAL, "HAL initialization completed");

    return disp;
}
