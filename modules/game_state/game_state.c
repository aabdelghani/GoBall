#include "game_state.h"
#include "event_bus.h"
#include "debug.h"
#include <string.h>

static game_state_t state;

game_state_t *game_state_get(void)
{
    return &state;
}

void game_state_reset(void)
{
    memset(&state.players, 0, sizeof(state.players));
    state.current_player_index = 0;
    state.all_players_completed = 1;
    state.update_flag = 0;
    state.highlighting_counter = 0;

    memset(state.prev_scores, 0, sizeof(state.prev_scores));
    memset(state.individual_scores, 0, sizeof(state.individual_scores));
    memset(state.final_scores, 0, sizeof(state.final_scores));

    state.sensor_pins[0] = PIN_THREE_POINTS_HOLE;
    state.sensor_pins[1] = PIN_FOUR_POINTS_HOLE;
    state.sensor_pins[2] = PIN_FIVE_POINTS_HOLE;
    state.sensor_pins[3] = PIN_ZERO_POINTS_HOLE;

    DEBUG_INFO(MODULE_GAME, "Game state reset");

    game_event_t event = { .type = EVENT_GAME_RESET };
    event_bus_publish(&event);
}

void game_state_set_game_mode(GameMode mode)
{
    state.game_mode = mode;
    DEBUG_INFO(MODULE_GAME, "Game mode set to: %d (%s)", mode, game_state_get_mode_name());

    game_event_t event = { .type = EVENT_GAME_MODE_SET };
    event.data.game_mode.mode = mode;
    event_bus_publish(&event);
}

void game_state_set_hole_mode(HoleMode mode)
{
    state.hole_mode = mode;
    DEBUG_INFO(MODULE_GAME, "Hole mode set to: %s", mode == HOLES_9 ? "9 holes" : "18 holes");

    game_event_t event = { .type = EVENT_HOLE_MODE_SET };
    event.data.hole_mode.mode = mode;
    event_bus_publish(&event);
}

void game_state_set_match_play_mode(MatchPlayMode mode)
{
    state.match_play_mode = mode;
    DEBUG_INFO(MODULE_GAME, "Match play mode set to: %s", mode == MATCH_PLAY_MODE_1V1 ? "1v1" : "2v2");

    game_event_t event = { .type = EVENT_MATCH_PLAY_MODE_SET };
    event.data.match_play_mode.mode = mode;
    event_bus_publish(&event);
}

void game_state_set_num_players(int count)
{
    if (count < 1 || count > MAX_PLAYERS) {
        DEBUG_ERROR(MODULE_GAME, "Invalid player count: %d", count);
        return;
    }

    state.num_players = count;
    DEBUG_INFO(MODULE_GAME, "Number of players set to: %d", count);

    game_event_t event = { .type = EVENT_NUM_PLAYERS_SET };
    event.data.num_players.count = count;
    event_bus_publish(&event);
}

void game_state_set_sensors_enabled(uint8_t enabled)
{
    state.sensors_enabled = enabled;
    DEBUG_INFO(MODULE_GAME, "Sensors %s", enabled ? "ENABLED" : "DISABLED");

    game_event_t event = { .type = EVENT_SENSORS_TOGGLED };
    event.data.sensors.enabled = enabled;
    event_bus_publish(&event);
}

void game_state_advance_player(void)
{
    state.current_player_index = (state.current_player_index + 1) % state.num_players;
    DEBUG_INFO(MODULE_GAME, "Switched to next player: Player %d", state.current_player_index + 1);

    game_event_t event = { .type = EVENT_PLAYER_SWITCHED };
    event.data.player.player_index = state.current_player_index;
    event.data.player.num_players = state.num_players;
    event_bus_publish(&event);
}

void game_state_reset_player(int index)
{
    if (index < 0 || index >= MAX_PLAYERS) return;

    state.players[index].score = 0;
    state.players[index].current_hole = 0;
    state.players[index].detection_count = 0;
    state.players[index].holes_won = 0;
    state.players[index].holes_halved = 0;
    state.players[index].upAndDown = 0;
    state.players[index].round_total_score = 0;
    state.players[index].par3_count = 0;
    state.players[index].par4_count = 0;
    state.players[index].par5_count = 0;
    memset(state.players[index].match_status, 0, sizeof(state.players[index].match_status));
}

int game_state_pin_to_score(unsigned int pin)
{
    switch (pin) {
        case PIN_THREE_POINTS_HOLE: return SCORE_THREE_POINTS;
        case PIN_FOUR_POINTS_HOLE:  return SCORE_FOUR_POINTS;
        case PIN_FIVE_POINTS_HOLE:  return SCORE_FIVE_POINTS;
        case PIN_ZERO_POINTS_HOLE:  return SCORE_ZERO_POINTS;
        default:
            DEBUG_WARN(MODULE_GAME, "Unknown pin: %u", pin);
            return -1;
    }
}

int game_state_pin_to_sensor_index(unsigned int pin)
{
    for (int i = 0; i < NUM_SENSORS; i++) {
        if (state.sensor_pins[i] == pin) return i;
    }
    return -1;
}

const char *game_state_get_mode_name(void)
{
    switch (state.game_mode) {
        case GAME_MODE_STROKE_PLAY: return "Stroke Play";
        case GAME_MODE_MATCH_PLAY:  return "Match Play";
        case GAME_MODE_QUOTA:       return "Quota";
        case GAME_MODE_VEGAS:       return "Vegas Quota";
        default:                    return "Unknown";
    }
}
