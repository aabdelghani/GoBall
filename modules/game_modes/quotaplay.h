#ifndef QUOTAPLAY_H
#define QUOTAPLAY_H
#include "game_modes.h"
#include "../logic/gpio_event.h"
#include "../led_logic/led_logic_event.h"
#include "../sound_logic/sound_logic_event.h"
#include "player.h"

// Quota presets for 9 holes
#define QUOTA_9H_PAR3  2
#define QUOTA_9H_PAR4  5
#define QUOTA_9H_PAR5  2

// Quota presets for 18 holes
#define QUOTA_18H_PAR3  4
#define QUOTA_18H_PAR4  10
#define QUOTA_18H_PAR5  4

void quota_play_process_pin(Player *player, int player_index, unsigned int gpio_pin, led_strip_controller_t *leds);
void vegas_quota_play_process_pin(Player *player, int player_index, unsigned int gpio_pin, led_strip_controller_t *leds, Player *all_players, uint8_t num_players);
int  quota_player_completed(Player *player);

#endif // QUOTAPLAY_H
