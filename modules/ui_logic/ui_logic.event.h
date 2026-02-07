#ifndef UI_LOGIC_EVENT_H
#define UI_LOGIC_EVENT_H

#include <stdint.h>
#include <time.h>

#include "../../ui/ui.h"
#include "../game_modes/game_modes.h"
#include "../logic/gpio_event.h"

#define MAX_PLAYERS 4

// Declare shared variables
extern uint8_t update_flag;
extern uint8_t prev_scores[MAX_PLAYERS]
                          [9];  // Previous cumulative scores for each hole for each player
extern uint8_t individual_scores[MAX_PLAYERS]
                                [10];      // Individual scores for each hole for each player
extern uint8_t final_scores[MAX_PLAYERS];  // Final cumulative scores for each player
extern uint8_t highliting_counter;         // Counter for highlighting players
extern uint8_t sensors_enabled;            // Flag to enable/disable sensors
// Declare new functions
extern void        set_num_players(uint8_t new_num_players);
extern void        reset_scores(void);
extern void        set_game_mode(GameMode mode);
extern const char* get_game_mode_name(void);
extern void        print_current_game_mode(void);  // New function declaration
extern void update_scoreCard(uint8_t player_index, uint8_t cumulative_score, uint8_t current_hole);
extern void set_sensors_enabled(uint8_t enable);
extern void print_sensors_status(void);
// Define the macro to extend functionality only inside ui.c
#ifdef _GO_BALL_RASPBERY_PI_UI_H
#define RESET_SCORES reset_scores();

#define SET_ONE_PLAYER set_num_players(1);

#define SET_TWO_PLAYER set_num_players(2);

#define SET_THREE_PLAYER set_num_players(3);

#define SET_FOUR_PLAYER set_num_players(4);

#define SET_GAME_MODE_STROKE_PLAY set_game_mode(GAME_MODE_STROKE_PLAY);

#define SET_GAME_MODE_MATCH_PLAY set_game_mode(GAME_MODE_MATCH_PLAY);

#define SET_GAME_MODE_QUOTA set_game_mode(GAME_MODE_QUOTA);

#define SET_GAME_MODE_VEGAS set_game_mode(GAME_MODE_VEGAS);

#define PRINT_CURRENT_GAME_MODE print_current_game_mode();

#define SET_HOLES_9 set_hole_mode(HOLES_9);

#define SET_HOLES_18 set_hole_mode(HOLES_18);

#define SET_MAtCH_PLAY_MODE_1V1 set_match_play_mode(MATCH_PLAY_MODE_1V1);

#define SET_MATCH_PLAY_MODE_2V2 set_match_play_mode(MATCH_PLAY_MODE_2V2);

#define PRINT_CURRENT_HOLE_MODE print_current_hole_mode();

#define ENABLE_SENSORS set_sensors_enabled(1);

#define DISABLE_SENSORS set_sensors_enabled(0);

#define PRINT_SENSORS_STATUS print_sensors_status();
#else
#endif

#endif  // UI_LOGIC_EVENT_H
