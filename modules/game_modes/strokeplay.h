#ifndef STROKEPLAY_H
#define STROKEPLAY_H
#include "game_modes.h"
#include "player.h"
#include "../led_logic/led_logic_event.h"

void stroke_play_process_pin(Player *player, int player_index, unsigned int gpio_pin, led_strip_controller_t *leds);
// Your code goes here

#endif // STROKEPLAY_H