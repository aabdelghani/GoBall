// Enable POSIX features for timers and signals
#define _POSIX_C_SOURCE 199309L

/*********************
 *      INCLUDES
 *********************/
#include "gpio_event.h"

#include <gpiod.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "ui/ui.h"
/*********************
 *      TYPEDEFS
 *********************/

/*********************
 *      STATIC VARIABLES
 *********************/
unsigned int sensor_pins[NUM_SENSORS] = {17, 26, 27, 24};  // GPIO pin numbers for sensors
Player       players[MAX_PLAYERS];                         // Array of players
int          current_player_index = 0;                     // Index of the current player
// Timer-based debouncing
timer_t                debounce_timers[NUM_SENSORS];    // POSIX timers for each sensor
volatile sig_atomic_t  sensor_debouncing[NUM_SENSORS];  // Flags: 1 = in debounce period, 0 = ready
struct gpiod_chip*     chip = NULL;                     // GPIO chip handle
struct gpiod_line_bulk lines;                           // GPIO lines for sensors
int                    num_players       = 0;           // Number of players
GameMode               current_game_mode = GAME_MODE_STROKE_PLAY;  // Default value
uint8_t      all_players_completed = 1;  // Flag to check if all players have completed the game
unsigned int playersmp[2]          = {0};

/*********************
 *      FUNCTIONS
 *********************/

/**
 * @brief Convert GPIO pin number to sensor index.
 * @param pin_number The GPIO pin number (17, 26, 27, or 24).
 * @return Sensor index (0-3), or -1 if not found.
 */
static int pin_to_sensor_index(unsigned int pin_number)
{
    for (int i = 0; i < NUM_SENSORS; i++)
    {
        if (sensor_pins[i] == pin_number)
        {
            return i;
        }
    }
    return -1;  // Pin not found
}

/**
 * @brief Signal handler for debounce timer expiry.
 * Called when a debounce timer expires, clears the debouncing flag for that sensor.
 */
static void debounce_timer_handler(int sig, siginfo_t* si, void* uc)
{
    (void)sig;
    (void)uc;
    int sensor_index = si->si_value.sival_int;
    if (sensor_index >= 0 && sensor_index < NUM_SENSORS)
    {
        sensor_debouncing[sensor_index] = 0;  // Clear debounce flag
        DEBUG_TRACE(MODULE_GPIO, "Debounce timer expired for sensor %d - ready for next event",
                    sensor_index);
    }
}

/**
 * @brief Initialize debounce timers for all sensors.
 * @return 0 on success, -1 on failure.
 */
static int init_debounce_timers(void)
{
    struct sigaction sa;
    struct sigevent  sev;

    // Set up signal handler for debounce timer
    sa.sa_flags     = SA_SIGINFO;
    sa.sa_sigaction = debounce_timer_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(DEBOUNCE_SIGNAL, &sa, NULL) == -1)
    {
        DEBUG_ERROR(MODULE_GPIO, "Failed to set up debounce signal handler");
        return -1;
    }

    // Create a timer for each sensor
    for (int i = 0; i < NUM_SENSORS; i++)
    {
        sensor_debouncing[i] = 0;  // Initialize as not debouncing

        sev.sigev_notify          = SIGEV_SIGNAL;
        sev.sigev_signo           = DEBOUNCE_SIGNAL;
        sev.sigev_value.sival_int = i;  // Pass sensor index to handler

        if (timer_create(CLOCK_MONOTONIC, &sev, &debounce_timers[i]) == -1)
        {
            DEBUG_ERROR(MODULE_GPIO, "Failed to create debounce timer for sensor %d", i);
            return -1;
        }
        DEBUG_TRACE(MODULE_GPIO, "Created debounce timer for sensor %d", i);
    }

    DEBUG_INFO(MODULE_GPIO, "Debounce timers initialized (interval: %d ms)", DEBOUNCE_TIME_MS);
    return 0;
}

/**
 * @brief Start debounce timer for a specific sensor.
 * @param sensor_index Index of the sensor (0-3).
 */
static void start_debounce_timer(int sensor_index)
{
    if (sensor_index < 0 || sensor_index >= NUM_SENSORS) return;

    struct itimerspec its;
    its.it_value.tv_sec     = DEBOUNCE_TIME_MS / 1000;
    its.it_value.tv_nsec    = (DEBOUNCE_TIME_MS % 1000) * 1000000;
    its.it_interval.tv_sec  = 0;  // One-shot timer
    its.it_interval.tv_nsec = 0;

    sensor_debouncing[sensor_index] = 1;  // Set debouncing flag

    if (timer_settime(debounce_timers[sensor_index], 0, &its, NULL) == -1)
    {
        DEBUG_ERROR(MODULE_GPIO, "Failed to start debounce timer for sensor %d", sensor_index);
        sensor_debouncing[sensor_index] = 0;  // Clear on error
    }
    else
    {
        DEBUG_TRACE(MODULE_GPIO, "Started debounce timer for sensor %d (%d ms)", sensor_index,
                    DEBOUNCE_TIME_MS);
    }
}

bool is_it_first_turn = 0;  // Flag to check if it's the first turn

