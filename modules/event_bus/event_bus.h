#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include <stdint.h>
#include "game_modes.h"

#define EVENT_BUS_MAX_SUBSCRIBERS 8

typedef enum {
    EVENT_SCORE_CHANGED,
    EVENT_PLAYER_SWITCHED,
    EVENT_HOLE_COMPLETED,
    EVENT_GAME_COMPLETED,
    EVENT_GAME_MODE_SET,
    EVENT_HOLE_MODE_SET,
    EVENT_MATCH_PLAY_MODE_SET,
    EVENT_NUM_PLAYERS_SET,
    EVENT_GAME_RESET,
    EVENT_SENSORS_TOGGLED,
    EVENT_QUOTA_COMPLETED,
    EVENT_COUNT
} event_type_t;

typedef struct {
    event_type_t type;
    union {
        struct {
            int player_index;
            int score;
            int hole;
            int detection_count;
            unsigned int pin;
            int num_players;
        } score;

        struct {
            int player_index;
            int num_players;
        } player;

        struct {
            GameMode mode;
        } game_mode;

        struct {
            HoleMode mode;
        } hole_mode;

        struct {
            MatchPlayMode mode;
        } match_play_mode;

        struct {
            int count;
        } num_players;

        struct {
            int winner_index;
            GameMode game_mode;
            HoleMode hole_mode;
            int num_players;
        } game_complete;

        struct {
            uint8_t enabled;
        } sensors;
    } data;
} game_event_t;

typedef void (*event_callback_t)(const game_event_t *event);

void event_bus_init(void);
void event_bus_subscribe(event_type_t type, event_callback_t callback);
void event_bus_publish(const game_event_t *event);

#endif // EVENT_BUS_H
