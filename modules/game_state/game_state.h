#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <stdint.h>
#include "game_modes.h"
#include "player.h"

#define MAX_PLAYERS 4
#define NUM_SENSORS 4
#define SENSORS_PER_TURN 2
#define MAX_HOLES 18

#define PIN_THREE_POINTS_HOLE 17
#define PIN_FOUR_POINTS_HOLE  26
#define PIN_FIVE_POINTS_HOLE  27
#define PIN_ZERO_POINTS_HOLE  24

#define SCORE_THREE_POINTS 3
#define SCORE_FOUR_POINTS  4
#define SCORE_FIVE_POINTS  5
#define SCORE_ZERO_POINTS  0

#define COLOR_HIGHLIGHT_1 0x34bb19
#define COLOR_HIGHLIGHT_2 0x5AB66A

typedef struct {
    Player players[MAX_PLAYERS];
    int current_player_index;
    int num_players;

    GameMode game_mode;
    HoleMode hole_mode;
    MatchPlayMode match_play_mode;

    uint8_t all_players_completed;
    uint8_t sensors_enabled;
    uint8_t update_flag;

    uint8_t prev_scores[MAX_PLAYERS][MAX_HOLES];
    uint8_t individual_scores[MAX_PLAYERS][MAX_HOLES + 1];
    uint8_t final_scores[MAX_PLAYERS];
    uint8_t highlighting_counter;

    unsigned int sensor_pins[NUM_SENSORS];
} game_state_t;

/* Get pointer to the singleton game state */
game_state_t *game_state_get(void);

/* Reset all game state to defaults */
void game_state_reset(void);

/* Setters (publish events via event_bus) */
void game_state_set_game_mode(GameMode mode);
void game_state_set_hole_mode(HoleMode mode);
void game_state_set_match_play_mode(MatchPlayMode mode);
void game_state_set_num_players(int count);
void game_state_set_sensors_enabled(uint8_t enabled);

/* Player management */
void game_state_advance_player(void);
void game_state_reset_player(int index);

/* Score helpers */
int  game_state_pin_to_score(unsigned int pin);
int  game_state_pin_to_sensor_index(unsigned int pin);
const char *game_state_get_mode_name(void);

#endif // GAME_STATE_H
