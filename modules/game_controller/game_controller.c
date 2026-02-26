#include "game_controller.h"
#include "game_state.h"
#include "event_bus.h"
#include "debug.h"
#include "strokeplay.h"
#include "matchplay.h"
#include "quotaplay.h"
#include "led_logic_event.h"
#include "sound_logic_event.h"

/* Match play round scores for comparing after both players complete a turn */
static unsigned int match_play_round_scores[2] = {0};

/**
 * Determine the winner index for stroke play / quota modes.
 * Returns winner index, or -1 if tied.
 */
static int determine_stroke_winner(game_state_t *gs)
{
    int best_score = gs->players[0].score;
    int winner = 0;
    int tie = 0;

    for (int i = 1; i < gs->num_players; i++) {
        if (gs->players[i].score > best_score) {
            best_score = gs->players[i].score;
            winner = i;
            tie = 0;
        } else if (gs->players[i].score == best_score) {
            tie = 1;
        }
    }

    return tie ? -1 : winner;
}

/**
 * Determine the winner for match play.
 * Returns winner index, or -1 if tied (all square).
 */
static int determine_match_winner(game_state_t *gs)
{
    if (gs->players[0].upAndDown > 0) return 0;
    if (gs->players[0].upAndDown < 0) return 1;
    return -1; /* All square */
}

/**
 * Handle stroke play turn completion.
 */
static void handle_stroke_play_turn(game_state_t *gs)
{
    Player *player = &gs->players[gs->current_player_index];

    DEBUG_INFO(MODULE_GAME, "Stroke Play - completing turn for Player %d",
               gs->current_player_index + 1);

    /* Score card update will be triggered via event */
    player->current_hole++;
    player->detection_count = 0;

    DEBUG_INFO(MODULE_GAME, "Player %d advanced to hole %d",
               gs->current_player_index + 1, player->current_hole);
}

/**
 * Handle match play turn completion.
 */
static void handle_match_play_turn(game_state_t *gs)
{
    Player *player = &gs->players[gs->current_player_index];

    match_play_round_scores[gs->current_player_index] = player->round_total_score;

    /* Announce next player/team */
    if (gs->current_player_index == 0) {
        if (gs->match_play_mode == MATCH_PLAY_MODE_1V1) {
            play_sound_once(load_sound_effect(SOUND_PLAYERTWO_WAV), SOUND_DELAY_TURN_SWITCH_MS);
        } else {
            play_sound_once(load_sound_effect(SOUND_TEAMTWO_WAV), SOUND_DELAY_TURN_SWITCH_MS);
        }
    }

    /* Both players completed their turn — evaluate hole */
    if (gs->current_player_index == 1) {
        DEBUG_INFO(MODULE_GAME, "Both players completed turn - evaluating match result");

        if (match_play_round_scores[0] > match_play_round_scores[1]) {
            gs->players[0].upAndDown++;
            gs->players[1].upAndDown--;
            gs->players[0].holes_won++;
            DEBUG_INFO(MODULE_GAME, "Player 1 wins the hole");
        } else if (match_play_round_scores[0] < match_play_round_scores[1]) {
            gs->players[0].upAndDown--;
            gs->players[1].upAndDown++;
            gs->players[1].holes_won++;
            DEBUG_INFO(MODULE_GAME, "Player 2 wins the hole");
        } else {
            DEBUG_INFO(MODULE_GAME, "Hole is halved (tie)");
        }

        /* Check early victory */
        int lead = (gs->players[0].upAndDown > 0) ? gs->players[0].upAndDown :
                   (gs->players[1].upAndDown > 0) ? gs->players[1].upAndDown : 0;
        int holes_remaining = gs->hole_mode - gs->players[0].current_hole - 1;
        int early_victory = (lead > 0 && lead > holes_remaining);

        /* Announce next player/team unless game over */
        if (gs->players[0].current_hole < gs->hole_mode && !early_victory) {
            if (gs->match_play_mode == MATCH_PLAY_MODE_1V1) {
                play_sound_once(load_sound_effect(SOUND_PLAYERONE_WAV), SOUND_DELAY_TURN_SWITCH_MS);
            } else {
                play_sound_once(load_sound_effect(SOUND_TEAMONE_WAV), SOUND_DELAY_TURN_SWITCH_MS);
            }
        }

        /* Advance both players */
        gs->players[0].current_hole++;
        gs->players[1].current_hole++;
        gs->players[0].round_total_score = 0;
        gs->players[1].round_total_score = 0;

        DEBUG_INFO(MODULE_GAME, "Both players advanced to hole %d", gs->players[0].current_hole);

        /* Publish hole completed event for UI to update match status display */
        game_event_t hole_event = { .type = EVENT_HOLE_COMPLETED };
        hole_event.data.player.player_index = gs->current_player_index;
        hole_event.data.player.num_players = gs->num_players;
        event_bus_publish(&hole_event);
    }

    player->detection_count = 0;
}

