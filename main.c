/**
 * @file main
 *
 */

/*********************
 *      INCLUDES
 *********************/
#define _DEFAULT_SOURCE /* needed for usleep() */
#include <stdlib.h>
#include <unistd.h>

#include "lvgl/lvgl.h"
#include "modules/debug/debug.h"
#include "modules/led_logic/led_logic_event.h"
#include "modules/logic/gpio_event.h"
#include "modules/sound_logic/sound_logic_event.h"
#include "modules/ui_logic/ui_logic.event.h"
#include "ui/ui.h"
/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static lv_display_t *hal_init(int32_t w, int32_t h);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *      VARIABLES
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

int main(int argc, char **argv)
{
    DEBUG_INFO(MODULE_MAIN, "#0 Application starting");

    /* Initialize debug system - auto-configures based on build type */
    DEBUG_TRACE(MODULE_MAIN, "#1 Initializing debug system");
    debug_init();

    DEBUG_INFO(MODULE_MAIN, "SquareLine Project starting");
    DEBUG_DEBUG(MODULE_MAIN, "Build type: %s", debug_get_build_type());

    /* Your existing initialization code */
    DEBUG_TRACE(MODULE_LVGL, "#2 Initializing LVGL");
    (void)argc; /*Unused*/
    (void)argv; /*Unused*/

    /*Initialize LVGL*/
    lv_init();
    DEBUG_INFO(MODULE_LVGL, "LVGL initialized successfully");

    /*Initialize the display, and the input devices*/
    DEBUG_TRACE(MODULE_MAIN, "#3 Initializing HAL");
    hal_init(2560, 720);
    DEBUG_INFO(MODULE_MAIN, "HAL initialized with resolution 2560x720");

    DEBUG_TRACE(MODULE_UI, "#4 Initializing UI");
    ui_init();
    DEBUG_INFO(MODULE_UI, "UI initialized successfully");

    /*My Custom Logic */
    struct gpiod_line_bulk  event_lines;
    struct gpiod_line_event event;

    /*********************
     *      GAME INITIALIZATION
     *********************/
    DEBUG_TRACE(MODULE_LOGIC, "#5 Initializing game logic");
    if (logic_initialize_game(NUM_PLAYERS) < 0)
    {
        DEBUG_ERROR(MODULE_LOGIC, "! Game initialization failed!");
        return EXIT_FAILURE;
    }
    DEBUG_INFO(MODULE_LOGIC, "Game initialized with %d players", NUM_PLAYERS);

    /* Initialize Sound System */
    DEBUG_TRACE(MODULE_SOUND, "#6 Initializing audio system");
    if (!init_audio_system())
    {
        DEBUG_ERROR(MODULE_SOUND, "Audio system initialization failed!");
        fprintf(stderr, "Failed to initialize audio!\n");
        return EXIT_FAILURE;
    }
    DEBUG_INFO(MODULE_SOUND, "Audio system initialized successfully");
    print_audio_driver_info();
    print_current_working_dir();

    /* Initialize LED System */
    DEBUG_TRACE(MODULE_LED, "#7 Initializing LED controller");
    // Remove the local 'leds' declaration and keep only initialization:
    leds = init_led_controller(3, 2, argc, argv);  // Uses the global variable
    DEBUG_INFO(MODULE_LED, "LED controller initialized");

    set_brightness(&leds, 50);  // Medium brightness 0 - 255 brightness
    DEBUG_INFO(MODULE_LED, "LED brightness set to 50");

    DEBUG_INFO(MODULE_MAIN, "#8 Entering main loop");

    // In your main code or event handler:
    int loop_counter = 0;
    while (1)
    {
        loop_counter++;

        // Update animation
        DEBUG_TRACE(MODULE_LED, "#%d.%d Updating LED animation", loop_counter, 1);
        update_led_animation(&leds);

        // Wait for events on any of the lines (non-blocking call)
        DEBUG_TRACE(MODULE_LOGIC, "#%d.%d Handling GPIO events", loop_counter, 2);
        logic_handle_events(&event_lines, &event, num_players);

        /* Periodically call the lv_task handler.
         * It could be done in a timer interrupt or an OS task too.*/
        DEBUG_TRACE(MODULE_LVGL, "#%d.%d Running LVGL timer handler", loop_counter, 3);
        lv_timer_handler();

        usleep(5 * 1000);
    }

    DEBUG_INFO(MODULE_MAIN, "#9 Application shutdown started");

    DEBUG_TRACE(MODULE_LVGL, "#9.1 Deinitializing LVGL");
    lv_deinit();
    DEBUG_INFO(MODULE_LVGL, "LVGL deinitialized");

    DEBUG_TRACE(MODULE_SOUND, "#9.2 Cleaning up audio system");
    cleanup_audio_system();  // Cleanup audio system
    DEBUG_INFO(MODULE_SOUND, "Audio system cleaned up");

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

    // LV_IMAGE_DECLARE(mouse_cursor_icon); /*Declare the image file.*/
    // cursor_obj = lv_image_create(lv_screen_active()); /*Create an image object for the cursor */
    // lv_image_set_src(cursor_obj, &mouse_cursor_icon);           /*Set the image source*/
    // lv_indev_set_cursor(mouse, cursor_obj);             /*Connect the image  object to the
    // driver*/

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