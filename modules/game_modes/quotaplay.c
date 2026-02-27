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

    // Count how many players have closed each category (quota == 0)
    // A category is "dead" (no bonus scoring) once 2+ players have closed it
    int closed_par3 = 0, closed_par4 = 0, closed_par5 = 0;
    for (uint8_t i = 0; i < num_players; i++)
    {
        if (all_players[i].par3_count == 0) closed_par3++;
        if (all_players[i].par4_count == 0) closed_par4++;
        if (all_players[i].par5_count == 0) closed_par5++;
    }
    // For 1P: category is never dead (only 1 player can close it)
    // For 2P+: dead once 2 players close it
    int par3_alive = (num_players == 1) || (closed_par3 < 2);
    int par4_alive = (num_players == 1) || (closed_par4 < 2);
    int par5_alive = (num_players == 1) || (closed_par5 < 2);

    switch (gpio_pin)
    {
        case PIN_THREE_POINTS_HOLE:
            DEBUG_INFO(MODULE_GAME, "3-POINT HOLE");
            if (player->par3_count > 0)
            {
                player->par3_count--;
                DEBUG_INFO(MODULE_GAME, "Vegas countdown: par3 now %d", player->par3_count);
            }
            else
            {
                // Category already at 0 — earn bonus points
                // For multiplayer: only if opponent still has this category > 0
                if (par3_alive)
                {
                    player->score += SCORE_THREE_POINTS;
                    DEBUG_INFO(MODULE_GAME, "Vegas bonus! Player %d earns +3, score now %d",
                               player_index + 1, player->score);
                }
                else
                {
                    DEBUG_INFO(MODULE_GAME, "Vegas 3pt category dead (%d players closed) - no bonus",
                               closed_par3);
                }
            }
            if (leds) trigger_flash_with_color(leds, 1000, COLOR_GREEN);
            PLAY_THREEPOINTS_WAV;
            break;

        case PIN_FOUR_POINTS_HOLE:
            DEBUG_INFO(MODULE_GAME, "4-POINT HOLE");
            if (player->par4_count > 0)
            {
                player->par4_count--;
                DEBUG_INFO(MODULE_GAME, "Vegas countdown: par4 now %d", player->par4_count);
            }
            else
            {
                if (par4_alive)
                {
                    player->score += SCORE_FOUR_POINTS;
                    DEBUG_INFO(MODULE_GAME, "Vegas bonus! Player %d earns +4, score now %d",
                               player_index + 1, player->score);
                }
                else
                {
                    DEBUG_INFO(MODULE_GAME, "Vegas 4pt category dead (%d players closed) - no bonus",
                               closed_par4);
                }
            }
            if (leds) trigger_flash_with_color(leds, 1000, COLOR_GREEN);
            PLAY_FOURPOINTS_WAV;
            break;

        case PIN_FIVE_POINTS_HOLE:
            DEBUG_INFO(MODULE_GAME, "5-POINT HOLE");
            if (player->par5_count > 0)
            {
                player->par5_count--;
                DEBUG_INFO(MODULE_GAME, "Vegas countdown: par5 now %d", player->par5_count);
            }
            else
            {
                if (par5_alive)
                {
                    player->score += SCORE_FIVE_POINTS;
                    DEBUG_INFO(MODULE_GAME, "Vegas bonus! Player %d earns +5, score now %d",
                               player_index + 1, player->score);
                }
                else
                {
                    DEBUG_INFO(MODULE_GAME, "Vegas 5pt category dead (%d players closed) - no bonus",
                               closed_par5);
                }
            }
            if (leds) trigger_flash_with_color(leds, 1000, COLOR_GREEN);
            PLAY_FIVEPOINTS_WAV;
            break;

        case PIN_ZERO_POINTS_HOLE:
            DEBUG_INFO(MODULE_GAME, "ZERO-POINT HOLE");
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