/**
 * Check if all players have completed all holes and if so, determine winner.
 */
static void check_completion(game_state_t *gs)
{
    int all_done = 1;

    switch (gs->game_mode) {
        case GAME_MODE_STROKE_PLAY:
            for (int i = 0; i < gs->num_players; i++) {
                if (gs->players[i].current_hole > gs->hole_mode + 1 ||
                    (gs->players[i].current_hole == gs->hole_mode + 1 &&
                     gs->players[i].detection_count == 0)) {
                    gs->players[i].round_total_score = 0;
                } else {
                    all_done = 0;
                }
            }
            break;

        case GAME_MODE_MATCH_PLAY: {
            int lead = (gs->players[0].upAndDown > 0) ? gs->players[0].upAndDown :
                       (gs->players[1].upAndDown > 0) ? gs->players[1].upAndDown : 0;
            int holes_remaining = gs->hole_mode - gs->players[0].current_hole;
            int early_victory = (lead > 0 && lead > holes_remaining);

            if (gs->players[0].current_hole > gs->hole_mode || early_victory) {
                all_done = 1;
            } else {
                all_done = 0;
            }
            break;
        }

        case GAME_MODE_QUOTA:
        case GAME_MODE_VEGAS:
            /* Quota completion is handled immediately in on_score_changed */
            all_done = 0;
            break;

        default:
            all_done = 0;
            break;
    }

    if (!all_done) return;

    gs->update_flag = 1;
    DEBUG_INFO(MODULE_GAME, "All players completed - determining winner");

    int winner = -1;
    if (gs->game_mode == GAME_MODE_MATCH_PLAY) {
        winner = determine_match_winner(gs);
    } else {
        winner = determine_stroke_winner(gs);
    }

    gs->sensors_enabled = 0;
    DEBUG_INFO(MODULE_GAME, "Sensors disabled - game complete");

    /* Calculate final scores */
    for (int i = 0; i < gs->num_players; i++) {
        gs->final_scores[i] = gs->players[i].score;
        DEBUG_INFO(MODULE_GAME, "Player %d final score: %d", i + 1, gs->players[i].score);
    }

    if (winner >= 0) {
        DEBUG_INFO(MODULE_GAME, "Winner: Player %d", winner + 1);
    } else {
        DEBUG_INFO(MODULE_GAME, "Game ended in a tie");
    }

    /* Publish game completed event — UI will show crowns, scorecards, etc. */
    game_event_t event = { .type = EVENT_GAME_COMPLETED };
    event.data.game_complete.winner_index = winner;
    event.data.game_complete.game_mode = gs->game_mode;
    event.data.game_complete.hole_mode = gs->hole_mode;
    event.data.game_complete.num_players = gs->num_players;
    event_bus_publish(&event);
}

/**
 * Event handler: called when GPIO driver detects a valid sensor hit.
 */
