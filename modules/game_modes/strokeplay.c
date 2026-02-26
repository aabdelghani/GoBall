#include "strokeplay.h"
#include "../debug/debug.h"
#include "../sound_logic/sound_logic_event.h"

void stroke_play_process_pin(Player* player, int player_index, unsigned int gpio_pin, led_strip_controller_t* leds)
{
    DEBUG_TRACE(MODULE_GAME, "stroke_play_process_pin called");
    DEBUG_DEBUG(MODULE_GAME, "Processing pin event - Player %d, GPIO:%u, LEDs:%p", player_index + 1,
                gpio_pin, (void*)leds);

    // Validate inputs
    if (player == NULL)
    {
        DEBUG_ERROR(MODULE_GAME, "Invalid player pointer (NULL)");
        return;
    }

    if (leds == NULL)
    {
        DEBUG_WARN(MODULE_GAME, "LED controller is NULL - visual feedback disabled");
    }

    DEBUG_INFO(MODULE_GAME, "Player %d current score: %d", player_index + 1, player->score);

    switch (gpio_pin)
    {
        case PIN_THREE_POINTS_HOLE:
            DEBUG_INFO(MODULE_GAME, "🏌️  3-POINT HOLE - Adding %d points", SCORE_THREE_POINTS);
            player->score += SCORE_THREE_POINTS;
            if (leds) trigger_flash_with_color(leds, 1000, COLOR_GREEN);
            PLAY_THREEPOINTS_WAV;
            break;

        case PIN_FOUR_POINTS_HOLE:
            DEBUG_INFO(MODULE_GAME, "🏌️  4-POINT HOLE - Adding %d points", SCORE_FOUR_POINTS);
            player->score += SCORE_FOUR_POINTS;
            if (leds) trigger_flash_with_color(leds, 1000, COLOR_GREEN);
            PLAY_FOURPOINTS_WAV;
            break;

        case PIN_FIVE_POINTS_HOLE:
            DEBUG_INFO(MODULE_GAME, "🏌️  5-POINT HOLE - Adding %d points", SCORE_FIVE_POINTS);
            player->score += SCORE_FIVE_POINTS;
            if (leds) trigger_flash_with_color(leds, 1000, COLOR_GREEN);
            PLAY_FIVEPOINTS_WAV;
            break;

        case PIN_ZERO_POINTS_HOLE:
            DEBUG_INFO(MODULE_GAME, "❌ ZERO-POINT HOLE - Adding %d points", SCORE_ZERO_POINTS);
            player->score += SCORE_ZERO_POINTS;
            if (leds) trigger_flash_with_color(leds, 1000, COLOR_WHITE);
            PLAY_ZEROPOINTS_WAV;
            break;

        default:
            DEBUG_WARN(MODULE_GAME, "Unknown GPIO pin triggered: %u", gpio_pin);
            DEBUG_DEBUG(MODULE_GAME, "Known pins: 3pt=%u, 4pt=%u, 5pt=%u, 0pt=%u",
                        PIN_THREE_POINTS_HOLE, PIN_FOUR_POINTS_HOLE, PIN_FIVE_POINTS_HOLE,
                        PIN_ZERO_POINTS_HOLE);
            break;
    }

    DEBUG_INFO(MODULE_GAME, "Player %d new score: %d", player_index + 1, player->score);
    DEBUG_TRACE(MODULE_GAME, "stroke_play_process_pin completed");
}