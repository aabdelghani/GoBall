#include "quotaplay.h"

void quota_play_process_pin(Player* player, int player_index, unsigned int gpio_pin, led_strip_controller_t* leds)
{
    DEBUG_TRACE(MODULE_GAME, "quota_play_process_pin called");
    DEBUG_DEBUG(MODULE_GAME, "Processing pin event - Player %d, GPIO:%u, LEDs:%p", player_index + 1,
                gpio_pin, (void*)leds);

    if (player == NULL)
    {
        DEBUG_ERROR(MODULE_GAME, "Invalid player pointer (NULL)");
        return;
    }

    if (leds == NULL)
    {
        DEBUG_WARN(MODULE_GAME, "LED controller is NULL - visual feedback disabled");
    }

    DEBUG_INFO(MODULE_GAME, "Player %d quota remaining: (3pt:%d, 4pt:%d, 5pt:%d)",
               player_index + 1, player->par3_count, player->par4_count, player->par5_count);

    switch (gpio_pin)
    {
        case PIN_THREE_POINTS_HOLE:
            DEBUG_INFO(MODULE_GAME, "3-POINT HOLE");
            player->score += SCORE_THREE_POINTS;
            if (player->par3_count > 0) player->par3_count--;
            if (leds) trigger_flash_with_color(leds, 1000, COLOR_GREEN);
            PLAY_THREEPOINTS_WAV;
            break;

        case PIN_FOUR_POINTS_HOLE:
            DEBUG_INFO(MODULE_GAME, "4-POINT HOLE");
            player->score += SCORE_FOUR_POINTS;
            if (player->par4_count > 0) player->par4_count--;
            if (leds) trigger_flash_with_color(leds, 1000, COLOR_GREEN);
            PLAY_FOURPOINTS_WAV;
            break;

        case PIN_FIVE_POINTS_HOLE:
            DEBUG_INFO(MODULE_GAME, "5-POINT HOLE");
            player->score += SCORE_FIVE_POINTS;
            if (player->par5_count > 0) player->par5_count--;
            if (leds) trigger_flash_with_color(leds, 1000, COLOR_GREEN);
            PLAY_FIVEPOINTS_WAV;
            break;

        case PIN_ZERO_POINTS_HOLE:
            DEBUG_INFO(MODULE_GAME, "ZERO-POINT HOLE");
            player->score += SCORE_ZERO_POINTS;
            if (leds) trigger_flash_with_color(leds, 1000, COLOR_WHITE);
            PLAY_ZEROPOINTS_WAV;
            break;

        default:
            DEBUG_WARN(MODULE_GAME, "Unknown GPIO pin triggered: %u", gpio_pin);
            break;
    }

    DEBUG_INFO(MODULE_GAME, "Player %d quota remaining: (3pt:%d, 4pt:%d, 5pt:%d)",
               player_index + 1, player->par3_count, player->par4_count, player->par5_count);
    DEBUG_TRACE(MODULE_GAME, "quota_play_process_pin completed");
}

void vegas_quota_play_process_pin(Player* player, int player_index, unsigned int gpio_pin,
                                  led_strip_controller_t* leds, Player* all_players,
                                  uint8_t num_players)
{
    DEBUG_TRACE(MODULE_GAME, "vegas_quota_play_process_pin called");

    if (player == NULL)
    {
        DEBUG_ERROR(MODULE_GAME, "Invalid player pointer (NULL)");
        return;
    }

    DEBUG_INFO(MODULE_GAME, "Vegas Player %d quota remaining: (3pt:%d, 4pt:%d, 5pt:%d) score:%d",
               player_index + 1, player->par3_count, player->par4_count, player->par5_count,
               player->score);

    // Check if any opponent still has count > 0 in a given category
    // For 1P, this is always false (no opponents)
    int opponent_has_par3 = 0, opponent_has_par4 = 0, opponent_has_par5 = 0;
    for (uint8_t i = 0; i < num_players; i++)
    {
        if (i == player_index) continue;
        if (all_players[i].par3_count > 0) opponent_has_par3 = 1;
        if (all_players[i].par4_count > 0) opponent_has_par4 = 1;
        if (all_players[i].par5_count > 0) opponent_has_par5 = 1;
    }

    switch (gpio_pin)
    {
        case PIN_THREE_POINTS_HOLE:
            DEBUG_INFO(MODULE_GAME, "3-POINT HOLE");
            player->score += SCORE_THREE_POINTS;
            if (player->par3_count > 0)
            {
                player->par3_count--;
            }
            else if (opponent_has_par3)
            {
                // Bonus: my par3 is 0 but opponent's isn't
                DEBUG_INFO(MODULE_GAME, "Vegas bonus! Player %d earns +3 (par3 completed, opponent has par3>0)",
                           player_index + 1);
            }
            if (leds) trigger_flash_with_color(leds, 1000, COLOR_GREEN);
            PLAY_THREEPOINTS_WAV;
            break;

        case PIN_FOUR_POINTS_HOLE:
            DEBUG_INFO(MODULE_GAME, "4-POINT HOLE");
            player->score += SCORE_FOUR_POINTS;
            if (player->par4_count > 0)
            {
                player->par4_count--;
            }
            else if (opponent_has_par4)
            {
                DEBUG_INFO(MODULE_GAME, "Vegas bonus! Player %d earns +4 (par4 completed, opponent has par4>0)",
                           player_index + 1);
            }
            if (leds) trigger_flash_with_color(leds, 1000, COLOR_GREEN);
            PLAY_FOURPOINTS_WAV;
            break;

        case PIN_FIVE_POINTS_HOLE:
            DEBUG_INFO(MODULE_GAME, "5-POINT HOLE");
            player->score += SCORE_FIVE_POINTS;
            if (player->par5_count > 0)
            {
                player->par5_count--;
            }
            else if (opponent_has_par5)
            {
                DEBUG_INFO(MODULE_GAME, "Vegas bonus! Player %d earns +5 (par5 completed, opponent has par5>0)",
                           player_index + 1);
            }
            if (leds) trigger_flash_with_color(leds, 1000, COLOR_GREEN);
            PLAY_FIVEPOINTS_WAV;
            break;

        case PIN_ZERO_POINTS_HOLE:
            DEBUG_INFO(MODULE_GAME, "ZERO-POINT HOLE");
            player->score += SCORE_ZERO_POINTS;
            if (leds) trigger_flash_with_color(leds, 1000, COLOR_WHITE);
            PLAY_ZEROPOINTS_WAV;
            break;

        default:
            DEBUG_WARN(MODULE_GAME, "Unknown GPIO pin triggered: %u", gpio_pin);
            break;
    }

    DEBUG_INFO(MODULE_GAME, "Vegas Player %d quota remaining: (3pt:%d, 4pt:%d, 5pt:%d) score:%d",
               player_index + 1, player->par3_count, player->par4_count, player->par5_count,
               player->score);
}

int quota_player_completed(Player* player)
{
    return (player->par3_count == 0 && player->par4_count == 0 && player->par5_count == 0);
}