static void on_score_changed(const game_event_t *event)
{
    game_state_t *gs = game_state_get();

    if (gs->update_flag) return; /* Game already completed */

    Player *player = &gs->players[gs->current_player_index];
    unsigned int pin = event->data.score.pin;

    DEBUG_DEBUG(MODULE_GAME, "Processing event for Player %d (detections: %d/%d)",
                gs->current_player_index + 1, player->detection_count, SENSORS_PER_TURN);

    if (player->detection_count >= SENSORS_PER_TURN) return;

    /* Dispatch to game mode processor */
    extern led_strip_controller_t leds;
    switch (gs->game_mode) {
        case GAME_MODE_STROKE_PLAY:
            stroke_play_process_pin(player, gs->current_player_index, pin, &leds);
            break;

        case GAME_MODE_MATCH_PLAY:
            match_play_process_pin(player, gs->current_player_index, pin, &leds);
            break;

        case GAME_MODE_QUOTA:
            quota_play_process_pin(player, gs->current_player_index, pin, &leds);
            player->round_total_score = player->score;
            break;

        case GAME_MODE_VEGAS:
            vegas_quota_play_process_pin(player, gs->current_player_index, pin, &leds,
                                         gs->players, gs->num_players);
            player->round_total_score = player->score;
            break;

        default:
            DEBUG_ERROR(MODULE_GAME, "Unknown game mode: %d", gs->game_mode);
            return;
    }

    player->detection_count++;
    DEBUG_DEBUG(MODULE_GAME, "Player %d detection count: %d",
                gs->current_player_index + 1, player->detection_count);

    /* Publish updated score for UI to display */
    game_event_t score_event = { .type = EVENT_SCORE_CHANGED };
    score_event.data.score.player_index = gs->current_player_index;
    score_event.data.score.score = player->score;
    score_event.data.score.hole = player->current_hole;
    score_event.data.score.detection_count = player->detection_count;
    score_event.data.score.pin = pin;
    score_event.data.score.num_players = gs->num_players;
    /* Note: This re-publishes EVENT_SCORE_CHANGED but with updated data.
     * The UI subscriber uses this to update labels. */

    /* Quota/Vegas: check immediate completion */
    if ((gs->game_mode == GAME_MODE_QUOTA || gs->game_mode == GAME_MODE_VEGAS) &&
        !gs->update_flag) {
        if (quota_player_completed(player)) {
            player->detection_count = 0;
            DEBUG_INFO(MODULE_GAME, "Player %d completed quota - ending game",
                       gs->current_player_index + 1);

            game_event_t qe = { .type = EVENT_QUOTA_COMPLETED };
            qe.data.player.player_index = gs->current_player_index;
            qe.data.player.num_players = gs->num_players;
            event_bus_publish(&qe);

            check_completion(gs);
            return;
        } else if (gs->num_players == 1 && player->detection_count == 1) {
            player->detection_count = 0;
            DEBUG_INFO(MODULE_GAME, "Quota 1P - hole completed immediately");
        }
    }

    /* Handle turn completion (2 detections per turn) */
    if (player->detection_count == SENSORS_PER_TURN && !gs->update_flag) {
        DEBUG_INFO(MODULE_GAME, "Player %d completed turn", gs->current_player_index + 1);

        switch (gs->game_mode) {
            case GAME_MODE_STROKE_PLAY:
                handle_stroke_play_turn(gs);
                check_completion(gs);
                break;

            case GAME_MODE_MATCH_PLAY:
                handle_match_play_turn(gs);
                if (gs->current_player_index == 1) {
                    check_completion(gs);
                }
                break;

            case GAME_MODE_QUOTA:
            case GAME_MODE_VEGAS:
                /* Quota modes handle completion above */
                player->current_hole++;
                player->detection_count = 0;
                break;

            default:
                break;
        }

        /* Switch to next player */
        if (!gs->update_flag) {
            gs->current_player_index = (gs->current_player_index + 1) % gs->num_players;
            DEBUG_INFO(MODULE_GAME, "Switched to Player %d", gs->current_player_index + 1);

            game_event_t pe = { .type = EVENT_PLAYER_SWITCHED };
            pe.data.player.player_index = gs->current_player_index;
            pe.data.player.num_players = gs->num_players;
            event_bus_publish(&pe);
        }
    }
}

void game_controller_init(void)
{
    event_bus_subscribe(EVENT_SCORE_CHANGED, on_score_changed);
    match_play_round_scores[0] = 0;
    match_play_round_scores[1] = 0;
    DEBUG_INFO(MODULE_GAME, "Game controller initialized");
}
