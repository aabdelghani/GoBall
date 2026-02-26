#include "event_bus.h"
#include "debug.h"
#include <string.h>

static event_callback_t subscribers[EVENT_COUNT][EVENT_BUS_MAX_SUBSCRIBERS];
static int subscriber_count[EVENT_COUNT];

static const char *event_names[] = {
    [EVENT_SCORE_CHANGED]      = "SCORE_CHANGED",
    [EVENT_PLAYER_SWITCHED]    = "PLAYER_SWITCHED",
    [EVENT_HOLE_COMPLETED]     = "HOLE_COMPLETED",
    [EVENT_GAME_COMPLETED]     = "GAME_COMPLETED",
    [EVENT_GAME_MODE_SET]      = "GAME_MODE_SET",
    [EVENT_HOLE_MODE_SET]      = "HOLE_MODE_SET",
    [EVENT_MATCH_PLAY_MODE_SET]= "MATCH_PLAY_MODE_SET",
    [EVENT_NUM_PLAYERS_SET]    = "NUM_PLAYERS_SET",
    [EVENT_GAME_RESET]         = "GAME_RESET",
    [EVENT_SENSORS_TOGGLED]    = "SENSORS_TOGGLED",
    [EVENT_QUOTA_COMPLETED]    = "QUOTA_COMPLETED",
};

void event_bus_init(void)
{
    memset(subscribers, 0, sizeof(subscribers));
    memset(subscriber_count, 0, sizeof(subscriber_count));
    DEBUG_INFO(MODULE_MAIN, "Event bus initialized (%d event types)", EVENT_COUNT);
}

void event_bus_subscribe(event_type_t type, event_callback_t callback)
{
    if (type >= EVENT_COUNT || !callback) {
        DEBUG_ERROR(MODULE_MAIN, "Invalid event subscription: type=%d", type);
        return;
    }

    if (subscriber_count[type] >= EVENT_BUS_MAX_SUBSCRIBERS) {
        DEBUG_ERROR(MODULE_MAIN, "Max subscribers reached for event %s", event_names[type]);
        return;
    }

    subscribers[type][subscriber_count[type]++] = callback;
    DEBUG_TRACE(MODULE_MAIN, "Subscribed to %s (total: %d)", event_names[type], subscriber_count[type]);
}

void event_bus_publish(const game_event_t *event)
{
    if (!event || event->type >= EVENT_COUNT) {
        DEBUG_ERROR(MODULE_MAIN, "Invalid event publish: type=%d", event ? event->type : -1);
        return;
    }

    DEBUG_TRACE(MODULE_MAIN, "Publishing %s to %d subscribers",
                event_names[event->type], subscriber_count[event->type]);

    for (int i = 0; i < subscriber_count[event->type]; i++) {
        subscribers[event->type][i](event);
    }
}
