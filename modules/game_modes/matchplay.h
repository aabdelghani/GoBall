#ifndef MATCHPLAY_H
#define MATCHPLAY_H

#include "game_modes.h"
#include "player.h"
#include "../led_logic/led_logic_event.h"

/**
 * Process a GPIO pin event for Match Play scoring.
 * Adds points to the player's round_total_score (not cumulative score).
 */
void match_play_process_pin(Player *player, int player_index,
                            unsigned int gpio_pin, led_strip_controller_t *leds);

#endif // MATCHPLAY_H
