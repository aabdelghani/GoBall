#ifndef UI_LOGIC_EVENT_H
#define UI_LOGIC_EVENT_H

#include <stdint.h>
#include <time.h>

#include "../../ui/ui.h"
#include "../game_modes/game_modes.h"
#include "../game_modes/quotaplay.h"
#include "../game_state/game_state.h"
#include "../event_bus/event_bus.h"

/* UI controller initialization - subscribes to game events */
extern void ui_controller_init(void);

/* State management (called from generated UI event handlers) */
extern void        set_num_players(uint8_t new_num_players);
extern void        reset_scores(void);
extern void        set_game_mode(GameMode mode);
extern const char* get_game_mode_name(void);
extern void        print_current_game_mode(void);
extern void update_scoreCard(uint8_t player_index, uint8_t cumulative_score, uint8_t current_hole);
extern void set_sensors_enabled(uint8_t enable);
extern void print_sensors_status(void);

/* Hole and match play mode setters */
extern void set_hole_mode(HoleMode mode);
extern void set_match_play_mode(MatchPlayMode mode);
extern void print_current_hole_mode(void);

/* Player highlighting */
extern void two_player_highlight_pattern(GameMode game_mode, HoleMode hole_mode);
extern void three_player_highlight_pattern(GameMode game_mode, HoleMode hole_mode);
extern void four_player_highlight_pattern(GameMode game_mode, HoleMode hole_mode);
extern void update_player_highlight(uint8_t detection_count, uint8_t current_hole, uint8_t nplayers);

/* Game completion and scoring UI */
extern void check_all_players_completed(GameMode gameMode);
extern void print_final_scores_and_winner(void);
extern void logic_update_label_text(int player_index, int current_hole, int score,
                                    int detection_count, int nplayers);

/*
 * Game state access macros for generated UI code.
 * These MUST come after game_state.h is included (struct definitions parsed first).
 * ui.c references 'players' and 'COLOR_1' directly - these macros provide access
 * to the centralized game state without modifying generated files.
 */
#define players (game_state_get()->players)
#define COLOR_1 COLOR_HIGHLIGHT_1
#define COLOR_2 COLOR_HIGHLIGHT_2

/* UI macros for generated UI event handlers */
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