void two_player_highlight_pattern(GameMode game_mode, HoleMode hole_mode)
{  // Update counter (special reset logic)
    highliting_counter = (highliting_counter >= 8) ? 1 : highliting_counter + 1;
    DEBUG_DEBUG(MODULE_UI, "Two player highlight - counter: %d, game_mode: %d, hole_mode: %d",
                highliting_counter, game_mode, hole_mode);

    switch (game_mode)
    {
        case GAME_MODE_MATCH_PLAY:
            switch (hole_mode)
            {
                // P1=01 p2=23 p1=45(01) p2=67(23)
                case NINE_HOLES:
                    if (highliting_counter % 4 == 0 || highliting_counter % 4 == 1)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player/Team 1");
                        if (current_match_play_mode == MATCH_PLAY_MODE_1V1)
                        {
                            lv_obj_set_style_bg_color(ui_MP1V19HGSP1SPanel, lv_color_hex(COLOR_1),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(ui_MP1V19HGSP2SPanel, lv_color_hex(COLOR_2),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        else if (current_match_play_mode == MATCH_PLAY_MODE_2V2)
                        {
                            lv_obj_set_style_bg_color(ui_MP2V29HGST1SPanel, lv_color_hex(COLOR_1),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(ui_MP2V29HGST2SPanel, lv_color_hex(COLOR_2),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                    }
                    else  //(highliting_counter%4 == 2 ||highliting_counter%4 == 3 )
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player/Team 2");
                        if (current_match_play_mode == MATCH_PLAY_MODE_1V1)
                        {
                            lv_obj_set_style_bg_color(ui_MP1V19HGSP1SPanel, lv_color_hex(COLOR_2),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(ui_MP1V19HGSP2SPanel, lv_color_hex(COLOR_1),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        else if (current_match_play_mode == MATCH_PLAY_MODE_2V2)
                        {
                            lv_obj_set_style_bg_color(ui_MP2V29HGST1SPanel, lv_color_hex(COLOR_2),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(ui_MP2V29HGST2SPanel, lv_color_hex(COLOR_1),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                    }
                    break;

                case EIGHTEEN_HOLES:
                    // Pattern for Match Play 18 holes: P1/T1=01,45,89,1213,1617 |
                    // P2/T2=23,67,1011,1415
                    if (highliting_counter % 4 == 0 || highliting_counter % 4 == 1)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player/Team 1 (18H Match Play)");
                        if (current_match_play_mode == MATCH_PLAY_MODE_1V1)
                        {
                            lv_obj_set_style_bg_color(ui_MP1V118HGSP1SPanel, lv_color_hex(COLOR_1),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(ui_MP1V118HGSP2SPanel, lv_color_hex(COLOR_2),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        else if (current_match_play_mode == MATCH_PLAY_MODE_2V2)
                        {
                            lv_obj_set_style_bg_color(ui_MP2V218HGST1SPanel, lv_color_hex(COLOR_1),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(ui_MP2V218HGST2SPanel, lv_color_hex(COLOR_2),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                    }
                    else  // (highliting_counter % 4 == 2 || highliting_counter % 4 == 3)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player/Team 2 (18H Match Play)");
                        if (current_match_play_mode == MATCH_PLAY_MODE_1V1)
                        {
                            lv_obj_set_style_bg_color(ui_MP1V118HGSP1SPanel, lv_color_hex(COLOR_2),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(ui_MP1V118HGSP2SPanel, lv_color_hex(COLOR_1),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        else if (current_match_play_mode == MATCH_PLAY_MODE_2V2)
                        {
                            lv_obj_set_style_bg_color(ui_MP2V218HGST1SPanel, lv_color_hex(COLOR_2),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(ui_MP2V218HGST2SPanel, lv_color_hex(COLOR_1),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                    }
                    break;

                    DEBUG_DEBUG(MODULE_GAME, "Highlighting inside game_mode: %d, hole_mode: %d",
                                game_mode, hole_mode);
                    // Handle highlight cases
                    DEBUG_TRACE(MODULE_GAME, "Highlight Counter Value: %d", highliting_counter);
                    break;
            }
            break;
        case GAME_MODE_QUOTA:
            switch (hole_mode)
            {
                case NINE_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Quota 2P 9H highlighting");
                    DEBUG_TRACE(MODULE_GAME, "Highlight Counter Value: %d", highliting_counter);

                    if (highliting_counter == 0 || highliting_counter == 1 ||
                        highliting_counter == 4 || highliting_counter == 5 ||
                        highliting_counter == 8)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Quota 2P 9H - Highlighting Player 1");
                        lv_obj_set_style_bg_color(ui_Q2P9HGSP1SPar3Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P9HGSP1SPar4Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P9HGSP1SPar5Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P9HGSP2SPar3Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P9HGSP2SPar4Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P9HGSP2SPar5Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else
                    {  // Cases 2,3,6,7
                        DEBUG_DEBUG(MODULE_UI, "Quota 2P 9H - Highlighting Player 2");
                        lv_obj_set_style_bg_color(ui_Q2P9HGSP1SPar3Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P9HGSP1SPar4Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P9HGSP1SPar5Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P9HGSP2SPar3Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P9HGSP2SPar4Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P9HGSP2SPar5Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    break;
                case EIGHTEEN_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Quota 2P 18H highlighting");
                    DEBUG_TRACE(MODULE_GAME, "Highlight Counter Value: %d", highliting_counter);

                    if (highliting_counter == 0 || highliting_counter == 1 ||
                        highliting_counter == 4 || highliting_counter == 5 ||
                        highliting_counter == 8)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Quota 2P 18H - Highlighting Player 1");
                        lv_obj_set_style_bg_color(ui_Q2P18HGSP1SPar3Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P18HGSP1SPar4Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P18HGSP1SPar5Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P18HGSP2SPar3Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P18HGSP2SPar4Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P18HGSP2SPar5Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else
                    {  // Cases 2,3,6,7
                        DEBUG_DEBUG(MODULE_UI, "Quota 2P 18H - Highlighting Player 2");
                        lv_obj_set_style_bg_color(ui_Q2P18HGSP1SPar3Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P18HGSP1SPar4Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P18HGSP1SPar5Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P18HGSP2SPar3Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P18HGSP2SPar4Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P18HGSP2SPar5Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    break;
            }
            break;
        case GAME_MODE_VEGAS:
            DEBUG_TRACE(MODULE_UI, "Vegas mode - no highlighting pattern defined");
            break;
        case GAME_MODE_STROKE_PLAY:
            switch (hole_mode)
            {
                case NINE_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Stroke Play - 9 holes highlighting");
                    DEBUG_TRACE(MODULE_GAME, "Highlight Counter Value: %d", highliting_counter);

                    // Handle highlight cases
                    if (highliting_counter == 0)
                    {
                        DEBUG_DEBUG(MODULE_UI, "No highlight - reset state");
                        lv_obj_set_style_bg_color(ui_SP2P9HGSP1SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP2P9HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 1 || highliting_counter == 4 ||
                             highliting_counter == 5 || highliting_counter == 8)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 1");
                        lv_obj_set_style_bg_color(ui_SP2P9HGSP1SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP2P9HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);

                    }  // zoom
                    else
                    {  // Cases 2,3,6,7
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 2");
                        lv_obj_set_style_bg_color(ui_SP2P9HGSP1SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP2P9HGSP2SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    break;
                case EIGHTEEN_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Stroke Play - 18 holes highlighting");
                    // Handle highlight cases
                    if (highliting_counter == 0)
                    {
                        DEBUG_DEBUG(MODULE_UI, "No highlight - reset state");
                        lv_obj_set_style_bg_color(ui_SP2P18HGSP1SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP2P18HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 1 || highliting_counter == 4 ||
                             highliting_counter == 5 || highliting_counter == 8)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 1");
                        lv_obj_set_style_bg_color(ui_SP2P18HGSP1SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP2P18HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else
                    {  // Cases 2,3,6,7
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 2");
                        lv_obj_set_style_bg_color(ui_SP2P18HGSP1SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP2P18HGSP2SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    break;
            }
            break;
    }
}

void three_player_highlight_pattern(GameMode game_mode, HoleMode hole_mode)
{
    // Update counter (special reset logic)
    highliting_counter = (highliting_counter >= 12) ? 1 : highliting_counter + 1;
    DEBUG_DEBUG(MODULE_UI, "Three player highlight - counter: %d, game_mode: %d, hole_mode: %d",
                highliting_counter, game_mode, hole_mode);

    switch (game_mode)
    {
        case GAME_MODE_MATCH_PLAY:
            DEBUG_TRACE(MODULE_UI, "Match Play mode - no three player highlighting defined");
            break;
        case GAME_MODE_QUOTA:
            switch (hole_mode)
            {
                case NINE_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Quota 3P 9H highlighting");
                    DEBUG_TRACE(MODULE_GAME, "Highlight Counter Value: %d", highliting_counter);

                    if (highliting_counter == 1 || highliting_counter == 6 ||
                        highliting_counter == 7 || highliting_counter == 12)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Quota 3P 9H - Highlighting Player 1");
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP1SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP1SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP1SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP2SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP2SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP2SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP3SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP3SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP3SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 2 || highliting_counter == 3 ||
                             highliting_counter == 8 || highliting_counter == 9)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Quota 3P 9H - Highlighting Player 2");
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP1SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP1SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP1SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP2SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP2SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP2SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP3SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP3SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP3SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else
                    {   // Cases 4,5,10,11
                        DEBUG_DEBUG(MODULE_UI, "Quota 3P 9H - Highlighting Player 3");
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP1SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP1SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP1SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP2SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP2SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP2SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP3SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP3SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP3SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    break;
                case EIGHTEEN_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Quota 3P 18H highlighting");
                    DEBUG_TRACE(MODULE_GAME, "Highlight Counter Value: %d", highliting_counter);

                    if (highliting_counter == 1 || highliting_counter == 6 ||
                        highliting_counter == 7 || highliting_counter == 12)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Quota 3P 18H - Highlighting Player 1");
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP1SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP1SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP1SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP2SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP2SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP2SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP3SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP3SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP3SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 2 || highliting_counter == 3 ||
                             highliting_counter == 8 || highliting_counter == 9)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Quota 3P 18H - Highlighting Player 2");
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP1SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP1SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP1SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP2SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP2SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP2SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP3SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP3SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP3SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else
                    {   // Cases 4,5,10,11
                        DEBUG_DEBUG(MODULE_UI, "Quota 3P 18H - Highlighting Player 3");
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP1SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP1SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP1SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP2SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP2SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP2SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP3SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP3SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP3SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    break;
            }
            break;
        case GAME_MODE_VEGAS:
        case GAME_MODE_STROKE_PLAY:
            switch (hole_mode)
            {
                case NINE_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Three Player - 9 holes highlighting");
                    // Handle highlight cases
                    if (highliting_counter == 0)
                    {
                        DEBUG_DEBUG(MODULE_UI, "No highlight - default state");
                        lv_obj_set_style_bg_color(ui_SP3P9HGSP1SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P9HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P9HGSP3SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 1 || highliting_counter == 6 ||
                             highliting_counter == 7 || highliting_counter == 12)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 1");
                        lv_obj_set_style_bg_color(ui_SP3P9HGSP1SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P9HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P9HGSP3SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 2 || highliting_counter == 3 ||
                             highliting_counter == 8 || highliting_counter == 9)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 2");
                        lv_obj_set_style_bg_color(ui_SP3P9HGSP1SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P9HGSP2SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P9HGSP3SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else
                    {  // Cases 4,5,10,11
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 3");
                        lv_obj_set_style_bg_color(ui_SP3P9HGSP1SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P9HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P9HGSP3SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    break;
                case EIGHTEEN_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Three Player - 18 holes highlighting");
                    // Handle highlight cases
                    if (highliting_counter == 0)
                    {
                        DEBUG_DEBUG(MODULE_UI, "No highlight - default state");
                        lv_obj_set_style_bg_color(ui_SP3P18HGSP1SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P18HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P18HGSP3SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 1 || highliting_counter == 6 ||
                             highliting_counter == 7 || highliting_counter == 12)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 1");
                        lv_obj_set_style_bg_color(ui_SP3P18HGSP1SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P18HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P18HGSP3SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 2 || highliting_counter == 3 ||
                             highliting_counter == 8 || highliting_counter == 9)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 2");
                        lv_obj_set_style_bg_color(ui_SP3P18HGSP1SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P18HGSP2SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P18HGSP3SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else
                    {  // Cases 4,5,10,11
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 3");
                        lv_obj_set_style_bg_color(ui_SP3P18HGSP1SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P18HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P18HGSP3SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    break;
            }
            break;
    }
}

void four_player_highlight_pattern(GameMode game_mode, HoleMode hole_mode)
{
    // Update counter (special reset logic)
    highliting_counter = (highliting_counter >= 24) ? 1 : highliting_counter + 1;
    DEBUG_DEBUG(MODULE_UI, "Four player highlight - counter: %d, game_mode: %d, hole_mode: %d",
                highliting_counter, game_mode, hole_mode);

    switch (game_mode)
    {
        case GAME_MODE_MATCH_PLAY:
            DEBUG_TRACE(MODULE_UI, "Match Play mode - no four player highlighting defined");
            break;
        case GAME_MODE_QUOTA:
            DEBUG_TRACE(MODULE_UI, "Quota mode - no four player highlighting defined");
            break;
        case GAME_MODE_VEGAS:
        case GAME_MODE_STROKE_PLAY:
            switch (hole_mode)
            {
                case NINE_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Four Player - 9 holes highlighting");
                    // Handle highlight cases
                    if (highliting_counter == 0)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 1 by default");
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP1SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP3SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP4SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 1 || highliting_counter == 8 ||
                             highliting_counter == 9 || highliting_counter == 16 ||
                             highliting_counter == 17 || highliting_counter == 24)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 1");
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP1SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP3SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP4SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 2 || highliting_counter == 3 ||
                             highliting_counter == 10 || highliting_counter == 11 ||
                             highliting_counter == 18 || highliting_counter == 19)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 2");
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP2SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP1SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP3SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP4SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 4 || highliting_counter == 5 ||
                             highliting_counter == 12 || highliting_counter == 13 ||
                             highliting_counter == 20 || highliting_counter == 21)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 3");
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP3SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP1SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP4SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else
                    {  // 6,7,14,15,22,23,etc.
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 4");
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP4SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP1SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP3SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    break;
                case EIGHTEEN_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Four Player - 18 holes highlighting");
                    // Handle highlight cases
                    if (highliting_counter == 0)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 1 by default");
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP1SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP3SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP4SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 1 || highliting_counter == 8 ||
                             highliting_counter == 9 || highliting_counter == 16 ||
                             highliting_counter == 17 || highliting_counter == 24)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 1");
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP1SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP3SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP4SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 2 || highliting_counter == 3 ||
                             highliting_counter == 10 || highliting_counter == 11 ||
                             highliting_counter == 18 || highliting_counter == 19)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 2");
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP2SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP1SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP3SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP4SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 4 || highliting_counter == 5 ||
                             highliting_counter == 12 || highliting_counter == 13 ||
                             highliting_counter == 20 || highliting_counter == 21)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 3");
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP3SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP1SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP4SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else
                    {  // 6,7,14,15,22,23,etc.
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 4");
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP4SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP1SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP3SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    break;
            }
            break;
    }
}

void update_player_highlight(uint8_t detection_count, uint8_t current_hole, uint8_t num_players)
{
    DEBUG_TRACE(
        MODULE_UI,
        "Updating player highlight - detection_count: %d, current_hole: %d, num_players: %d",
        detection_count, current_hole, num_players);

    uint8_t highlighted_player = 1;  // Default to player 1

    switch (current_game_mode)
    {
        case GAME_MODE_MATCH_PLAY:
            DEBUG_DEBUG(MODULE_UI, "Match Play mode highlighting");
            switch (current_hole_mode)
            {
                case NINE_HOLES:
                    switch (num_players)
                    {
                        case 2:  // Exact pattern for 2-player stroke play
                            DEBUG_DEBUG(MODULE_UI, "Match Play - 2 players, 9 holes");
                            two_player_highlight_pattern(GAME_MODE_MATCH_PLAY, NINE_HOLES);
                            break;

                        default:
                            DEBUG_WARN(MODULE_UI, "Unsupported player count for Match Play: %d",
                                       num_players);
                            highlighted_player = 1;
                            break;
                    }
                    break;
                case EIGHTEEN_HOLES:
                    switch (num_players)
                    {
                        case 2:
                            DEBUG_DEBUG(MODULE_UI, "Match Play - 2 players, 18 holes");
                            two_player_highlight_pattern(GAME_MODE_MATCH_PLAY, EIGHTEEN_HOLES);
                            break;
                        default:
                            DEBUG_WARN(MODULE_UI, "Unsupported player count for Match Play 18H: %d",
                                       num_players);
                            highlighted_player = 1;
                            break;
                    }
                    break;

                default:
                    DEBUG_WARN(MODULE_UI, "Unsupported hole mode for Match Play: %d",
                               current_hole_mode);
                    break;
            }
            break;
        case GAME_MODE_QUOTA:
            DEBUG_DEBUG(MODULE_UI, "Quota mode highlighting");
            switch (current_hole_mode)
            {
                case NINE_HOLES:
                case EIGHTEEN_HOLES:
                    if (num_players == 2)
                    {
                        two_player_highlight_pattern(GAME_MODE_QUOTA, current_hole_mode);
                    }
                    else if (num_players == 3)
                    {
                        three_player_highlight_pattern(GAME_MODE_QUOTA, current_hole_mode);
                    }
                    break;
            }
            break;

        case GAME_MODE_VEGAS:
            DEBUG_DEBUG(MODULE_UI, "Vegas mode - using default alternating pattern");
            // Default alternating pattern for other modes
            highlighted_player = (detection_count % num_players) + 1;
            DEBUG_TRACE(MODULE_UI, "Vegas mode highlighted player: %d", highlighted_player);
            break;

        case GAME_MODE_STROKE_PLAY:
            DEBUG_DEBUG(MODULE_UI, "Stroke Play mode highlighting");
            switch (current_hole_mode)
            {
                case NINE_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Stroke Play - 9 holes");
                    switch (num_players)
                    {
                        case 2:  // Exact pattern for 2-player stroke play
                            DEBUG_DEBUG(MODULE_UI, "Stroke Play - 2 players, 9 holes");
                            two_player_highlight_pattern(GAME_MODE_STROKE_PLAY, NINE_HOLES);
                            break;

                        case 3:
                            DEBUG_DEBUG(MODULE_UI, "Stroke Play - 3 players, 9 holes");
                            three_player_highlight_pattern(GAME_MODE_STROKE_PLAY, NINE_HOLES);
                            break;
                        case 4:
                            DEBUG_DEBUG(MODULE_UI, "Stroke Play - 4 players, 9 holes");
                            four_player_highlight_pattern(GAME_MODE_STROKE_PLAY, NINE_HOLES);
                            break;

                        default:
                            DEBUG_WARN(MODULE_UI,
                                       "Unsupported player count for Stroke Play 9 holes: %d",
                                       num_players);
                            highlighted_player = 1;
                            break;
                    }
                    break;

                case EIGHTEEN_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Stroke Play - 18 holes");
                    switch (num_players)
                    {
                        case 2:  // Exact pattern for 2-player stroke play
                            DEBUG_DEBUG(MODULE_UI, "Stroke Play - 2 players, 18 holes");
                            two_player_highlight_pattern(GAME_MODE_STROKE_PLAY, EIGHTEEN_HOLES);
                            break;
                        case 3:
                            DEBUG_DEBUG(MODULE_UI, "Stroke Play - 3 players, 18 holes");
                            three_player_highlight_pattern(GAME_MODE_STROKE_PLAY, EIGHTEEN_HOLES);
                            break;
                        case 4:
                            DEBUG_DEBUG(MODULE_UI, "Stroke Play - 4 players, 18 holes");
                            four_player_highlight_pattern(GAME_MODE_STROKE_PLAY, EIGHTEEN_HOLES);
                            break;
                        default:
                            DEBUG_WARN(MODULE_UI,
                                       "Unsupported player count for Stroke Play 18 holes: %d",
                                       num_players);
                            highlighted_player = 1;
                            break;
                    }
                    break;
                default:
                    DEBUG_ERROR(MODULE_UI, "Unknown hole mode: %d", current_hole_mode);
                    break;
            }
            break;
        default:
            DEBUG_ERROR(MODULE_UI, "Unknown game mode: %d", current_game_mode);
            break;
    }

    DEBUG_TRACE(MODULE_UI, "Highlight update completed - final highlighted player: %d",
                highlighted_player);
}

HoleMode      current_hole_mode       = HOLES_9;              // Default to 9 holes
MatchPlayMode current_match_play_mode = MATCH_PLAY_MODE_1V1;  // Default to 1v1 mode

// Set hole mode (9 or 18 holes)
void set_hole_mode(HoleMode mode)
{
    DEBUG_INFO(MODULE_LOGIC, "========== HOLE MODE SELECTION ==========");
    DEBUG_INFO(MODULE_LOGIC, "Setting hole mode to: %s",
               (mode == HOLES_9) ? "9 Holes" : "18 Holes");
    current_hole_mode = mode;
    DEBUG_INFO(MODULE_LOGIC,
               "Current settings: game_mode=%d, hole_mode=%d, match_play_mode=%d, num_players=%d",
               current_game_mode, current_hole_mode, current_match_play_mode, num_players);
    DEBUG_INFO(MODULE_LOGIC, "=========================================");
}

// Set match play mode (1v1 or 2v2)
void set_match_play_mode(MatchPlayMode mode)
{
    DEBUG_INFO(MODULE_LOGIC, "========== MATCH PLAY MODE SELECTION ==========");
    DEBUG_INFO(MODULE_LOGIC, "Setting match play mode to: %s",
               (mode == MATCH_PLAY_MODE_1V1) ? "1v1" : "2v2");
    current_match_play_mode = mode;
    DEBUG_INFO(MODULE_LOGIC,
               "Current settings: game_mode=%d, hole_mode=%d, match_play_mode=%d, num_players=%d",
               current_game_mode, current_hole_mode, current_match_play_mode, num_players);
    DEBUG_INFO(MODULE_LOGIC, "===============================================");
}

// Print current hole mode (for debugging/menus)
void print_current_hole_mode()
{
    DEBUG_INFO(MODULE_LOGIC, "Current hole mode: %s",
               (current_hole_mode == HOLES_9) ? "9 Holes" : "18 Holes");
}

/**
 * @brief Check if all players have completed 9 to 18 holes
 */
void check_all_players_completed(GameMode gameMode)
{
    DEBUG_TRACE(MODULE_GAME, "Checking if all players completed - game mode: %d, hole mode: %d",
                gameMode, current_hole_mode);

    static uint8_t player_is_finished[MAX_PLAYERS] = {0};  // 1=finished, 0=not finished
    uint8_t        all_players_completed           = 1;

    switch (gameMode)
    {
        case GAME_MODE_STROKE_PLAY:
            DEBUG_INFO(MODULE_GAME,
                       "Stroke Play - checking completion for %d players on hole mode %d",
                       num_players, current_hole_mode);

            for (uint8_t i = 0; i < num_players; i++)
            {
                // Check if player has completed the current hole
                if (players[i].current_hole > current_hole_mode + 1 ||
                    (players[i].current_hole == current_hole_mode + 1 &&
                     players[i].detection_count == 0))
                {
                    /***************************************** */
                    players[i].round_total_score = 0;
                    /***************************************** */
                    player_is_finished[i] = 1;
                    DEBUG_INFO(MODULE_GAME, "Player %d FINISHED hole %d", i + 1, current_hole_mode);
                }
                else
                {
                    player_is_finished[i] = 0;
                    all_players_completed = 0;
                    DEBUG_DEBUG(MODULE_GAME, "Player %d NOT finished (Hole:%d Detections:%d)",
                                i + 1, players[i].current_hole, players[i].detection_count);
                }
            }

            update_flag = all_players_completed ? 1 : 0;

            if (update_flag)
            {
                DEBUG_INFO(MODULE_GAME, "All players completed hole %d. Advancing to next round",
                           current_hole_mode);
                print_final_scores_and_winner();  // All holes are done

                // Navigate to scorecard based on number of players
                if (num_players == 1)
                {
                    if (current_hole_mode == NINE_HOLES)
                    {
                        _ui_screen_change(&ui_SP1P9HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_SP1P9HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 1P 9H scorecard");
                    }
                    else if (current_hole_mode == EIGHTEEN_HOLES)
                    {
                        _ui_screen_change(&ui_SP1P18HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_SP1P18HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 1P 18H scorecard");
                    }
                }
                else if (num_players == 2)
                {
                    // Determine winner (higher score wins in stroke play)
                    if (players[0].score > players[1].score)
                    {
                        DEBUG_INFO(MODULE_GAME, "Player 1 wins with score %d vs %d",
                                   players[0].score, players[1].score);
                        PLAY_PLAYER1WINS_WAV;
                    }
                    else if (players[1].score > players[0].score)
                    {
                        DEBUG_INFO(MODULE_GAME, "Player 2 wins with score %d vs %d",
                                   players[1].score, players[0].score);
                        PLAY_PLAYER2WINS_WAV;
                    }
                    else
                    {
                        DEBUG_INFO(MODULE_GAME, "Game tied at %d points", players[0].score);
                    }

                    if (current_hole_mode == NINE_HOLES)
                    {
                        _ui_screen_change(&ui_SP2P9HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_SP2P9HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 2P 9H scorecard");
                    }
                    else if (current_hole_mode == EIGHTEEN_HOLES)
                    {
                        _ui_screen_change(&ui_SP2P18HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_SP2P18HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 2P 18H scorecard");
                    }
                }
                else if (num_players == 3)
                {
                    // Determine winner among 3 players (higher score wins)
                    int winner    = 0;
                    int max_score = players[0].score;
                    int tie_count = 1;

                    for (int i = 1; i < 3; i++)
                    {
                        if (players[i].score > max_score)
                        {
                            max_score = players[i].score;
                            winner    = i;
                            tie_count = 1;
                        }
                        else if (players[i].score == max_score)
                        {
                            tie_count++;
                        }
                    }

                    if (tie_count == 1)
                    {
                        DEBUG_INFO(MODULE_GAME, "Player %d wins with score %d", winner + 1,
                                   max_score);
                        switch (winner)
                        {
                            case 0:
                                PLAY_PLAYER1WINS_WAV;
                                break;
                            case 1:
                                PLAY_PLAYER2WINS_WAV;
                                break;
                            case 2:
                                PLAY_PLAYER3WINS_WAV;
                                break;
                        }
                    }
                    else
                    {
                        DEBUG_INFO(MODULE_GAME, "Game tied with %d players at %d points", tie_count,
                                   max_score);
                    }

                    if (current_hole_mode == NINE_HOLES)
                    {
                        _ui_screen_change(&ui_SP3P9HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_SP3P9HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 3P 9H scorecard");
                    }
                    else if (current_hole_mode == EIGHTEEN_HOLES)
                    {
                        _ui_screen_change(&ui_SP3P18HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_SP3P18HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 3P 18H scorecard");
                    }
                }
                else if (num_players == 4)
                {
                    // Determine winner among 4 players (higher score wins)
                    int winner    = 0;
                    int max_score = players[0].score;
                    int tie_count = 1;

                    for (int i = 1; i < 4; i++)
                    {
                        if (players[i].score > max_score)
                        {
                            max_score = players[i].score;
                            winner    = i;
                            tie_count = 1;
                        }
                        else if (players[i].score == max_score)
                        {
                            tie_count++;
                        }
                    }

                    if (tie_count == 1)
                    {
                        DEBUG_INFO(MODULE_GAME, "Player %d wins with score %d", winner + 1,
                                   max_score);
                        switch (winner)
                        {
                            case 0:
                                PLAY_PLAYER1WINS_WAV;
                                break;
                            case 1:
                                PLAY_PLAYER2WINS_WAV;
                                break;
                            case 2:
                                PLAY_PLAYER3WINS_WAV;
                                break;
                            case 3:
                                PLAY_PLAYER4WINS_WAV;
                                break;
                        }
                    }
                    else
                    {
                        DEBUG_INFO(MODULE_GAME, "Game tied with %d players at %d points", tie_count,
                                   max_score);
                    }

                    if (current_hole_mode == NINE_HOLES)
                    {
                        _ui_screen_change(&ui_SP4P9HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_SP4P9HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 4P 9H scorecard");
                    }
                    else if (current_hole_mode == EIGHTEEN_HOLES)
                    {
                        _ui_screen_change(&ui_SP4P18HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_SP4P18HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 4P 18H scorecard");
                    }
                }
            }
            else
            {
                DEBUG_TRACE(MODULE_GAME, "Not all players completed - %d players still playing",
                            num_players - all_players_completed);
            }
            break;

        case GAME_MODE_QUOTA:
            DEBUG_INFO(MODULE_GAME, "Quota Points - checking completion for %d players",
                       num_players);

            for (uint8_t i = 0; i < num_players; i++)
            {
                if (quota_player_completed(&players[i]))
                {
                    player_is_finished[i] = 1;
                    DEBUG_INFO(MODULE_GAME, "Player %d completed quota!", i + 1);
                }
                else
                {
                    player_is_finished[i] = 0;
                    DEBUG_DEBUG(MODULE_GAME, "Player %d quota remaining (3pt:%d, 4pt:%d, 5pt:%d)",
                                i + 1, players[i].par3_count, players[i].par4_count,
                                players[i].par5_count);
                }
            }

            // For 1P: player wins when quota is done
            // For 2P+: first player to complete quota wins
            switch (num_players)
            {
                case 1:
                    if (player_is_finished[0])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "1P Quota complete - Score:%d", players[0].score);
                        PLAY_PLAYER1WINS_WAV;
                    }
                    break;
                case 2:
                    if (player_is_finished[0] && !player_is_finished[1])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "Player 1 completed quota first! Score:%d",
                                   players[0].score);
                        PLAY_PLAYER1WINS_WAV;
                        if (current_hole_mode == NINE_HOLES)
                            lv_obj_clear_flag(ui_Q2P9HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                        else
                            lv_obj_clear_flag(ui_Q2P18HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                    }
                    else if (player_is_finished[1] && !player_is_finished[0])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "Player 2 completed quota first! Score:%d",
                                   players[1].score);
                        PLAY_PLAYER2WINS_WAV;
                        if (current_hole_mode == NINE_HOLES)
                            lv_obj_clear_flag(ui_Q2P9HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                        else
                            lv_obj_clear_flag(ui_Q2P18HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                    }
                    else if (player_is_finished[0] && player_is_finished[1])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "Both players completed quota simultaneously!");
                        // Both finished at the same time - show both crowns
                        if (current_hole_mode == NINE_HOLES)
                        {
                            lv_obj_clear_flag(ui_Q2P9HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                            lv_obj_clear_flag(ui_Q2P9HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                        }
                        else
                        {
                            lv_obj_clear_flag(ui_Q2P18HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                            lv_obj_clear_flag(ui_Q2P18HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                        }
                    }
                    break;
                case 3:
                    // Check each player individually for first-to-finish
                    if (player_is_finished[0] && !player_is_finished[1] && !player_is_finished[2])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "Player 1 completed quota first! Score:%d",
                                   players[0].score);
                        PLAY_PLAYER1WINS_WAV;
                        if (current_hole_mode == NINE_HOLES)
                            lv_obj_clear_flag(ui_Q3P9HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                        else
                            lv_obj_clear_flag(ui_Q3P18HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                    }
                    else if (player_is_finished[1] && !player_is_finished[0] && !player_is_finished[2])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "Player 2 completed quota first! Score:%d",
                                   players[1].score);
                        PLAY_PLAYER2WINS_WAV;
                        if (current_hole_mode == NINE_HOLES)
                            lv_obj_clear_flag(ui_Q3P9HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                        else
                            lv_obj_clear_flag(ui_Q3P18HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                    }
                    else if (player_is_finished[2] && !player_is_finished[0] && !player_is_finished[1])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "Player 3 completed quota first! Score:%d",
                                   players[2].score);
                        PLAY_PLAYER3WINS_WAV;
                        if (current_hole_mode == NINE_HOLES)
                            lv_obj_clear_flag(ui_Q3P9HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);
                        else
                            lv_obj_clear_flag(ui_Q3P18HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);
                    }
                    else if (player_is_finished[0] || player_is_finished[1] || player_is_finished[2])
                    {
                        // Multiple players finished simultaneously - show all their crowns
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "Multiple players completed quota simultaneously!");
                        if (current_hole_mode == NINE_HOLES)
                        {
                            if (player_is_finished[0])
                                lv_obj_clear_flag(ui_Q3P9HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                            if (player_is_finished[1])
                                lv_obj_clear_flag(ui_Q3P9HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                            if (player_is_finished[2])
                                lv_obj_clear_flag(ui_Q3P9HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);
                        }
                        else
                        {
                            if (player_is_finished[0])
                                lv_obj_clear_flag(ui_Q3P18HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                            if (player_is_finished[1])
                                lv_obj_clear_flag(ui_Q3P18HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                            if (player_is_finished[2])
                                lv_obj_clear_flag(ui_Q3P18HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);
                        }
                    }
                    break;
            }
            break;

        case GAME_MODE_MATCH_PLAY:
            DEBUG_INFO(MODULE_GAME,
                       "Match Play - checking completion for %d players on hole mode %d",
                       num_players, current_hole_mode);

            for (uint8_t i = 0; i < num_players; i++)
            {
                // Check if player has completed the current hole
                if (players[i].current_hole > current_hole_mode + 1 ||
                    (players[i].current_hole == current_hole_mode + 1 &&
                     players[i].detection_count == 0))
                {
                    /***************************************** */
                    players[i].round_total_score = 0;
                    /***************************************** */
                    player_is_finished[i] = 1;
                    DEBUG_INFO(MODULE_GAME, "Player %d FINISHED hole %d", i + 1, current_hole_mode);
                }
                else
                {
                    player_is_finished[i] = 0;
                    all_players_completed = 0;
                    DEBUG_DEBUG(MODULE_GAME, "Player %d NOT finished (Hole:%d Detections:%d)",
                                i + 1, players[i].current_hole, players[i].detection_count);
                }
            }

            // Check for early win
            // Note: current_hole has already been incremented, so add 1 to get correct remaining
            int lead            = players[0].holes_won - players[1].holes_won;
            int holes_remaining = current_hole_mode - players[0].current_hole + 1;

            DEBUG_DEBUG(MODULE_GAME, "Match Play status - Lead: %d, Holes remaining: %d", lead,
                        holes_remaining);

            // Show match status for both players
            if (lead > 0)
                DEBUG_INFO(MODULE_GAME, "Match status: Player 1: %dUP | Player 2: %dDN", lead,
                           lead);
            else if (lead < 0)
                DEBUG_INFO(MODULE_GAME, "Match status: Player 1: %dDN | Player 2: %dUP", -lead,
                           -lead);
            else
                DEBUG_INFO(MODULE_GAME, "Match status: All Square (E)");

            if (lead > holes_remaining)
            {
                all_players_completed = 1;
                player_is_finished[0] = 1;
                player_is_finished[1] = 1;
                DISABLE_SENSORS

                if (current_match_play_mode == MATCH_PLAY_MODE_1V1)
                {
                    DEBUG_INFO(MODULE_GAME, "Game ends early: Player 1 wins %d&%d", lead,
                               holes_remaining);
                    DEBUG_INFO(MODULE_GAME,
                               "Early victory - Player 1 wins the match - sensors disabled");
                    PLAY_PLAYER1WINS_WAV;
                    // Update F column with X&Y format for early victory
                    if (current_hole_mode == NINE_HOLES)
                    {
                        lv_label_set_text_fmt(ui_MP1V19HScP1STextF, "%d&%d", lead, holes_remaining);
                        lv_label_set_text(ui_MP1V19HScP2STextF, "-");
                    }
                    else if (current_hole_mode == EIGHTEEN_HOLES)
                    {
                        lv_label_set_text_fmt(ui_MP1V118HScP1STextF, "%d&%d", lead,
                                              holes_remaining);
                        lv_label_set_text(ui_MP1V118HScP2STextF, "-");
                    }
                }
                else if (current_match_play_mode == MATCH_PLAY_MODE_2V2)
                {
                    DEBUG_INFO(MODULE_GAME, "Game ends early: Team 1 wins %d&%d", lead,
                               holes_remaining);
                    DEBUG_INFO(MODULE_GAME,
                               "Early victory - Team 1 wins the match - sensors disabled");
                    PLAY_TEAM1WINS_WAV;
                    // Update F column with X&Y format for early victory
                    if (current_hole_mode == NINE_HOLES)
                    {
                        lv_label_set_text_fmt(ui_MP2V29HScT1STextF, "%d&%d", lead, holes_remaining);
                        lv_label_set_text(ui_MP2V29HScT2STextF, "-");
                    }
                    else if (current_hole_mode == EIGHTEEN_HOLES)
                    {
                        lv_label_set_text_fmt(ui_MP2V218HScT1STextF, "%d&%d", lead,
                                              holes_remaining);
                        lv_label_set_text(ui_MP2V218HScT2STextF, "-");
                    }
                }
            }
            else if (-lead > holes_remaining)
            {
                all_players_completed = 1;
                player_is_finished[0] = 1;
                player_is_finished[1] = 1;
                DISABLE_SENSORS

                if (current_match_play_mode == MATCH_PLAY_MODE_1V1)
                {
                    DEBUG_INFO(MODULE_GAME, "Game ends early: Player 2 wins %d&%d", -lead,
                               holes_remaining);
                    DEBUG_INFO(MODULE_GAME,
                               "Early victory - Player 2 wins the match - sensors disabled");
                    PLAY_PLAYER2WINS_WAV;
                    // Update F column with X&Y format for early victory
                    if (current_hole_mode == NINE_HOLES)
                    {
                        lv_label_set_text(ui_MP1V19HScP1STextF, "-");
                        lv_label_set_text_fmt(ui_MP1V19HScP2STextF, "%d&%d", -lead,
                                              holes_remaining);
                    }
                    else if (current_hole_mode == EIGHTEEN_HOLES)
                    {
                        lv_label_set_text(ui_MP1V118HScP1STextF, "-");
                        lv_label_set_text_fmt(ui_MP1V118HScP2STextF, "%d&%d", -lead,
                                              holes_remaining);
                    }
                }
                else if (current_match_play_mode == MATCH_PLAY_MODE_2V2)
                {
                    DEBUG_INFO(MODULE_GAME, "Game ends early: Team 2 wins %d&%d", -lead,
                               holes_remaining);
                    DEBUG_INFO(MODULE_GAME,
                               "Early victory - Team 2 wins the match - sensors disabled");
                    PLAY_TEAM2WINS_WAV;
                    // Update F column with X&Y format for early victory
                    if (current_hole_mode == NINE_HOLES)
                    {
                        lv_label_set_text(ui_MP2V29HScT1STextF, "-");
                        lv_label_set_text_fmt(ui_MP2V29HScT2STextF, "%d&%d", -lead,
                                              holes_remaining);
                    }
                    else if (current_hole_mode == EIGHTEEN_HOLES)
                    {
                        lv_label_set_text(ui_MP2V218HScT1STextF, "-");
                        lv_label_set_text_fmt(ui_MP2V218HScT2STextF, "%d&%d", -lead,
                                              holes_remaining);
                    }
                }
            }
            else
            {
                all_players_completed = 0;
                DEBUG_TRACE(MODULE_GAME, "Match continues - no early victory condition met");
            }

            update_flag = all_players_completed ? 1 : 0;

            if (update_flag)
            {
                DEBUG_INFO(MODULE_GAME, "Match completed - all players finished hole %d",
                           current_hole_mode);
                print_final_scores_and_winner();  // All holes are done

                // Navigate to scorecard and announce winner for completed matches
                if (current_match_play_mode == MATCH_PLAY_MODE_1V1)
                {
                    // Announce winner for 1v1 (if not early victory - already announced above)
                    if (lead == 0 && -lead == 0)
                    {
                        // Tie game - determine by total score or holes won
                        if (players[0].holes_won > players[1].holes_won)
                        {
                            PLAY_PLAYER1WINS_WAV;
                        }
                        else if (players[1].holes_won > players[0].holes_won)
                        {
                            PLAY_PLAYER2WINS_WAV;
                        }
                    }

                    if (current_hole_mode == NINE_HOLES)
                    {
                        _ui_screen_change(&ui_MP1V19HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_MP1V19HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 1v1 9H scorecard");
                    }
                    else if (current_hole_mode == EIGHTEEN_HOLES)
                    {
                        _ui_screen_change(&ui_MP1V118HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_MP1V118HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 1v1 18H scorecard");
                    }
                }
                else if (current_match_play_mode == MATCH_PLAY_MODE_2V2)
                {
                    // Announce winner for 2v2 (if not early victory - already announced above)
                    if (lead == 0 && -lead == 0)
                    {
                        // Tie game - determine by total score or holes won
                        if (players[0].holes_won > players[1].holes_won)
                        {
                            PLAY_TEAM1WINS_WAV;
                        }
                        else if (players[1].holes_won > players[0].holes_won)
                        {
                            PLAY_TEAM2WINS_WAV;
                        }
                    }

                    if (current_hole_mode == NINE_HOLES)
                    {
                        _ui_screen_change(&ui_MP2V29HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_MP2V29HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 2v2 9H scorecard");
                    }
                    else if (current_hole_mode == EIGHTEEN_HOLES)
                    {
                        _ui_screen_change(&ui_MP2V218HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_MP2V218HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 2v2 18H scorecard");
                    }
                }
            }
            else
            {
                DEBUG_TRACE(MODULE_GAME, "Match continues - players still competing");
            }
            break;

        default:
            DEBUG_WARN(MODULE_GAME, "Unknown game mode in completion check: %d", gameMode);
            break;
    }

    DEBUG_TRACE(MODULE_GAME,
                "Completion check finished - all_players_completed: %d, update_flag: %d",
                all_players_completed, update_flag);
}
/**
 * @brief Prints all players' final scores and announces the winner or a tie.
 */
/**
 * @brief Prints all players' final scores and announces the winner or a tie.
 *        Assumes higher score is better.
 */
/**
 * @brief Prints all players' final scores, announces the winner, and shows their crown.
 *        Assumes higher score is better.
 */
void print_final_scores_and_winner(void)
{
    DEBUG_INFO(MODULE_GAME, "Calculating final scores and determining winner");

    int best_score   = players[0].score;
    int winner_index = 0;
    int tie          = 0;

    // === Determine winner based on game mode ===
    if (current_game_mode == GAME_MODE_MATCH_PLAY)
    {
        // For Match Play, winner is determined by holes won (upAndDown)
        DEBUG_INFO(MODULE_GAME, "=== Match Play Final Result ===");
        DEBUG_INFO(MODULE_GAME, "Player 1: %d holes won, upAndDown: %d", players[0].holes_won,
                   players[0].upAndDown);
        DEBUG_INFO(MODULE_GAME, "Player 2: %d holes won, upAndDown: %d", players[1].holes_won,
                   players[1].upAndDown);

        if (players[0].upAndDown > 0)
        {
            winner_index = 0;
            tie          = 0;
            DISABLE_SENSORS
            DEBUG_INFO(MODULE_GAME, "Player 1 wins - %dUP - sensors disabled",
                       players[0].upAndDown);
        }
        else if (players[0].upAndDown < 0)
        {
            winner_index = 1;
            tie          = 0;
            DISABLE_SENSORS
            DEBUG_INFO(MODULE_GAME, "Player 2 wins - %dUP - sensors disabled",
                       -players[0].upAndDown);
        }
        else
        {
            tie = 1;
            DISABLE_SENSORS
            DEBUG_INFO(MODULE_GAME, "Match is tied - All Square - sensors disabled");
        }
    }
    else
    {
        // For Stroke Play and other modes, winner is determined by score
        DEBUG_INFO(MODULE_GAME, "=== Final Scores ===");
        for (uint8_t i = 0; i < num_players; i++)
        {
            DEBUG_INFO(MODULE_GAME, "Player %d: Total Score = %d", i + 1, players[i].score);

            if (players[i].score > best_score)
            {
                best_score   = players[i].score;
                winner_index = i;
                tie          = 0;
                DEBUG_DEBUG(MODULE_GAME, "New leader: Player %d with score %d", winner_index + 1,
                            best_score);
            }
            else if (players[i].score == best_score && i != winner_index)
            {
                tie = 1;
                DEBUG_DEBUG(MODULE_GAME, "Tie detected between Player %d and Player %d",
                            winner_index + 1, i + 1);
            }
        }
    }

    // === Game Result ===
    DEBUG_INFO(MODULE_GAME, "=== Game Result ===");

    if (tie)
    {
        DEBUG_INFO(MODULE_GAME, "It's a tie! 🎯");
    }
    else
    {
        DEBUG_INFO(MODULE_GAME, "🏆 Player %d is the WINNER with a score of %d!", winner_index + 1,
                   best_score);

        switch (current_game_mode)
        {
            case GAME_MODE_STROKE_PLAY:
                DEBUG_DEBUG(MODULE_UI, "Stroke Play - showing winner crown for player %d",
                            winner_index + 1);
                switch (num_players)
                {
                    case 2:
                        switch (current_hole_mode)
                        {
                            case NINE_HOLES:
                                switch (winner_index)
                                {
                                    case 0:
                                        lv_obj_clear_flag(ui_SP2P9HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP2P9HScP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                        PLAY_PLAYER1WINS_WAV;
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 1 - 9 holes, 2 players");
                                        break;
                                    case 1:
                                        lv_obj_clear_flag(ui_SP2P9HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP2P9HScP2SCrown, LV_OBJ_FLAG_HIDDEN);
                                        PLAY_PLAYER2WINS_WAV;
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 2 - 9 holes, 2 players");
                                        break;
                                }
                                // Navigate to scorecard
                                _ui_screen_change(&ui_SP2P9HScorecard, LV_SCR_LOAD_ANIM_FADE_ON,
                                                  500, 0, &ui_SP2P9HScorecard_screen_init);
                                DEBUG_INFO(MODULE_UI, "Navigating to 2P 9H scorecard");
                                break;

                            case EIGHTEEN_HOLES:
                                switch (winner_index)
                                {
                                    case 0:
                                        lv_obj_clear_flag(ui_SP2P18HGSP1SPCrown,
                                                          LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP2P18HScP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                        PLAY_PLAYER1WINS_WAV;
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 1 - 18 holes, 2 players");
                                        break;
                                    case 1:
                                        lv_obj_clear_flag(ui_SP2P18HGSP2SPCrown,
                                                          LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP2P18HScP2SCrown, LV_OBJ_FLAG_HIDDEN);
                                        PLAY_PLAYER2WINS_WAV;
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 2 - 18 holes, 2 players");
                                        break;
                                }
                                // Navigate to scorecard
                                _ui_screen_change(&ui_SP2P18HScorecard, LV_SCR_LOAD_ANIM_FADE_ON,
                                                  500, 0, &ui_SP2P18HScorecard_screen_init);
                                DEBUG_INFO(MODULE_UI, "Navigating to 2P 18H scorecard");
                                break;
                        }
                        break;

                    case 3:
                        DEBUG_DEBUG(MODULE_UI, "3 players - showing winner crown");
                        switch (current_hole_mode)
                        {
                            case NINE_HOLES:
                                switch (winner_index)
                                {
                                    case 0:
                                        lv_obj_clear_flag(ui_SP3P9HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP3P9HScP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 1 - 9 holes, 3 players");
                                        break;
                                    case 1:
                                        lv_obj_clear_flag(ui_SP3P9HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP3P9HScP2SCrown, LV_OBJ_FLAG_HIDDEN);
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 2 - 9 holes, 3 players");
                                        break;
                                    case 2:
                                        lv_obj_clear_flag(ui_SP3P9HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP3P9HScP3SCrown, LV_OBJ_FLAG_HIDDEN);
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 3 - 9 holes, 3 players");
                                        break;
                                }
                                break;

                            case EIGHTEEN_HOLES:
                                switch (winner_index)
                                {
                                    case 0:
                                        lv_obj_clear_flag(ui_SP3P18HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP3P18HScP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 1 - 18 holes, 3 players");
                                        break;
                                    case 1:
                                        lv_obj_clear_flag(ui_SP3P18HGSP2Crown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP3P18HScP2SCrown, LV_OBJ_FLAG_HIDDEN);
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 2 - 18 holes, 3 players");
                                        break;
                                    case 2:
                                        lv_obj_clear_flag(ui_SP3P18HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP3P18HScP3SCrown, LV_OBJ_FLAG_HIDDEN);
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 3 - 18 holes, 3 players");
                                        break;
                                }
                                break;
                        }
                        break;

                    case 4:
                        DEBUG_DEBUG(MODULE_UI, "4 players - showing winner crown");
                        switch (current_hole_mode)
                        {
                            case NINE_HOLES:
                                switch (winner_index)
                                {
                                    case 0:
                                        lv_obj_clear_flag(ui_SP4P9HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP4P9HScP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 1 - 9 holes, 4 players");
                                        break;
                                    case 1:
                                        lv_obj_clear_flag(ui_SP4P9HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP4P9HScP2SCrown, LV_OBJ_FLAG_HIDDEN);
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 2 - 9 holes, 4 players");
                                        break;
                                    case 2:
                                        lv_obj_clear_flag(ui_SP4P9HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP4P9HScP3SCrown, LV_OBJ_FLAG_HIDDEN);
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 3 - 9 holes, 4 players");
                                        break;
                                    case 3:
                                        lv_obj_clear_flag(ui_SP4P9HGSP4SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP4P9HScP4SCrown, LV_OBJ_FLAG_HIDDEN);
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 4 - 9 holes, 4 players");
                                        break;
                                }
                                break;

                            case EIGHTEEN_HOLES:
                                switch (winner_index)
                                {
                                    case 0:
                                        lv_obj_clear_flag(ui_SP4P18HScP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP4P18HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 1 - 18 holes, 4 players");
                                        break;
                                    case 1:
                                        lv_obj_clear_flag(ui_SP4P18HScP2SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP4P18HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 2 - 18 holes, 4 players");
                                        break;
                                    case 2:
                                        lv_obj_clear_flag(ui_SP4P18HScP3SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP4P18HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 3 - 18 holes, 4 players");
                                        break;
                                    case 3:
                                        lv_obj_clear_flag(ui_SP4P18HScP4SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP4P18HGSP4SCrown, LV_OBJ_FLAG_HIDDEN);
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 4 - 18 holes, 4 players");
                                        break;
                                }
                                break;
                        }
                        break;

                    default:
                        DEBUG_WARN(MODULE_GAME, "Unsupported number of players: %d", num_players);
                        break;
                }
                break;

            // === Other Game Modes ===
            case GAME_MODE_MATCH_PLAY:
                DEBUG_DEBUG(MODULE_GAME, "Match Play winner determined - Player %d",
                            winner_index + 1);

                // Display crown for Match Play winner
                if (current_match_play_mode == MATCH_PLAY_MODE_1V1)
                {
                    if (current_hole_mode == NINE_HOLES)
                    {
                        switch (winner_index)
                        {
                            case 0:
                                lv_obj_clear_flag(ui_MP1V19HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                lv_obj_clear_flag(ui_MP1V19HScP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                PLAY_PLAYER1WINS_WAV;
                                DEBUG_DEBUG(MODULE_UI,
                                            "Showing crown for Player 1 - Match Play 1v1 9H");
                                break;
                            case 1:
                                lv_obj_clear_flag(ui_MP1V19HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                                lv_obj_clear_flag(ui_MP1V19HScP2SCrown, LV_OBJ_FLAG_HIDDEN);
                                PLAY_PLAYER2WINS_WAV;
                                DEBUG_DEBUG(MODULE_UI,
                                            "Showing crown for Player 2 - Match Play 1v1 9H");
                                break;
                        }
                    }
                    else if (current_hole_mode == EIGHTEEN_HOLES)
                    {
                        switch (winner_index)
                        {
                            case 0:
                                lv_obj_clear_flag(ui_MP1V118HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                lv_obj_clear_flag(ui_MP1V118HScP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                PLAY_PLAYER1WINS_WAV;
                                DEBUG_DEBUG(MODULE_UI,
                                            "Showing crown for Player 1 - Match Play 1v1 18H");
                                break;
                            case 1:
                                lv_obj_clear_flag(ui_MP1V118HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                                lv_obj_clear_flag(ui_MP1V118HScP2SCrown, LV_OBJ_FLAG_HIDDEN);
                                PLAY_PLAYER2WINS_WAV;
                                DEBUG_DEBUG(MODULE_UI,
                                            "Showing crown for Player 2 - Match Play 1v1 18H");
                                break;
                        }
                    }
                }
                else if (current_match_play_mode == MATCH_PLAY_MODE_2V2)
                {
                    if (current_hole_mode == NINE_HOLES)
                    {
                        switch (winner_index)
                        {
                            case 0:
                                lv_obj_clear_flag(ui_MP2V29HGST1SCrown, LV_OBJ_FLAG_HIDDEN);
                                lv_obj_clear_flag(ui_MP2V29HScT1SCrown, LV_OBJ_FLAG_HIDDEN);
                                DEBUG_DEBUG(MODULE_UI,
                                            "Showing crown for Team 1 - Match Play 2v2 9H");
                                break;
                            case 1:
                                lv_obj_clear_flag(ui_MP2V29HGST2SCrown, LV_OBJ_FLAG_HIDDEN);
                                lv_obj_clear_flag(ui_MP2V29HScT2SCrown, LV_OBJ_FLAG_HIDDEN);
                                DEBUG_DEBUG(MODULE_UI,
                                            "Showing crown for Team 2 - Match Play 2v2 9H");
                                break;
                        }
                    }
                    else if (current_hole_mode == EIGHTEEN_HOLES)
                    {
                        switch (winner_index)
                        {
                            case 0:
                                lv_obj_clear_flag(ui_MP2V218HGST1SCrown, LV_OBJ_FLAG_HIDDEN);
                                lv_obj_clear_flag(ui_MP2V218HScT1SCrown, LV_OBJ_FLAG_HIDDEN);
                                DEBUG_DEBUG(MODULE_UI,
                                            "Showing crown for Team 1 - Match Play 2v2 18H");
                                break;
                            case 1:
                                lv_obj_clear_flag(ui_MP2V218HGST2SCrown, LV_OBJ_FLAG_HIDDEN);
                                lv_obj_clear_flag(ui_MP2V218HScT2SCrown, LV_OBJ_FLAG_HIDDEN);
                                DEBUG_DEBUG(MODULE_UI,
                                            "Showing crown for Team 2 - Match Play 2v2 18H");
                                break;
                        }
                    }
                }

                // Navigate to scorecard screen based on mode
                if (current_match_play_mode == MATCH_PLAY_MODE_1V1)
                {
                    if (current_hole_mode == NINE_HOLES)
                    {
                        _ui_screen_change(&ui_MP1V19HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_MP1V19HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 1v1 9H scorecard");
                    }
                    else if (current_hole_mode == EIGHTEEN_HOLES)
                    {
                        _ui_screen_change(&ui_MP1V118HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_MP1V118HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 1v1 18H scorecard");
                    }
                }
                else if (current_match_play_mode == MATCH_PLAY_MODE_2V2)
                {
                    if (current_hole_mode == NINE_HOLES)
                    {
                        _ui_screen_change(&ui_MP2V29HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_MP2V29HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 2v2 9H scorecard");
                    }
                    else if (current_hole_mode == EIGHTEEN_HOLES)
                    {
                        _ui_screen_change(&ui_MP2V218HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_MP2V218HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 2v2 18H scorecard");
                    }
                }
                break;

            case GAME_MODE_QUOTA:
                DEBUG_DEBUG(MODULE_GAME, "Quota Points mode completed");
                switch (num_players)
                {
                    case 1:
                        DEBUG_INFO(MODULE_GAME,
                                   "1P Quota complete - Score:%d (3pt:%d, 4pt:%d, 5pt:%d)",
                                   players[0].score, players[0].par3_count, players[0].par4_count,
                                   players[0].par5_count);
                        // Navigate back to home screen for 1 player
                        _ui_screen_change(&ui_HScreen, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_HScreen_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to home screen (Quota 1P complete)");
                        break;
                }
                break;

            case GAME_MODE_VEGAS:
                DEBUG_DEBUG(MODULE_GAME, "Vegas mode winner determined - Player %d",
                            winner_index + 1);
                // TODO: Add switch(num_players) logic per game mode
                break;

            default:
                DEBUG_ERROR(MODULE_GAME, "Unknown game mode: %d", current_game_mode);
                break;
        }
    }

    DEBUG_INFO(MODULE_GAME, "Winner determination completed");
}

/**
 * @brief Signal handler for cleanup on program exit.
 * @param signal Signal number.
 */
void logic_cleanup_handler(int signal)
{
    DEBUG_INFO(MODULE_LOGIC, "Caught signal %d. Cleaning up and exiting...", signal);

    // Release GPIO lines and close the chip
    if (chip)
    {
        gpiod_line_release_bulk(&lines);
        gpiod_chip_close(chip);
        DEBUG_INFO(MODULE_LOGIC, "GPIO resources released");
    }
    else
    {
        DEBUG_WARN(MODULE_LOGIC, "No GPIO chip to cleanup");
    }

    // exit(EXIT_SUCCESS); // Commented out as per original
}

/**
 * @brief Initialize GPIO settings.
 * @return 0 on success, -1 on failure.
 */
int logic_gpio_init(void)
{
    DEBUG_INFO(MODULE_LOGIC, "Initializing GPIO system");

    // Open the GPIO chip
    chip = gpiod_chip_open("/dev/gpiochip0");
    if (!chip)
    {
        DEBUG_ERROR(MODULE_LOGIC, "Failed to open GPIO chip: /dev/gpiochip0");
        return -1;
    }

    // Get all lines in bulk
    if (gpiod_chip_get_lines(chip, sensor_pins, NUM_SENSORS, &lines) < 0)
    {
        DEBUG_ERROR(MODULE_LOGIC, "Failed to get GPIO lines");
        gpiod_chip_close(chip);
        return -1;
    }

    // Request falling edge events on all lines
    if (gpiod_line_request_bulk_falling_edge_events(&lines, "falling-edge-example") < 0)
    {
        DEBUG_ERROR(MODULE_LOGIC, "Failed to request falling edge events");
        gpiod_line_release_bulk(&lines);
        gpiod_chip_close(chip);
        return -1;
    }

    DEBUG_INFO(MODULE_LOGIC, "GPIO initialization successful - %d sensors configured", NUM_SENSORS);
    return 0;
}
/**
 * @brief Initialize game settings.
 * @param new_num_players Number of players.
 * @return 0 on success, -1 on failure.
 */
int logic_initialize_game(int new_num_players)
{
    DEBUG_INFO(MODULE_LOGIC, "Initializing game with %d players", new_num_players);

    // Validate number of players
    if (new_num_players < 1 || new_num_players > MAX_PLAYERS)
    {
        DEBUG_ERROR(MODULE_LOGIC, "Invalid number of players: %d (max: %d)", new_num_players,
                    MAX_PLAYERS);
        return -1;
    }

    // Update the global num_players
    num_players = new_num_players;

    // Initialize player data
    for (int i = 0; i < num_players; i++)
    {
        players[i].score           = 0;
        players[i].current_hole    = 0;
        players[i].detection_count = 0;
        players[i].holes_halved    = 0;
        players[i].holes_won       = 0;
        players[i].upAndDown       = 0;
        DEBUG_DEBUG(MODULE_LOGIC, "Initialized Player %d data", i + 1);
    }

    // Initialize debounce timers (interrupt-based)
    if (init_debounce_timers() < 0)
    {
        DEBUG_ERROR(MODULE_LOGIC, "Failed to initialize debounce timers");
        return -1;
    }

    // Initialize GPIO
    if (logic_gpio_init() < 0)
    {
        DEBUG_ERROR(MODULE_LOGIC, "Failed to initialize GPIO system");
        return -1;
    }

    DEBUG_INFO(MODULE_LOGIC, "Game initialization completed successfully");
    return 0;
}
/**
 * @brief Handle GPIO events.
 * @param event_lines Pointer to gpiod_line_bulk structure for event lines.
 * @param event Pointer to gpiod_line_event structure for events.
 * @param num_players Number of players.
 */
void logic_handle_events(struct gpiod_line_bulk* event_lines, struct gpiod_line_event* event,
                         int num_players)
{
    DEBUG_TRACE(MODULE_GPIO, "Checking for GPIO events with %d players", num_players);

    // Wait for events on any of the lines (non-blocking call)
    if (gpiod_line_event_wait_bulk(&lines, &(struct timespec){0, 0}, event_lines) > 0)
    {
        DEBUG_TRACE(MODULE_GPIO, "GPIO event detected - processing %d triggered lines",
                    gpiod_line_bulk_num_lines(event_lines));

        // Iterate over the lines that triggered events
        for (unsigned int i = 0; i < gpiod_line_bulk_num_lines(event_lines); i++)
        {
            struct gpiod_line* line = gpiod_line_bulk_get_line(event_lines, i);
            if (!line)
            {
                DEBUG_ERROR(MODULE_GPIO, "Failed to get line from bulk at index %d", i);
                continue;
            }

            // Read the event
            if (gpiod_line_event_read(line, event) == 0 && sensors_enabled)
            {
                uint8_t pin_offset   = gpiod_line_offset(line);
                int     sensor_index = pin_to_sensor_index(pin_offset);

                if (sensor_index < 0)
                {
                    DEBUG_ERROR(MODULE_GPIO, "Unknown GPIO pin triggered: %d", pin_offset);
                    continue;
                }

                DEBUG_TRACE(MODULE_GPIO, "Processing GPIO event - pin: %d, sensor_index: %d",
                            pin_offset, sensor_index);

                // Check debounce using timer-based interrupt flag
                if (!sensor_debouncing[sensor_index])
                {
                    start_debounce_timer(sensor_index);  // Start timer, sets debouncing flag
                    DEBUG_TRACE(MODULE_GPIO, "Debounce check passed for sensor %d (pin %d)",
                                sensor_index, pin_offset);

                    Player* player = &players[current_player_index];
                    DEBUG_DEBUG(MODULE_GAME, "Processing event for Player %d (detections: %d/%d)",
                                current_player_index + 1, player->detection_count,
                                SENSORS_PER_TURN);

                    if (player->detection_count < SENSORS_PER_TURN)
                    {
                        // Handle scoring based on game mode
                        switch (current_game_mode)
                        {
                            case GAME_MODE_STROKE_PLAY:
                                DEBUG_DEBUG(MODULE_GAME, "Stroke Play - processing pin event");
                                DEBUG_TRACE(MODULE_GAME,
                                            "Calling stroke_play_process_pin for Player %d, pin %d",
                                            current_player_index + 1, pin_offset);

                                stroke_play_process_pin(player, current_player_index, pin_offset,
                                                        &leds);
                                break;

                            case GAME_MODE_MATCH_PLAY:
                                DEBUG_DEBUG(MODULE_GAME, "Match Play - processing pin event");
                                switch (pin_offset)
                                {
                                    case 17:
                                        player->round_total_score += SCORE_THREE_POINTS;
                                        trigger_flash_with_color(&leds, 1000, COLOR_GREEN);
                                        PLAY_THREEPOINTS_WAV;
                                        DEBUG_INFO(MODULE_GAME,
                                                   "Match Play - 3 points scored (pin 17)");
                                        break;
                                    case 26:
                                        player->round_total_score += SCORE_FOUR_POINTS;
                                        trigger_flash_with_color(&leds, 1000, COLOR_GREEN);
                                        PLAY_FOURPOINTS_WAV;
                                        DEBUG_INFO(MODULE_GAME,
                                                   "Match Play - 4 points scored (pin 26)");
                                        break;
                                    case 27:
                                        player->round_total_score += SCORE_FIVE_POINTS;
                                        trigger_flash_with_color(&leds, 1000, COLOR_GREEN);
                                        PLAY_FIVEPOINTS_WAV;
                                        DEBUG_INFO(MODULE_GAME,
                                                   "Match Play - 5 points scored (pin 27)");
                                        break;
                                    case 24:
                                        player->round_total_score += SCORE_ZERO_POINTS;
                                        trigger_flash_with_color(&leds, 1000, COLOR_WHITE);
                                        PLAY_ZEROPOINTS_WAV;
                                        DEBUG_INFO(MODULE_GAME,
                                                   "Match Play - 0 points scored (pin 24)");
                                        break;
                                    default:
                                        DEBUG_WARN(MODULE_GAME, "Unknown GPIO pin triggered: %d",
                                                   sensor_pins[pin_offset]);
                                        break;
                                }
                                DEBUG_DEBUG(MODULE_GAME, "Player %d round total score: %d",
                                            current_player_index + 1, player->round_total_score);
                                break;

                            case GAME_MODE_QUOTA:
                                quota_play_process_pin(player, current_player_index, pin_offset,
                                                       &leds);
                                player->round_total_score = player->score;
                                DEBUG_DEBUG(MODULE_GAME, "Player %d round total score: %d",
                                            current_player_index + 1, player->round_total_score);
                                break;

                            case GAME_MODE_VEGAS:
                                DEBUG_TRACE(MODULE_GAME,
                                            "Vegas mode - pin event (not yet implemented)");
                                // Placeholder for Vegas - will be implemented in vegas.c
                                break;

                            default:
                                DEBUG_ERROR(MODULE_GAME, "Unknown game mode: %d",
                                            current_game_mode);
                                break;
                        }

                        player->detection_count++;
                        DEBUG_DEBUG(MODULE_GAME, "Player %d detection count increased to: %d",
                                    current_player_index + 1, player->detection_count);

                        logic_update_label_text(current_player_index, player->current_hole,
                                                player->score, player->detection_count,
                                                num_players);

                        // Quota: check completion after every detection
                        // If player reaches 0,0,0, game ends immediately regardless of remaining
                        // balls
                        if (current_game_mode == GAME_MODE_QUOTA && !update_flag)
                        {
                            if (quota_player_completed(player))
                            {
                                player->detection_count = 0;
                                DEBUG_INFO(
                                    MODULE_GAME,
                                    "Quota Player %d completed quota - ending game immediately",
                                    current_player_index + 1);
                                check_all_players_completed(current_game_mode);
                            }
                            else if (num_players == 1 && player->detection_count == 1)
                            {
                                // 1P: each single detection completes a turn (no 2-ball
                                // requirement)
                                player->detection_count = 0;
                                DEBUG_INFO(MODULE_GAME, "Quota 1P - hole completed immediately");
                            }
                        }

                        // Handle turn completion based on game mode
                        if (player->detection_count == SENSORS_PER_TURN && !update_flag)
                        {
                            DEBUG_INFO(MODULE_GAME, "Player %d completed turn (%d detections)",
                                       current_player_index + 1, SENSORS_PER_TURN);

                            switch (current_game_mode)
                            {
                                case GAME_MODE_STROKE_PLAY:
                                    DEBUG_INFO(MODULE_GAME,
                                               "Stroke Play - completing turn for Player %d",
                                               current_player_index + 1);
                                    DEBUG_DEBUG(MODULE_GAME,
                                                "Updating score: Player %d, Score: %d, Hole: %d",
                                                current_player_index + 1, player->score,
                                                player->current_hole);

                                    update_scoreCard(current_player_index, player->score,
                                                     player->current_hole);

                                    player->current_hole++;
                                    DEBUG_INFO(MODULE_GAME, "Player %d advanced to hole %d",
                                               current_player_index + 1, player->current_hole);

                                    player->detection_count = 0;
                                    DEBUG_DEBUG(MODULE_GAME, "Reset detection count for Player %d",
                                                current_player_index + 1);

                                    check_all_players_completed(current_game_mode);
                                    break;

                                case GAME_MODE_MATCH_PLAY:
                                    DEBUG_INFO(MODULE_GAME,
                                               "Match Play - completing turn for Player %d",
                                               current_player_index + 1);
                                    DEBUG_DEBUG(MODULE_GAME,
                                                "Player %d round total score: %d, Current hole: %d",
                                                current_player_index + 1, player->round_total_score,
                                                player->current_hole);

                                    playersmp[current_player_index] = player->round_total_score;

                                    // Announce next player/team
                                    if (current_player_index == 0)
                                    {
                                        if (current_match_play_mode == MATCH_PLAY_MODE_1V1)
                                        {
                                            DEBUG_INFO(MODULE_LOGIC, "Playing Player Two sound");
                                            play_sound_once(load_sound_effect(SOUND_PLAYERTWO_WAV),
                                                            SOUND_DELAY_TURN_SWITCH_MS);
                                        }
                                        else if (current_match_play_mode == MATCH_PLAY_MODE_2V2)
                                        {
                                            DEBUG_INFO(MODULE_LOGIC, "Playing Team Two sound");
                                            play_sound_once(load_sound_effect(SOUND_TEAMTWO_WAV),
                                                            SOUND_DELAY_TURN_SWITCH_MS);
                                        }
                                    }

                                    if (current_player_index == 1)
                                    {
                                        DEBUG_INFO(MODULE_GAME,
                                                   "Both players completed turn - evaluating match "
                                                   "result");

                                        /* Evaluate match result */
                                        if (playersmp[0] > playersmp[1])
                                        {
                                            players[0].upAndDown++;
                                            players[1].upAndDown--;
                                            players[0].holes_won++;
                                            DEBUG_INFO(MODULE_GAME, "Player 1 wins the hole");
                                        }
                                        else if (playersmp[0] < playersmp[1])
                                        {
                                            players[0].upAndDown--;
                                            players[1].upAndDown++;
                                            players[1].holes_won++;
                                            DEBUG_INFO(MODULE_GAME, "Player 2 wins the hole");
                                        }
                                        else
                                        {
                                            DEBUG_INFO(MODULE_GAME, "Hole is halved (tie)");
                                            /* players.halved = true */
                                        }

                                        DEBUG_INFO(MODULE_GAME,
                                                   "Match status - Player 1: %d holes won, Player "
                                                   "2: %d holes won",
                                                   players[0].holes_won, players[1].holes_won);
                                        DEBUG_DEBUG(MODULE_GAME,
                                                    "Player scores - Player 1: %d, Player 2: %d",
                                                    playersmp[0], playersmp[1]);
                                        DEBUG_DEBUG(MODULE_GAME,
                                                    "Up/Down status - Player 1: %d, Player 2: %d",
                                                    players[0].upAndDown, players[1].upAndDown);

                                        // Calculate if early victory condition is met
                                        int lead = (players[0].upAndDown > 0) ? players[0].upAndDown
                                                   : (players[1].upAndDown > 0)
                                                       ? players[1].upAndDown
                                                       : 0;
                                        int holes_remaining =
                                            current_hole_mode - players[0].current_hole - 1;
                                        int early_victory = (lead > 0 && lead > holes_remaining);

                                        DEBUG_DEBUG(MODULE_LOGIC,
                                                    "Early victory check: lead=%d, "
                                                    "holes_remaining=%d, early_victory=%d",
                                                    lead, holes_remaining, early_victory);

                                        // Announce Player/Team One for next hole (unless game is
                                        // over) Check for early victory condition before announcing
                                        if (players[0].current_hole < current_hole_mode &&
                                            !early_victory)
                                        {
                                            if (current_match_play_mode == MATCH_PLAY_MODE_1V1)
                                            {
                                                DEBUG_INFO(MODULE_LOGIC,
                                                           "Playing Player One sound");
                                                play_sound_once(
                                                    load_sound_effect(SOUND_PLAYERONE_WAV),
                                                    SOUND_DELAY_TURN_SWITCH_MS);
                                            }
                                            else if (current_match_play_mode == MATCH_PLAY_MODE_2V2)
                                            {
                                                DEBUG_INFO(MODULE_LOGIC, "Playing Team One sound");
                                                play_sound_once(
                                                    load_sound_effect(SOUND_TEAMONE_WAV),
                                                    SOUND_DELAY_TURN_SWITCH_MS);
                                            }
                                        }
                                        else
                                        {
                                            DEBUG_INFO(MODULE_LOGIC,
                                                       "Skipping player announcement - game ended "
                                                       "(early_victory=%d) or last hole",
                                                       early_victory);
                                        }

                                        // Advance both players to next hole
                                        players[0].current_hole++;
                                        players[1].current_hole++;
                                        players[0].round_total_score = 0;
                                        players[1].round_total_score = 0;

                                        DEBUG_INFO(MODULE_GAME, "Both players advanced to hole %d",
                                                   players[0].current_hole);

                                        // Update match play UI display
                                        DEBUG_DEBUG(MODULE_UI, "Updating Match Play UI display");

                                        if (current_match_play_mode == MATCH_PLAY_MODE_1V1)
                                        {
                                            if (current_hole_mode == NINE_HOLES)
                                            {
                                                // 1v1 9-hole UI updates
                                                if (players[0].upAndDown < 0)
                                                {
                                                    lv_label_set_text_fmt(
                                                        ui_MP1V19HGSP1SPText, "%dDN",
                                                        players[0].upAndDown * -1);
                                                    lv_label_set_text_fmt(ui_MP1V19HGSP2SPText,
                                                                          "%dUP",
                                                                          players[1].upAndDown);
                                                }
                                                else if (players[1].upAndDown < 0)
                                                {
                                                    lv_label_set_text_fmt(
                                                        ui_MP1V19HGSP2SPText, "%dDN",
                                                        players[1].upAndDown * -1);
                                                    lv_label_set_text_fmt(ui_MP1V19HGSP1SPText,
                                                                          "%dUP",
                                                                          players[0].upAndDown);
                                                }
                                                else
                                                {
                                                    lv_label_set_text_fmt(ui_MP1V19HGSP1SPText,
                                                                          "E");
                                                    lv_label_set_text_fmt(ui_MP1V19HGSP2SPText,
                                                                          "E");
                                                }
                                            }
                                            else if (current_hole_mode == EIGHTEEN_HOLES)
                                            {
                                                // New 18-hole UI updates
                                                if (players[0].upAndDown < 0)
                                                {
                                                    lv_label_set_text_fmt(
                                                        ui_MP1V118HGSP1SPText, "%dDN",
                                                        players[0].upAndDown * -1);
                                                    lv_label_set_text_fmt(ui_MP1V118HGSP2SPText,
                                                                          "%dUP",
                                                                          players[1].upAndDown);
                                                    DEBUG_DEBUG(
                                                        MODULE_UI,
                                                        "UI updated (18H) - Player 1: %dDN, "
                                                        "Player 2: %dUP",
                                                        players[0].upAndDown * -1,
                                                        players[1].upAndDown);
                                                }
                                                else if (players[1].upAndDown < 0)
                                                {
                                                    lv_label_set_text_fmt(
                                                        ui_MP1V118HGSP2SPText, "%dDN",
                                                        players[1].upAndDown * -1);
                                                    lv_label_set_text_fmt(ui_MP1V118HGSP1SPText,
                                                                          "%dUP",
                                                                          players[0].upAndDown);
                                                    DEBUG_DEBUG(
                                                        MODULE_UI,
                                                        "UI updated (18H) - Player 1: %dUP, "
                                                        "Player 2: %dDN",
                                                        players[0].upAndDown,
                                                        players[1].upAndDown * -1);
                                                }
                                                else
                                                {
                                                    lv_label_set_text_fmt(ui_MP1V118HGSP1SPText,
                                                                          "E");
                                                    lv_label_set_text_fmt(ui_MP1V118HGSP2SPText,
                                                                          "E");
                                                    DEBUG_DEBUG(MODULE_UI,
                                                                "UI updated (18H) - Both players: "
                                                                "E (Even)");
                                                }
                                            }
                                        }  // Close MATCH_PLAY_MODE_1V1 check
                                        else if (current_match_play_mode == MATCH_PLAY_MODE_2V2)
                                        {
                                            if (current_hole_mode == NINE_HOLES)
                                            {
                                                // 2v2 9-hole UI updates
                                                if (players[0].upAndDown < 0)
                                                {
                                                    lv_label_set_text_fmt(
                                                        ui_MP2V29HGST1SPText, "%dDN",
                                                        players[0].upAndDown * -1);
                                                    lv_label_set_text_fmt(ui_MP2V29HGST2SPText,
                                                                          "%dUP",
                                                                          players[1].upAndDown);
                                                }
                                                else if (players[1].upAndDown < 0)
                                                {
                                                    lv_label_set_text_fmt(
                                                        ui_MP2V29HGST2SPText, "%dDN",
                                                        players[1].upAndDown * -1);
                                                    lv_label_set_text_fmt(ui_MP2V29HGST1SPText,
                                                                          "%dUP",
                                                                          players[0].upAndDown);
                                                }
                                                else
                                                {
                                                    lv_label_set_text_fmt(ui_MP2V29HGST1SPText,
                                                                          "E");
                                                    lv_label_set_text_fmt(ui_MP2V29HGST2SPText,
                                                                          "E");
                                                }
                                            }
                                            else if (current_hole_mode == EIGHTEEN_HOLES)
                                            {
                                                // 2v2 18-hole UI updates
                                                if (players[0].upAndDown < 0)
                                                {
                                                    lv_label_set_text_fmt(
                                                        ui_MP2V218HGST1SPText, "%dDN",
                                                        players[0].upAndDown * -1);
                                                    lv_label_set_text_fmt(ui_MP2V218HGST2SPText,
                                                                          "%dUP",
                                                                          players[1].upAndDown);
                                                    DEBUG_DEBUG(
                                                        MODULE_UI,
                                                        "UI updated (2v2 18H) - Team 1: %dDN, "
                                                        "Team 2: %dUP",
                                                        players[0].upAndDown * -1,
                                                        players[1].upAndDown);
                                                }
                                                else if (players[1].upAndDown < 0)
                                                {
                                                    lv_label_set_text_fmt(
                                                        ui_MP2V218HGST2SPText, "%dDN",
                                                        players[1].upAndDown * -1);
                                                    lv_label_set_text_fmt(ui_MP2V218HGST1SPText,
                                                                          "%dUP",
                                                                          players[0].upAndDown);
                                                    DEBUG_DEBUG(
                                                        MODULE_UI,
                                                        "UI updated (2v2 18H) - Team 1: %dUP, "
                                                        "Team 2: %dDN",
                                                        players[0].upAndDown,
                                                        players[1].upAndDown * -1);
                                                }
                                                else
                                                {
                                                    lv_label_set_text_fmt(ui_MP2V218HGST1SPText,
                                                                          "E");
                                                    lv_label_set_text_fmt(ui_MP2V218HGST2SPText,
                                                                          "E");
                                                    DEBUG_DEBUG(MODULE_UI,
                                                                "UI updated (2v2 18H) - Both "
                                                                "teams: E (Even)");
                                                }
                                            }
                                        }  // Close MATCH_PLAY_MODE_2V2 check

                                        // Update score card for match play
                                        // Note: current_hole was already incremented, so use
                                        // (current_hole - 1) for the just-completed hole Update
                                        // score Update scorecard with hole result (1U, 1D, or E)
                                        // Update scorecard with hole result (1U, 1D, or E)
                                        if (current_match_play_mode == MATCH_PLAY_MODE_1V1)
                                        {
                                            uint8_t   completed_hole = players[0].current_hole;
                                            lv_obj_t* p1_text        = NULL;
                                            lv_obj_t* p2_text        = NULL;

                                            // Determine which UI elements to update based on hole
                                            // mode
                                            if (current_hole_mode == NINE_HOLES)
                                            {
                                                switch (completed_hole)
                                                {
                                                    case 1:
                                                        p1_text = ui_MP1V19HScP1SText1;
                                                        p2_text = ui_MP1V19HScP2SText1;
                                                        break;
                                                    case 2:
                                                        p1_text = ui_MP1V19HScP1SText2;
                                                        p2_text = ui_MP1V19HScP2SText2;
                                                        break;
                                                    case 3:
                                                        p1_text = ui_MP1V19HScP1SText3;
                                                        p2_text = ui_MP1V19HScP2SText3;
                                                        break;
                                                    case 4:
                                                        p1_text = ui_MP1V19HScP1SText4;
                                                        p2_text = ui_MP1V19HScP2SText4;
                                                        break;
                                                    case 5:
                                                        p1_text = ui_MP1V19HScP1SText5;
                                                        p2_text = ui_MP1V19HScP2SText5;
                                                        break;
                                                    case 6:
                                                        p1_text = ui_MP1V19HScP1SText6;
                                                        p2_text = ui_MP1V19HScP2SText6;
                                                        break;
                                                    case 7:
                                                        p1_text = ui_MP1V19HScP1SText7;
                                                        p2_text = ui_MP1V19HScP2SText7;
                                                        break;
                                                    case 8:
                                                        p1_text = ui_MP1V19HScP1SText8;
                                                        p2_text = ui_MP1V19HScP2SText8;
                                                        break;
                                                    case 9:
                                                        p1_text = ui_MP1V19HScP1SText9;
                                                        p2_text = ui_MP1V19HScP2SText9;
                                                        // Update final score
                                                        if (players[0].upAndDown > 0)
                                                        {
                                                            lv_label_set_text_fmt(
                                                                ui_MP1V19HScP1STextF, "%dU",
                                                                players[0].upAndDown);
                                                            lv_label_set_text_fmt(
                                                                ui_MP1V19HScP2STextF, "%dD",
                                                                players[1].upAndDown * -1);
                                                        }
                                                        else if (players[0].upAndDown < 0)
                                                        {
                                                            lv_label_set_text_fmt(
                                                                ui_MP1V19HScP1STextF, "%dD",
                                                                players[0].upAndDown * -1);
                                                            lv_label_set_text_fmt(
                                                                ui_MP1V19HScP2STextF, "%dU",
                                                                players[1].upAndDown);
                                                        }
                                                        else
                                                        {
                                                            lv_label_set_text(ui_MP1V19HScP1STextF,
                                                                              "E");
                                                            lv_label_set_text(ui_MP1V19HScP2STextF,
                                                                              "E");
                                                        }
                                                        break;
                                                }
                                            }
                                            else if (current_hole_mode == EIGHTEEN_HOLES)
                                            {
                                                switch (completed_hole)
                                                {
                                                    case 1:
                                                        p1_text = ui_MP1V118HScP1SText19;
                                                        p2_text = ui_MP1V118HScP2SText1;
                                                        break;
                                                    case 2:
                                                        p1_text = ui_MP1V118HScP1SText2;
                                                        p2_text = ui_MP1V118HScP2SText2;
                                                        break;
                                                    case 3:
                                                        p1_text = ui_MP1V118HScP1SText3;
                                                        p2_text = ui_MP1V118HScP2SText3;
                                                        break;
                                                    case 4:
                                                        p1_text = ui_MP1V118HScP1SText4;
                                                        p2_text = ui_MP1V118HScP2SText4;
                                                        break;
                                                    case 5:
                                                        p1_text = ui_MP1V118HScP1SText5;
                                                        p2_text = ui_MP1V118HScP2SText5;
                                                        break;
                                                    case 6:
                                                        p1_text = ui_MP1V118HScP1SText6;
                                                        p2_text = ui_MP1V118HScP2SText6;
                                                        break;
                                                    case 7:
                                                        p1_text = ui_MP1V118HScP1SText7;
                                                        p2_text = ui_MP1V118HScP2SText7;
                                                        break;
                                                    case 8:
                                                        p1_text = ui_MP1V118HScP1SText8;
                                                        p2_text = ui_MP1V118HScP2SText8;
                                                        break;
                                                    case 9:
                                                        p1_text = ui_MP1V118HScP1SText9;
                                                        p2_text = ui_MP1V118HScP2SText9;
                                                        break;
                                                    case 10:
                                                        p1_text = ui_MP1V118HScP1SText10;
                                                        p2_text = ui_MP1V118HScP2SText10;
                                                        break;
                                                    case 11:
                                                        p1_text = ui_MP1V118HScP1SText11;
                                                        p2_text = ui_MP1V118HScP2SText11;
                                                        break;
                                                    case 12:
                                                        p1_text = ui_MP1V118HScP1SText12;
                                                        p2_text = ui_MP1V118HScP2SText12;
                                                        break;
                                                    case 13:
                                                        p1_text = ui_MP1V118HScP1SText13;
                                                        p2_text = ui_MP1V118HScP2SText13;
                                                        break;
                                                    case 14:
                                                        p1_text = ui_MP1V118HScP1SText14;
                                                        p2_text = ui_MP1V118HScP2SText14;
                                                        break;
                                                    case 15:
                                                        p1_text = ui_MP1V118HScP1SText15;
                                                        p2_text = ui_MP1V118HScP2SText15;
                                                        break;
                                                    case 16:
                                                        p1_text = ui_MP1V118HScP1SText16;
                                                        p2_text = ui_MP1V118HScP2SText16;
                                                        break;
                                                    case 17:
                                                        p1_text = ui_MP1V118HScP1SText17;
                                                        p2_text = ui_MP1V118HScP2SText17;
                                                        break;
                                                    case 18:
                                                        p1_text = ui_MP1V118HScP1SText18;
                                                        p2_text = ui_MP1V118HScP2SText18;
                                                        // Update final score for 18 holes
                                                        if (players[0].upAndDown > 0)
                                                        {
                                                            lv_label_set_text_fmt(
                                                                ui_MP1V118HScP1STextF, "%dU",
                                                                players[0].upAndDown);
                                                            lv_label_set_text_fmt(
                                                                ui_MP1V118HScP2STextF, "%dD",
                                                                players[1].upAndDown * -1);
                                                        }
                                                        else if (players[0].upAndDown < 0)
                                                        {
                                                            lv_label_set_text_fmt(
                                                                ui_MP1V118HScP1STextF, "%dD",
                                                                players[0].upAndDown * -1);
                                                            lv_label_set_text_fmt(
                                                                ui_MP1V118HScP2STextF, "%dU",
                                                                players[1].upAndDown);
                                                        }
                                                        else
                                                        {
                                                            lv_label_set_text(ui_MP1V118HScP1STextF,
                                                                              "E");
                                                            lv_label_set_text(ui_MP1V118HScP2STextF,
                                                                              "E");
                                                        }
                                                        break;
                                                }
                                            }
                                            if (p1_text && p2_text)
                                            {
                                                if (players[0].upAndDown > 0)
                                                {
                                                    // Player 1 is up
                                                    lv_label_set_text_fmt(p1_text, "%dU",
                                                                          players[0].upAndDown);
                                                    lv_label_set_text_fmt(
                                                        p2_text, "%dD", players[1].upAndDown * -1);
                                                    DEBUG_DEBUG(MODULE_UI,
                                                                "Scorecard Hole %d: P1=%dU, P2=%dD",
                                                                completed_hole,
                                                                players[0].upAndDown,
                                                                players[1].upAndDown * -1);
                                                }
                                                else if (players[0].upAndDown < 0)
                                                {
                                                    // Player 1 is down
                                                    lv_label_set_text_fmt(
                                                        p1_text, "%dD", players[0].upAndDown * -1);
                                                    lv_label_set_text_fmt(p2_text, "%dU",
                                                                          players[1].upAndDown);
                                                    DEBUG_DEBUG(MODULE_UI,
                                                                "Scorecard Hole %d: P1=%dD, P2=%dU",
                                                                completed_hole,
                                                                players[0].upAndDown * -1,
                                                                players[1].upAndDown);
                                                }
                                                else
                                                {
                                                    // Match is even
                                                    lv_label_set_text(p1_text, "E");
                                                    lv_label_set_text(p2_text, "E");
                                                    DEBUG_DEBUG(MODULE_UI,
                                                                "Scorecard Hole %d: P1=E, P2=E",
                                                                completed_hole);
                                                }
                                            }
                                        }  // Close MATCH_PLAY_MODE_1V1 scorecard
                                        else if (current_match_play_mode == MATCH_PLAY_MODE_2V2)
                                        {
                                            uint8_t   completed_hole = players[0].current_hole;
                                            lv_obj_t* t1_text        = NULL;
                                            lv_obj_t* t2_text        = NULL;

                                            // Determine which UI elements to update based on hole
                                            // mode
                                            if (current_hole_mode == NINE_HOLES)
                                            {
                                                switch (completed_hole)
                                                {
                                                    case 1:
                                                        t1_text = ui_MP2V29HScT1SText1;
                                                        t2_text = ui_MP2V29HScT2SText1;
                                                        break;
                                                    case 2:
                                                        t1_text = ui_MP2V29HScT1SText2;
                                                        t2_text = ui_MP2V29HScT2SText2;
                                                        break;
                                                    case 3:
                                                        t1_text = ui_MP2V29HScT1SText3;
                                                        t2_text = ui_MP2V29HScT2SText3;
                                                        break;
                                                    case 4:
                                                        t1_text = ui_MP2V29HScT1SText4;
                                                        t2_text = ui_MP2V29HScT2SText4;
                                                        break;
                                                    case 5:
                                                        t1_text = ui_MP2V29HScT1SText5;
                                                        t2_text = ui_MP2V29HScT2SText5;
                                                        break;
                                                    case 6:
                                                        t1_text = ui_MP2V29HScT1SText6;
                                                        t2_text = ui_MP2V29HScT2SText6;
                                                        break;
                                                    case 7:
                                                        t1_text = ui_MP2V29HScT1SText7;
                                                        t2_text = ui_MP2V29HScT2SText7;
                                                        break;
                                                    case 8:
                                                        t1_text = ui_MP2V29HScT1SText8;
                                                        t2_text = ui_MP2V29HScT2SText8;
                                                        break;
                                                    case 9:
                                                        t1_text = ui_MP2V29HScT1SText9;
                                                        t2_text = ui_MP2V29HScT2SText9;
                                                        // Update final score
                                                        if (players[0].upAndDown > 0)
                                                        {
                                                            lv_label_set_text_fmt(
                                                                ui_MP2V29HScT1STextF, "%dU",
                                                                players[0].upAndDown);
                                                            lv_label_set_text_fmt(
                                                                ui_MP2V29HScT2STextF, "%dD",
                                                                players[1].upAndDown * -1);
                                                        }
                                                        else if (players[0].upAndDown < 0)
                                                        {
                                                            lv_label_set_text_fmt(
                                                                ui_MP2V29HScT1STextF, "%dD",
                                                                players[0].upAndDown * -1);
                                                            lv_label_set_text_fmt(
                                                                ui_MP2V29HScT2STextF, "%dU",
                                                                players[1].upAndDown);
                                                        }
                                                        else
                                                        {
                                                            lv_label_set_text(ui_MP2V29HScT1STextF,
                                                                              "E");
                                                            lv_label_set_text(ui_MP2V29HScT2STextF,
                                                                              "E");
                                                        }
                                                        break;
                                                }
                                            }
                                            else if (current_hole_mode == EIGHTEEN_HOLES)
                                            {
                                                switch (completed_hole)
                                                {
                                                    case 1:
                                                        t1_text = ui_MP2V218HScT1SText1;
                                                        t2_text = ui_MP2V218HScT2SText1;
                                                        break;
                                                    case 2:
                                                        t1_text = ui_MP2V218HScT1SText2;
                                                        t2_text = ui_MP2V218HScT2SText2;
                                                        break;
                                                    case 3:
                                                        t1_text = ui_MP2V218HScT1SText3;
                                                        t2_text = ui_MP2V218HScT2SText3;
                                                        break;
                                                    case 4:
                                                        t1_text = ui_MP2V218HScT1SText4;
                                                        t2_text = ui_MP2V218HScT2SText4;
                                                        break;
                                                    case 5:
                                                        t1_text = ui_MP2V218HScT1SText5;
                                                        t2_text = ui_MP2V218HScT2SText5;
                                                        break;
                                                    case 6:
                                                        t1_text = ui_MP2V218HScT1SText6;
                                                        t2_text = ui_MP2V218HScT2SText6;
                                                        break;
                                                    case 7:
                                                        t1_text = ui_MP2V218HScT1SText7;
                                                        t2_text = ui_MP2V218HScT2SText7;
                                                        break;
                                                    case 8:
                                                        t1_text = ui_MP2V218HScT1SText8;
                                                        t2_text = ui_MP2V218HScT2SText8;
                                                        break;
                                                    case 9:
                                                        t1_text = ui_MP2V218HScT1SText9;
                                                        t2_text = ui_MP2V218HScT2SText9;
                                                        break;
                                                    case 10:
                                                        t1_text = ui_MP2V218HScT1SText10;
                                                        t2_text = ui_MP2V218HScT2SText10;
                                                        break;
                                                    case 11:
                                                        t1_text = ui_MP2V218HScT1SText11;
                                                        t2_text = ui_MP2V218HScT2SText11;
                                                        break;
                                                    case 12:
                                                        t1_text = ui_MP2V218HScT1SText12;
                                                        t2_text = ui_MP2V218HScT2SText12;
                                                        break;
                                                    case 13:
                                                        t1_text = ui_MP2V218HScT1SText13;
                                                        t2_text = ui_MP2V218HScT2SText13;
                                                        break;
                                                    case 14:
                                                        t1_text = ui_MP2V218HScT1SText14;
                                                        t2_text = ui_MP2V218HScT2SText14;
                                                        break;
                                                    case 15:
                                                        t1_text = ui_MP2V218HScT1SText15;
                                                        t2_text = ui_MP2V218HScT2SText15;
                                                        break;
                                                    case 16:
                                                        t1_text = ui_MP2V218HScT1SText16;
                                                        t2_text = ui_MP2V218HScT2SText16;
                                                        break;
                                                    case 17:
                                                        t1_text = ui_MP2V218HScT1SText17;
                                                        t2_text = ui_MP2V218HScT2SText17;
                                                        break;
                                                    case 18:
                                                        t1_text = ui_MP2V218HScT1SText18;
                                                        t2_text = ui_MP2V218HScT2SText18;
                                                        // Update final score for 18 holes
                                                        if (players[0].upAndDown > 0)
                                                        {
                                                            lv_label_set_text_fmt(
                                                                ui_MP2V218HScT1STextF, "%dU",
                                                                players[0].upAndDown);
                                                            lv_label_set_text_fmt(
                                                                ui_MP2V218HScT2STextF, "%dD",
                                                                players[1].upAndDown * -1);
                                                        }
                                                        else if (players[0].upAndDown < 0)
                                                        {
                                                            lv_label_set_text_fmt(
                                                                ui_MP2V218HScT1STextF, "%dD",
                                                                players[0].upAndDown * -1);
                                                            lv_label_set_text_fmt(
                                                                ui_MP2V218HScT2STextF, "%dU",
                                                                players[1].upAndDown);
                                                        }
                                                        else
                                                        {
                                                            lv_label_set_text(ui_MP2V218HScT1STextF,
                                                                              "E");
                                                            lv_label_set_text(ui_MP2V218HScT2STextF,
                                                                              "E");
                                                        }
                                                        break;
                                                }
                                            }

                                            if (t1_text && t2_text)
                                            {
                                                if (players[0].upAndDown > 0)
                                                {
                                                    // Team 1 is up
                                                    lv_label_set_text_fmt(t1_text, "%dU",
                                                                          players[0].upAndDown);
                                                    lv_label_set_text_fmt(
                                                        t2_text, "%dD", players[1].upAndDown * -1);
                                                    DEBUG_DEBUG(MODULE_UI,
                                                                "Scorecard Hole %d: T1=%dU, T2=%dD",
                                                                completed_hole,
                                                                players[0].upAndDown,
                                                                players[1].upAndDown * -1);
                                                }
                                                else if (players[0].upAndDown < 0)
                                                {
                                                    // Team 1 is down
                                                    lv_label_set_text_fmt(
                                                        t1_text, "%dD", players[0].upAndDown * -1);
                                                    lv_label_set_text_fmt(t2_text, "%dU",
                                                                          players[1].upAndDown);
                                                    DEBUG_DEBUG(MODULE_UI,
                                                                "Scorecard Hole %d: T1=%dD, T2=%dU",
                                                                completed_hole,
                                                                players[0].upAndDown * -1,
                                                                players[1].upAndDown);
                                                }
                                                else
                                                {
                                                    // Match is even
                                                    lv_label_set_text(t1_text, "E");
                                                    lv_label_set_text(t2_text, "E");
                                                    DEBUG_DEBUG(MODULE_UI,
                                                                "Scorecard Hole %d: T1=E, T2=E",
                                                                completed_hole);
                                                }
                                            }
                                        }  // Close MATCH_PLAY_MODE_2V2 scorecard

                                        player->detection_count = 0;
                                        DEBUG_DEBUG(MODULE_GAME,
                                                    "Reset detection count for Player %d after "
                                                    "match evaluation",
                                                    current_player_index + 1);
                                    }

                                    // After evaluating the match play for both players, update the
                                    // hole number and reset detection count
                                    DEBUG_INFO(MODULE_GAME, "Player %d advanced to hole %d",
                                               current_player_index + 1, player->current_hole);

                                    player->detection_count = 0;
                                    DEBUG_DEBUG(MODULE_GAME, "Detection count reset for Player %d",
                                                current_player_index + 1);

                                    // Check if all players are done with their shots
                                    check_all_players_completed(current_game_mode);
                                    break;

                                case GAME_MODE_QUOTA:
                                    player->current_hole++;
                                    player->detection_count = 0;
                                    DEBUG_INFO(MODULE_GAME,
                                               "Quota turn complete - Player %d, Hole %d",
                                               current_player_index + 1, player->current_hole);

                                    // Check if any player completed their quota
                                    check_all_players_completed(current_game_mode);

                                    // Announce next player if game is still running
                                    if (!update_flag && num_players == 2)
                                    {
                                        if (current_player_index == 0)
                                        {
                                            play_sound_once(load_sound_effect(SOUND_PLAYERTWO_WAV),
                                                            SOUND_DELAY_TURN_SWITCH_MS);
                                        }
                                        else
                                        {
                                            play_sound_once(load_sound_effect(SOUND_PLAYERONE_WAV),
                                                            SOUND_DELAY_TURN_SWITCH_MS);
                                        }
                                    }
                                    else if (!update_flag && num_players == 3)
                                    {
                                        if (current_player_index == 0)
                                        {
                                            play_sound_once(load_sound_effect(SOUND_PLAYERTWO_WAV),
                                                            SOUND_DELAY_TURN_SWITCH_MS);
                                        }
                                        else if (current_player_index == 1)
                                        {
                                            play_sound_once(load_sound_effect(SOUND_PLAYERTHREE_WAV),
                                                            SOUND_DELAY_TURN_SWITCH_MS);
                                        }
                                        else
                                        {
                                            play_sound_once(load_sound_effect(SOUND_PLAYERONE_WAV),
                                                            SOUND_DELAY_TURN_SWITCH_MS);
                                        }
                                    }
                                    break;

                                case GAME_MODE_VEGAS:
                                    DEBUG_TRACE(MODULE_GAME,
                                                "Vegas mode turn completion (not yet implemented)");
                                    break;
                            }

                            // Advance to next player
                            current_player_index = (current_player_index + 1) % num_players;
                            DEBUG_INFO(MODULE_GAME, "Switched to next player: Player %d",
                                       current_player_index + 1);
                        }
                    }
                    else
                    {
                        DEBUG_DEBUG(MODULE_GAME, "Player %d already has maximum detections (%d)",
                                    current_player_index + 1, SENSORS_PER_TURN);
                    }
                }
                else
                {
                    DEBUG_TRACE(
                        MODULE_GPIO,
                        "Sensor %d (pin %d) in debounce period (timer active) - ignoring event",
                        sensor_index, pin_offset);
                }
            }
            else
            {
                if (!sensors_enabled)
                {
                    DEBUG_TRACE(MODULE_GPIO, "Sensors disabled - ignoring GPIO event");
                }
                else
                {
                    DEBUG_ERROR(MODULE_GPIO, "Failed to read GPIO line event");
                }
            }
        }
    }
    else
    {
        DEBUG_TRACE(MODULE_GPIO, "No GPIO events detected");
    }
}

/**
 * @brief Update player data and print to console.
 * @param player_index Index of the player (0-based).
 * @param current_hole Current hole the player is on.
 * @param score Player's current score.
 * @param detection_count Number of detections in the current turn.
 */
void logic_update_label_text(int player_index, int current_hole, int score, int detection_count,
                             int num_players)
{
    DEBUG_TRACE(MODULE_LOGIC, "logic_update_label_text called");
    DEBUG_DEBUG(MODULE_LOGIC, "Player:%d, Hole:%d, Score:%d, Detections:%d, Players:%d",
                player_index + 1, current_hole + 1, score, detection_count, num_players);

    // Return early if game is completed
    if (update_flag)
    {
        DEBUG_INFO(MODULE_LOGIC, "Update flag set, skipping label update");
        return;
    }

    update_player_highlight(detection_count, current_hole + 1, num_players);

    // Use a switch statement for num_players
    switch (current_game_mode)
    {
        case GAME_MODE_MATCH_PLAY:
            DEBUG_TRACE(MODULE_LOGIC, "Match Play mode");
            switch (current_match_play_mode)
            {
                case MATCH_PLAY_MODE_1V1:
                {
                    DEBUG_TRACE(MODULE_LOGIC, "1v1 Match Play");
                    switch (current_hole_mode)
                    {
                        case NINE_HOLES:
                            DEBUG_TRACE(MODULE_LOGIC, "Nine Holes mode");
                            DEBUG_INFO(MODULE_LOGIC, "Player#%d Hole:%d - Ball%d",
                                       current_player_index, current_hole + 1, detection_count);

                            if (players[1].current_hole >= current_hole_mode)
                            {
                                DEBUG_DEBUG(MODULE_LOGIC,
                                            "Condition TRUE - players[1].current_hole (%d) >= "
                                            "current_hole_mode (%d)",
                                            players[1].current_hole, current_hole_mode);
                                lv_label_set_text_fmt(ui_MP1V19HGSHCPText, "%d",
                                                      current_hole_mode + 1);
                                DEBUG_INFO(MODULE_LOGIC, "Set ui_MP1V19HGSHCPText to: %d",
                                           current_hole_mode + 1);
                            }
                            else
                            {
                                DEBUG_DEBUG(MODULE_LOGIC,
                                            "Condition FALSE - players[1].current_hole (%d) < "
                                            "current_hole_mode (%d)",
                                            players[1].current_hole, current_hole_mode);
                                lv_label_set_text_fmt(ui_MP1V19HGSHCPText, "%d",
                                                      players[1].current_hole + 1);
                                DEBUG_INFO(MODULE_LOGIC, "Set ui_MP1V19HGSHCPText to: %d",
                                           players[1].current_hole + 1);
                            }

                            DEBUG_INFO(
                                MODULE_LOGIC,
                                "SCORE DISPLAY: Setting score value to %d on ui_MP1V19HGSHCPText",
                                score);
                            lv_label_set_text_fmt(ui_MP1V19HGSHCPText, "%d", current_hole + 1);

                            DEBUG_INFO(
                                MODULE_LOGIC,
                                "HOLE DISPLAY: Setting current hole to %d on ui_MP1V19HGSBCPText",
                                current_hole + 1);
                            lv_label_set_text_fmt(ui_MP1V19HGSBCPText, "%d", detection_count);
                            break;

                        case EIGHTEEN_HOLES:
                            DEBUG_TRACE(MODULE_LOGIC, "Eighteen Holes mode");
                            DEBUG_INFO(MODULE_LOGIC, "Player#%d Hole:%d - Ball%d",
                                       current_player_index + 1, current_hole + 1, detection_count);

                            // Update hole counter display
                            if (players[1].current_hole >= current_hole_mode)
                            {
                                DEBUG_DEBUG(MODULE_LOGIC,
                                            "Condition TRUE - players[1].current_hole (%d) >= "
                                            "current_hole_mode (%d)",
                                            players[1].current_hole, current_hole_mode);
                                lv_label_set_text_fmt(ui_MP1V118HGSHCPText, "%d",
                                                      current_hole_mode + 1);
                                DEBUG_INFO(MODULE_LOGIC, "Set ui_MP1V118HGSHCPText to: %d",
                                           current_hole_mode + 1);
                            }
                            else
                            {
                                DEBUG_DEBUG(MODULE_LOGIC,
                                            "Condition FALSE - players[1].current_hole (%d) < "
                                            "current_hole_mode (%d)",
                                            players[1].current_hole, current_hole_mode);
                                lv_label_set_text_fmt(ui_MP1V118HGSHCPText, "%d",
                                                      players[1].current_hole + 1);
                                DEBUG_INFO(MODULE_LOGIC, "Set ui_MP1V118HGSHCPText to: %d",
                                           players[1].current_hole + 1);
                            }

                            // Update current hole display
                            DEBUG_INFO(
                                MODULE_LOGIC,
                                "HOLE DISPLAY: Setting current hole to %d on ui_MP1V118HGSHCPText",
                                current_hole + 1);
                            lv_label_set_text_fmt(ui_MP1V118HGSHCPText, "%d", current_hole + 1);

                            // Update ball counter display
                            DEBUG_INFO(
                                MODULE_LOGIC,
                                "BALL DISPLAY: Setting ball count to %d on ui_MP1V118HGSBCPText",
                                detection_count);
                            lv_label_set_text_fmt(ui_MP1V118HGSBCPText, "%d", detection_count);
                            break;
                    }
                    break;
                }
                case MATCH_PLAY_MODE_2V2:
                {
                    DEBUG_TRACE(MODULE_LOGIC, "2v2 Match Play");
                    switch (current_hole_mode)
                    {
                        case NINE_HOLES:
                            DEBUG_TRACE(MODULE_LOGIC, "Nine Holes mode");
                            DEBUG_INFO(MODULE_LOGIC, "Team#%d Hole:%d - Ball%d",
                                       current_player_index + 1, current_hole + 1, detection_count);

                            // Update hole counter display
                            if (players[1].current_hole >= current_hole_mode)
                            {
                                lv_label_set_text_fmt(ui_MP2V29HGSHCPText, "%d",
                                                      current_hole_mode + 1);
                                DEBUG_INFO(MODULE_LOGIC, "Set ui_MP2V29HGSHCPText to: %d",
                                           current_hole_mode + 1);
                            }
                            else
                            {
                                lv_label_set_text_fmt(ui_MP2V29HGSHCPText, "%d",
                                                      players[1].current_hole + 1);
                                DEBUG_INFO(MODULE_LOGIC, "Set ui_MP2V29HGSHCPText to: %d",
                                           players[1].current_hole + 1);
                            }

                            // Update current hole and ball counter
                            lv_label_set_text_fmt(ui_MP2V29HGSHCPText, "%d", current_hole + 1);
                            lv_label_set_text_fmt(ui_MP2V29HGSBCPText, "%d", detection_count);
                            break;

                        case EIGHTEEN_HOLES:
                            DEBUG_TRACE(MODULE_LOGIC, "Eighteen Holes mode - 2v2");
                            DEBUG_INFO(MODULE_LOGIC, "Team#%d Hole:%d - Ball%d",
                                       current_player_index + 1, current_hole + 1, detection_count);

                            // Update hole counter display
                            if (players[1].current_hole >= current_hole_mode)
                            {
                                lv_label_set_text_fmt(ui_MP2V218HGSHCPText, "%d",
                                                      current_hole_mode + 1);
                                DEBUG_INFO(MODULE_LOGIC, "Set ui_MP2V218HGSHCPText to: %d",
                                           current_hole_mode + 1);
                            }
                            else
                            {
                                lv_label_set_text_fmt(ui_MP2V218HGSHCPText, "%d",
                                                      players[1].current_hole + 1);
                                DEBUG_INFO(MODULE_LOGIC, "Set ui_MP2V218HGSHCPText to: %d",
                                           players[1].current_hole + 1);
                            }

                            // Update current hole and ball counter
                            lv_label_set_text_fmt(ui_MP2V218HGSHCPText, "%d", current_hole + 1);
                            lv_label_set_text_fmt(ui_MP2V218HGSBCPText, "%d", detection_count);
                            break;
                    }
                    break;
                }
            }
            break;

        case GAME_MODE_QUOTA:
            DEBUG_TRACE(MODULE_LOGIC, "Quota mode");
            switch (current_hole_mode)
            {
                case NINE_HOLES:
                    switch (num_players)
                    {
                        case 1:
                            DEBUG_TRACE(MODULE_LOGIC, "Quota 1P 9H");
                            lv_label_set_text_fmt(ui_Q1P9HGSBCPText, "%d", detection_count);
                            lv_label_set_text_fmt(ui_Q1P9HGSPSPar3PText, "%d",
                                                  players[player_index].par3_count);
                            lv_label_set_text_fmt(ui_Q1P9HGSPSPar4PText, "%d",
                                                  players[player_index].par4_count);
                            lv_label_set_text_fmt(ui_Q1P9HGSPSPar5PText, "%d",
                                                  players[player_index].par5_count);
                            DEBUG_INFO(MODULE_LOGIC,
                                       "Quota 1P 9H updated - Balls:%d, 3pt:%d, 4pt:%d, 5pt:%d",
                                       detection_count, players[player_index].par3_count,
                                       players[player_index].par4_count,
                                       players[player_index].par5_count);
                            break;
                        case 2:
                            DEBUG_TRACE(MODULE_LOGIC, "Quota 2P 9H");
                            lv_label_set_text_fmt(ui_Q2P9HGSBCPText, "%d", detection_count);
                            lv_label_set_text_fmt(ui_Q2P9HGSP1SPar3PText, "%d",
                                                  players[0].par3_count);
                            lv_label_set_text_fmt(ui_Q2P9HGSP1SPar4PText, "%d",
                                                  players[0].par4_count);
                            lv_label_set_text_fmt(ui_Q2P9HGSP1SPar5PText, "%d",
                                                  players[0].par5_count);
                            lv_label_set_text_fmt(ui_Q2P9HGSP2SPar3PText, "%d",
                                                  players[1].par3_count);
                            lv_label_set_text_fmt(ui_Q2P9HGSP2SPar4PText, "%d",
                                                  players[1].par4_count);
                            lv_label_set_text_fmt(ui_Q2P9HGSP2SPar5PText, "%d",
                                                  players[1].par5_count);
                            break;
                        case 3:
                            DEBUG_TRACE(MODULE_LOGIC, "Quota 3P 9H");
                            lv_label_set_text_fmt(ui_Q3P9HGSBCPText, "%d", detection_count);
                            lv_label_set_text_fmt(ui_Q3P9HGSP1SPar3PText, "%d",
                                                  players[0].par3_count);
                            lv_label_set_text_fmt(ui_Q3P9HGSP1SPar4PText, "%d",
                                                  players[0].par4_count);
                            lv_label_set_text_fmt(ui_Q3P9HGSP1SPar5PText, "%d",
                                                  players[0].par5_count);
                            lv_label_set_text_fmt(ui_Q3P9HGSP2SPar3PText, "%d",
                                                  players[1].par3_count);
                            lv_label_set_text_fmt(ui_Q3P9HGSP2SPar4PText, "%d",
                                                  players[1].par4_count);
                            lv_label_set_text_fmt(ui_Q3P9HGSP2SPar5PText, "%d",
                                                  players[1].par5_count);
                            lv_label_set_text_fmt(ui_Q3P9HGSP3SPar3PText, "%d",
                                                  players[2].par3_count);
                            lv_label_set_text_fmt(ui_Q3P9HGSP3SPar4PText, "%d",
                                                  players[2].par4_count);
                            lv_label_set_text_fmt(ui_Q3P9HGSP3SPar5PText, "%d",
                                                  players[2].par5_count);
                            break;
                    }
                    break;
                case EIGHTEEN_HOLES:
                    switch (num_players)
                    {
                        case 1:
                            DEBUG_TRACE(MODULE_LOGIC, "Quota 1P 18H");
                            lv_label_set_text_fmt(ui_Q1P18HGSBCPText, "%d", detection_count);
                            lv_label_set_text_fmt(ui_Q1P18HGSPSPar3PText, "%d",
                                                  players[player_index].par3_count);
                            lv_label_set_text_fmt(ui_Q1P18HGSPSPar4PText, "%d",
                                                  players[player_index].par4_count);
                            lv_label_set_text_fmt(ui_Q1P18HGSPSPar5PText, "%d",
                                                  players[player_index].par5_count);
                            DEBUG_INFO(MODULE_LOGIC,
                                       "Quota 1P 18H updated - Balls:%d, 3pt:%d, 4pt:%d, 5pt:%d",
                                       detection_count, players[player_index].par3_count,
                                       players[player_index].par4_count,
                                       players[player_index].par5_count);
                            break;
                        case 2:
                            DEBUG_TRACE(MODULE_LOGIC, "Quota 2P 18H");
                            lv_label_set_text_fmt(ui_Q2P18HGSBCPText, "%d", detection_count);
                            lv_label_set_text_fmt(ui_Q2P9HGSP1SPar3PText1, "%d",
                                                  players[0].par3_count);
                            lv_label_set_text_fmt(ui_Q2P9HGSP1SPar4PText1, "%d",
                                                  players[0].par4_count);
                            lv_label_set_text_fmt(ui_Q2P9HGSP1SPar5PText1, "%d",
                                                  players[0].par5_count);
                            lv_label_set_text_fmt(ui_Q2P18HGSP2SPar3PText, "%d",
                                                  players[1].par3_count);
                            lv_label_set_text_fmt(ui_Q2P18HGSP2SPar4PText, "%d",
                                                  players[1].par4_count);
                            lv_label_set_text_fmt(ui_Q2P18HGSP2SPar5PText, "%d",
                                                  players[1].par5_count);
                            break;
                        case 3:
                            DEBUG_TRACE(MODULE_LOGIC, "Quota 3P 18H");
                            lv_label_set_text_fmt(ui_Q3P18HGSBCPText, "%d", detection_count);
                            lv_label_set_text_fmt(ui_Q3P18HGSP1SPar3PText, "%d",
                                                  players[0].par3_count);
                            lv_label_set_text_fmt(ui_Q3P18HGSP1SPar4PText, "%d",
                                                  players[0].par4_count);
                            lv_label_set_text_fmt(ui_Q3P18HGSP1SPar5PText, "%d",
                                                  players[0].par5_count);
                            lv_label_set_text_fmt(ui_Q3P18HGSP2SPar3PText, "%d",
                                                  players[1].par3_count);
                            lv_label_set_text_fmt(ui_Q3P18HGSP2SPar4PText, "%d",
                                                  players[1].par4_count);
                            lv_label_set_text_fmt(ui_Q3P18HGSP2SPar5PText, "%d",
                                                  players[1].par5_count);
                            lv_label_set_text_fmt(ui_Q3P18HGSP3SPar3PText, "%d",
                                                  players[2].par3_count);
                            lv_label_set_text_fmt(ui_Q3P18HGSP3SPar4PText, "%d",
                                                  players[2].par4_count);
                            lv_label_set_text_fmt(ui_Q3P18HGSP3SPar5PText, "%d",
                                                  players[2].par5_count);
                            break;
                    }
                    break;
            }
            break;

        case GAME_MODE_VEGAS:
            DEBUG_TRACE(MODULE_LOGIC, "Vegas mode");
        case GAME_MODE_STROKE_PLAY:
            DEBUG_TRACE(MODULE_LOGIC, "Stroke Play mode");
            switch (current_hole_mode)
            {
                case NINE_HOLES:
                    DEBUG_TRACE(MODULE_LOGIC, "Nine Holes mode");
                    switch (num_players)
                    {
                        case 1:
                            DEBUG_TRACE(MODULE_LOGIC, "Single Player");
                            DEBUG_INFO(MODULE_LOGIC,
                                       "Updating UI for Single Player Mode - Player %d",
                                       player_index + 1);
                            lv_label_set_text_fmt(ui_SP1P9HGSPSText, "Player %d", player_index + 1);
                            lv_label_set_text_fmt(ui_SP1P9HGSPSPText, "%d", score);
                            lv_label_set_text_fmt(ui_SP1P9HGSHCPText, "%d", current_hole + 1);
                            lv_label_set_text_fmt(ui_SP1P9HGSBCPText, "%d", detection_count);
                            lv_obj_set_style_bg_color(ui_SP1P9HGSPSPanel, lv_color_hex(COLOR_1),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                            DEBUG_INFO(
                                MODULE_LOGIC,
                                "Single Player UI updated - Player:%d, Score:%d, Hole:%d, Balls:%d",
                                player_index + 1, score, current_hole + 1, detection_count);
                            break;

                        case 2:
                            DEBUG_TRACE(MODULE_LOGIC, "Two Players");
                            DEBUG_INFO(MODULE_LOGIC, "Two Player Mode");
                            switch (player_index)
                            {
                                case 0:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 1");
                                    lv_label_set_text_fmt(ui_SP2P9HGSP1SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP2P9HGSP1SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP2P9HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP2P9HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC,
                                               "Player 1 updated - Score:%d, Hole:%d, Balls:%d",
                                               score, current_hole + 1, detection_count);

                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player Two sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERTWO_WAV),
                                                        750);
                                    }
                                    break;

                                case 1:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 2");
                                    lv_label_set_text_fmt(ui_SP2P9HGSP2SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP2P9HGSP2SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP2P9HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP2P9HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC,
                                               "Player 2 updated - Score:%d, Hole:%d, Balls:%d",
                                               score, current_hole + 1, detection_count);
                                    if (detection_count == 2 && current_hole != NINE_HOLES)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player One sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERONE_WAV),
                                                        750);
                                    }
                                    break;
                            }
                            break;

                        case 3:
                            DEBUG_TRACE(MODULE_LOGIC, "Three Players");
                            DEBUG_INFO(MODULE_LOGIC, "Three Player Mode");
                            switch (player_index)
                            {
                                case 0:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 1");
                                    lv_label_set_text_fmt(ui_SP3P9HGSP1SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP3P9HGSP1SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP3P9HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP3P9HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC, "Player 1 updated");
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player Two sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERTWO_WAV),
                                                        750);
                                    }
                                    break;
                                case 1:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 2");
                                    lv_label_set_text_fmt(ui_SP3P9HGSP2SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP3P9HGSP2SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP3P9HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP3P9HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC, "Player 2 updated");
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player Three sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERTHREE_WAV),
                                                        SOUND_DELAY_TURN_SWITCH_MS);
                                    }
                                    break;
                                case 2:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 3");
                                    lv_label_set_text_fmt(ui_SP3P9HGSP3SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP3P9HGSP3SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP3P9HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP3P9HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC, "Player 3 updated");
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player One sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERONE_WAV),
                                                        750);
                                    }
                                    break;
                            }
                            break;

                        case 4:
                            DEBUG_TRACE(MODULE_LOGIC, "Four Players");
                            DEBUG_INFO(MODULE_LOGIC, "Four Player Mode");
                            switch (player_index)
                            {
                                case 0:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 1");
                                    lv_label_set_text_fmt(ui_SP4P9HGSP1SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP4P9HGSP1SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP4P9HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP4P9HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC, "Player 1 updated");
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player Two sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERTWO_WAV),
                                                        750);
                                    }
                                    break;
                                case 1:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 2");
                                    lv_label_set_text_fmt(ui_SP4P9HGSP2SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP4P9HGSP2SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP4P9HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP4P9HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC, "Player 2 updated");
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player Three sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERTHREE_WAV),
                                                        SOUND_DELAY_TURN_SWITCH_MS);
                                    }
                                    break;
                                case 2:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 3");
                                    lv_label_set_text_fmt(ui_SP4P9HGSP3SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP4P9HGSP3SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP4P9HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP4P9HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC, "Player 3 updated");
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player Four sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERFOUR_WAV),
                                                        SOUND_DELAY_TURN_SWITCH_MS);
                                    }
                                    break;
                                case 3:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 4");
                                    lv_label_set_text_fmt(ui_SP4P9HGSP4SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP4P9HGSP4SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP4P9HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP4P9HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC, "Player 4 updated");
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player One sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERONE_WAV),
                                                        750);
                                    }
                                    break;
                            }
                            break;

                        default:
                            DEBUG_ERROR(MODULE_LOGIC, "Invalid number of players: %d", num_players);
                            break;
                    }
                    break;
                case EIGHTEEN_HOLES:
                    DEBUG_TRACE(MODULE_LOGIC, "Eighteen Holes mode");
                    switch (num_players)
                    {
                        case 1:
                            DEBUG_TRACE(MODULE_LOGIC, "Single Player 18 Holes");
                            DEBUG_INFO(MODULE_LOGIC,
                                       "Updating UI for Single Player Mode - Player %d",
                                       player_index + 1);
                            lv_label_set_text_fmt(ui_SP1P18HGSPSPText, "Player %d",
                                                  player_index + 1);
                            lv_label_set_text_fmt(ui_SP1P18HGSPSPText, "%d", score);
                            lv_label_set_text_fmt(ui_SP1P18HGSHCPText, "%d", current_hole + 1);
                            lv_label_set_text_fmt(ui_SP1P18HGSBCPText, "%d", detection_count);
                            DEBUG_INFO(MODULE_LOGIC,
                                       "Single Player 18 Holes UI updated - Player:%d, Score:%d, "
                                       "Hole:%d, Balls:%d",
                                       player_index + 1, score, current_hole + 1, detection_count);
                            break;
                        case 2:
                            DEBUG_TRACE(MODULE_LOGIC, "Two Players 18 Holes");
                            DEBUG_INFO(MODULE_LOGIC, "Two Player Mode 18 Holes");
                            switch (player_index)
                            {
                                case 0:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 1 18 Holes");
                                    lv_label_set_text_fmt(ui_SP2P18HGSP1SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP2P18HGSP1SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP2P18HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP2P18HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(
                                        MODULE_LOGIC,
                                        "Player 1 18 Holes updated - Score:%d, Hole:%d, Balls:%d",
                                        score, current_hole + 1, detection_count);
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player Two sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERTWO_WAV),
                                                        750);
                                    }
                                    break;
                                case 1:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 2 18 Holes");
                                    lv_label_set_text_fmt(ui_SP2P18HGSP2SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP2P18HGSP2SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP2P18HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP2P18HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(
                                        MODULE_LOGIC,
                                        "Player 2 18 Holes updated - Score:%d, Hole:%d, Balls:%d",
                                        score, current_hole + 1, detection_count);
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player One sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERONE_WAV),
                                                        750);
                                    }
                                    break;
                            }
                            break;

                        case 3:
                            DEBUG_TRACE(MODULE_LOGIC, "Three Players 18 Holes");
                            DEBUG_INFO(MODULE_LOGIC, "Three Player Mode 18 Holes");
                            switch (player_index)
                            {
                                case 0:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 1 18 Holes");
                                    lv_label_set_text_fmt(ui_SP3P18HGSP1SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP3P18HGSP1SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP3P18HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP3P18HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC, "Player 1 18 Holes updated");
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player Two sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERTWO_WAV),
                                                        750);
                                    }
                                    break;
                                case 1:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 2 18 Holes");
                                    lv_label_set_text_fmt(ui_SP3P18HGSP2SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP3P18HGSP2SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP3P18HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP3P18HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC, "Player 2 18 Holes updated");
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player Three sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERTHREE_WAV),
                                                        SOUND_DELAY_TURN_SWITCH_MS);
                                    }
                                    break;
                                case 2:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 3 18 Holes");
                                    lv_label_set_text_fmt(ui_SP3P18HGSP3SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP3P18HGSP3SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP3P18HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP3P18HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC, "Player 3 18 Holes updated");
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player One sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERONE_WAV),
                                                        750);
                                    }
                                    break;
                            }
                            break;

                        case 4:
                            DEBUG_TRACE(MODULE_LOGIC, "Four Players 18 Holes");
                            DEBUG_INFO(MODULE_LOGIC, "Four Player Mode 18 Holes");
                            switch (player_index)
                            {
                                case 0:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 1 18 Holes");
                                    lv_label_set_text_fmt(ui_SP4P18HGSP1SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP4P18HGSP1SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP4P18HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP4P18HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC, "Player 1 18 Holes updated");
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player Two sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERTWO_WAV),
                                                        750);
                                    }
                                    break;
                                case 1:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 2 18 Holes");
                                    lv_label_set_text_fmt(ui_SP4P18HGSP2SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP4P18HGSP2SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP4P18HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP4P18HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC, "Player 2 18 Holes updated");
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player Three sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERTHREE_WAV),
                                                        SOUND_DELAY_TURN_SWITCH_MS);
                                    }
                                    break;
                                case 2:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 3 18 Holes");
                                    lv_label_set_text_fmt(ui_SP4P18HGSP3SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP4P18HGSP3SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP4P18HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP4P18HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC, "Player 3 18 Holes updated");
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player Four sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERFOUR_WAV),
                                                        SOUND_DELAY_TURN_SWITCH_MS);
                                    }
                                    break;
                                case 3:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 4 18 Holes");
                                    lv_label_set_text_fmt(ui_SP4P18HGSP4SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP4P18HGSP4SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP4P18HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP4P18HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC, "Player 4 18 Holes updated");
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player One sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERONE_WAV),
                                                        750);
                                    }
                                    break;
                            }
                            break;

                        default:
                            DEBUG_ERROR(MODULE_LOGIC, "Invalid number of players: %d", num_players);
                            break;
                    }
                    break;
            }
    }
}