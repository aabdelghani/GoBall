#ifndef GPIO_EVENT_H
#define GPIO_EVENT_H

/*********************
 *      INCLUDES
 *********************/
#include <gpiod.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "../../ui/ui.h"
#include "../debug/debug.h"
#include "../game_modes/game_modes.h"
#include "../game_modes/player.h"
#include "../game_modes/strokeplay.h"
#include "../game_modes/quotaplay.h"
#include "led_logic_event.h"
/*********************
 *      DEFINES
 *********************/
#define DEBOUNCE_TIME_MS 3000     // Debounce time in milliseconds
#define DEBOUNCE_SIGNAL SIGRTMIN  // Real-time signal for debounce timer
#define MAX_PLAYERS 4             // Maximum number of players
#define SENSORS_PER_TURN 2        // Number of sensors to detect per turn
#define NUM_SENSORS 4             // Total number of sensors
#define NUM_PLAYERS 1
#define COLOR_1 0x34bb19  // Color for highlighting player 1
#define COLOR_2 0x5AB66A  // Color for highlighting player 2

#define PIN_THREE_POINTS_HOLE 17
#define PIN_FOUR_POINTS_HOLE 26
#define PIN_FIVE_POINTS_HOLE 27
#define PIN_ZERO_POINTS_HOLE 24

// Corresponding Scores
#define SCORE_THREE_POINTS 3
#define SCORE_FOUR_POINTS 4
#define SCORE_FIVE_POINTS 5
#define SCORE_ZERO_POINTS 0

/*********************
 *      TYPEDEFS
 *********************/

/*********************
 *      GLOBAL VARIABLES
 *********************/
extern GameMode      current_game_mode;         // Declaration of the current mode variable
extern HoleMode      current_hole_mode;         // Tracks 9 or 18 holes
extern MatchPlayMode current_match_play_mode;   // Tracks 1v1 or 2v2 for Match Play
extern unsigned int  sensor_pins[NUM_SENSORS];  // GPIO pin numbers for sensors
extern Player        players[MAX_PLAYERS];      // Array of players
extern int           current_player_index;      // Index of the current player
extern time_t        last_activation_times[NUM_SENSORS];  // Last activation times for debouncing
extern struct gpiod_chip*     chip;                       // GPIO chip handle
extern struct gpiod_line_bulk lines;                      // GPIO lines for sensors
extern int                    num_players;                // Number of players
extern uint8_t                update_flag;                // Game completion flag

/*********************
 *      FUNCTION PROTOTYPES
 *********************/
// Initialization and cleanup functions (setup/teardown)
extern int  logic_gpio_init(void);
extern int  logic_initialize_game(int new_num_players);
extern void logic_cleanup_handler(int signal);

// Core game loop functions
extern void logic_handle_events(struct gpiod_line_bulk* event_lines, struct gpiod_line_event* event,
                                int num_players);

// Game state update functions
extern void update_scoreCard(uint8_t player_index, uint8_t cumulative_score, uint8_t current_hole);
// UI update functions
extern void logic_update_label_text(int player_index, int current_hole, int score,
                                    int detection_count, int num_players);
// Function to set hole mode and match play mode
extern void        set_hole_mode(HoleMode mode);
extern void        set_match_play_mode(MatchPlayMode mode);
extern void        print_current_hole_mode();
extern void        two_player_highlight_pattern(GameMode game_mode, HoleMode hole_mode);
extern void        three_player_highlight_pattern(GameMode game_mode, HoleMode hole_mode);
extern void        four_player_highlight_pattern(GameMode game_mode, HoleMode hole_mode);
extern void        check_all_players_completed(GameMode gameMode);
extern void        print_final_scores_and_winner(void);
extern void        set_num_players(uint8_t new_num_players);
extern void        reset_scores(void);
extern void        set_game_mode(GameMode mode);
extern const char* get_game_mode_name(void);
extern void        print_current_game_mode(void);  // New function declaration
extern void        update_player_highlight(uint8_t detection_count, uint8_t current_hole,
                                           uint8_t num_players);

#endif  // GPIO_EVENT_H
