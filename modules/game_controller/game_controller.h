#ifndef GAME_CONTROLLER_H
#define GAME_CONTROLLER_H

#include "event_bus.h"

/**
 * Initialize the game controller.
 * Subscribes to EVENT_SCORE_CHANGED and orchestrates game flow.
 */
void game_controller_init(void);

#endif // GAME_CONTROLLER_H
