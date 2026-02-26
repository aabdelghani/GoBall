/*
 * gpio_event.h - Compatibility shim
 *
 * All functionality has been moved to the new modular architecture:
 *   - modules/game_state/      (game state management)
 *   - modules/gpio_driver/     (GPIO hardware)
 *   - modules/game_controller/ (game flow logic)
 *   - modules/ui_logic/        (UI updates)
 *   - modules/event_bus/       (publish/subscribe)
 */
#ifndef GPIO_EVENT_H
#define GPIO_EVENT_H

#include "../game_state/game_state.h"
#include "../gpio_driver/gpio_driver.h"
#include "../game_controller/game_controller.h"
#include "../event_bus/event_bus.h"
#include "../debug/debug.h"
#include "../game_modes/game_modes.h"
#include "../game_modes/player.h"

/* Legacy defines preserved for backward compatibility */
#define NUM_PLAYERS 1

#endif  /* GPIO_EVENT_H */
