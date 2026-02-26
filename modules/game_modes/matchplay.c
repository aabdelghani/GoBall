#include "matchplay.h"
#include "../debug/debug.h"
#include "../sound_logic/sound_logic_event.h"

void match_play_process_pin(Player *player, int player_index,
                            unsigned int gpio_pin, led_strip_controller_t *leds)
{
    DEBUG_TRACE(MODULE_GAME, "match_play_process_pin called");
    DEBUG_DEBUG(MODULE_GAME, "Processing pin event - Player %d, GPIO:%u",
                player_index + 1, gpio_pin);

    if (!player) {
        DEBUG_ERROR(MODULE_GAME, "Invalid player pointer (NULL)");
        return;
    }

    DEBUG_INFO(MODULE_GAME, "Player %d round score before: %d",
               player_index + 1, player->round_total_score);

    switch (gpio_pin) {
        case PIN_THREE_POINTS_HOLE:
            player->round_total_score += SCORE_THREE_POINTS;
            if (leds) trigger_flash_with_color(leds, 1000, COLOR_GREEN);
            PLAY_THREEPOINTS_WAV;
            DEBUG_INFO(MODULE_GAME, "Match Play - 3 points scored (pin 17)");
            break;

        case PIN_FOUR_POINTS_HOLE:
            player->round_total_score += SCORE_FOUR_POINTS;
            if (leds) trigger_flash_with_color(leds, 1000, COLOR_GREEN);
            PLAY_FOURPOINTS_WAV;
            DEBUG_INFO(MODULE_GAME, "Match Play - 4 points scored (pin 26)");
            break;

        case PIN_FIVE_POINTS_HOLE:
            player->round_total_score += SCORE_FIVE_POINTS;
            if (leds) trigger_flash_with_color(leds, 1000, COLOR_GREEN);
            PLAY_FIVEPOINTS_WAV;
            DEBUG_INFO(MODULE_GAME, "Match Play - 5 points scored (pin 27)");
            break;

        case PIN_ZERO_POINTS_HOLE:
            player->round_total_score += SCORE_ZERO_POINTS;
            if (leds) trigger_flash_with_color(leds, 1000, COLOR_WHITE);
            PLAY_ZEROPOINTS_WAV;
            DEBUG_INFO(MODULE_GAME, "Match Play - 0 points scored (pin 24)");
            break;

        default:
            DEBUG_WARN(MODULE_GAME, "Unknown GPIO pin triggered: %u", gpio_pin);
            break;
    }

    DEBUG_INFO(MODULE_GAME, "Player %d round score after: %d",
               player_index + 1, player->round_total_score);
    DEBUG_TRACE(MODULE_GAME, "match_play_process_pin completed");
}
