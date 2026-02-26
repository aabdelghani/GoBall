#include "ui_logic.event.h"
#include "../debug/debug.h"
#include "../sound_logic/sound_logic_event.h"
#include <stdbool.h>

/*
 * Game state access macros.
 * These map the legacy global variable names used throughout this file
 * to the centralized game_state module. Must be defined AFTER includes
 * to avoid breaking struct definitions in headers.
 */
#define current_game_mode       (game_state_get()->game_mode)
#define current_hole_mode       (game_state_get()->hole_mode)
#define current_match_play_mode (game_state_get()->match_play_mode)
#define num_players             (game_state_get()->num_players)
#define players                 (game_state_get()->players)
#define current_player_index    (game_state_get()->current_player_index)
#define all_players_completed   (game_state_get()->all_players_completed)
#define update_flag             (game_state_get()->update_flag)
#define highliting_counter      (game_state_get()->highlighting_counter)
#define sensors_enabled         (game_state_get()->sensors_enabled)
#define prev_scores             (game_state_get()->prev_scores)
#define individual_scores       (game_state_get()->individual_scores)
#define final_scores            (game_state_get()->final_scores)

/* Highlight color aliases */
#define COLOR_1 COLOR_HIGHLIGHT_1
#define COLOR_2 COLOR_HIGHLIGHT_2

void    set_game_mode(GameMode mode)
{
    DEBUG_INFO(MODULE_UI, "========== GAME MODE SELECTION ==========");
    if (mode < NUM_GAME_MODES)
    {
        current_game_mode = mode;
        DEBUG_INFO(MODULE_UI, "Game mode set to: %d (%s)", mode,
                   (mode == GAME_MODE_STROKE_PLAY)  ? "Stroke Play"
                   : (mode == GAME_MODE_MATCH_PLAY) ? "Match Play"
                   : (mode == GAME_MODE_QUOTA)      ? "Quota"
                   : (mode == GAME_MODE_VEGAS)      ? "Vegas"
                                                    : "Unknown");
    }
    DEBUG_INFO(MODULE_UI, "=========================================");
}

// Set sensor state (1 = enabled, 0 = disabled)
void set_sensors_enabled(uint8_t enable)
{
    sensors_enabled = enable;
    // Optional: Add hardware/sensor control logic here
}

// Print current sensor state
void print_sensors_status()
{
    if (sensors_enabled)
    {
        DEBUG_INFO(MODULE_UI, "Sensors: ENABLED (1)");
    }
    else
    {
        DEBUG_INFO(MODULE_UI, "Sensors: DISABLED (0)");
    }
}

const char *get_game_mode_name(void)
{
    switch (current_game_mode)
    {
        case GAME_MODE_STROKE_PLAY:
            return "Stroke Play";
        case GAME_MODE_MATCH_PLAY:
            return "Match Play";
        case GAME_MODE_QUOTA:
            return "Quota";
        case GAME_MODE_VEGAS:
            return "Vegas";
        default:
            return "Unknown";
    }
}

void print_current_game_mode(void)
{
    DEBUG_INFO(MODULE_UI, "Game mode set to: %s", get_game_mode_name());
}

void set_num_players(uint8_t new_num_players)
{
    DEBUG_INFO(MODULE_UI, "========== PLAYER COUNT SELECTION ==========");
    if (new_num_players < 1 || new_num_players > MAX_PLAYERS)
    {
        DEBUG_WARN(MODULE_UI, "Invalid number of players. Please select between 1 and %d",
                   MAX_PLAYERS);
        return;
    }
    num_players = new_num_players;
    DEBUG_INFO(MODULE_UI, "Number of players set to: %d", num_players);
    DEBUG_INFO(MODULE_UI, "Current settings: game_mode=%d, num_players=%d", current_game_mode,
               num_players);
    DEBUG_INFO(MODULE_UI, "=============================================");
    for (uint8_t i = 0; i < num_players; i++)
    {
        final_scores[i] = 0;
        for (uint8_t j = 0; j < current_hole_mode; j++)
        {
            individual_scores[i][j] = 0;
        }
    }
}

void reset_scores(void)
{
    sensors_enabled = 0;  // Reset sensors enabled flag
    // Reset all players' data
    for (uint8_t i = 0; i < MAX_PLAYERS; i++)
    {
        players[i].score           = 0;
        players[i].current_hole    = 0;
        players[i].detection_count = 0;
        if (current_hole_mode == EIGHTEEN_HOLES)
        {
            players[i].par3_count = QUOTA_18H_PAR3;
            players[i].par4_count = QUOTA_18H_PAR4;
            players[i].par5_count = QUOTA_18H_PAR5;
        }
        else
        {
            players[i].par3_count = QUOTA_9H_PAR3;
            players[i].par4_count = QUOTA_9H_PAR4;
            players[i].par5_count = QUOTA_9H_PAR5;
        }
        final_scores[i] = 0;

        // Reset score arrays
        for (uint8_t j = 0; j < 9; j++)
        {
            individual_scores[i][j] = 0;
            prev_scores[i][j]       = 0;
        }
    }
    // Match Play UI reset (1v1, 9 holes)
    lv_label_set_text(ui_MP1V19HGSP1SPText, "E");     // Player 1 Up/Down status
    lv_label_set_text(ui_MP1V19HGSP2SPText, "E");     // Player 2 Up/Down status
    lv_label_set_text_fmt(ui_MP1V19HGSHCPText, "0");  // Holes won
    lv_label_set_text_fmt(ui_MP1V19HGSBCPText, "0");  // Balls won

    // Hide crowns for Match Play 1v1 9H
    lv_obj_add_flag(ui_MP1V19HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_MP1V19HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_MP1V19HScP1SCrown, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_MP1V19HScP2SCrown, LV_OBJ_FLAG_HIDDEN);

    // Resetting Match Play scorecard UI using loop
    lv_obj_t *mp_p1_scores[] = {ui_MP1V19HScP1SText1, ui_MP1V19HScP1SText2, ui_MP1V19HScP1SText3,
                                ui_MP1V19HScP1SText4, ui_MP1V19HScP1SText5, ui_MP1V19HScP1SText6,
                                ui_MP1V19HScP1SText7, ui_MP1V19HScP1SText8, ui_MP1V19HScP1SText9,
                                ui_MP1V19HScP1STextF};
    lv_obj_t *mp_p2_scores[] = {ui_MP1V19HScP2SText1, ui_MP1V19HScP2SText2, ui_MP1V19HScP2SText3,
                                ui_MP1V19HScP2SText4, ui_MP1V19HScP2SText5, ui_MP1V19HScP2SText6,
                                ui_MP1V19HScP2SText7, ui_MP1V19HScP2SText8, ui_MP1V19HScP2SText9,
                                ui_MP1V19HScP2STextF};

    for (uint8_t i = 0; i < 10; i++)
    {
        lv_label_set_text(mp_p1_scores[i], "-");
        lv_label_set_text(mp_p2_scores[i], "-");
    }

    // Reset Match Play player data
    players[0].upAndDown = 0;
    players[1].upAndDown = 0;
    players[0].holes_won = 0;
    players[1].holes_won = 0;

    // Match Play UI reset (1v1, 18 holes)
    lv_label_set_text(ui_MP1V118HGSP1SPText, "E");     // Player 1 Up/Down status
    lv_label_set_text(ui_MP1V118HGSP2SPText, "E");     // Player 2 Up/Down status
    lv_label_set_text_fmt(ui_MP1V118HGSHCPText, "0");  // Holes won
    lv_label_set_text_fmt(ui_MP1V118HGSBCPText, "0");  // Balls won

    // Hide crowns for Match Play 1v1 18H
    lv_obj_add_flag(ui_MP1V118HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_MP1V118HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_MP1V118HScP1SCrown, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_MP1V118HScP2SCrown, LV_OBJ_FLAG_HIDDEN);

    // Resetting Match Play scorecard UI for 18 holes using loop
    lv_obj_t *mp18_p1_scores[] = {
        ui_MP1V118HScP1SText19, ui_MP1V118HScP1SText2,  ui_MP1V118HScP1SText3,
        ui_MP1V118HScP1SText4,  ui_MP1V118HScP1SText5,  ui_MP1V118HScP1SText6,
        ui_MP1V118HScP1SText7,  ui_MP1V118HScP1SText8,  ui_MP1V118HScP1SText9,
        ui_MP1V118HScP1SText10, ui_MP1V118HScP1SText11, ui_MP1V118HScP1SText12,
        ui_MP1V118HScP1SText13, ui_MP1V118HScP1SText14, ui_MP1V118HScP1SText15,
        ui_MP1V118HScP1SText16, ui_MP1V118HScP1SText17, ui_MP1V118HScP1SText18,
        ui_MP1V118HScP1STextF};

    lv_obj_t *mp18_p2_scores[] = {
        ui_MP1V118HScP2SText1,  ui_MP1V118HScP2SText2,  ui_MP1V118HScP2SText3,
        ui_MP1V118HScP2SText4,  ui_MP1V118HScP2SText5,  ui_MP1V118HScP2SText6,
        ui_MP1V118HScP2SText7,  ui_MP1V118HScP2SText8,  ui_MP1V118HScP2SText9,
        ui_MP1V118HScP2SText10, ui_MP1V118HScP2SText11, ui_MP1V118HScP2SText12,
        ui_MP1V118HScP2SText13, ui_MP1V118HScP2SText14, ui_MP1V118HScP2SText15,
        ui_MP1V118HScP2SText16, ui_MP1V118HScP2SText17, ui_MP1V118HScP2SText18,
        ui_MP1V118HScP2STextF};

    for (int i = 0; i < 19; i++)
    {
        lv_label_set_text(mp18_p1_scores[i], "-");
        lv_label_set_text(mp18_p2_scores[i], "-");
    }

    // Match Play UI reset (2v2, 9 holes)
    lv_label_set_text(ui_MP2V29HGST1SPText, "E");     // Team 1 Up/Down status
    lv_label_set_text(ui_MP2V29HGST2SPText, "E");     // Team 2 Up/Down status
    lv_label_set_text_fmt(ui_MP2V29HGSHCPText, "0");  // Holes won
    lv_label_set_text_fmt(ui_MP2V29HGSBCPText, "0");  // Balls won

    // Hide crowns for Match Play 2v2 9H
    lv_obj_add_flag(ui_MP2V29HGST1SCrown, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_MP2V29HGST2SCrown, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_MP2V29HScT1SCrown, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_MP2V29HScT2SCrown, LV_OBJ_FLAG_HIDDEN);

    // Resetting Match Play 2v2 scorecard UI using loop
    lv_obj_t *mp2v2_t1_scores[] = {ui_MP2V29HScT1SText1, ui_MP2V29HScT1SText2, ui_MP2V29HScT1SText3,
                                   ui_MP2V29HScT1SText4, ui_MP2V29HScT1SText5, ui_MP2V29HScT1SText6,
                                   ui_MP2V29HScT1SText7, ui_MP2V29HScT1SText8, ui_MP2V29HScT1SText9,
                                   ui_MP2V29HScT1STextF};
    lv_obj_t *mp2v2_t2_scores[] = {ui_MP2V29HScT2SText1, ui_MP2V29HScT2SText2, ui_MP2V29HScT2SText3,
                                   ui_MP2V29HScT2SText4, ui_MP2V29HScT2SText5, ui_MP2V29HScT2SText6,
                                   ui_MP2V29HScT2SText7, ui_MP2V29HScT2SText8, ui_MP2V29HScT2SText9,
                                   ui_MP2V29HScT2STextF};

    for (uint8_t i = 0; i < 10; i++)
    {
        lv_label_set_text(mp2v2_t1_scores[i], "-");
        lv_label_set_text(mp2v2_t2_scores[i], "-");
    }

    // Match Play UI reset (2v2, 18 holes)
    lv_label_set_text(ui_MP2V218HGST1SPText, "E");     // Team 1 Up/Down status
    lv_label_set_text(ui_MP2V218HGST2SPText, "E");     // Team 2 Up/Down status
    lv_label_set_text_fmt(ui_MP2V218HGSHCPText, "0");  // Holes won
    lv_label_set_text_fmt(ui_MP2V218HGSBCPText, "0");  // Balls won

    // Hide crowns for Match Play 2v2 18H
    lv_obj_add_flag(ui_MP2V218HGST1SCrown, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_MP2V218HGST2SCrown, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_MP2V218HScT1SCrown, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_MP2V218HScT2SCrown, LV_OBJ_FLAG_HIDDEN);

    // Resetting Match Play 2v2 18H scorecard UI using loop
    lv_obj_t *mp2v218_t1_scores[] = {
        ui_MP2V218HScT1SText1,  ui_MP2V218HScT1SText2,  ui_MP2V218HScT1SText3,
        ui_MP2V218HScT1SText4,  ui_MP2V218HScT1SText5,  ui_MP2V218HScT1SText6,
        ui_MP2V218HScT1SText7,  ui_MP2V218HScT1SText8,  ui_MP2V218HScT1SText9,
        ui_MP2V218HScT1SText10, ui_MP2V218HScT1SText11, ui_MP2V218HScT1SText12,
        ui_MP2V218HScT1SText13, ui_MP2V218HScT1SText14, ui_MP2V218HScT1SText15,
        ui_MP2V218HScT1SText16, ui_MP2V218HScT1SText17, ui_MP2V218HScT1SText18,
        ui_MP2V218HScT1STextF};
    lv_obj_t *mp2v218_t2_scores[] = {
        ui_MP2V218HScT2SText1,  ui_MP2V218HScT2SText2,  ui_MP2V218HScT2SText3,
        ui_MP2V218HScT2SText4,  ui_MP2V218HScT2SText5,  ui_MP2V218HScT2SText6,
        ui_MP2V218HScT2SText7,  ui_MP2V218HScT2SText8,  ui_MP2V218HScT2SText9,
        ui_MP2V218HScT2SText10, ui_MP2V218HScT2SText11, ui_MP2V218HScT2SText12,
        ui_MP2V218HScT2SText13, ui_MP2V218HScT2SText14, ui_MP2V218HScT2SText15,
        ui_MP2V218HScT2SText16, ui_MP2V218HScT2SText17, ui_MP2V218HScT2SText18,
        ui_MP2V218HScT2STextF};

    for (int i = 0; i < 19; i++)
    {
        lv_label_set_text(mp2v218_t1_scores[i], "-");
        lv_label_set_text(mp2v218_t2_scores[i], "-");
    }

    // Reset scorecard UI
    if (num_players == 1)
    {
        // Single-player UI reset 9 Holes
        lv_label_set_text(ui_SP1P9HGSPSPText, "0");  // Score
        lv_label_set_text(ui_SP1P9HGSHCPText, "0");  // Holes
        lv_label_set_text(ui_SP1P9HGSBCPText, "0");  // Balls

        // Resetting score card UI 9 Holes
        lv_label_set_text(ui_SP1P9HScPS1Text, "0");
        lv_label_set_text(ui_SP1P9HScPS2Text, "0");
        lv_label_set_text(ui_SP1P9HScPS3Text, "0");
        lv_label_set_text(ui_SP1P9HScPS4Text, "0");
        lv_label_set_text(ui_SP1P9HScPS5Text, "0");
        lv_label_set_text(ui_SP1P9HScPS6Text, "0");
        lv_label_set_text(ui_SP1P9HScPS7Text, "0");
        lv_label_set_text(ui_SP1P9HScPS8Text, "0");
        lv_label_set_text(ui_SP1P9HScPS9Text, "0");

        lv_label_set_text(ui_SP1P9HScPSFText, "0");  // Final score

        // Single-player UI reset 18 Holes
        lv_label_set_text(ui_SP1P18HGSPSPText, "0");  // Score
        lv_label_set_text(ui_SP1P18HGSHCPText, "0");  // Holes
        lv_label_set_text(ui_SP1P18HGSBCPText, "0");  // Balls

        // Resetting score card UI 18 Holes
        lv_label_set_text(ui_SP1P18HScPSText1, "0");
        lv_label_set_text(ui_SP1P18HScPSText2, "0");
        lv_label_set_text(ui_SP1P18HScPSText3, "0");
        lv_label_set_text(ui_SP1P18HScPSText4, "0");
        lv_label_set_text(ui_SP1P18HScPSText5, "0");
        lv_label_set_text(ui_SP1P18HScPSText6, "0");
        lv_label_set_text(ui_SP1P18HScPSText7, "0");
        lv_label_set_text(ui_SP1P18HScPSText8, "0");
        lv_label_set_text(ui_SP1P18HScPSText9, "0");
        lv_label_set_text(ui_SP1P18HScPSText10, "0");
        lv_label_set_text(ui_SP1P18HScPSText11, "0");
        lv_label_set_text(ui_SP1P18HScPSText12, "0");
        lv_label_set_text(ui_SP1P18HScPSText13, "0");
        lv_label_set_text(ui_SP1P18HScPSText14, "0");
        lv_label_set_text(ui_SP1P18HScPSText15, "0");
        lv_label_set_text(ui_SP1P18HScPSText16, "0");
        lv_label_set_text(ui_SP1P18HScPSText17, "0");
        lv_label_set_text(ui_SP1P18HScPSText18, "0");

        lv_label_set_text(ui_SP1P18HScPSText19, "0");  // Final score
    }
    else if (num_players == 2)
    {
        // Two-player UI reset 9 holes
        lv_label_set_text(ui_SP2P9HGSP1SPText, "0");               // Player 1 Score
        lv_obj_add_flag(ui_SP2P9HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);  // Hide crown for player 1
        lv_obj_add_flag(ui_SP2P9HScP1SCrown,
                        LV_OBJ_FLAG_HIDDEN);  // Hide crown for player 1 scorecard

        lv_label_set_text(ui_SP2P9HGSP2SPText, "0");               // Player 2 Score
        lv_obj_add_flag(ui_SP2P9HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);  // Hide player 2 scoreboard
        lv_obj_add_flag(ui_SP2P9HScP2SCrown,
                        LV_OBJ_FLAG_HIDDEN);  // Hide crown for player 2 scorecard

        lv_label_set_text(ui_SP2P9HGSHCPText, "0");  // Holes
        lv_label_set_text(ui_SP2P9HGSBCPText, "0");  // Balls

        // Resetting score card UI 9 holes
        lv_label_set_text(ui_SP2P9HScP1S1Text, "0");
        lv_label_set_text(ui_SP2P9HScP1S2Text, "0");
        lv_label_set_text(ui_SP2P9HScP1S3Text, "0");
        lv_label_set_text(ui_SP2P9HScP1S4Text, "0");
        lv_label_set_text(ui_SP2P9HScP1S5Text, "0");
        lv_label_set_text(ui_SP2P9HScP1S6Text, "0");
        lv_label_set_text(ui_SP2P9HScP1S7Text, "0");
        lv_label_set_text(ui_SP2P9HScP1S8Text, "0");
        lv_label_set_text(ui_SP2P9HScP1S9Text, "0");

        lv_label_set_text(ui_SP2P9HScP2S1Text, "0");
        lv_label_set_text(ui_SP2P9HScP2S2Text, "0");
        lv_label_set_text(ui_SP2P9HScP2S3Text, "0");
        lv_label_set_text(ui_SP2P9HScP2S4Text, "0");
        lv_label_set_text(ui_SP2P9HScP2S5Text, "0");
        lv_label_set_text(ui_SP2P9HScP2S6Text, "0");
        lv_label_set_text(ui_SP2P9HScP2S7Text, "0");
        lv_label_set_text(ui_SP2P9HScP2S8Text, "0");
        lv_label_set_text(ui_SP2P9HScP2S9Text, "0");

        lv_label_set_text(ui_SP2P9HScP1SFText, "0");  // Player 1 Final score
        lv_label_set_text(ui_SP2P9HScP2SFText, "0");  // Player 2 Final score

        // Two-player UI reset 18 holes
        lv_label_set_text(ui_SP2P18HGSP1SPText, "0");  // Player 1 Score
        lv_label_set_text(ui_SP2P18HGSP2SPText, "0");  // Player 2 Score
        lv_label_set_text(ui_SP2P18HGSHCPText, "0");   // Holes
        lv_label_set_text(ui_SP2P18HGSBCPText, "0");   // Balls

        // Resetting score card UI 18 holes player 1
        lv_label_set_text(ui_SP2P18HScP1SText1, "0");
        lv_label_set_text(ui_SP2P18HScP1SText2, "0");
        lv_label_set_text(ui_SP2P18HScP1SText3, "0");
        lv_label_set_text(ui_SP2P18HScP1SText4, "0");
        lv_label_set_text(ui_SP2P18HScP1SText5, "0");
        lv_label_set_text(ui_SP2P18HScP1SText6, "0");
        lv_label_set_text(ui_SP2P18HScP1SText7, "0");
        lv_label_set_text(ui_SP2P18HScP1SText8, "0");
        lv_label_set_text(ui_SP2P18HScP1SText9, "0");
        lv_label_set_text(ui_SP2P18HScP1SText10, "0");
        lv_label_set_text(ui_SP2P18HScP1SText11, "0");
        lv_label_set_text(ui_SP2P18HScP1SText12, "0");
        lv_label_set_text(ui_SP2P18HScP1SText13, "0");
        lv_label_set_text(ui_SP2P18HScP1SText14, "0");
        lv_label_set_text(ui_SP2P18HScP1SText15, "0");
        lv_label_set_text(ui_SP2P18HScP1SText16, "0");
        lv_label_set_text(ui_SP2P18HScP1SText17, "0");
        lv_label_set_text(ui_SP2P18HScP1SText18, "0");

        lv_label_set_text(ui_SP2P18HScP2SText1, "0");
        lv_label_set_text(ui_SP2P18HScP2SText2, "0");
        lv_label_set_text(ui_SP2P18HScP2SText3, "0");
        lv_label_set_text(ui_SP2P18HScP2SText4, "0");
        lv_label_set_text(ui_SP2P18HScP2SText5, "0");
        lv_label_set_text(ui_SP2P18HScP2SText6, "0");
        lv_label_set_text(ui_SP2P18HScP2SText7, "0");
        lv_label_set_text(ui_SP2P18HScP2SText8, "0");
        lv_label_set_text(ui_SP2P18HScP2SText9, "0");
        lv_label_set_text(ui_SP2P18HScP2SText10, "0");
        lv_label_set_text(ui_SP2P18HScP2SText11, "0");
        lv_label_set_text(ui_SP2P18HScP2SText12, "0");
        lv_label_set_text(ui_SP2P18HScP2SText13, "0");
        lv_label_set_text(ui_SP2P18HScP2SText14, "0");
        lv_label_set_text(ui_SP2P18HScP2SText15, "0");
        lv_label_set_text(ui_SP2P18HScP2SText16, "0");
        lv_label_set_text(ui_SP2P18HScP2SText17, "0");
        lv_label_set_text(ui_SP2P18HScP2SText18, "0");

        lv_label_set_text(ui_SP2P18HScP1STextF, "0");  // Player 1 Final score
        lv_label_set_text(ui_SP2P18HScP2STextF, "0");  // Player 2 Final score
    }
    else if (num_players == 3)
    {
        // Three-player UI reset 9 Holes
        lv_label_set_text(ui_SP3P9HGSP1SPText, "0");               // Player 1 Score
        lv_obj_add_flag(ui_SP3P9HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);  // Hide crown for player 1
        lv_obj_add_flag(ui_SP3P9HScP1SCrown,
                        LV_OBJ_FLAG_HIDDEN);          // Hide crown for player 1 scorecard
        lv_label_set_text(ui_SP3P9HGSP2SPText, "0");  // Player 2 Score
        lv_obj_add_flag(ui_SP3P9HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);  // Hide player 2 scoreboard
        lv_obj_add_flag(ui_SP3P9HScP2SCrown,
                        LV_OBJ_FLAG_HIDDEN);          // Hide crown for player 2 scorecard
        lv_label_set_text(ui_SP3P9HGSP3SPText, "0");  // Player 3 Score
        lv_obj_add_flag(ui_SP3P9HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);  // Hide player 3 scoreboard
        lv_obj_add_flag(ui_SP3P9HScP3SCrown,
                        LV_OBJ_FLAG_HIDDEN);         // Hide crown for player 3 scorecard
        lv_label_set_text(ui_SP3P9HGSHCPText, "0");  // Holes
        lv_label_set_text(ui_SP3P9HGSBCPText, "0");  // Balls

        // Resetting score card UI 9 Holes
        lv_label_set_text(ui_SP3P9HScP1S1Text, "0");
        lv_label_set_text(ui_SP3P9HScP1S2Text, "0");
        lv_label_set_text(ui_SP3P9HScP1S3Text, "0");
        lv_label_set_text(ui_SP3P9HScP1S4Text, "0");
        lv_label_set_text(ui_SP3P9HScP1S5Text, "0");
        lv_label_set_text(ui_SP3P9HScP1S6Text, "0");
        lv_label_set_text(ui_SP3P9HScP1S7Text, "0");
        lv_label_set_text(ui_SP3P9HScP1S8Text, "0");
        lv_label_set_text(ui_SP3P9HScP1S9Text, "0");

        lv_label_set_text(ui_SP3P9HScP2S1Text, "0");
        lv_label_set_text(ui_SP3P9HScP2S2Text, "0");
        lv_label_set_text(ui_SP3P9HScP2S3Text, "0");
        lv_label_set_text(ui_SP3P9HScP2S4Text, "0");
        lv_label_set_text(ui_SP3P9HScP2S5Text, "0");
        lv_label_set_text(ui_SP3P9HScP2S6Text, "0");
        lv_label_set_text(ui_SP3P9HScP2S7Text, "0");
        lv_label_set_text(ui_SP3P9HScP2S8Text, "0");
        lv_label_set_text(ui_SP3P9HScP2S9Text, "0");

        lv_label_set_text(ui_SP3P9HScP3S1Text, "0");
        lv_label_set_text(ui_SP3P9HScP3S2Text, "0");
        lv_label_set_text(ui_SP3P9HScP3S3Text, "0");
        lv_label_set_text(ui_SP3P9HScP3S4Text, "0");
        lv_label_set_text(ui_SP3P9HScP3S5Text, "0");
        lv_label_set_text(ui_SP3P9HScP3S6Text, "0");
        lv_label_set_text(ui_SP3P9HScP3S7Text, "0");
        lv_label_set_text(ui_SP3P9HScP3S8Text, "0");
        lv_label_set_text(ui_SP3P9HScP3S9Text, "0");

        lv_label_set_text(ui_SP3P9HScP1SFText, "0");  // Player 1 Final score
        lv_label_set_text(ui_SP3P9HScP2SFText, "0");  // Player 2 Final score
        lv_label_set_text(ui_SP3P9HScP3SFText, "0");  // Player 3 Final score

        // Two-player UI reset 9 holes
        lv_label_set_text(ui_SP2P9HGSP1SPText, "0");  // Player 1 Score
        lv_label_set_text(ui_SP2P9HGSP2SPText, "0");  // Player 2 Score
        lv_label_set_text(ui_SP2P9HGSHCPText, "0");   // Holes
        lv_label_set_text(ui_SP2P9HGSBCPText, "0");   // Balls

        // Resetting score card UI 9 holes
        lv_label_set_text(ui_SP2P9HScP1S1Text, "0");
        lv_label_set_text(ui_SP2P9HScP1S2Text, "0");
        lv_label_set_text(ui_SP2P9HScP1S3Text, "0");
        lv_label_set_text(ui_SP2P9HScP1S4Text, "0");
        lv_label_set_text(ui_SP2P9HScP1S5Text, "0");
        lv_label_set_text(ui_SP2P9HScP1S6Text, "0");
        lv_label_set_text(ui_SP2P9HScP1S7Text, "0");
        lv_label_set_text(ui_SP2P9HScP1S8Text, "0");
        lv_label_set_text(ui_SP2P9HScP1S9Text, "0");

        lv_label_set_text(ui_SP2P9HScP2S1Text, "0");
        lv_label_set_text(ui_SP2P9HScP2S2Text, "0");
        lv_label_set_text(ui_SP2P9HScP2S3Text, "0");
        lv_label_set_text(ui_SP2P9HScP2S4Text, "0");
        lv_label_set_text(ui_SP2P9HScP2S5Text, "0");
        lv_label_set_text(ui_SP2P9HScP2S6Text, "0");
        lv_label_set_text(ui_SP2P9HScP2S7Text, "0");
        lv_label_set_text(ui_SP2P9HScP2S8Text, "0");
        lv_label_set_text(ui_SP2P9HScP2S9Text, "0");

        lv_label_set_text(ui_SP2P9HScP1SFText, "0");  // Player 1 Final score
        lv_label_set_text(ui_SP2P9HScP2SFText, "0");  // Player 2 Final score

        // Three-player UI reset 18 holes
        lv_label_set_text(ui_SP3P18HGSP1SPText, "0");  // Player 1 Score
        lv_label_set_text(ui_SP3P18HGSP2SPText, "0");  // Player 2 Score
        lv_label_set_text(ui_SP3P18HGSP3SPText, "0");  // Player 3 Score

        lv_label_set_text(ui_SP3P18HGSHCPText, "0");  // Holes
        lv_label_set_text(ui_SP3P18HGSBCPText, "0");  // Balls

        // Resetting score card UI 18 holes player 1
        lv_label_set_text(ui_SP3P18HScP1SText1, "0");
        lv_label_set_text(ui_SP3P18HScP1SText2, "0");
        lv_label_set_text(ui_SP3P18HScP1SText3, "0");
        lv_label_set_text(ui_SP3P18HScP1SText4, "0");
        lv_label_set_text(ui_SP3P18HScP1SText5, "0");
        lv_label_set_text(ui_SP3P18HScP1SText6, "0");
        lv_label_set_text(ui_SP3P18HScP1SText7, "0");
        lv_label_set_text(ui_SP3P18HScP1SText8, "0");
        lv_label_set_text(ui_SP3P18HScP1SText9, "0");
        lv_label_set_text(ui_SP3P18HScP1SText10, "0");
        lv_label_set_text(ui_SP3P18HScP1SText11, "0");
        lv_label_set_text(ui_SP3P18HScP1SText12, "0");
        lv_label_set_text(ui_SP3P18HScP1SText13, "0");
        lv_label_set_text(ui_SP3P18HScP1SText14, "0");
        lv_label_set_text(ui_SP3P18HScP1SText15, "0");
        lv_label_set_text(ui_SP3P18HScP1SText16, "0");
        lv_label_set_text(ui_SP3P18HScP1SText17, "0");
        lv_label_set_text(ui_SP3P18HScP1SText18, "0");

        // Resetting score card UI 18 holes player 2
        lv_label_set_text(ui_SP3P18HScP2SText1, "0");
        lv_label_set_text(ui_SP3P18HScP2SText2, "0");
        lv_label_set_text(ui_SP3P18HScP2SText3, "0");
        lv_label_set_text(ui_SP3P18HScP2SText4, "0");
        lv_label_set_text(ui_SP3P18HScP2SText5, "0");
        lv_label_set_text(ui_SP3P18HScP2SText6, "0");
        lv_label_set_text(ui_SP3P18HScP2SText7, "0");
        lv_label_set_text(ui_SP3P18HScP2SText8, "0");
        lv_label_set_text(ui_SP3P18HScP2SText9, "0");
        lv_label_set_text(ui_SP3P18HScP2SText10, "0");
        lv_label_set_text(ui_SP3P18HScP2SText11, "0");
        lv_label_set_text(ui_SP3P18HScP2SText12, "0");
        lv_label_set_text(ui_SP3P18HScP2SText13, "0");
        lv_label_set_text(ui_SP3P18HScP2SText14, "0");
        lv_label_set_text(ui_SP3P18HScP2SText15, "0");
        lv_label_set_text(ui_SP3P18HScP2SText16, "0");
        lv_label_set_text(ui_SP3P18HScP2SText17, "0");
        lv_label_set_text(ui_SP3P18HScP2SText18, "0");

        // Resetting score card UI 18 holes player 3
        lv_label_set_text(ui_SP3P18HScP3SText1, "0");
        lv_label_set_text(ui_SP3P18HScP3SText2, "0");
        lv_label_set_text(ui_SP3P18HScP3SText3, "0");
        lv_label_set_text(ui_SP3P18HScP3SText4, "0");
        lv_label_set_text(ui_SP3P18HScP3SText5, "0");
        lv_label_set_text(ui_SP3P18HScP3SText6, "0");
        lv_label_set_text(ui_SP3P18HScP3SText7, "0");
        lv_label_set_text(ui_SP3P18HScP3SText8, "0");
        lv_label_set_text(ui_SP3P18HScP3SText9, "0");
        lv_label_set_text(ui_SP3P18HScP3SText10, "0");
        lv_label_set_text(ui_SP3P18HScP3SText11, "0");
        lv_label_set_text(ui_SP3P18HScP3SText12, "0");
        lv_label_set_text(ui_SP3P18HScP3SText13, "0");
        lv_label_set_text(ui_SP3P18HScP3SText14, "0");
        lv_label_set_text(ui_SP3P18HScP3SText15, "0");
        lv_label_set_text(ui_SP3P18HScP3SText16, "0");
        lv_label_set_text(ui_SP3P18HScP3SText17, "0");
        lv_label_set_text(ui_SP3P18HScP3SText18, "0");

        lv_label_set_text(ui_SP3P18HScP1STextF, "0");   // Player 1 Final score
        lv_label_set_text(ui_SP3P18HScP2SText19, "0");  // Player 2 Final score
        lv_label_set_text(ui_SP3P18HScP3SText19, "0");  // Player 3 Final score
    }
    else if (num_players == 4)
    {
        // Four-player UI reset 9 holes
        lv_label_set_text(ui_SP4P9HGSP1SPText, "0");               // Player 1 Score
        lv_obj_add_flag(ui_SP4P9HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);  // Hide crown for player 1
        lv_obj_add_flag(ui_SP4P9HScP1SCrown,
                        LV_OBJ_FLAG_HIDDEN);          // Hide crown for player 1 scorecard
        lv_label_set_text(ui_SP4P9HGSP2SPText, "0");  // Player 2 Score
        lv_obj_add_flag(ui_SP4P9HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);  // Hide player 2 scoreboard
        lv_obj_add_flag(ui_SP4P9HScP2SCrown,
                        LV_OBJ_FLAG_HIDDEN);          // Hide crown for player 2 scorecard
        lv_label_set_text(ui_SP4P9HGSP3SPText, "0");  // Player 3 Score
        lv_obj_add_flag(ui_SP4P9HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);  // Hide player 3 scoreboard
        lv_obj_add_flag(ui_SP4P9HScP3SCrown,
                        LV_OBJ_FLAG_HIDDEN);          // Hide crown for player 3 scorecard
        lv_label_set_text(ui_SP4P9HGSP4SPText, "0");  // Player 4 Score
        lv_obj_add_flag(ui_SP4P9HGSP4SCrown, LV_OBJ_FLAG_HIDDEN);  // Hide player 4 scoreboard
        lv_obj_add_flag(
            ui_SP4P9HScP4SCrown,
            LV_OBJ_FLAG_HIDDEN);  // Hide crown for player 4 scorecard9999999999999999999999999
        lv_label_set_text(ui_SP4P9HGSHCPText, "0");  // Holes
        lv_label_set_text(ui_SP4P9HGSBCPText, "0");  // Balls

        // Resetting score card UI 9 Holes
        lv_label_set_text(ui_SP4P9HScP1S1Text, "0");
        lv_label_set_text(ui_SP4P9HScP1S2Text, "0");
        lv_label_set_text(ui_SP4P9HScP1S3Text, "0");
        lv_label_set_text(ui_SP4P9HScP1S4Text, "0");
        lv_label_set_text(ui_SP4P9HScP1S5Text, "0");
        lv_label_set_text(ui_SP4P9HScP1S6Text, "0");
        lv_label_set_text(ui_SP4P9HScP1S7Text, "0");
        lv_label_set_text(ui_SP4P9HScP1S8Text, "0");
        lv_label_set_text(ui_SP4P9HScP1S9Text, "0");

        lv_label_set_text(ui_SP4P9HScP2S1Text, "0");
        lv_label_set_text(ui_SP4P9HScP2S2Text, "0");
        lv_label_set_text(ui_SP4P9HScP2S3Text, "0");
        lv_label_set_text(ui_SP4P9HScP2S4Text, "0");
        lv_label_set_text(ui_SP4P9HScP2S5Text, "0");
        lv_label_set_text(ui_SP4P9HScP2S6Text, "0");
        lv_label_set_text(ui_SP4P9HScP2S7Text, "0");
        lv_label_set_text(ui_SP4P9HScP2S8Text, "0");
        lv_label_set_text(ui_SP4P9HScP2S9Text, "0");

        lv_label_set_text(ui_SP4P9HScP3S1Text, "0");
        lv_label_set_text(ui_SP4P9HScP3S2Text, "0");
        lv_label_set_text(ui_SP4P9HScP3S3Text, "0");
        lv_label_set_text(ui_SP4P9HScP3S4Text, "0");
        lv_label_set_text(ui_SP4P9HScP3S5Text, "0");
        lv_label_set_text(ui_SP4P9HScP3S6Text, "0");
        lv_label_set_text(ui_SP4P9HScP3S7Text, "0");
        lv_label_set_text(ui_SP4P9HScP3S8Text, "0");
        lv_label_set_text(ui_SP4P9HScP3S9Text, "0");

        lv_label_set_text(ui_SP4P9HScP4S1Text, "0");
        lv_label_set_text(ui_SP4P9HScP4S2Text, "0");
        lv_label_set_text(ui_SP4P9HScP4S3Text, "0");
        lv_label_set_text(ui_SP4P9HScP4S4Text, "0");
        lv_label_set_text(ui_SP4P9HScP4S5Text, "0");
        lv_label_set_text(ui_SP4P9HScP4S6Text, "0");
        lv_label_set_text(ui_SP4P9HScP4S7Text, "0");
        lv_label_set_text(ui_SP4P9HScP4S8Text, "0");
        lv_label_set_text(ui_SP4P9HScP4S9Text, "0");

        lv_label_set_text(ui_SP4P9HScP1SFText, "0");  // Player 1 Final score
        lv_label_set_text(ui_SP4P9HScP2SFText, "0");  // Player 2 Final score
        lv_label_set_text(ui_SP4P9HScP3SFText, "0");  // Player 3 Final score
        lv_label_set_text(ui_SP4P9HScP4SFText, "0");  // Player 3 Final score

        // Four-player UI reset 18 holes
        lv_label_set_text(ui_SP4P18HGSP1SPText, "0");  // Player 1 Score
        lv_label_set_text(ui_SP4P18HGSP2SPText, "0");  // Player 2 Score
        lv_label_set_text(ui_SP4P18HGSP3SPText, "0");  // Player 3 Score
        lv_label_set_text(ui_SP4P18HGSP4SPText, "0");  // Player 4 Score
        lv_label_set_text(ui_SP4P18HGSHCPText, "0");   // Holes
        lv_label_set_text(ui_SP4P18HGSBCPText, "0");   // Balls

        // Resetting score card UI 18 Holes
        lv_label_set_text(ui_SP4P18HScP1SText1, "0");
        lv_label_set_text(ui_SP4P18HScP1SText2, "0");
        lv_label_set_text(ui_SP4P18HScP1SText3, "0");
        lv_label_set_text(ui_SP4P18HScP1SText4, "0");
        lv_label_set_text(ui_SP4P18HScP1SText5, "0");
        lv_label_set_text(ui_SP4P18HScP1SText6, "0");
        lv_label_set_text(ui_SP4P18HScP1SText7, "0");
        lv_label_set_text(ui_SP4P18HScP1SText8, "0");
        lv_label_set_text(ui_SP4P18HScP1SText9, "0");
        lv_label_set_text(ui_SP4P18HScP1SText10, "0");
        lv_label_set_text(ui_SP4P18HScP1SText11, "0");
        lv_label_set_text(ui_SP4P18HScP1SText12, "0");
        lv_label_set_text(ui_SP4P18HScP1SText13, "0");
        lv_label_set_text(ui_SP4P18HScP1SText14, "0");
        lv_label_set_text(ui_SP4P18HScP1SText15, "0");
        lv_label_set_text(ui_SP4P18HScP1SText16, "0");
        lv_label_set_text(ui_SP4P18HScP1SText17, "0");
        lv_label_set_text(ui_SP4P18HScP1SText18, "0");

        lv_label_set_text(ui_SP4P18HScP2SText1, "0");
        lv_label_set_text(ui_SP4P18HScP2SText2, "0");
        lv_label_set_text(ui_SP4P18HScP2SText3, "0");
        lv_label_set_text(ui_SP4P18HScP2SText4, "0");
        lv_label_set_text(ui_SP4P18HScP2SText5, "0");
        lv_label_set_text(ui_SP4P18HScP2SText6, "0");
        lv_label_set_text(ui_SP4P18HScP2SText7, "0");
        lv_label_set_text(ui_SP4P18HScP2SText8, "0");
        lv_label_set_text(ui_SP4P18HScP2SText9, "0");
        lv_label_set_text(ui_SP4P18HScP2SText10, "0");
        lv_label_set_text(ui_SP4P18HScP2SText11, "0");
        lv_label_set_text(ui_SP4P18HScP2SText12, "0");
        lv_label_set_text(ui_SP4P18HScP2SText13, "0");
        lv_label_set_text(ui_SP4P18HScP2SText14, "0");
        lv_label_set_text(ui_SP4P18HScP2SText15, "0");
        lv_label_set_text(ui_SP4P18HScP2SText16, "0");
        lv_label_set_text(ui_SP4P18HScP2SText17, "0");
        lv_label_set_text(ui_SP4P18HScP2SText18, "0");

        lv_label_set_text(ui_SP4P18HScP3SText1, "0");
        lv_label_set_text(ui_SP4P18HScP3SText2, "0");
        lv_label_set_text(ui_SP4P18HScP3SText3, "0");
        lv_label_set_text(ui_SP4P18HScP3SText4, "0");
        lv_label_set_text(ui_SP4P18HScP3SText5, "0");
        lv_label_set_text(ui_SP4P18HScP3SText6, "0");
        lv_label_set_text(ui_SP4P18HScP3SText7, "0");
        lv_label_set_text(ui_SP4P18HScP3SText8, "0");
        lv_label_set_text(ui_SP4P18HScP3SText9, "0");
        lv_label_set_text(ui_SP4P18HScP3SText10, "0");
        lv_label_set_text(ui_SP4P18HScP3SText11, "0");
        lv_label_set_text(ui_SP4P18HScP3SText12, "0");
        lv_label_set_text(ui_SP4P18HScP3SText13, "0");
        lv_label_set_text(ui_SP4P18HScP3SText14, "0");
        lv_label_set_text(ui_SP4P18HScP3SText15, "0");
        lv_label_set_text(ui_SP4P18HScP3SText16, "0");
        lv_label_set_text(ui_SP4P18HScP3SText17, "0");
        lv_label_set_text(ui_SP4P18HScP3SText18, "0");

        lv_label_set_text(ui_SP4P18HScP4SText1, "0");
        lv_label_set_text(ui_SP4P18HScP4SText2, "0");
        lv_label_set_text(ui_SP4P18HScP4SText3, "0");
        lv_label_set_text(ui_SP4P18HScP4SText4, "0");
        lv_label_set_text(ui_SP4P18HScP4SText5, "0");
        lv_label_set_text(ui_SP4P18HScP4SText6, "0");
        lv_label_set_text(ui_SP4P18HScP4SText7, "0");
        lv_label_set_text(ui_SP4P18HScP4SText8, "0");
        lv_label_set_text(ui_SP4P18HScP4SText9, "0");
        lv_label_set_text(ui_SP4P18HScP4SText10, "0");
        lv_label_set_text(ui_SP4P18HScP4SText11, "0");
        lv_label_set_text(ui_SP4P18HScP4SText12, "0");
        lv_label_set_text(ui_SP4P18HScP4SText13, "0");
        lv_label_set_text(ui_SP4P18HScP4SText14, "0");
        lv_label_set_text(ui_SP4P18HScP4SText15, "0");
        lv_label_set_text(ui_SP4P18HScP4SText16, "0");
        lv_label_set_text(ui_SP4P18HScP4SText17, "0");
        lv_label_set_text(ui_SP4P18HScP4SText18, "0");

        lv_label_set_text(ui_SP4P18HScP1STextF, "0");  // Player 1 Final score
        lv_label_set_text(ui_SP4P18HScP2STextF, "0");  // Player 2 Final score
        lv_label_set_text(ui_SP4P18HScP3STextF, "0");  // Player 3 Final score
        lv_label_set_text(ui_SP4P18HScP4STextF, "0");  // Player 4 Final score
    }

    // Reset global variables
    current_player_index = 0;
    update_flag          = 0;
    highliting_counter   = 0;

    // Restting Highlighting for 9 holes, Stroke play, 2, 3 , 5players
    printf("Restting Highligting highlight, By Setting Player 1 always Highlighted \n");
    lv_obj_set_style_bg_color(ui_SP1P9HGSPSPanel, lv_color_hex(COLOR_1),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_SP2P9HGSP1SPanel, lv_color_hex(COLOR_1),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_SP2P9HGSP2SPanel, lv_color_hex(COLOR_2),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_SP3P9HGSP1SPanel, lv_color_hex(COLOR_1),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_SP3P9HGSP2SPanel, lv_color_hex(COLOR_2),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_SP3P9HGSP3SPanel, lv_color_hex(COLOR_2),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_SP4P9HGSP1SPanel, lv_color_hex(COLOR_1),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_SP4P9HGSP2SPanel, lv_color_hex(COLOR_2),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_SP4P9HGSP3SPanel, lv_color_hex(COLOR_2),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_SP4P9HGSP4SPanel, lv_color_hex(COLOR_2),
                              LV_PART_MAIN | LV_STATE_DEFAULT);

    // ------------------------------
    // Single Player (18 holes)
    // ------------------------------
    lv_obj_set_style_bg_color(ui_SP1P18HGSPSPanel, lv_color_hex(COLOR_1),
                              LV_PART_MAIN | LV_STATE_DEFAULT);

    // ------------------------------
    // Two Players (18 holes)
    // ------------------------------
    lv_obj_set_style_bg_color(ui_SP2P18HGSP1SPanel, lv_color_hex(COLOR_1),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_SP2P18HGSP2SPanel, lv_color_hex(COLOR_2),
                              LV_PART_MAIN | LV_STATE_DEFAULT);

    // ------------------------------
    // Three Players (18 holes)
    // ------------------------------
    lv_obj_set_style_bg_color(ui_SP3P18HGSP1SPanel, lv_color_hex(COLOR_1),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_SP3P18HGSP2SPanel, lv_color_hex(COLOR_2),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_SP3P18HGSP3SPanel, lv_color_hex(COLOR_2),
                              LV_PART_MAIN | LV_STATE_DEFAULT);

    // ------------------------------
    // Four Players (18 holes)
    // ------------------------------
    lv_obj_set_style_bg_color(ui_SP4P18HGSP1SPanel, lv_color_hex(COLOR_1),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_SP4P18HGSP2SPanel, lv_color_hex(COLOR_2),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_SP4P18HGSP3SPanel, lv_color_hex(COLOR_2),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_SP4P18HGSP4SPanel, lv_color_hex(COLOR_2),
                              LV_PART_MAIN | LV_STATE_DEFAULT);

    // Restting Highlighting for 9 holes, Match play
    lv_obj_set_style_bg_color(ui_MP1V19HGSP1SPanel, lv_color_hex(COLOR_1),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_MP1V19HGSP2SPanel, lv_color_hex(COLOR_2),
                              LV_PART_MAIN | LV_STATE_DEFAULT);

    // Restting Highlighting for 18 holes, Match play
    lv_obj_set_style_bg_color(ui_MP1V118HGSP1SPanel, lv_color_hex(COLOR_1),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_MP1V118HGSP2SPanel, lv_color_hex(COLOR_2),
                              LV_PART_MAIN | LV_STATE_DEFAULT);

    // Restting Highlighting for 2v2 9 holes, Match play
    lv_obj_set_style_bg_color(ui_MP2V29HGST1SPanel, lv_color_hex(COLOR_1),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_MP2V29HGST2SPanel, lv_color_hex(COLOR_2),
                              LV_PART_MAIN | LV_STATE_DEFAULT);

    // Restting Highlighting for 2v2 18 holes, Match play
    lv_obj_set_style_bg_color(ui_MP2V218HGST1SPanel, lv_color_hex(COLOR_1),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_MP2V218HGST2SPanel, lv_color_hex(COLOR_2),
                              LV_PART_MAIN | LV_STATE_DEFAULT);

    PRINT_SENSORS_STATUS
}

void update_scoreCard(uint8_t player_index, uint8_t cumulative_score, uint8_t current_hole)
{
    // Check current_hole bounds
    if (current_hole < 0 || current_hole > current_hole_mode)
    {
        DEBUG_ERROR(MODULE_LOGIC, "Invalid hole number: %d (max allowed: %d)", current_hole,
                    current_hole_mode);
        return;
    }
    // Calculate the individual score for the current hole
    DEBUG_TRACE(MODULE_LOGIC, "Calculating the individual score for the current hole");
    uint8_t individual_score;
    if (current_hole == 0)
    {
        // For the first hole, the previous score is 0
        DEBUG_TRACE(MODULE_LOGIC, "For the first hole, the previous score is 0");
        individual_score = cumulative_score;
    }
    else
    {
        // For subsequent holes, subtract the previous cumulative score
        DEBUG_TRACE(MODULE_LOGIC, "For subsequent holes, subtract the previous cumulative score");
        individual_score = cumulative_score - prev_scores[player_index][current_hole - 1];
    }

    // Store the individual score
    DEBUG_DEBUG(MODULE_LOGIC, "Storing the individual score: %d for player %d, hole %d",
                individual_score, player_index + 1, current_hole + 1);
    individual_scores[player_index][current_hole] = individual_score;

    // Print debug information before updating prev_scores
    DEBUG_DEBUG(MODULE_LOGIC, "Player %d, Hole %d:", player_index + 1, current_hole + 1);
    if (current_hole == 0)
    {
        DEBUG_TRACE(MODULE_LOGIC, "Previous Score: None (First Hole)");
    }
    else
    {
        DEBUG_TRACE(MODULE_LOGIC, "Previous Score: %d",
                    prev_scores[player_index][current_hole - 1]);
    }
    DEBUG_DEBUG(MODULE_LOGIC, "Cumulative Score Received: %d", cumulative_score);
    DEBUG_DEBUG(MODULE_LOGIC, "Individual Score Calculated: %d", individual_score);

    // Update the previous cumulative score for this hole
    prev_scores[player_index][current_hole] = cumulative_score;
    DEBUG_TRACE(MODULE_LOGIC, "Updated Previous Score: %d",
                prev_scores[player_index][current_hole]);

    // Update the scorecard UI for the current hole (individual score)
    if (num_players == 1 && current_hole_mode == NINE_HOLES)
    {
        switch (current_hole)
        {
            case 0:
                DEBUG_DEBUG(MODULE_UI, "Updating UI for Hole 1 with Individual Score: %d",
                            individual_score);
                lv_label_set_text_fmt(ui_SP1P9HScPS1Text, "%d", individual_score);
                // lv_label_set_text_fmt(ui_SP1P18HScPSText1, "%d", individual_score);
                break;
            case 1:
                DEBUG_DEBUG(MODULE_UI, "Updating UI for Hole 2 with Individual Score: %d",
                            individual_score);
                lv_label_set_text_fmt(ui_SP1P9HScPS2Text, "%d", individual_score);
                // lv_label_set_text_fmt(ui_SP1P18HScPSText2, "%d", individual_score);
                break;
            case 2:
                DEBUG_DEBUG(MODULE_UI, "Updating UI for Hole 3 with Individual Score: %d",
                            individual_score);
                lv_label_set_text_fmt(ui_SP1P9HScPS3Text, "%d", individual_score);
                break;
            case 3:
                DEBUG_DEBUG(MODULE_UI, "Updating UI for Hole 4 with Individual Score: %d",
                            individual_score);
                lv_label_set_text_fmt(ui_SP1P9HScPS4Text, "%d", individual_score);
                break;
            case 4:
                DEBUG_DEBUG(MODULE_UI, "Updating UI for Hole 5 with Individual Score: %d",
                            individual_score);
                lv_label_set_text_fmt(ui_SP1P9HScPS5Text, "%d", individual_score);
                break;
            case 5:
                DEBUG_DEBUG(MODULE_UI, "Updating UI for Hole 6 with Individual Score: %d",
                            individual_score);
                lv_label_set_text_fmt(ui_SP1P9HScPS6Text, "%d", individual_score);

                break;
            case 6:
                DEBUG_DEBUG(MODULE_UI, "Updating UI for Hole 7 with Individual Score: %d",
                            individual_score);
                lv_label_set_text_fmt(ui_SP1P9HScPS7Text, "%d", individual_score);
                break;
            case 7:
                DEBUG_DEBUG(MODULE_UI, "Updating UI for Hole 8 with Individual Score: %d",
                            individual_score);
                lv_label_set_text_fmt(ui_SP1P9HScPS8Text, "%d", individual_score);
                break;
            case 8:
                DEBUG_DEBUG(MODULE_UI, "Updating UI for Hole 9 with Individual Score: %d",
                            individual_score);
                lv_label_set_text_fmt(ui_SP1P9HScPS9Text, "%d", individual_score);
                break;
            default:
                DEBUG_ERROR(MODULE_UI, "Invalid hole number: %d", current_hole);
                break;
        }
    }
    else if (num_players == 1 && current_hole_mode == EIGHTEEN_HOLES)
    {
        switch (current_hole)
        {
            case 0:
                DEBUG_DEBUG(MODULE_UI, "Updating UI for Hole 1 with Individual Score: %d",
                            individual_score);
                lv_label_set_text_fmt(ui_SP1P18HScPSText1, "%d", individual_score);
                break;
            case 1:
                DEBUG_DEBUG(MODULE_UI, "Updating UI for Hole 2 with Individual Score: %d",
                            individual_score);
                lv_label_set_text_fmt(ui_SP1P18HScPSText2, "%d", individual_score);
                break;
            case 2:
                DEBUG_DEBUG(MODULE_UI, "Updating UI for Hole 3 with Individual Score: %d",
                            individual_score);
                lv_label_set_text_fmt(ui_SP1P18HScPSText3, "%d", individual_score);
                break;
            case 3:
                DEBUG_DEBUG(MODULE_UI, "Updating UI for Hole 4 with Individual Score: %d",
                            individual_score);
                lv_label_set_text_fmt(ui_SP1P18HScPSText4, "%d", individual_score);
                break;
            case 4:
                DEBUG_DEBUG(MODULE_UI, "Updating UI for Hole 5 with Individual Score: %d",
                            individual_score);
                lv_label_set_text_fmt(ui_SP1P18HScPSText5, "%d", individual_score);
                break;
            case 5:
                DEBUG_DEBUG(MODULE_UI, "Updating UI for Hole 6 with Individual Score: %d",
                            individual_score);
                lv_label_set_text_fmt(ui_SP1P18HScPSText6, "%d", individual_score);

                break;
            case 6:
                DEBUG_DEBUG(MODULE_UI, "Updating UI for Hole 7 with Individual Score: %d",
                            individual_score);
                lv_label_set_text_fmt(ui_SP1P18HScPSText7, "%d", individual_score);
                break;
            case 7:
                DEBUG_DEBUG(MODULE_UI, "Updating UI for Hole 8 with Individual Score: %d",
                            individual_score);
                lv_label_set_text_fmt(ui_SP1P18HScPSText8, "%d", individual_score);
                break;
            case 8:
                DEBUG_DEBUG(MODULE_UI, "Updating UI for Hole 9 with Individual Score: %d",
                            individual_score);
                lv_label_set_text_fmt(ui_SP1P18HScPSText9, "%d", individual_score);
                break;
            case 9:
                DEBUG_DEBUG(MODULE_UI, "Updating UI for Hole 10 with Individual Score: %d",
                            individual_score);
                lv_label_set_text_fmt(ui_SP1P18HScPSText10, "%d", individual_score);
                break;
            case 10:
                DEBUG_DEBUG(MODULE_UI, "Updating UI for Hole 11 with Individual Score: %d",
                            individual_score);
                lv_label_set_text_fmt(ui_SP1P18HScPSText11, "%d", individual_score);
                break;
            case 11:
                DEBUG_DEBUG(MODULE_UI, "Updating UI for Hole 12 with Individual Score: %d",
                            individual_score);
                lv_label_set_text_fmt(ui_SP1P18HScPSText12, "%d", individual_score);
                break;
            case 12:
                DEBUG_DEBUG(MODULE_UI, "Updating UI for Hole 13 with Individual Score: %d",
                            individual_score);
                lv_label_set_text_fmt(ui_SP1P18HScPSText13, "%d", individual_score);
                break;
            case 13:
                DEBUG_DEBUG(MODULE_UI, "Updating UI for Hole 14 with Individual Score: %d",
                            individual_score);
                lv_label_set_text_fmt(ui_SP1P18HScPSText14, "%d", individual_score);
                break;
            case 14:
                DEBUG_DEBUG(MODULE_UI, "Updating UI for Hole 15 with Individual Score: %d",
                            individual_score);
                lv_label_set_text_fmt(ui_SP1P18HScPSText15, "%d", individual_score);
                break;
            case 15:
                DEBUG_DEBUG(MODULE_UI, "Updating UI for Hole 16 with Individual Score: %d",
                            individual_score);
                lv_label_set_text_fmt(ui_SP1P18HScPSText16, "%d", individual_score);
                break;
            case 16:
                DEBUG_DEBUG(MODULE_UI, "Updating UI for Hole 17 with Individual Score: %d",
                            individual_score);
                lv_label_set_text_fmt(ui_SP1P18HScPSText17, "%d", individual_score);
                break;
            case 17:
                DEBUG_DEBUG(MODULE_UI, "Updating UI for Hole 18 with Individual Score: %d",
                            individual_score);
                lv_label_set_text_fmt(ui_SP1P18HScPSText18, "%d", individual_score);
                break;
            default:
                DEBUG_ERROR(MODULE_UI, "Invalid hole number: %d", current_hole);
                break;
        }
    }
    else if (num_players == 2 && current_hole_mode == NINE_HOLES)
    {
        if (player_index == 0)
        {
            switch (current_hole)
            {
                case 0:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 1 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P9HScP1S1Text, "%d", individual_score);
                    break;
                case 1:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 2 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P9HScP1S2Text, "%d", individual_score);
                    break;
                case 2:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 3 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P9HScP1S3Text, "%d", individual_score);
                    break;
                case 3:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 4 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P9HScP1S4Text, "%d", individual_score);
                    break;
                case 4:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 5 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P9HScP1S5Text, "%d", individual_score);
                    break;
                case 5:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 6 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P9HScP1S6Text, "%d", individual_score);
                    break;
                case 6:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 7 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P9HScP1S7Text, "%d", individual_score);
                    break;
                case 7:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 8 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P9HScP1S8Text, "%d", individual_score);
                    break;
                case 8:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 9 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P9HScP1S9Text, "%d", individual_score);
                    break;
                default:
                    DEBUG_ERROR(MODULE_UI, "Invalid hole number: %d", current_hole);
                    break;
            }
        }
        else if (player_index == 1)
        {
            switch (current_hole)
            {
                case 0:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 1 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P9HScP2S1Text, "%d", individual_score);
                    break;
                case 1:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 2 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P9HScP2S2Text, "%d", individual_score);
                    break;
                case 2:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 3 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P9HScP2S3Text, "%d", individual_score);
                    break;
                case 3:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 4 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P9HScP2S4Text, "%d", individual_score);
                    break;
                case 4:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 5 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P9HScP2S5Text, "%d", individual_score);
                    break;
                case 5:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 6 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P9HScP2S6Text, "%d", individual_score);
                    break;
                case 6:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 7 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P9HScP2S7Text, "%d", individual_score);
                    break;
                case 7:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 8 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P9HScP2S8Text, "%d", individual_score);
                    break;
                case 8:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 9 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P9HScP2S9Text, "%d", individual_score);
                    break;
                default:
                    DEBUG_ERROR(MODULE_UI, "Invalid hole number: %d", current_hole);
                    break;
            }
        }
    }
    else if (num_players == 2 && current_hole_mode == EIGHTEEN_HOLES)
    {
        if (player_index == 0)
        {
            switch (current_hole)
            {
                case 0:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 1 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP1SText1, "%d", individual_score);
                    break;
                case 1:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 2 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP1SText2, "%d", individual_score);
                    break;
                case 2:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 3 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP1SText3, "%d", individual_score);
                    break;
                case 3:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 4 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP1SText4, "%d", individual_score);
                    break;
                case 4:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 5 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP1SText5, "%d", individual_score);
                    break;
                case 5:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 6 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP1SText6, "%d", individual_score);
                    break;
                case 6:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 7 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP1SText7, "%d", individual_score);
                    break;
                case 7:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 8 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP1SText8, "%d", individual_score);
                    break;
                case 8:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 9 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP1SText9, "%d", individual_score);
                    break;
                case 9:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 10 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP1SText10, "%d", individual_score);
                    break;
                case 10:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 11 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP1SText11, "%d", individual_score);
                    break;
                case 11:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 12 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP1SText12, "%d", individual_score);
                    break;
                case 12:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 13 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP1SText13, "%d", individual_score);
                    break;
                case 13:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 14 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP1SText14, "%d", individual_score);
                    break;
                case 14:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 15 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP1SText15, "%d", individual_score);
                    break;
                case 15:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 16 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP1SText16, "%d", individual_score);
                    break;
                case 16:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 17 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP1SText17, "%d", individual_score);
                    break;
                case 17:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 18 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP1SText18, "%d", individual_score);
                    break;
                default:
                    DEBUG_ERROR(MODULE_UI, "Invalid hole number: %d", current_hole);
                    break;
            }
        }
        else if (player_index == 1)
        {
            switch (current_hole)
            {
                case 0:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 1 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP2SText1, "%d", individual_score);
                    break;
                case 1:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 2 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP2SText2, "%d", individual_score);
                    break;
                case 2:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 3 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP2SText3, "%d", individual_score);
                    break;
                case 3:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 4 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP2SText4, "%d", individual_score);
                    break;
                case 4:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 5 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP2SText5, "%d", individual_score);
                    break;
                case 5:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 6 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP2SText6, "%d", individual_score);
                    break;
                case 6:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 7 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP2SText7, "%d", individual_score);
                    break;
                case 7:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 8 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP2SText8, "%d", individual_score);
                    break;
                case 8:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 9 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP2SText9, "%d", individual_score);
                    break;
                case 9:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 10 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP2SText10, "%d", individual_score);
                    break;
                case 10:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 11 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP2SText11, "%d", individual_score);
                    break;
                case 11:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 12 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP2SText12, "%d", individual_score);
                    break;
                case 12:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 13 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP2SText13, "%d", individual_score);
                    break;
                case 13:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 14 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP2SText14, "%d", individual_score);
                    break;
                case 14:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 15 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP2SText15, "%d", individual_score);
                    break;
                case 15:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 16 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP2SText16, "%d", individual_score);
                    break;
                case 16:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 17 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP2SText17, "%d", individual_score);
                    break;
                case 17:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 18 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP2P18HScP2SText18, "%d", individual_score);
                    break;
                default:
                    DEBUG_ERROR(MODULE_UI, "Invalid hole number: %d", current_hole);
                    break;
            }
        }
    }
    else if (num_players == 3 && current_hole_mode == NINE_HOLES)
    {
        if (player_index == 0)
        {
            switch (current_hole)
            {
                case 0:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 1 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P9HScP1S1Text, "%d", individual_score);
                    break;
                case 1:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 2 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P9HScP1S2Text, "%d", individual_score);
                    break;
                case 2:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 3 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P9HScP1S3Text, "%d", individual_score);
                    break;
                case 3:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 4 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P9HScP1S4Text, "%d", individual_score);
                    break;
                case 4:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 5 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P9HScP1S5Text, "%d", individual_score);
                    break;
                case 5:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 6 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P9HScP1S6Text, "%d", individual_score);
                    break;
                case 6:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 7 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P9HScP1S7Text, "%d", individual_score);
                    break;
                case 7:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 8 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P9HScP1S8Text, "%d", individual_score);
                    break;
                case 8:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 9 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P9HScP1S9Text, "%d", individual_score);
                    break;
                default:
                    DEBUG_ERROR(MODULE_UI, "Invalid hole number: %d", current_hole);
                    break;
            }
        }
        else if (player_index == 1)
        {
            switch (current_hole)
            {
                case 0:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 1 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P9HScP2S1Text, "%d", individual_score);
                    break;
                case 1:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 2 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P9HScP2S2Text, "%d", individual_score);
                    break;
                case 2:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 3 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P9HScP2S3Text, "%d", individual_score);
                    break;
                case 3:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 4 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P9HScP2S4Text, "%d", individual_score);
                    break;
                case 4:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 5 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P9HScP2S5Text, "%d", individual_score);
                    break;
                case 5:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 6 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P9HScP2S6Text, "%d", individual_score);
                    break;
                case 6:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 7 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P9HScP2S7Text, "%d", individual_score);
                    break;
                case 7:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 8 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P9HScP2S8Text, "%d", individual_score);
                    break;
                case 8:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 9 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P9HScP2S9Text, "%d", individual_score);
                    break;
                default:
                    DEBUG_ERROR(MODULE_UI, "Invalid hole number: %d", current_hole);
                    break;
            }
        }
        else if (player_index == 2)
        {
            switch (current_hole)
            {
                case 0:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 1 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P9HScP3S1Text, "%d", individual_score);
                    break;
                case 1:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 2 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P9HScP3S2Text, "%d", individual_score);
                    break;
                case 2:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 3 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P9HScP3S3Text, "%d", individual_score);
                    break;
                case 3:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 4 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P9HScP3S4Text, "%d", individual_score);
                    break;
                case 4:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 5 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P9HScP3S5Text, "%d", individual_score);
                    break;
                case 5:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 6 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P9HScP3S6Text, "%d", individual_score);
                    break;
                case 6:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 7 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P9HScP3S7Text, "%d", individual_score);
                    break;
                case 7:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 8 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P9HScP3S8Text, "%d", individual_score);
                    break;
                case 8:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 9 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P9HScP3S9Text, "%d", individual_score);
                    break;
                default:
                    DEBUG_ERROR(MODULE_UI, "Invalid hole number: %d", current_hole);
                    break;
            }
        }
    }
    else if (num_players == 3 && current_hole_mode == EIGHTEEN_HOLES)
    {
        if (player_index == 0)
        {
            switch (current_hole)
            {
                case 0:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 1 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP1SText1, "%d", individual_score);
                    break;
                case 1:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 2 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP1SText2, "%d", individual_score);
                    break;
                case 2:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 3 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP1SText3, "%d", individual_score);
                    break;
                case 3:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 4 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP1SText4, "%d", individual_score);
                    break;
                case 4:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 5 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP1SText5, "%d", individual_score);
                    break;
                case 5:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 6 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP1SText6, "%d", individual_score);
                    break;
                case 6:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 7 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP1SText7, "%d", individual_score);
                    break;
                case 7:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 8 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP1SText8, "%d", individual_score);
                    break;
                case 8:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 9 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP1SText9, "%d", individual_score);
                    break;
                case 9:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 10 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP1SText10, "%d", individual_score);
                    break;
                case 10:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 11 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP1SText11, "%d", individual_score);
                    break;
                case 11:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 12 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP1SText12, "%d", individual_score);
                    break;
                case 12:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 13 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP1SText13, "%d", individual_score);
                    break;
                case 13:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 14 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP1SText14, "%d", individual_score);
                    break;
                case 14:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 15 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP1SText15, "%d", individual_score);
                    break;
                case 15:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 16 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP1SText16, "%d", individual_score);
                    break;
                case 16:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 17 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP1SText17, "%d", individual_score);
                    break;
                case 17:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 18 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP1SText18, "%d", individual_score);
                    break;
                default:
                    DEBUG_ERROR(MODULE_UI, "Invalid hole number: %d", current_hole);
                    break;
            }
        }
        else if (player_index == 1)
        {
            switch (current_hole)
            {
                case 0:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 1 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP2SText1, "%d", individual_score);
                    break;
                case 1:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 2 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP2SText2, "%d", individual_score);
                    break;
                case 2:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 3 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP2SText3, "%d", individual_score);
                    break;
                case 3:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 4 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP2SText4, "%d", individual_score);
                    break;
                case 4:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 5 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP2SText5, "%d", individual_score);
                    break;
                case 5:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 6 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP2SText6, "%d", individual_score);
                    break;
                case 6:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 7 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP2SText7, "%d", individual_score);
                    break;
                case 7:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 8 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP2SText8, "%d", individual_score);
                    break;
                case 8:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 9 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP2SText9, "%d", individual_score);
                    break;
                case 9:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 10 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP2SText10, "%d", individual_score);
                    break;
                case 10:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 11 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP2SText11, "%d", individual_score);
                    break;
                case 11:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 12 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP2SText12, "%d", individual_score);
                    break;
                case 12:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 13 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP2SText13, "%d", individual_score);
                    break;
                case 13:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 14 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP2SText14, "%d", individual_score);
                    break;
                case 14:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 15 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP2SText15, "%d", individual_score);
                    break;
                case 15:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 16 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP2SText16, "%d", individual_score);
                    break;
                case 16:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 17 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP2SText17, "%d", individual_score);
                    break;
                case 17:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 18 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP2SText18, "%d", individual_score);
                    break;
                default:
                    DEBUG_ERROR(MODULE_UI, "Invalid hole number: %d", current_hole);
                    break;
            }
        }
        else if (player_index == 2)
        {
            switch (current_hole)
            {
                case 0:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 1 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP3SText1, "%d", individual_score);
                    break;
                case 1:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 2 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP3SText2, "%d", individual_score);
                    break;
                case 2:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 3 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP3SText3, "%d", individual_score);
                    break;
                case 3:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 4 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP3SText4, "%d", individual_score);
                    break;
                case 4:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 5 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP3SText5, "%d", individual_score);
                    break;
                case 5:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 6 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP3SText6, "%d", individual_score);
                    break;
                case 6:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 7 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP3SText7, "%d", individual_score);
                    break;
                case 7:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 8 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP3SText8, "%d", individual_score);
                    break;
                case 8:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 9 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP3SText9, "%d", individual_score);
                    break;
                case 9:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 10 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP3SText10, "%d", individual_score);
                    break;
                case 10:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 11 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP3SText11, "%d", individual_score);
                    break;
                case 11:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 12 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP3SText12, "%d", individual_score);
                    break;
                case 12:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 13 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP3SText13, "%d", individual_score);
                    break;
                case 13:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 14 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP3SText14, "%d", individual_score);
                    break;
                case 14:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 15 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP3SText15, "%d", individual_score);
                    break;
                case 15:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 16 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP3SText16, "%d", individual_score);
                    break;
                case 16:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 17 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP3SText17, "%d", individual_score);
                    break;
                case 17:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 18 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP3P18HScP3SText18, "%d", individual_score);
                    break;
                default:
                    DEBUG_ERROR(MODULE_UI, "Invalid hole number: %d", current_hole);
                    break;
            }
        }
    }
    else if (num_players == 4 && current_hole_mode == NINE_HOLES)
    {
        if (player_index == 0)
        {
            switch (current_hole)
            {
                case 0:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 1 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP1S1Text, "%d", individual_score);
                    break;
                case 1:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 2 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP1S2Text, "%d", individual_score);
                    break;
                case 2:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 3 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP1S3Text, "%d", individual_score);
                    break;
                case 3:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 4 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP1S4Text, "%d", individual_score);
                    break;
                case 4:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 5 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP1S5Text, "%d", individual_score);
                    break;
                case 5:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 6 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP1S6Text, "%d", individual_score);
                    break;
                case 6:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 7 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP1S7Text, "%d", individual_score);
                    break;
                case 7:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 8 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP1S8Text, "%d", individual_score);
                    break;
                case 8:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 9 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP1S9Text, "%d", individual_score);
                    break;
                default:
                    DEBUG_ERROR(MODULE_UI, "Invalid hole number: %d", current_hole);
                    break;
            }
        }
        else if (player_index == 1)
        {
            switch (current_hole)
            {
                case 0:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 1 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP2S1Text, "%d", individual_score);
                    break;
                case 1:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 2 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP2S2Text, "%d", individual_score);
                    break;
                case 2:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 3 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP2S3Text, "%d", individual_score);
                    break;
                case 3:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 4 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP2S4Text, "%d", individual_score);
                    break;
                case 4:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 5 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP2S5Text, "%d", individual_score);
                    break;
                case 5:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 6 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP2S6Text, "%d", individual_score);
                    break;
                case 6:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 7 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP2S7Text, "%d", individual_score);
                    break;
                case 7:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 8 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP2S8Text, "%d", individual_score);
                    break;
                case 8:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 9 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP2S9Text, "%d", individual_score);
                    break;
                default:
                    DEBUG_ERROR(MODULE_UI, "Invalid hole number: %d", current_hole);
                    break;
            }
        }
        else if (player_index == 2)
        {
            switch (current_hole)
            {
                case 0:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 1 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP3S1Text, "%d", individual_score);
                    break;
                case 1:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 2 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP3S2Text, "%d", individual_score);
                    break;
                case 2:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 3 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP3S3Text, "%d", individual_score);
                    break;
                case 3:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 4 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP3S4Text, "%d", individual_score);
                    break;
                case 4:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 5 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP3S5Text, "%d", individual_score);
                    break;
                case 5:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 6 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP3S6Text, "%d", individual_score);
                    break;
                case 6:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 7 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP3S7Text, "%d", individual_score);
                    break;
                case 7:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 8 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP3S8Text, "%d", individual_score);
                    break;
                case 8:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 9 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP3S9Text, "%d", individual_score);
                    break;
                default:
                    DEBUG_ERROR(MODULE_UI, "Invalid hole number: %d", current_hole);
                    break;
            }
        }
        else if (player_index == 3)
        {
            switch (current_hole)
            {
                case 0:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 4's UI for Hole 1 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP4S1Text, "%d", individual_score);
                    break;
                case 1:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 4's UI for Hole 2 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP4S2Text, "%d", individual_score);
                    break;
                case 2:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 4's UI for Hole 3 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP4S3Text, "%d", individual_score);
                    break;
                case 3:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 4's UI for Hole 4 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP4S4Text, "%d", individual_score);
                    break;
                case 4:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 4's UI for Hole 5 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP4S5Text, "%d", individual_score);
                    break;
                case 5:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 4's UI for Hole 6 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP4S6Text, "%d", individual_score);
                    break;
                case 6:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 4's UI for Hole 7 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP4S7Text, "%d", individual_score);
                    break;
                case 7:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 4's UI for Hole 8 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP4S8Text, "%d", individual_score);
                    break;
                case 8:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 4's UI for Hole 9 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P9HScP4S9Text, "%d", individual_score);
                    break;
                default:
                    DEBUG_ERROR(MODULE_UI, "Invalid hole number: %d", current_hole);
                    break;
            }
        }
    }
    else if (num_players == 4 && current_hole_mode == EIGHTEEN_HOLES)
    {
        if (player_index == 0)
        {
            switch (current_hole)
            {
                case 0:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 1 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP1SText1, "%d", individual_score);
                    break;
                case 1:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 2 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP1SText2, "%d", individual_score);
                    break;
                case 2:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 3 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP1SText3, "%d", individual_score);
                    break;
                case 3:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 4 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP1SText4, "%d", individual_score);
                    break;
                case 4:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 5 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP1SText5, "%d", individual_score);
                    break;
                case 5:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 6 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP1SText6, "%d", individual_score);
                    break;
                case 6:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 7 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP1SText7, "%d", individual_score);
                    break;
                case 7:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 8 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP1SText8, "%d", individual_score);
                    break;
                case 8:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 9 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP1SText9, "%d", individual_score);
                    break;
                case 9:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 10 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP1SText10, "%d", individual_score);
                    break;
                case 10:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 11 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP1SText11, "%d", individual_score);
                    break;
                case 11:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 12 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP1SText12, "%d", individual_score);
                    break;
                case 12:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 13 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP1SText13, "%d", individual_score);
                    break;
                case 13:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 14 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP1SText14, "%d", individual_score);
                    break;
                case 14:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 15 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP1SText15, "%d", individual_score);
                    break;
                case 15:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 16 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP1SText16, "%d", individual_score);
                    break;
                case 16:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 17 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP1SText17, "%d", individual_score);
                    break;
                case 17:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 1's UI for Hole 18 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP1SText18, "%d", individual_score);
                    break;
                default:
                    DEBUG_ERROR(MODULE_UI, "Invalid hole number: %d", current_hole);
                    break;
            }
        }
        else if (player_index == 1)
        {
            switch (current_hole)
            {
                case 0:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 1 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP2SText1, "%d", individual_score);
                    break;
                case 1:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 2 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP2SText2, "%d", individual_score);
                    break;
                case 2:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 3 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP2SText3, "%d", individual_score);
                    break;
                case 3:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 4 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP2SText4, "%d", individual_score);
                    break;
                case 4:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 5 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP2SText5, "%d", individual_score);
                    break;
                case 5:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 6 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP2SText6, "%d", individual_score);
                    break;
                case 6:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 7 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP2SText7, "%d", individual_score);
                    break;
                case 7:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 8 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP2SText8, "%d", individual_score);
                    break;
                case 8:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 9 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP2SText9, "%d", individual_score);
                    break;
                case 9:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 10 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP2SText10, "%d", individual_score);
                    break;
                case 10:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 11 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP2SText11, "%d", individual_score);
                    break;
                case 11:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 12 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP2SText12, "%d", individual_score);
                    break;
                case 12:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 13 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP2SText13, "%d", individual_score);
                    break;
                case 13:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 14 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP2SText14, "%d", individual_score);
                    break;
                case 14:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 15 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP2SText15, "%d", individual_score);
                    break;
                case 15:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 16 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP2SText16, "%d", individual_score);
                    break;
                case 16:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 17 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP2SText17, "%d", individual_score);
                    break;
                case 17:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 2's UI for Hole 18 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP2SText18, "%d", individual_score);
                    break;
                default:
                    DEBUG_ERROR(MODULE_UI, "Invalid hole number: %d", current_hole);
                    break;
            }
        }
        else if (player_index == 2)
        {
            switch (current_hole)
            {
                case 0:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 1 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP3SText1, "%d", individual_score);
                    break;
                case 1:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 2 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP3SText2, "%d", individual_score);
                    break;
                case 2:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 3 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP3SText3, "%d", individual_score);
                    break;
                case 3:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 4 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP3SText4, "%d", individual_score);
                    break;
                case 4:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 5 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP3SText5, "%d", individual_score);
                    break;
                case 5:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 6 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP3SText6, "%d", individual_score);
                    break;
                case 6:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 7 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP3SText7, "%d", individual_score);
                    break;
                case 7:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 8 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP3SText8, "%d", individual_score);
                    break;
                case 8:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 9 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP3SText9, "%d", individual_score);
                    break;
                case 9:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 10 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP3SText10, "%d", individual_score);
                    break;
                case 10:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 11 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP3SText11, "%d", individual_score);
                    break;
                case 11:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 12 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP3SText12, "%d", individual_score);
                    break;
                case 12:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 13 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP3SText13, "%d", individual_score);
                    break;
                case 13:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 14 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP3SText14, "%d", individual_score);
                    break;
                case 14:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 15 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP3SText15, "%d", individual_score);
                    break;
                case 15:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 16 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP3SText16, "%d", individual_score);
                    break;
                case 16:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 17 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP3SText17, "%d", individual_score);
                    break;
                case 17:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 3's UI for Hole 18 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP3SText18, "%d", individual_score);
                    break;
                default:
                    DEBUG_ERROR(MODULE_UI, "Invalid hole number: %d", current_hole);
                    break;
            }
        }
        else if (player_index == 3)
        {
            switch (current_hole)
            {
                case 0:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 4's UI for Hole 1 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP4SText1, "%d", individual_score);
                    break;
                case 1:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 4's UI for Hole 2 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP4SText2, "%d", individual_score);
                    break;
                case 2:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 4's UI for Hole 3 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP4SText3, "%d", individual_score);
                    break;
                case 3:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 4's UI for Hole 4 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP4SText4, "%d", individual_score);
                    break;
                case 4:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 4's UI for Hole 5 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP4SText5, "%d", individual_score);
                    break;
                case 5:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 4's UI for Hole 6 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP4SText6, "%d", individual_score);
                    break;
                case 6:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 4's UI for Hole 7 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP4SText7, "%d", individual_score);
                    break;
                case 7:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 4's UI for Hole 8 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP4SText8, "%d", individual_score);
                    break;
                case 8:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 4's UI for Hole 9 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP4SText9, "%d", individual_score);
                    break;
                case 9:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 4's UI for Hole 10 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP4SText10, "%d", individual_score);
                    break;
                case 10:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 4's UI for Hole 11 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP4SText11, "%d", individual_score);
                    break;
                case 11:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 4's UI for Hole 12 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP4SText12, "%d", individual_score);
                    break;
                case 12:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 4's UI for Hole 13 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP4SText13, "%d", individual_score);
                    break;
                case 13:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 4's UI for Hole 14 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP4SText14, "%d", individual_score);
                    break;
                case 14:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 4's UI for Hole 15 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP4SText15, "%d", individual_score);
                    break;
                case 15:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 4's UI for Hole 16 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP4SText16, "%d", individual_score);
                    break;
                case 16:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 4's UI for Hole 17 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP4SText17, "%d", individual_score);
                    break;
                case 17:
                    DEBUG_DEBUG(MODULE_UI,
                                "Updating Player 4's UI for Hole 18 with Individual Score: %d",
                                individual_score);
                    lv_label_set_text_fmt(ui_SP4P18HScP4SText18, "%d", individual_score);
                    break;

                default:
                    DEBUG_ERROR(MODULE_UI, "Invalid hole number: %d", current_hole);
                    break;
            }
        }
    }

    // Calculate the final score for this player
    final_scores[player_index] = 0;
    DEBUG_DEBUG(MODULE_LOGIC, "Resetting Final Score - Player %d Final Score: %d", player_index + 1,
                final_scores[player_index]);

    for (uint8_t i = 0; i <= current_hole_mode; i++)
    {
        DEBUG_TRACE(MODULE_LOGIC,
                    "Final Score calculation - iteration %d - Player %d Final Score: %d, "
                    "Individual score: %d",
                    i, player_index + 1, final_scores[player_index],
                    individual_scores[player_index][i]);
        final_scores[player_index] += individual_scores[player_index][i];
    }
    DEBUG_INFO(MODULE_LOGIC, "Player %d Final Score: %d", player_index + 1,
               final_scores[player_index]);

    // Update the final score label
    if (num_players == 1 && current_hole_mode == NINE_HOLES)
    {
        lv_label_set_text_fmt(ui_SP1P9HScPSFText, "%d", final_scores[player_index]);
    }
    else if (num_players == 1 && current_hole_mode == EIGHTEEN_HOLES)
    {
        lv_label_set_text_fmt(ui_SP1P18HScPSText19, "%d", final_scores[player_index]);
    }
    else if (num_players == 2 && current_hole_mode == NINE_HOLES)
    {
        if (player_index == 0)
        {
            lv_label_set_text_fmt(ui_SP2P9HScP1SFText, "%d", final_scores[player_index]);
        }
        else if (player_index == 1)
        {
            lv_label_set_text_fmt(ui_SP2P9HScP2SFText, "%d", final_scores[player_index]);
        }
    }
    else if (num_players == 2 && current_hole_mode == EIGHTEEN_HOLES)
    {
        if (player_index == 0)
        {
            lv_label_set_text_fmt(ui_SP2P18HScP1STextF, "%d", cumulative_score);
            DEBUG_DEBUG(MODULE_UI, "Updating Player %d Final Score: %d", player_index + 1,
                        cumulative_score);
        }
        else if (player_index == 1)
        {
            lv_label_set_text_fmt(ui_SP2P18HScP2STextF, "%d", cumulative_score);
            DEBUG_DEBUG(MODULE_UI, "Updating Player %d Final Score: %d", player_index + 1,
                        cumulative_score);
        }
    }
    else if (num_players == 3 && current_hole_mode == NINE_HOLES)
    {
        if (player_index == 0)
        {
            lv_label_set_text_fmt(ui_SP3P9HScP1SFText, "%d", cumulative_score);
            DEBUG_DEBUG(MODULE_UI, "Updating Player %d Final Score: %d", player_index + 1,
                        cumulative_score);
        }
        else if (player_index == 1)
        {
            lv_label_set_text_fmt(ui_SP3P9HScP2SFText, "%d", cumulative_score);
            DEBUG_DEBUG(MODULE_UI, "Updating Player %d Final Score: %d", player_index + 1,
                        cumulative_score);
        }
        else if (player_index == 2)
        {
            lv_label_set_text_fmt(ui_SP3P9HScP3SFText, "%d", cumulative_score);
            DEBUG_DEBUG(MODULE_UI, "Updating Player %d Final Score: %d", player_index + 1,
                        cumulative_score);
        }
    }
    else if (num_players == 3 && current_hole_mode == EIGHTEEN_HOLES)
    {
        if (player_index == 0)
        {
            lv_label_set_text_fmt(ui_SP3P18HScP1STextF, "%d", cumulative_score);
            DEBUG_DEBUG(MODULE_UI, "Updating Player %d Final Score: %d", player_index + 1,
                        cumulative_score);
        }
        else if (player_index == 1)
        {
            lv_label_set_text_fmt(ui_SP3P18HScP2SText19, "%d", cumulative_score);
            DEBUG_DEBUG(MODULE_UI, "Updating Player %d Final Score: %d", player_index + 1,
                        cumulative_score);
        }
        else if (player_index == 2)
        {
            lv_label_set_text_fmt(ui_SP3P18HScP3SText19, "%d", cumulative_score);
            DEBUG_DEBUG(MODULE_UI, "Updating Player %d Final Score: %d", player_index + 1,
                        cumulative_score);
        }
    }
    else if (num_players == 4 && current_hole_mode == NINE_HOLES)
    {
        if (player_index == 0)
        {
            lv_label_set_text_fmt(ui_SP4P9HScP1SFText, "%d", cumulative_score);
            DEBUG_DEBUG(MODULE_UI, "Updating Player %d Final Score: %d", player_index + 1,
                        cumulative_score);
        }
        else if (player_index == 1)
        {
            lv_label_set_text_fmt(ui_SP4P9HScP2SFText, "%d", cumulative_score);
            DEBUG_DEBUG(MODULE_UI, "Updating Player %d Final Score: %d", player_index + 1,
                        cumulative_score);
        }
        else if (player_index == 2)
        {
            lv_label_set_text_fmt(ui_SP4P9HScP3SFText, "%d", cumulative_score);
            DEBUG_DEBUG(MODULE_UI, "Updating Player %d Final Score: %d", player_index + 1,
                        cumulative_score);
        }
        else if (player_index == 3)
        {
            lv_label_set_text_fmt(ui_SP4P9HScP4SFText, "%d", cumulative_score);
            DEBUG_DEBUG(MODULE_UI, "Updating Player %d Final Score: %d", player_index + 1,
                        cumulative_score);
        }
    }
    else if (num_players == 4 && current_hole_mode == EIGHTEEN_HOLES)
    {
        if (player_index == 0)
        {
            lv_label_set_text_fmt(ui_SP4P18HScP1STextF, "%d", cumulative_score);
            DEBUG_DEBUG(MODULE_UI, "Updating Player %d Final Score: %d", player_index + 1,
                        cumulative_score);
        }
        else if (player_index == 1)
        {
            lv_label_set_text_fmt(ui_SP4P18HScP2STextF, "%d", cumulative_score);
            DEBUG_DEBUG(MODULE_UI, "Updating Player %d Final Score: %d", player_index + 1,
                        cumulative_score);
        }
        else if (player_index == 2)
        {
            lv_label_set_text_fmt(ui_SP4P18HScP3STextF, "%d", cumulative_score);
            DEBUG_DEBUG(MODULE_UI, "Updating Player %d Final Score: %d", player_index + 1,
                        cumulative_score);
        }
        else if (player_index == 3)
        {
            lv_label_set_text_fmt(ui_SP4P18HScP4STextF, "%d", cumulative_score);
            DEBUG_DEBUG(MODULE_UI, "Updating Player %d Final Score: %d", player_index + 1,
                        cumulative_score);
        }
    }
}
bool is_it_first_turn = 0;  // Flag to check if it's the first turn

void two_player_highlight_pattern(GameMode game_mode, HoleMode hole_mode)
{  // Update counter (special reset logic)
    highliting_counter = (highliting_counter >= 8) ? 1 : highliting_counter + 1;
    DEBUG_DEBUG(MODULE_UI, "Two player highlight - counter: %d, game_mode: %d, hole_mode: %d",
                highliting_counter, game_mode, hole_mode);

    switch (game_mode)
    {
        case GAME_MODE_MATCH_PLAY:
            switch (hole_mode)
            {
                // P1=01 p2=23 p1=45(01) p2=67(23)
                case NINE_HOLES:
                    if (highliting_counter % 4 == 0 || highliting_counter % 4 == 1)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player/Team 1");
                        if (current_match_play_mode == MATCH_PLAY_MODE_1V1)
                        {
                            lv_obj_set_style_bg_color(ui_MP1V19HGSP1SPanel, lv_color_hex(COLOR_1),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(ui_MP1V19HGSP2SPanel, lv_color_hex(COLOR_2),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        else if (current_match_play_mode == MATCH_PLAY_MODE_2V2)
                        {
                            lv_obj_set_style_bg_color(ui_MP2V29HGST1SPanel, lv_color_hex(COLOR_1),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(ui_MP2V29HGST2SPanel, lv_color_hex(COLOR_2),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                    }
                    else  //(highliting_counter%4 == 2 ||highliting_counter%4 == 3 )
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player/Team 2");
                        if (current_match_play_mode == MATCH_PLAY_MODE_1V1)
                        {
                            lv_obj_set_style_bg_color(ui_MP1V19HGSP1SPanel, lv_color_hex(COLOR_2),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(ui_MP1V19HGSP2SPanel, lv_color_hex(COLOR_1),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        else if (current_match_play_mode == MATCH_PLAY_MODE_2V2)
                        {
                            lv_obj_set_style_bg_color(ui_MP2V29HGST1SPanel, lv_color_hex(COLOR_2),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(ui_MP2V29HGST2SPanel, lv_color_hex(COLOR_1),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                    }
                    break;

                case EIGHTEEN_HOLES:
                    // Pattern for Match Play 18 holes: P1/T1=01,45,89,1213,1617 |
                    // P2/T2=23,67,1011,1415
                    if (highliting_counter % 4 == 0 || highliting_counter % 4 == 1)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player/Team 1 (18H Match Play)");
                        if (current_match_play_mode == MATCH_PLAY_MODE_1V1)
                        {
                            lv_obj_set_style_bg_color(ui_MP1V118HGSP1SPanel, lv_color_hex(COLOR_1),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(ui_MP1V118HGSP2SPanel, lv_color_hex(COLOR_2),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        else if (current_match_play_mode == MATCH_PLAY_MODE_2V2)
                        {
                            lv_obj_set_style_bg_color(ui_MP2V218HGST1SPanel, lv_color_hex(COLOR_1),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(ui_MP2V218HGST2SPanel, lv_color_hex(COLOR_2),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                    }
                    else  // (highliting_counter % 4 == 2 || highliting_counter % 4 == 3)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player/Team 2 (18H Match Play)");
                        if (current_match_play_mode == MATCH_PLAY_MODE_1V1)
                        {
                            lv_obj_set_style_bg_color(ui_MP1V118HGSP1SPanel, lv_color_hex(COLOR_2),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(ui_MP1V118HGSP2SPanel, lv_color_hex(COLOR_1),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                        else if (current_match_play_mode == MATCH_PLAY_MODE_2V2)
                        {
                            lv_obj_set_style_bg_color(ui_MP2V218HGST1SPanel, lv_color_hex(COLOR_2),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                            lv_obj_set_style_bg_color(ui_MP2V218HGST2SPanel, lv_color_hex(COLOR_1),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                        }
                    }
                    break;

                    DEBUG_DEBUG(MODULE_GAME, "Highlighting inside game_mode: %d, hole_mode: %d",
                                game_mode, hole_mode);
                    // Handle highlight cases
                    DEBUG_TRACE(MODULE_GAME, "Highlight Counter Value: %d", highliting_counter);
                    break;
            }
            break;
        case GAME_MODE_QUOTA:
            switch (hole_mode)
            {
                case NINE_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Quota 2P 9H highlighting");
                    DEBUG_TRACE(MODULE_GAME, "Highlight Counter Value: %d", highliting_counter);

                    if (highliting_counter == 0 || highliting_counter == 1 ||
                        highliting_counter == 4 || highliting_counter == 5 ||
                        highliting_counter == 8)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Quota 2P 9H - Highlighting Player 1");
                        lv_obj_set_style_bg_color(ui_Q2P9HGSP1SPar3Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P9HGSP1SPar4Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P9HGSP1SPar5Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P9HGSP2SPar3Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P9HGSP2SPar4Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P9HGSP2SPar5Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else
                    {  // Cases 2,3,6,7
                        DEBUG_DEBUG(MODULE_UI, "Quota 2P 9H - Highlighting Player 2");
                        lv_obj_set_style_bg_color(ui_Q2P9HGSP1SPar3Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P9HGSP1SPar4Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P9HGSP1SPar5Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P9HGSP2SPar3Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P9HGSP2SPar4Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P9HGSP2SPar5Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    break;
                case EIGHTEEN_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Quota 2P 18H highlighting");
                    DEBUG_TRACE(MODULE_GAME, "Highlight Counter Value: %d", highliting_counter);

                    if (highliting_counter == 0 || highliting_counter == 1 ||
                        highliting_counter == 4 || highliting_counter == 5 ||
                        highliting_counter == 8)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Quota 2P 18H - Highlighting Player 1");
                        lv_obj_set_style_bg_color(ui_Q2P18HGSP1SPar3Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P18HGSP1SPar4Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P18HGSP1SPar5Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P18HGSP2SPar3Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P18HGSP2SPar4Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P18HGSP2SPar5Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else
                    {  // Cases 2,3,6,7
                        DEBUG_DEBUG(MODULE_UI, "Quota 2P 18H - Highlighting Player 2");
                        lv_obj_set_style_bg_color(ui_Q2P18HGSP1SPar3Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P18HGSP1SPar4Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P18HGSP1SPar5Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P18HGSP2SPar3Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P18HGSP2SPar4Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q2P18HGSP2SPar5Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    break;
            }
            break;
        case GAME_MODE_VEGAS:
            switch (hole_mode)
            {
                case NINE_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Vegas 2P 9H highlighting");
                    DEBUG_TRACE(MODULE_GAME, "Highlight Counter Value: %d", highliting_counter);

                    if (highliting_counter == 0 || highliting_counter == 1 ||
                        highliting_counter == 4 || highliting_counter == 5 ||
                        highliting_counter == 8)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Vegas 2P 9H - Highlighting Player 1");
                        lv_obj_set_style_bg_color(ui_VQ2P9HGSP1SPar3Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ2P9HGSP1SPar4Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ2P9HGSP1SPar5Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ2P9HGSP2SPar3Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ2P9HGSP2SPar4Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ2P9HGSP2SPar5Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else
                    {
                        DEBUG_DEBUG(MODULE_UI, "Vegas 2P 9H - Highlighting Player 2");
                        lv_obj_set_style_bg_color(ui_VQ2P9HGSP1SPar3Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ2P9HGSP1SPar4Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ2P9HGSP1SPar5Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ2P9HGSP2SPar3Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ2P9HGSP2SPar4Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ2P9HGSP2SPar5Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    break;
                case EIGHTEEN_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Vegas 2P 18H highlighting");
                    DEBUG_TRACE(MODULE_GAME, "Highlight Counter Value: %d", highliting_counter);

                    if (highliting_counter == 0 || highliting_counter == 1 ||
                        highliting_counter == 4 || highliting_counter == 5 ||
                        highliting_counter == 8)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Vegas 2P 18H - Highlighting Player 1");
                        lv_obj_set_style_bg_color(ui_VQ2P18HGSP1SPar3Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ2P18HGSP1SPar4Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ2P18HGSP1SPar5Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ2P18HGSP2SPar3Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ2P18HGSP2SPar4Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ2P18HGSP2SPar5Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else
                    {
                        DEBUG_DEBUG(MODULE_UI, "Vegas 2P 18H - Highlighting Player 2");
                        lv_obj_set_style_bg_color(ui_VQ2P18HGSP1SPar3Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ2P18HGSP1SPar4Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ2P18HGSP1SPar5Panel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ2P18HGSP2SPar3Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ2P18HGSP2SPar4Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ2P18HGSP2SPar5Panel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    break;
            }
            break;
        case GAME_MODE_STROKE_PLAY:
            switch (hole_mode)
            {
                case NINE_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Stroke Play - 9 holes highlighting");
                    DEBUG_TRACE(MODULE_GAME, "Highlight Counter Value: %d", highliting_counter);

                    // Handle highlight cases
                    if (highliting_counter == 0)
                    {
                        DEBUG_DEBUG(MODULE_UI, "No highlight - reset state");
                        lv_obj_set_style_bg_color(ui_SP2P9HGSP1SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP2P9HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 1 || highliting_counter == 4 ||
                             highliting_counter == 5 || highliting_counter == 8)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 1");
                        lv_obj_set_style_bg_color(ui_SP2P9HGSP1SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP2P9HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);

                    }  // zoom
                    else
                    {  // Cases 2,3,6,7
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 2");
                        lv_obj_set_style_bg_color(ui_SP2P9HGSP1SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP2P9HGSP2SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    break;
                case EIGHTEEN_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Stroke Play - 18 holes highlighting");
                    // Handle highlight cases
                    if (highliting_counter == 0)
                    {
                        DEBUG_DEBUG(MODULE_UI, "No highlight - reset state");
                        lv_obj_set_style_bg_color(ui_SP2P18HGSP1SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP2P18HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 1 || highliting_counter == 4 ||
                             highliting_counter == 5 || highliting_counter == 8)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 1");
                        lv_obj_set_style_bg_color(ui_SP2P18HGSP1SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP2P18HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else
                    {  // Cases 2,3,6,7
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 2");
                        lv_obj_set_style_bg_color(ui_SP2P18HGSP1SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP2P18HGSP2SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    break;
            }
            break;
    }
}

void three_player_highlight_pattern(GameMode game_mode, HoleMode hole_mode)
{
    // Update counter (special reset logic)
    highliting_counter = (highliting_counter >= 12) ? 1 : highliting_counter + 1;
    DEBUG_DEBUG(MODULE_UI, "Three player highlight - counter: %d, game_mode: %d, hole_mode: %d",
                highliting_counter, game_mode, hole_mode);

    switch (game_mode)
    {
        case GAME_MODE_MATCH_PLAY:
            DEBUG_TRACE(MODULE_UI, "Match Play mode - no three player highlighting defined");
            break;
        case GAME_MODE_QUOTA:
            switch (hole_mode)
            {
                case NINE_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Quota 3P 9H highlighting");
                    DEBUG_TRACE(MODULE_GAME, "Highlight Counter Value: %d", highliting_counter);

                    if (highliting_counter == 1 || highliting_counter == 6 ||
                        highliting_counter == 7 || highliting_counter == 12)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Quota 3P 9H - Highlighting Player 1");
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP1SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP1SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP1SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP2SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP2SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP2SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP3SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP3SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP3SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 2 || highliting_counter == 3 ||
                             highliting_counter == 8 || highliting_counter == 9)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Quota 3P 9H - Highlighting Player 2");
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP1SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP1SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP1SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP2SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP2SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP2SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP3SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP3SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP3SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else
                    {   // Cases 4,5,10,11
                        DEBUG_DEBUG(MODULE_UI, "Quota 3P 9H - Highlighting Player 3");
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP1SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP1SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP1SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP2SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP2SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP2SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP3SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP3SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P9HGSP3SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    break;
                case EIGHTEEN_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Quota 3P 18H highlighting");
                    DEBUG_TRACE(MODULE_GAME, "Highlight Counter Value: %d", highliting_counter);

                    if (highliting_counter == 1 || highliting_counter == 6 ||
                        highliting_counter == 7 || highliting_counter == 12)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Quota 3P 18H - Highlighting Player 1");
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP1SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP1SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP1SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP2SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP2SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP2SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP3SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP3SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP3SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 2 || highliting_counter == 3 ||
                             highliting_counter == 8 || highliting_counter == 9)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Quota 3P 18H - Highlighting Player 2");
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP1SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP1SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP1SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP2SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP2SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP2SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP3SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP3SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP3SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else
                    {   // Cases 4,5,10,11
                        DEBUG_DEBUG(MODULE_UI, "Quota 3P 18H - Highlighting Player 3");
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP1SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP1SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP1SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP2SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP2SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP2SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP3SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP3SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q3P18HGSP3SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    break;
            }
            break;
        case GAME_MODE_VEGAS:
            switch (hole_mode)
            {
                case NINE_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Vegas 3P 9H highlighting");
                    DEBUG_TRACE(MODULE_GAME, "Highlight Counter Value: %d", highliting_counter);

                    if (highliting_counter == 1 || highliting_counter == 6 ||
                        highliting_counter == 7 || highliting_counter == 12)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Vegas 3P 9H - Highlighting Player 1");
                        lv_obj_set_style_bg_color(ui_VQ3P9HGSP1SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P9HGSP1SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P9HGSP1SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P9HGSP2SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P9HGSP2SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P9HGSP2SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P9HGSP3SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P9HGSP3SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P9HGSP3SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 2 || highliting_counter == 3 ||
                             highliting_counter == 8 || highliting_counter == 9)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Vegas 3P 9H - Highlighting Player 2");
                        lv_obj_set_style_bg_color(ui_VQ3P9HGSP1SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P9HGSP1SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P9HGSP1SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P9HGSP2SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P9HGSP2SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P9HGSP2SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P9HGSP3SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P9HGSP3SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P9HGSP3SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else
                    {   // Cases 4,5,10,11
                        DEBUG_DEBUG(MODULE_UI, "Vegas 3P 9H - Highlighting Player 3");
                        lv_obj_set_style_bg_color(ui_VQ3P9HGSP1SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P9HGSP1SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P9HGSP1SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P9HGSP2SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P9HGSP2SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P9HGSP2SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P9HGSP3SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P9HGSP3SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P9HGSP3SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    break;
                case EIGHTEEN_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Vegas 3P 18H highlighting");
                    DEBUG_TRACE(MODULE_GAME, "Highlight Counter Value: %d", highliting_counter);

                    if (highliting_counter == 1 || highliting_counter == 6 ||
                        highliting_counter == 7 || highliting_counter == 12)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Vegas 3P 18H - Highlighting Player 1");
                        lv_obj_set_style_bg_color(ui_VQ3P18HGSP1SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P18HGSP1SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P18HGSP1SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P18HGSP2SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P18HGSP2SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P18HGSP2SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P18HGSP3SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P18HGSP3SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P18HGSP3SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 2 || highliting_counter == 3 ||
                             highliting_counter == 8 || highliting_counter == 9)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Vegas 3P 18H - Highlighting Player 2");
                        lv_obj_set_style_bg_color(ui_VQ3P18HGSP1SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P18HGSP1SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P18HGSP1SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P18HGSP2SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P18HGSP2SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P18HGSP2SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P18HGSP3SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P18HGSP3SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P18HGSP3SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else
                    {   // Cases 4,5,10,11
                        DEBUG_DEBUG(MODULE_UI, "Vegas 3P 18H - Highlighting Player 3");
                        lv_obj_set_style_bg_color(ui_VQ3P18HGSP1SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P18HGSP1SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P18HGSP1SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P18HGSP2SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P18HGSP2SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P18HGSP2SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P18HGSP3SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P18HGSP3SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ3P18HGSP3SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    break;
            }
            break;
        case GAME_MODE_STROKE_PLAY:
            switch (hole_mode)
            {
                case NINE_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Three Player - 9 holes highlighting");
                    // Handle highlight cases
                    if (highliting_counter == 0)
                    {
                        DEBUG_DEBUG(MODULE_UI, "No highlight - default state");
                        lv_obj_set_style_bg_color(ui_SP3P9HGSP1SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P9HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P9HGSP3SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 1 || highliting_counter == 6 ||
                             highliting_counter == 7 || highliting_counter == 12)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 1");
                        lv_obj_set_style_bg_color(ui_SP3P9HGSP1SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P9HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P9HGSP3SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 2 || highliting_counter == 3 ||
                             highliting_counter == 8 || highliting_counter == 9)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 2");
                        lv_obj_set_style_bg_color(ui_SP3P9HGSP1SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P9HGSP2SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P9HGSP3SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else
                    {  // Cases 4,5,10,11
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 3");
                        lv_obj_set_style_bg_color(ui_SP3P9HGSP1SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P9HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P9HGSP3SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    break;
                case EIGHTEEN_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Three Player - 18 holes highlighting");
                    // Handle highlight cases
                    if (highliting_counter == 0)
                    {
                        DEBUG_DEBUG(MODULE_UI, "No highlight - default state");
                        lv_obj_set_style_bg_color(ui_SP3P18HGSP1SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P18HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P18HGSP3SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 1 || highliting_counter == 6 ||
                             highliting_counter == 7 || highliting_counter == 12)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 1");
                        lv_obj_set_style_bg_color(ui_SP3P18HGSP1SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P18HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P18HGSP3SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 2 || highliting_counter == 3 ||
                             highliting_counter == 8 || highliting_counter == 9)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 2");
                        lv_obj_set_style_bg_color(ui_SP3P18HGSP1SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P18HGSP2SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P18HGSP3SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else
                    {  // Cases 4,5,10,11
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 3");
                        lv_obj_set_style_bg_color(ui_SP3P18HGSP1SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P18HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP3P18HGSP3SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    break;
            }
            break;
    }
}

void four_player_highlight_pattern(GameMode game_mode, HoleMode hole_mode)
{
    // Update counter (special reset logic)
    highliting_counter = (highliting_counter >= 24) ? 1 : highliting_counter + 1;
    DEBUG_DEBUG(MODULE_UI, "Four player highlight - counter: %d, game_mode: %d, hole_mode: %d",
                highliting_counter, game_mode, hole_mode);

    switch (game_mode)
    {
        case GAME_MODE_MATCH_PLAY:
            DEBUG_TRACE(MODULE_UI, "Match Play mode - no four player highlighting defined");
            break;
        case GAME_MODE_QUOTA:
            DEBUG_DEBUG(MODULE_UI, "Quota mode - four player highlighting");
            switch (hole_mode)
            {
                case NINE_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Quota 4P - 9 holes highlighting");
                    if (highliting_counter == 1 || highliting_counter == 8 ||
                        highliting_counter == 9 || highliting_counter == 16 ||
                        highliting_counter == 17 || highliting_counter == 24)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 1");
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP1SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP1SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP1SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP2SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP2SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP2SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP3SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP3SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP3SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP4SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP4SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP4SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 2 || highliting_counter == 3 ||
                             highliting_counter == 10 || highliting_counter == 11 ||
                             highliting_counter == 18 || highliting_counter == 19)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 2");
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP2SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP2SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP2SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP1SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP1SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP1SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP3SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP3SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP3SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP4SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP4SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP4SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 4 || highliting_counter == 5 ||
                             highliting_counter == 12 || highliting_counter == 13 ||
                             highliting_counter == 20 || highliting_counter == 21)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 3");
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP3SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP3SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP3SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP1SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP1SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP1SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP2SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP2SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP2SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP4SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP4SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP4SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else
                    {  // 6,7,14,15,22,23
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 4");
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP4SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP4SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP4SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP1SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP1SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP1SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP2SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP2SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP2SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP3SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP3SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P9HGSP3SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    break;
                case EIGHTEEN_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Quota 4P - 18 holes highlighting");
                    if (highliting_counter == 1 || highliting_counter == 8 ||
                        highliting_counter == 9 || highliting_counter == 16 ||
                        highliting_counter == 17 || highliting_counter == 24)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 1");
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP1SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP1SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP1SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP2SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP2SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP2SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP3SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP3SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP3SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP4SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP4SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP4SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 2 || highliting_counter == 3 ||
                             highliting_counter == 10 || highliting_counter == 11 ||
                             highliting_counter == 18 || highliting_counter == 19)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 2");
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP2SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP2SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP2SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP1SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP1SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP1SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP3SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP3SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP3SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP4SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP4SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP4SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 4 || highliting_counter == 5 ||
                             highliting_counter == 12 || highliting_counter == 13 ||
                             highliting_counter == 20 || highliting_counter == 21)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 3");
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP3SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP3SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP3SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP1SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP1SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP1SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP2SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP2SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP2SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP4SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP4SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP4SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else
                    {  // 6,7,14,15,22,23
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 4");
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP4SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP4SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP4SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP1SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP1SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP1SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP2SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP2SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP2SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP3SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP3SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_Q4P18HGSP3SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    break;
            }
            break;
        case GAME_MODE_VEGAS:
            switch (hole_mode)
            {
                case NINE_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Vegas 4P 9H highlighting");
                    DEBUG_TRACE(MODULE_GAME, "Highlight Counter Value: %d", highliting_counter);

                    if (highliting_counter == 1 || highliting_counter == 8 ||
                        highliting_counter == 9 || highliting_counter == 16 ||
                        highliting_counter == 17 || highliting_counter == 24)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Vegas 4P 9H - Highlighting Player 1");
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP1SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP1SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP1SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP2SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP2SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP2SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP3SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP3SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP3SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP4SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP4SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP4SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 2 || highliting_counter == 3 ||
                             highliting_counter == 10 || highliting_counter == 11 ||
                             highliting_counter == 18 || highliting_counter == 19)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Vegas 4P 9H - Highlighting Player 2");
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP1SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP1SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP1SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP2SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP2SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP2SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP3SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP3SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP3SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP4SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP4SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP4SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 4 || highliting_counter == 5 ||
                             highliting_counter == 12 || highliting_counter == 13 ||
                             highliting_counter == 20 || highliting_counter == 21)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Vegas 4P 9H - Highlighting Player 3");
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP1SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP1SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP1SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP2SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP2SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP2SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP3SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP3SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP3SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP4SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP4SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP4SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else
                    {   // 6,7,14,15,22,23
                        DEBUG_DEBUG(MODULE_UI, "Vegas 4P 9H - Highlighting Player 4");
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP1SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP1SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP1SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP2SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP2SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP2SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP3SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP3SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP3SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP4SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP4SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P9HGSP4SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    break;
                case EIGHTEEN_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Vegas 4P 18H highlighting");
                    DEBUG_TRACE(MODULE_GAME, "Highlight Counter Value: %d", highliting_counter);

                    if (highliting_counter == 1 || highliting_counter == 8 ||
                        highliting_counter == 9 || highliting_counter == 16 ||
                        highliting_counter == 17 || highliting_counter == 24)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Vegas 4P 18H - Highlighting Player 1");
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP1SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP1SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP1SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP2SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP2SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP2SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP3SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP3SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP3SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP4SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP4SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP4SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 2 || highliting_counter == 3 ||
                             highliting_counter == 10 || highliting_counter == 11 ||
                             highliting_counter == 18 || highliting_counter == 19)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Vegas 4P 18H - Highlighting Player 2");
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP1SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP1SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP1SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP2SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP2SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP2SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP3SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP3SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP3SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP4SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP4SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP4SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 4 || highliting_counter == 5 ||
                             highliting_counter == 12 || highliting_counter == 13 ||
                             highliting_counter == 20 || highliting_counter == 21)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Vegas 4P 18H - Highlighting Player 3");
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP1SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP1SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP1SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP2SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP2SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP2SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP3SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP3SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP3SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP4SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP4SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP4SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else
                    {   // 6,7,14,15,22,23
                        DEBUG_DEBUG(MODULE_UI, "Vegas 4P 18H - Highlighting Player 4");
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP1SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP1SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP1SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP2SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP2SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP2SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP3SPar3Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP3SPar4Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP3SPar5Panel, lv_color_hex(COLOR_2), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP4SPar3Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP4SPar4Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_VQ4P18HGSP4SPar5Panel, lv_color_hex(COLOR_1), LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    break;
            }
            break;
        case GAME_MODE_STROKE_PLAY:
            switch (hole_mode)
            {
                case NINE_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Four Player - 9 holes highlighting");
                    // Handle highlight cases
                    if (highliting_counter == 0)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 1 by default");
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP1SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP3SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP4SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 1 || highliting_counter == 8 ||
                             highliting_counter == 9 || highliting_counter == 16 ||
                             highliting_counter == 17 || highliting_counter == 24)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 1");
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP1SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP3SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP4SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 2 || highliting_counter == 3 ||
                             highliting_counter == 10 || highliting_counter == 11 ||
                             highliting_counter == 18 || highliting_counter == 19)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 2");
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP2SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP1SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP3SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP4SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 4 || highliting_counter == 5 ||
                             highliting_counter == 12 || highliting_counter == 13 ||
                             highliting_counter == 20 || highliting_counter == 21)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 3");
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP3SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP1SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP4SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else
                    {  // 6,7,14,15,22,23,etc.
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 4");
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP4SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP1SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P9HGSP3SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    break;
                case EIGHTEEN_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Four Player - 18 holes highlighting");
                    // Handle highlight cases
                    if (highliting_counter == 0)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 1 by default");
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP1SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP3SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP4SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 1 || highliting_counter == 8 ||
                             highliting_counter == 9 || highliting_counter == 16 ||
                             highliting_counter == 17 || highliting_counter == 24)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 1");
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP1SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP3SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP4SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 2 || highliting_counter == 3 ||
                             highliting_counter == 10 || highliting_counter == 11 ||
                             highliting_counter == 18 || highliting_counter == 19)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 2");
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP2SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP1SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP3SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP4SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else if (highliting_counter == 4 || highliting_counter == 5 ||
                             highliting_counter == 12 || highliting_counter == 13 ||
                             highliting_counter == 20 || highliting_counter == 21)
                    {
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 3");
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP3SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP1SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP4SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    else
                    {  // 6,7,14,15,22,23,etc.
                        DEBUG_DEBUG(MODULE_UI, "Highlighting Player 4");
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP4SPanel, lv_color_hex(COLOR_1),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP1SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP2SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                        lv_obj_set_style_bg_color(ui_SP4P18HGSP3SPanel, lv_color_hex(COLOR_2),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);
                    }
                    break;
            }
            break;
    }
}

void update_player_highlight(uint8_t detection_count, uint8_t current_hole, uint8_t nplayers)
{
    DEBUG_TRACE(
        MODULE_UI,
        "Updating player highlight - detection_count: %d, current_hole: %d, nplayers: %d",
        detection_count, current_hole, nplayers);

    uint8_t highlighted_player = 1;  // Default to player 1

    switch (current_game_mode)
    {
        case GAME_MODE_MATCH_PLAY:
            DEBUG_DEBUG(MODULE_UI, "Match Play mode highlighting");
            switch (current_hole_mode)
            {
                case NINE_HOLES:
                    switch (nplayers)
                    {
                        case 2:  // Exact pattern for 2-player stroke play
                            DEBUG_DEBUG(MODULE_UI, "Match Play - 2 players, 9 holes");
                            two_player_highlight_pattern(GAME_MODE_MATCH_PLAY, NINE_HOLES);
                            break;

                        default:
                            DEBUG_WARN(MODULE_UI, "Unsupported player count for Match Play: %d",
                                       nplayers);
                            highlighted_player = 1;
                            break;
                    }
                    break;
                case EIGHTEEN_HOLES:
                    switch (nplayers)
                    {
                        case 2:
                            DEBUG_DEBUG(MODULE_UI, "Match Play - 2 players, 18 holes");
                            two_player_highlight_pattern(GAME_MODE_MATCH_PLAY, EIGHTEEN_HOLES);
                            break;
                        default:
                            DEBUG_WARN(MODULE_UI, "Unsupported player count for Match Play 18H: %d",
                                       nplayers);
                            highlighted_player = 1;
                            break;
                    }
                    break;

                default:
                    DEBUG_WARN(MODULE_UI, "Unsupported hole mode for Match Play: %d",
                               current_hole_mode);
                    break;
            }
            break;
        case GAME_MODE_QUOTA:
            DEBUG_DEBUG(MODULE_UI, "Quota mode highlighting");
            switch (current_hole_mode)
            {
                case NINE_HOLES:
                case EIGHTEEN_HOLES:
                    if (nplayers == 2)
                    {
                        two_player_highlight_pattern(GAME_MODE_QUOTA, current_hole_mode);
                    }
                    else if (nplayers == 3)
                    {
                        three_player_highlight_pattern(GAME_MODE_QUOTA, current_hole_mode);
                    }
                    else if (nplayers == 4)
                    {
                        four_player_highlight_pattern(GAME_MODE_QUOTA, current_hole_mode);
                    }
                    break;
            }
            break;

        case GAME_MODE_VEGAS:
            DEBUG_DEBUG(MODULE_UI, "Vegas mode highlighting");
            switch (current_hole_mode)
            {
                case NINE_HOLES:
                case EIGHTEEN_HOLES:
                    switch (nplayers)
                    {
                        case 2:
                            DEBUG_DEBUG(MODULE_UI, "Vegas - 2 players");
                            two_player_highlight_pattern(GAME_MODE_VEGAS, current_hole_mode);
                            break;
                        case 3:
                            DEBUG_DEBUG(MODULE_UI, "Vegas - 3 players");
                            three_player_highlight_pattern(GAME_MODE_VEGAS, current_hole_mode);
                            break;
                        case 4:
                            DEBUG_DEBUG(MODULE_UI, "Vegas - 4 players");
                            four_player_highlight_pattern(GAME_MODE_VEGAS, current_hole_mode);
                            break;
                        default:
                            highlighted_player = (detection_count % nplayers) + 1;
                            DEBUG_TRACE(MODULE_UI, "Vegas mode highlighted player: %d",
                                        highlighted_player);
                            break;
                    }
                    break;
            }
            break;

        case GAME_MODE_STROKE_PLAY:
            DEBUG_DEBUG(MODULE_UI, "Stroke Play mode highlighting");
            switch (current_hole_mode)
            {
                case NINE_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Stroke Play - 9 holes");
                    switch (nplayers)
                    {
                        case 2:  // Exact pattern for 2-player stroke play
                            DEBUG_DEBUG(MODULE_UI, "Stroke Play - 2 players, 9 holes");
                            two_player_highlight_pattern(GAME_MODE_STROKE_PLAY, NINE_HOLES);
                            break;

                        case 3:
                            DEBUG_DEBUG(MODULE_UI, "Stroke Play - 3 players, 9 holes");
                            three_player_highlight_pattern(GAME_MODE_STROKE_PLAY, NINE_HOLES);
                            break;
                        case 4:
                            DEBUG_DEBUG(MODULE_UI, "Stroke Play - 4 players, 9 holes");
                            four_player_highlight_pattern(GAME_MODE_STROKE_PLAY, NINE_HOLES);
                            break;

                        default:
                            DEBUG_WARN(MODULE_UI,
                                       "Unsupported player count for Stroke Play 9 holes: %d",
                                       nplayers);
                            highlighted_player = 1;
                            break;
                    }
                    break;

                case EIGHTEEN_HOLES:
                    DEBUG_DEBUG(MODULE_UI, "Stroke Play - 18 holes");
                    switch (nplayers)
                    {
                        case 2:  // Exact pattern for 2-player stroke play
                            DEBUG_DEBUG(MODULE_UI, "Stroke Play - 2 players, 18 holes");
                            two_player_highlight_pattern(GAME_MODE_STROKE_PLAY, EIGHTEEN_HOLES);
                            break;
                        case 3:
                            DEBUG_DEBUG(MODULE_UI, "Stroke Play - 3 players, 18 holes");
                            three_player_highlight_pattern(GAME_MODE_STROKE_PLAY, EIGHTEEN_HOLES);
                            break;
                        case 4:
                            DEBUG_DEBUG(MODULE_UI, "Stroke Play - 4 players, 18 holes");
                            four_player_highlight_pattern(GAME_MODE_STROKE_PLAY, EIGHTEEN_HOLES);
                            break;
                        default:
                            DEBUG_WARN(MODULE_UI,
                                       "Unsupported player count for Stroke Play 18 holes: %d",
                                       nplayers);
                            highlighted_player = 1;
                            break;
                    }
                    break;
                default:
                    DEBUG_ERROR(MODULE_UI, "Unknown hole mode: %d", current_hole_mode);
                    break;
            }
            break;
        default:
            DEBUG_ERROR(MODULE_UI, "Unknown game mode: %d", current_game_mode);
            break;
    }

    DEBUG_TRACE(MODULE_UI, "Highlight update completed - final highlighted player: %d",
                highlighted_player);
}

/* current_hole_mode and current_match_play_mode are now macros accessing game_state */

// Set hole mode (9 or 18 holes)
void set_hole_mode(HoleMode mode)
{
    DEBUG_INFO(MODULE_LOGIC, "========== HOLE MODE SELECTION ==========");
    DEBUG_INFO(MODULE_LOGIC, "Setting hole mode to: %s",
               (mode == HOLES_9) ? "9 Holes" : "18 Holes");
    current_hole_mode = mode;
    DEBUG_INFO(MODULE_LOGIC,
               "Current settings: game_mode=%d, hole_mode=%d, match_play_mode=%d, num_players=%d",
               current_game_mode, current_hole_mode, current_match_play_mode, num_players);
    DEBUG_INFO(MODULE_LOGIC, "=========================================");
}

// Set match play mode (1v1 or 2v2)
void set_match_play_mode(MatchPlayMode mode)
{
    DEBUG_INFO(MODULE_LOGIC, "========== MATCH PLAY MODE SELECTION ==========");
    DEBUG_INFO(MODULE_LOGIC, "Setting match play mode to: %s",
               (mode == MATCH_PLAY_MODE_1V1) ? "1v1" : "2v2");
    current_match_play_mode = mode;
    DEBUG_INFO(MODULE_LOGIC,
               "Current settings: game_mode=%d, hole_mode=%d, match_play_mode=%d, num_players=%d",
               current_game_mode, current_hole_mode, current_match_play_mode, num_players);
    DEBUG_INFO(MODULE_LOGIC, "===============================================");
}

// Print current hole mode (for debugging/menus)
void print_current_hole_mode()
{
    DEBUG_INFO(MODULE_LOGIC, "Current hole mode: %s",
               (current_hole_mode == HOLES_9) ? "9 Holes" : "18 Holes");
}
void check_all_players_completed(GameMode gameMode)
{
    DEBUG_TRACE(MODULE_GAME, "Checking if all players completed - game mode: %d, hole mode: %d",
                gameMode, current_hole_mode);

    static uint8_t player_is_finished[MAX_PLAYERS] = {0};  // 1=finished, 0=not finished
    uint8_t        all_done           = 1;

    switch (gameMode)
    {
        case GAME_MODE_STROKE_PLAY:
            DEBUG_INFO(MODULE_GAME,
                       "Stroke Play - checking completion for %d players on hole mode %d",
                       num_players, current_hole_mode);

            for (uint8_t i = 0; i < num_players; i++)
            {
                // Check if player has completed the current hole
                if (players[i].current_hole > current_hole_mode + 1 ||
                    (players[i].current_hole == current_hole_mode + 1 &&
                     players[i].detection_count == 0))
                {
                    /***************************************** */
                    players[i].round_total_score = 0;
                    /***************************************** */
                    player_is_finished[i] = 1;
                    DEBUG_INFO(MODULE_GAME, "Player %d FINISHED hole %d", i + 1, current_hole_mode);
                }
                else
                {
                    player_is_finished[i] = 0;
                    all_done = 0;
                    DEBUG_DEBUG(MODULE_GAME, "Player %d NOT finished (Hole:%d Detections:%d)",
                                i + 1, players[i].current_hole, players[i].detection_count);
                }
            }

            update_flag = all_done ? 1 : 0;

            if (update_flag)
            {
                DEBUG_INFO(MODULE_GAME, "All players completed hole %d. Advancing to next round",
                           current_hole_mode);
                print_final_scores_and_winner();  // All holes are done

                // Navigate to scorecard based on number of players
                if (num_players == 1)
                {
                    if (current_hole_mode == NINE_HOLES)
                    {
                        _ui_screen_change(&ui_SP1P9HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_SP1P9HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 1P 9H scorecard");
                    }
                    else if (current_hole_mode == EIGHTEEN_HOLES)
                    {
                        _ui_screen_change(&ui_SP1P18HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_SP1P18HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 1P 18H scorecard");
                    }
                }
                else if (num_players == 2)
                {
                    // Determine winner (higher score wins in stroke play)
                    if (players[0].score > players[1].score)
                    {
                        DEBUG_INFO(MODULE_GAME, "Player 1 wins with score %d vs %d",
                                   players[0].score, players[1].score);
                        PLAY_PLAYER1WINS_WAV;
                    }
                    else if (players[1].score > players[0].score)
                    {
                        DEBUG_INFO(MODULE_GAME, "Player 2 wins with score %d vs %d",
                                   players[1].score, players[0].score);
                        PLAY_PLAYER2WINS_WAV;
                    }
                    else
                    {
                        DEBUG_INFO(MODULE_GAME, "Game tied at %d points", players[0].score);
                    }

                    if (current_hole_mode == NINE_HOLES)
                    {
                        _ui_screen_change(&ui_SP2P9HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_SP2P9HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 2P 9H scorecard");
                    }
                    else if (current_hole_mode == EIGHTEEN_HOLES)
                    {
                        _ui_screen_change(&ui_SP2P18HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_SP2P18HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 2P 18H scorecard");
                    }
                }
                else if (num_players == 3)
                {
                    // Determine winner among 3 players (higher score wins)
                    int winner    = 0;
                    int max_score = players[0].score;
                    int tie_count = 1;

                    for (int i = 1; i < 3; i++)
                    {
                        if (players[i].score > max_score)
                        {
                            max_score = players[i].score;
                            winner    = i;
                            tie_count = 1;
                        }
                        else if (players[i].score == max_score)
                        {
                            tie_count++;
                        }
                    }

                    if (tie_count == 1)
                    {
                        DEBUG_INFO(MODULE_GAME, "Player %d wins with score %d", winner + 1,
                                   max_score);
                        switch (winner)
                        {
                            case 0:
                                PLAY_PLAYER1WINS_WAV;
                                break;
                            case 1:
                                PLAY_PLAYER2WINS_WAV;
                                break;
                            case 2:
                                PLAY_PLAYER3WINS_WAV;
                                break;
                        }
                    }
                    else
                    {
                        DEBUG_INFO(MODULE_GAME, "Game tied with %d players at %d points", tie_count,
                                   max_score);
                    }

                    if (current_hole_mode == NINE_HOLES)
                    {
                        _ui_screen_change(&ui_SP3P9HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_SP3P9HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 3P 9H scorecard");
                    }
                    else if (current_hole_mode == EIGHTEEN_HOLES)
                    {
                        _ui_screen_change(&ui_SP3P18HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_SP3P18HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 3P 18H scorecard");
                    }
                }
                else if (num_players == 4)
                {
                    // Determine winner among 4 players (higher score wins)
                    int winner    = 0;
                    int max_score = players[0].score;
                    int tie_count = 1;

                    for (int i = 1; i < 4; i++)
                    {
                        if (players[i].score > max_score)
                        {
                            max_score = players[i].score;
                            winner    = i;
                            tie_count = 1;
                        }
                        else if (players[i].score == max_score)
                        {
                            tie_count++;
                        }
                    }

                    if (tie_count == 1)
                    {
                        DEBUG_INFO(MODULE_GAME, "Player %d wins with score %d", winner + 1,
                                   max_score);
                        switch (winner)
                        {
                            case 0:
                                PLAY_PLAYER1WINS_WAV;
                                break;
                            case 1:
                                PLAY_PLAYER2WINS_WAV;
                                break;
                            case 2:
                                PLAY_PLAYER3WINS_WAV;
                                break;
                            case 3:
                                PLAY_PLAYER4WINS_WAV;
                                break;
                        }
                    }
                    else
                    {
                        DEBUG_INFO(MODULE_GAME, "Game tied with %d players at %d points", tie_count,
                                   max_score);
                    }

                    if (current_hole_mode == NINE_HOLES)
                    {
                        _ui_screen_change(&ui_SP4P9HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_SP4P9HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 4P 9H scorecard");
                    }
                    else if (current_hole_mode == EIGHTEEN_HOLES)
                    {
                        _ui_screen_change(&ui_SP4P18HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_SP4P18HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 4P 18H scorecard");
                    }
                }
            }
            else
            {
                DEBUG_TRACE(MODULE_GAME, "Not all players completed - %d players still playing",
                            num_players - all_done);
            }
            break;

        case GAME_MODE_QUOTA:
            DEBUG_INFO(MODULE_GAME, "Quota Points - checking completion for %d players",
                       num_players);

            for (uint8_t i = 0; i < num_players; i++)
            {
                if (quota_player_completed(&players[i]))
                {
                    player_is_finished[i] = 1;
                    DEBUG_INFO(MODULE_GAME, "Player %d completed quota!", i + 1);
                }
                else
                {
                    player_is_finished[i] = 0;
                    DEBUG_DEBUG(MODULE_GAME, "Player %d quota remaining (3pt:%d, 4pt:%d, 5pt:%d)",
                                i + 1, players[i].par3_count, players[i].par4_count,
                                players[i].par5_count);
                }
            }

            // For 1P: player wins when quota is done
            // For 2P+: first player to complete quota wins
            switch (num_players)
            {
                case 1:
                    if (player_is_finished[0])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "1P Quota complete - Score:%d", players[0].score);
                        PLAY_PLAYER1WINS_WAV;
                    }
                    break;
                case 2:
                    if (player_is_finished[0] && !player_is_finished[1])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "Player 1 completed quota first! Score:%d",
                                   players[0].score);
                        PLAY_PLAYER1WINS_WAV;
                        if (current_hole_mode == NINE_HOLES)
                            lv_obj_clear_flag(ui_Q2P9HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                        else
                            lv_obj_clear_flag(ui_Q2P18HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                    }
                    else if (player_is_finished[1] && !player_is_finished[0])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "Player 2 completed quota first! Score:%d",
                                   players[1].score);
                        PLAY_PLAYER2WINS_WAV;
                        if (current_hole_mode == NINE_HOLES)
                            lv_obj_clear_flag(ui_Q2P9HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                        else
                            lv_obj_clear_flag(ui_Q2P18HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                    }
                    else if (player_is_finished[0] && player_is_finished[1])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "Both players completed quota simultaneously!");
                        // Both finished at the same time - show both crowns
                        if (current_hole_mode == NINE_HOLES)
                        {
                            lv_obj_clear_flag(ui_Q2P9HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                            lv_obj_clear_flag(ui_Q2P9HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                        }
                        else
                        {
                            lv_obj_clear_flag(ui_Q2P18HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                            lv_obj_clear_flag(ui_Q2P18HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                        }
                    }
                    break;
                case 3:
                    // Check each player individually for first-to-finish
                    if (player_is_finished[0] && !player_is_finished[1] && !player_is_finished[2])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "Player 1 completed quota first! Score:%d",
                                   players[0].score);
                        PLAY_PLAYER1WINS_WAV;
                        if (current_hole_mode == NINE_HOLES)
                            lv_obj_clear_flag(ui_Q3P9HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                        else
                            lv_obj_clear_flag(ui_Q3P18HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                    }
                    else if (player_is_finished[1] && !player_is_finished[0] && !player_is_finished[2])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "Player 2 completed quota first! Score:%d",
                                   players[1].score);
                        PLAY_PLAYER2WINS_WAV;
                        if (current_hole_mode == NINE_HOLES)
                            lv_obj_clear_flag(ui_Q3P9HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                        else
                            lv_obj_clear_flag(ui_Q3P18HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                    }
                    else if (player_is_finished[2] && !player_is_finished[0] && !player_is_finished[1])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "Player 3 completed quota first! Score:%d",
                                   players[2].score);
                        PLAY_PLAYER3WINS_WAV;
                        if (current_hole_mode == NINE_HOLES)
                            lv_obj_clear_flag(ui_Q3P9HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);
                        else
                            lv_obj_clear_flag(ui_Q3P18HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);
                    }
                    else if (player_is_finished[0] || player_is_finished[1] || player_is_finished[2])
                    {
                        // Multiple players finished simultaneously - show all their crowns
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "Multiple players completed quota simultaneously!");
                        if (current_hole_mode == NINE_HOLES)
                        {
                            if (player_is_finished[0])
                                lv_obj_clear_flag(ui_Q3P9HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                            if (player_is_finished[1])
                                lv_obj_clear_flag(ui_Q3P9HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                            if (player_is_finished[2])
                                lv_obj_clear_flag(ui_Q3P9HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);
                        }
                        else
                        {
                            if (player_is_finished[0])
                                lv_obj_clear_flag(ui_Q3P18HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                            if (player_is_finished[1])
                                lv_obj_clear_flag(ui_Q3P18HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                            if (player_is_finished[2])
                                lv_obj_clear_flag(ui_Q3P18HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);
                        }
                    }
                    break;
                case 4:
                    // Check each player individually for first-to-finish
                    if (player_is_finished[0] && !player_is_finished[1] &&
                        !player_is_finished[2] && !player_is_finished[3])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "Player 1 completed quota first! Score:%d",
                                   players[0].score);
                        PLAY_PLAYER1WINS_WAV;
                        if (current_hole_mode == NINE_HOLES)
                            lv_obj_clear_flag(ui_Q4P9HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                        else
                            lv_obj_clear_flag(ui_Q4P18HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                    }
                    else if (player_is_finished[1] && !player_is_finished[0] &&
                             !player_is_finished[2] && !player_is_finished[3])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "Player 2 completed quota first! Score:%d",
                                   players[1].score);
                        PLAY_PLAYER2WINS_WAV;
                        if (current_hole_mode == NINE_HOLES)
                            lv_obj_clear_flag(ui_Q4P9HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                        else
                            lv_obj_clear_flag(ui_Q4P18HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                    }
                    else if (player_is_finished[2] && !player_is_finished[0] &&
                             !player_is_finished[1] && !player_is_finished[3])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "Player 3 completed quota first! Score:%d",
                                   players[2].score);
                        PLAY_PLAYER3WINS_WAV;
                        if (current_hole_mode == NINE_HOLES)
                            lv_obj_clear_flag(ui_Q4P9HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);
                        else
                            lv_obj_clear_flag(ui_Q4P18HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);
                    }
                    else if (player_is_finished[3] && !player_is_finished[0] &&
                             !player_is_finished[1] && !player_is_finished[2])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "Player 4 completed quota first! Score:%d",
                                   players[3].score);
                        PLAY_PLAYER4WINS_WAV;
                        if (current_hole_mode == NINE_HOLES)
                            lv_obj_clear_flag(ui_Q4P9HGSP4SCrown, LV_OBJ_FLAG_HIDDEN);
                        else
                            lv_obj_clear_flag(ui_Q4P18HGSP4SCrown, LV_OBJ_FLAG_HIDDEN);
                    }
                    else if (player_is_finished[0] || player_is_finished[1] ||
                             player_is_finished[2] || player_is_finished[3])
                    {
                        // Multiple players finished simultaneously - show all their crowns
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "Multiple players completed quota simultaneously!");
                        if (current_hole_mode == NINE_HOLES)
                        {
                            if (player_is_finished[0])
                                lv_obj_clear_flag(ui_Q4P9HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                            if (player_is_finished[1])
                                lv_obj_clear_flag(ui_Q4P9HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                            if (player_is_finished[2])
                                lv_obj_clear_flag(ui_Q4P9HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);
                            if (player_is_finished[3])
                                lv_obj_clear_flag(ui_Q4P9HGSP4SCrown, LV_OBJ_FLAG_HIDDEN);
                        }
                        else
                        {
                            if (player_is_finished[0])
                                lv_obj_clear_flag(ui_Q4P18HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                            if (player_is_finished[1])
                                lv_obj_clear_flag(ui_Q4P18HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                            if (player_is_finished[2])
                                lv_obj_clear_flag(ui_Q4P18HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);
                            if (player_is_finished[3])
                                lv_obj_clear_flag(ui_Q4P18HGSP4SCrown, LV_OBJ_FLAG_HIDDEN);
                        }
                    }
                    break;
            }
            break;

        case GAME_MODE_VEGAS:
            DEBUG_INFO(MODULE_GAME, "Vegas Quota - checking completion for %d players",
                       num_players);

            for (uint8_t i = 0; i < num_players; i++)
            {
                if (quota_player_completed(&players[i]))
                {
                    player_is_finished[i] = 1;
                    DEBUG_INFO(MODULE_GAME, "Vegas Player %d completed quota!", i + 1);
                }
                else
                {
                    player_is_finished[i] = 0;
                    DEBUG_DEBUG(MODULE_GAME, "Vegas Player %d quota remaining (3pt:%d, 4pt:%d, 5pt:%d)",
                                i + 1, players[i].par3_count, players[i].par4_count,
                                players[i].par5_count);
                }
            }

            switch (num_players)
            {
                case 1:
                    if (player_is_finished[0])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "1P Vegas Quota complete - Score:%d",
                                   players[0].score);
                        PLAY_PLAYER1WINS_WAV;
                    }
                    break;
                case 2:
                    if (player_is_finished[0] && !player_is_finished[1])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "Vegas P1 completed first! Score:%d vs P2 Score:%d",
                                   players[0].score, players[1].score);
                        PLAY_PLAYER1WINS_WAV;
                        if (current_hole_mode == NINE_HOLES)
                            lv_obj_clear_flag(ui_VQ2P9HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                        else
                            lv_obj_clear_flag(ui_VQ2P18HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                    }
                    else if (player_is_finished[1] && !player_is_finished[0])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "Vegas P2 completed first! Score:%d vs P1 Score:%d",
                                   players[1].score, players[0].score);
                        PLAY_PLAYER2WINS_WAV;
                        if (current_hole_mode == NINE_HOLES)
                            lv_obj_clear_flag(ui_VQ2P9HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                        else
                            lv_obj_clear_flag(ui_VQ2P18HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                    }
                    else if (player_is_finished[0] && player_is_finished[1])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME,
                                   "Vegas both completed! P1 Score:%d, P2 Score:%d",
                                   players[0].score, players[1].score);
                        if (players[0].score > players[1].score)
                        {
                            PLAY_PLAYER1WINS_WAV;
                            if (current_hole_mode == NINE_HOLES)
                                lv_obj_clear_flag(ui_VQ2P9HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                            else
                                lv_obj_clear_flag(ui_VQ2P18HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                        }
                        else if (players[1].score > players[0].score)
                        {
                            PLAY_PLAYER2WINS_WAV;
                            if (current_hole_mode == NINE_HOLES)
                                lv_obj_clear_flag(ui_VQ2P9HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                            else
                                lv_obj_clear_flag(ui_VQ2P18HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                        }
                        else
                        {
                            // Tie - both crowns
                            if (current_hole_mode == NINE_HOLES)
                            {
                                lv_obj_clear_flag(ui_VQ2P9HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                lv_obj_clear_flag(ui_VQ2P9HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                            }
                            else
                            {
                                lv_obj_clear_flag(ui_VQ2P18HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                lv_obj_clear_flag(ui_VQ2P18HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                            }
                        }
                    }
                    break;
                case 3:
                    // Check each player individually for first-to-finish
                    if (player_is_finished[0] && !player_is_finished[1] && !player_is_finished[2])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "Vegas P1 completed first! Score:%d",
                                   players[0].score);
                        PLAY_PLAYER1WINS_WAV;
                        if (current_hole_mode == NINE_HOLES)
                            lv_obj_clear_flag(ui_VQ3P9HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                        else
                            lv_obj_clear_flag(ui_VQ3P18HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                    }
                    else if (player_is_finished[1] && !player_is_finished[0] && !player_is_finished[2])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "Vegas P2 completed first! Score:%d",
                                   players[1].score);
                        PLAY_PLAYER2WINS_WAV;
                        if (current_hole_mode == NINE_HOLES)
                            lv_obj_clear_flag(ui_VQ3P9HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                        else
                            lv_obj_clear_flag(ui_VQ3P18HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                    }
                    else if (player_is_finished[2] && !player_is_finished[0] && !player_is_finished[1])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "Vegas P3 completed first! Score:%d",
                                   players[2].score);
                        PLAY_PLAYER3WINS_WAV;
                        if (current_hole_mode == NINE_HOLES)
                            lv_obj_clear_flag(ui_VQ3P9HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);
                        else
                            lv_obj_clear_flag(ui_VQ3P18HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);
                    }
                    else if (player_is_finished[0] || player_is_finished[1] || player_is_finished[2])
                    {
                        // Multiple players finished simultaneously - compare scores
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME,
                                   "Vegas multiple completed! P1:%d(%d) P2:%d(%d) P3:%d(%d)",
                                   player_is_finished[0], players[0].score,
                                   player_is_finished[1], players[1].score,
                                   player_is_finished[2], players[2].score);

                        // Find highest score among finished players
                        int max_score = -1;
                        for (int i = 0; i < 3; i++)
                        {
                            if (player_is_finished[i] && players[i].score > max_score)
                                max_score = players[i].score;
                        }

                        // Crown all finished players with the highest score
                        if (current_hole_mode == NINE_HOLES)
                        {
                            if (player_is_finished[0] && players[0].score == max_score)
                                lv_obj_clear_flag(ui_VQ3P9HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                            if (player_is_finished[1] && players[1].score == max_score)
                                lv_obj_clear_flag(ui_VQ3P9HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                            if (player_is_finished[2] && players[2].score == max_score)
                                lv_obj_clear_flag(ui_VQ3P9HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);
                        }
                        else
                        {
                            if (player_is_finished[0] && players[0].score == max_score)
                                lv_obj_clear_flag(ui_VQ3P18HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                            if (player_is_finished[1] && players[1].score == max_score)
                                lv_obj_clear_flag(ui_VQ3P18HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                            if (player_is_finished[2] && players[2].score == max_score)
                                lv_obj_clear_flag(ui_VQ3P18HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);
                        }
                    }
                    break;
                case 4:
                    // Check each player individually for first-to-finish
                    if (player_is_finished[0] && !player_is_finished[1] &&
                        !player_is_finished[2] && !player_is_finished[3])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "Vegas P1 completed first! Score:%d",
                                   players[0].score);
                        PLAY_PLAYER1WINS_WAV;
                        if (current_hole_mode == NINE_HOLES)
                            lv_obj_clear_flag(ui_VQ4P9HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                        else
                            lv_obj_clear_flag(ui_VQ4P18HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                    }
                    else if (player_is_finished[1] && !player_is_finished[0] &&
                             !player_is_finished[2] && !player_is_finished[3])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "Vegas P2 completed first! Score:%d",
                                   players[1].score);
                        PLAY_PLAYER2WINS_WAV;
                        if (current_hole_mode == NINE_HOLES)
                            lv_obj_clear_flag(ui_VQ4P9HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                        else
                            lv_obj_clear_flag(ui_VQ4P18HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                    }
                    else if (player_is_finished[2] && !player_is_finished[0] &&
                             !player_is_finished[1] && !player_is_finished[3])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "Vegas P3 completed first! Score:%d",
                                   players[2].score);
                        PLAY_PLAYER3WINS_WAV;
                        if (current_hole_mode == NINE_HOLES)
                            lv_obj_clear_flag(ui_VQ4P9HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);
                        else
                            lv_obj_clear_flag(ui_VQ4P18HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);
                    }
                    else if (player_is_finished[3] && !player_is_finished[0] &&
                             !player_is_finished[1] && !player_is_finished[2])
                    {
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME, "Vegas P4 completed first! Score:%d",
                                   players[3].score);
                        PLAY_PLAYER4WINS_WAV;
                        if (current_hole_mode == NINE_HOLES)
                            lv_obj_clear_flag(ui_VQ4P9HGSP4SCrown, LV_OBJ_FLAG_HIDDEN);
                        else
                            lv_obj_clear_flag(ui_VQ4P18HGSP4SCrown, LV_OBJ_FLAG_HIDDEN);
                    }
                    else if (player_is_finished[0] || player_is_finished[1] ||
                             player_is_finished[2] || player_is_finished[3])
                    {
                        // Multiple players finished simultaneously - compare scores
                        update_flag = 1;
                        set_sensors_enabled(0);
                        DEBUG_INFO(MODULE_GAME,
                                   "Vegas 4P multiple completed! P1:%d(%d) P2:%d(%d) P3:%d(%d) P4:%d(%d)",
                                   player_is_finished[0], players[0].score,
                                   player_is_finished[1], players[1].score,
                                   player_is_finished[2], players[2].score,
                                   player_is_finished[3], players[3].score);

                        // Find highest score among finished players
                        int max_score = -1;
                        for (int i = 0; i < 4; i++)
                        {
                            if (player_is_finished[i] && players[i].score > max_score)
                                max_score = players[i].score;
                        }

                        // Crown all finished players with the highest score
                        if (current_hole_mode == NINE_HOLES)
                        {
                            if (player_is_finished[0] && players[0].score == max_score)
                                lv_obj_clear_flag(ui_VQ4P9HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                            if (player_is_finished[1] && players[1].score == max_score)
                                lv_obj_clear_flag(ui_VQ4P9HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                            if (player_is_finished[2] && players[2].score == max_score)
                                lv_obj_clear_flag(ui_VQ4P9HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);
                            if (player_is_finished[3] && players[3].score == max_score)
                                lv_obj_clear_flag(ui_VQ4P9HGSP4SCrown, LV_OBJ_FLAG_HIDDEN);
                        }
                        else
                        {
                            if (player_is_finished[0] && players[0].score == max_score)
                                lv_obj_clear_flag(ui_VQ4P18HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                            if (player_is_finished[1] && players[1].score == max_score)
                                lv_obj_clear_flag(ui_VQ4P18HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                            if (player_is_finished[2] && players[2].score == max_score)
                                lv_obj_clear_flag(ui_VQ4P18HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);
                            if (player_is_finished[3] && players[3].score == max_score)
                                lv_obj_clear_flag(ui_VQ4P18HGSP4SCrown, LV_OBJ_FLAG_HIDDEN);
                        }
                    }
                    break;
            }
            break;

        case GAME_MODE_MATCH_PLAY:
            DEBUG_INFO(MODULE_GAME,
                       "Match Play - checking completion for %d players on hole mode %d",
                       num_players, current_hole_mode);

            for (uint8_t i = 0; i < num_players; i++)
            {
                // Check if player has completed the current hole
                if (players[i].current_hole > current_hole_mode + 1 ||
                    (players[i].current_hole == current_hole_mode + 1 &&
                     players[i].detection_count == 0))
                {
                    /***************************************** */
                    players[i].round_total_score = 0;
                    /***************************************** */
                    player_is_finished[i] = 1;
                    DEBUG_INFO(MODULE_GAME, "Player %d FINISHED hole %d", i + 1, current_hole_mode);
                }
                else
                {
                    player_is_finished[i] = 0;
                    all_done = 0;
                    DEBUG_DEBUG(MODULE_GAME, "Player %d NOT finished (Hole:%d Detections:%d)",
                                i + 1, players[i].current_hole, players[i].detection_count);
                }
            }

            // Check for early win
            // Note: current_hole has already been incremented, so add 1 to get correct remaining
            int lead            = players[0].holes_won - players[1].holes_won;
            int holes_remaining = current_hole_mode - players[0].current_hole + 1;

            DEBUG_DEBUG(MODULE_GAME, "Match Play status - Lead: %d, Holes remaining: %d", lead,
                        holes_remaining);

            // Show match status for both players
            if (lead > 0)
                DEBUG_INFO(MODULE_GAME, "Match status: Player 1: %dUP | Player 2: %dDN", lead,
                           lead);
            else if (lead < 0)
                DEBUG_INFO(MODULE_GAME, "Match status: Player 1: %dDN | Player 2: %dUP", -lead,
                           -lead);
            else
                DEBUG_INFO(MODULE_GAME, "Match status: All Square (E)");

            if (lead > holes_remaining)
            {
                all_done = 1;
                player_is_finished[0] = 1;
                player_is_finished[1] = 1;
                DISABLE_SENSORS

                if (current_match_play_mode == MATCH_PLAY_MODE_1V1)
                {
                    DEBUG_INFO(MODULE_GAME, "Game ends early: Player 1 wins %d&%d", lead,
                               holes_remaining);
                    DEBUG_INFO(MODULE_GAME,
                               "Early victory - Player 1 wins the match - sensors disabled");
                    PLAY_PLAYER1WINS_WAV;
                    // Update F column with X&Y format for early victory
                    if (current_hole_mode == NINE_HOLES)
                    {
                        lv_label_set_text_fmt(ui_MP1V19HScP1STextF, "%d&%d", lead, holes_remaining);
                        lv_label_set_text(ui_MP1V19HScP2STextF, "-");
                    }
                    else if (current_hole_mode == EIGHTEEN_HOLES)
                    {
                        lv_label_set_text_fmt(ui_MP1V118HScP1STextF, "%d&%d", lead,
                                              holes_remaining);
                        lv_label_set_text(ui_MP1V118HScP2STextF, "-");
                    }
                }
                else if (current_match_play_mode == MATCH_PLAY_MODE_2V2)
                {
                    DEBUG_INFO(MODULE_GAME, "Game ends early: Team 1 wins %d&%d", lead,
                               holes_remaining);
                    DEBUG_INFO(MODULE_GAME,
                               "Early victory - Team 1 wins the match - sensors disabled");
                    PLAY_TEAM1WINS_WAV;
                    // Update F column with X&Y format for early victory
                    if (current_hole_mode == NINE_HOLES)
                    {
                        lv_label_set_text_fmt(ui_MP2V29HScT1STextF, "%d&%d", lead, holes_remaining);
                        lv_label_set_text(ui_MP2V29HScT2STextF, "-");
                    }
                    else if (current_hole_mode == EIGHTEEN_HOLES)
                    {
                        lv_label_set_text_fmt(ui_MP2V218HScT1STextF, "%d&%d", lead,
                                              holes_remaining);
                        lv_label_set_text(ui_MP2V218HScT2STextF, "-");
                    }
                }
            }
            else if (-lead > holes_remaining)
            {
                all_done = 1;
                player_is_finished[0] = 1;
                player_is_finished[1] = 1;
                DISABLE_SENSORS

                if (current_match_play_mode == MATCH_PLAY_MODE_1V1)
                {
                    DEBUG_INFO(MODULE_GAME, "Game ends early: Player 2 wins %d&%d", -lead,
                               holes_remaining);
                    DEBUG_INFO(MODULE_GAME,
                               "Early victory - Player 2 wins the match - sensors disabled");
                    PLAY_PLAYER2WINS_WAV;
                    // Update F column with X&Y format for early victory
                    if (current_hole_mode == NINE_HOLES)
                    {
                        lv_label_set_text(ui_MP1V19HScP1STextF, "-");
                        lv_label_set_text_fmt(ui_MP1V19HScP2STextF, "%d&%d", -lead,
                                              holes_remaining);
                    }
                    else if (current_hole_mode == EIGHTEEN_HOLES)
                    {
                        lv_label_set_text(ui_MP1V118HScP1STextF, "-");
                        lv_label_set_text_fmt(ui_MP1V118HScP2STextF, "%d&%d", -lead,
                                              holes_remaining);
                    }
                }
                else if (current_match_play_mode == MATCH_PLAY_MODE_2V2)
                {
                    DEBUG_INFO(MODULE_GAME, "Game ends early: Team 2 wins %d&%d", -lead,
                               holes_remaining);
                    DEBUG_INFO(MODULE_GAME,
                               "Early victory - Team 2 wins the match - sensors disabled");
                    PLAY_TEAM2WINS_WAV;
                    // Update F column with X&Y format for early victory
                    if (current_hole_mode == NINE_HOLES)
                    {
                        lv_label_set_text(ui_MP2V29HScT1STextF, "-");
                        lv_label_set_text_fmt(ui_MP2V29HScT2STextF, "%d&%d", -lead,
                                              holes_remaining);
                    }
                    else if (current_hole_mode == EIGHTEEN_HOLES)
                    {
                        lv_label_set_text(ui_MP2V218HScT1STextF, "-");
                        lv_label_set_text_fmt(ui_MP2V218HScT2STextF, "%d&%d", -lead,
                                              holes_remaining);
                    }
                }
            }
            else
            {
                all_done = 0;
                DEBUG_TRACE(MODULE_GAME, "Match continues - no early victory condition met");
            }

            update_flag = all_done ? 1 : 0;

            if (update_flag)
            {
                DEBUG_INFO(MODULE_GAME, "Match completed - all players finished hole %d",
                           current_hole_mode);
                print_final_scores_and_winner();  // All holes are done

                // Navigate to scorecard and announce winner for completed matches
                if (current_match_play_mode == MATCH_PLAY_MODE_1V1)
                {
                    // Announce winner for 1v1 (if not early victory - already announced above)
                    if (lead == 0 && -lead == 0)
                    {
                        // Tie game - determine by total score or holes won
                        if (players[0].holes_won > players[1].holes_won)
                        {
                            PLAY_PLAYER1WINS_WAV;
                        }
                        else if (players[1].holes_won > players[0].holes_won)
                        {
                            PLAY_PLAYER2WINS_WAV;
                        }
                    }

                    if (current_hole_mode == NINE_HOLES)
                    {
                        _ui_screen_change(&ui_MP1V19HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_MP1V19HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 1v1 9H scorecard");
                    }
                    else if (current_hole_mode == EIGHTEEN_HOLES)
                    {
                        _ui_screen_change(&ui_MP1V118HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_MP1V118HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 1v1 18H scorecard");
                    }
                }
                else if (current_match_play_mode == MATCH_PLAY_MODE_2V2)
                {
                    // Announce winner for 2v2 (if not early victory - already announced above)
                    if (lead == 0 && -lead == 0)
                    {
                        // Tie game - determine by total score or holes won
                        if (players[0].holes_won > players[1].holes_won)
                        {
                            PLAY_TEAM1WINS_WAV;
                        }
                        else if (players[1].holes_won > players[0].holes_won)
                        {
                            PLAY_TEAM2WINS_WAV;
                        }
                    }

                    if (current_hole_mode == NINE_HOLES)
                    {
                        _ui_screen_change(&ui_MP2V29HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_MP2V29HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 2v2 9H scorecard");
                    }
                    else if (current_hole_mode == EIGHTEEN_HOLES)
                    {
                        _ui_screen_change(&ui_MP2V218HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_MP2V218HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 2v2 18H scorecard");
                    }
                }
            }
            else
            {
                DEBUG_TRACE(MODULE_GAME, "Match continues - players still competing");
            }
            break;

        default:
            DEBUG_WARN(MODULE_GAME, "Unknown game mode in completion check: %d", gameMode);
            break;
    }

    DEBUG_TRACE(MODULE_GAME,
                "Completion check finished - all_done: %d, update_flag: %d",
                all_done, update_flag);
}
/**
 * @brief Prints all players' final scores and announces the winner or a tie.
 */
/**
 * @brief Prints all players' final scores and announces the winner or a tie.
 *        Assumes higher score is better.
 */
/**
 * @brief Prints all players' final scores, announces the winner, and shows their crown.
 *        Assumes higher score is better.
 */
void print_final_scores_and_winner(void)
{
    DEBUG_INFO(MODULE_GAME, "Calculating final scores and determining winner");

    int best_score   = players[0].score;
    int winner_index = 0;
    int tie          = 0;

    // === Determine winner based on game mode ===
    if (current_game_mode == GAME_MODE_MATCH_PLAY)
    {
        // For Match Play, winner is determined by holes won (upAndDown)
        DEBUG_INFO(MODULE_GAME, "=== Match Play Final Result ===");
        DEBUG_INFO(MODULE_GAME, "Player 1: %d holes won, upAndDown: %d", players[0].holes_won,
                   players[0].upAndDown);
        DEBUG_INFO(MODULE_GAME, "Player 2: %d holes won, upAndDown: %d", players[1].holes_won,
                   players[1].upAndDown);

        if (players[0].upAndDown > 0)
        {
            winner_index = 0;
            tie          = 0;
            DISABLE_SENSORS
            DEBUG_INFO(MODULE_GAME, "Player 1 wins - %dUP - sensors disabled",
                       players[0].upAndDown);
        }
        else if (players[0].upAndDown < 0)
        {
            winner_index = 1;
            tie          = 0;
            DISABLE_SENSORS
            DEBUG_INFO(MODULE_GAME, "Player 2 wins - %dUP - sensors disabled",
                       -players[0].upAndDown);
        }
        else
        {
            tie = 1;
            DISABLE_SENSORS
            DEBUG_INFO(MODULE_GAME, "Match is tied - All Square - sensors disabled");
        }
    }
    else
    {
        // For Stroke Play and other modes, winner is determined by score
        DEBUG_INFO(MODULE_GAME, "=== Final Scores ===");
        for (uint8_t i = 0; i < num_players; i++)
        {
            DEBUG_INFO(MODULE_GAME, "Player %d: Total Score = %d", i + 1, players[i].score);

            if (players[i].score > best_score)
            {
                best_score   = players[i].score;
                winner_index = i;
                tie          = 0;
                DEBUG_DEBUG(MODULE_GAME, "New leader: Player %d with score %d", winner_index + 1,
                            best_score);
            }
            else if (players[i].score == best_score && i != winner_index)
            {
                tie = 1;
                DEBUG_DEBUG(MODULE_GAME, "Tie detected between Player %d and Player %d",
                            winner_index + 1, i + 1);
            }
        }
    }

    // === Game Result ===
    DEBUG_INFO(MODULE_GAME, "=== Game Result ===");

    if (tie)
    {
        DEBUG_INFO(MODULE_GAME, "It's a tie! 🎯");
    }
    else
    {
        DEBUG_INFO(MODULE_GAME, "🏆 Player %d is the WINNER with a score of %d!", winner_index + 1,
                   best_score);

        switch (current_game_mode)
        {
            case GAME_MODE_STROKE_PLAY:
                DEBUG_DEBUG(MODULE_UI, "Stroke Play - showing winner crown for player %d",
                            winner_index + 1);
                switch (num_players)
                {
                    case 2:
                        switch (current_hole_mode)
                        {
                            case NINE_HOLES:
                                switch (winner_index)
                                {
                                    case 0:
                                        lv_obj_clear_flag(ui_SP2P9HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP2P9HScP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                        PLAY_PLAYER1WINS_WAV;
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 1 - 9 holes, 2 players");
                                        break;
                                    case 1:
                                        lv_obj_clear_flag(ui_SP2P9HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP2P9HScP2SCrown, LV_OBJ_FLAG_HIDDEN);
                                        PLAY_PLAYER2WINS_WAV;
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 2 - 9 holes, 2 players");
                                        break;
                                }
                                // Navigate to scorecard
                                _ui_screen_change(&ui_SP2P9HScorecard, LV_SCR_LOAD_ANIM_FADE_ON,
                                                  500, 0, &ui_SP2P9HScorecard_screen_init);
                                DEBUG_INFO(MODULE_UI, "Navigating to 2P 9H scorecard");
                                break;

                            case EIGHTEEN_HOLES:
                                switch (winner_index)
                                {
                                    case 0:
                                        lv_obj_clear_flag(ui_SP2P18HGSP1SPCrown,
                                                          LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP2P18HScP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                        PLAY_PLAYER1WINS_WAV;
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 1 - 18 holes, 2 players");
                                        break;
                                    case 1:
                                        lv_obj_clear_flag(ui_SP2P18HGSP2SPCrown,
                                                          LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP2P18HScP2SCrown, LV_OBJ_FLAG_HIDDEN);
                                        PLAY_PLAYER2WINS_WAV;
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 2 - 18 holes, 2 players");
                                        break;
                                }
                                // Navigate to scorecard
                                _ui_screen_change(&ui_SP2P18HScorecard, LV_SCR_LOAD_ANIM_FADE_ON,
                                                  500, 0, &ui_SP2P18HScorecard_screen_init);
                                DEBUG_INFO(MODULE_UI, "Navigating to 2P 18H scorecard");
                                break;
                        }
                        break;

                    case 3:
                        DEBUG_DEBUG(MODULE_UI, "3 players - showing winner crown");
                        switch (current_hole_mode)
                        {
                            case NINE_HOLES:
                                switch (winner_index)
                                {
                                    case 0:
                                        lv_obj_clear_flag(ui_SP3P9HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP3P9HScP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 1 - 9 holes, 3 players");
                                        break;
                                    case 1:
                                        lv_obj_clear_flag(ui_SP3P9HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP3P9HScP2SCrown, LV_OBJ_FLAG_HIDDEN);
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 2 - 9 holes, 3 players");
                                        break;
                                    case 2:
                                        lv_obj_clear_flag(ui_SP3P9HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP3P9HScP3SCrown, LV_OBJ_FLAG_HIDDEN);
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 3 - 9 holes, 3 players");
                                        break;
                                }
                                break;

                            case EIGHTEEN_HOLES:
                                switch (winner_index)
                                {
                                    case 0:
                                        lv_obj_clear_flag(ui_SP3P18HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP3P18HScP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 1 - 18 holes, 3 players");
                                        break;
                                    case 1:
                                        lv_obj_clear_flag(ui_SP3P18HGSP2Crown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP3P18HScP2SCrown, LV_OBJ_FLAG_HIDDEN);
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 2 - 18 holes, 3 players");
                                        break;
                                    case 2:
                                        lv_obj_clear_flag(ui_SP3P18HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP3P18HScP3SCrown, LV_OBJ_FLAG_HIDDEN);
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 3 - 18 holes, 3 players");
                                        break;
                                }
                                break;
                        }
                        break;

                    case 4:
                        DEBUG_DEBUG(MODULE_UI, "4 players - showing winner crown");
                        switch (current_hole_mode)
                        {
                            case NINE_HOLES:
                                switch (winner_index)
                                {
                                    case 0:
                                        lv_obj_clear_flag(ui_SP4P9HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP4P9HScP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 1 - 9 holes, 4 players");
                                        break;
                                    case 1:
                                        lv_obj_clear_flag(ui_SP4P9HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP4P9HScP2SCrown, LV_OBJ_FLAG_HIDDEN);
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 2 - 9 holes, 4 players");
                                        break;
                                    case 2:
                                        lv_obj_clear_flag(ui_SP4P9HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP4P9HScP3SCrown, LV_OBJ_FLAG_HIDDEN);
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 3 - 9 holes, 4 players");
                                        break;
                                    case 3:
                                        lv_obj_clear_flag(ui_SP4P9HGSP4SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP4P9HScP4SCrown, LV_OBJ_FLAG_HIDDEN);
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 4 - 9 holes, 4 players");
                                        break;
                                }
                                break;

                            case EIGHTEEN_HOLES:
                                switch (winner_index)
                                {
                                    case 0:
                                        lv_obj_clear_flag(ui_SP4P18HScP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP4P18HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 1 - 18 holes, 4 players");
                                        break;
                                    case 1:
                                        lv_obj_clear_flag(ui_SP4P18HScP2SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP4P18HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 2 - 18 holes, 4 players");
                                        break;
                                    case 2:
                                        lv_obj_clear_flag(ui_SP4P18HScP3SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP4P18HGSP3SCrown, LV_OBJ_FLAG_HIDDEN);
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 3 - 18 holes, 4 players");
                                        break;
                                    case 3:
                                        lv_obj_clear_flag(ui_SP4P18HScP4SCrown, LV_OBJ_FLAG_HIDDEN);
                                        lv_obj_clear_flag(ui_SP4P18HGSP4SCrown, LV_OBJ_FLAG_HIDDEN);
                                        DEBUG_DEBUG(
                                            MODULE_UI,
                                            "Showing crown for Player 4 - 18 holes, 4 players");
                                        break;
                                }
                                break;
                        }
                        break;

                    default:
                        DEBUG_WARN(MODULE_GAME, "Unsupported number of players: %d", num_players);
                        break;
                }
                break;

            // === Other Game Modes ===
            case GAME_MODE_MATCH_PLAY:
                DEBUG_DEBUG(MODULE_GAME, "Match Play winner determined - Player %d",
                            winner_index + 1);

                // Display crown for Match Play winner
                if (current_match_play_mode == MATCH_PLAY_MODE_1V1)
                {
                    if (current_hole_mode == NINE_HOLES)
                    {
                        switch (winner_index)
                        {
                            case 0:
                                lv_obj_clear_flag(ui_MP1V19HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                lv_obj_clear_flag(ui_MP1V19HScP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                PLAY_PLAYER1WINS_WAV;
                                DEBUG_DEBUG(MODULE_UI,
                                            "Showing crown for Player 1 - Match Play 1v1 9H");
                                break;
                            case 1:
                                lv_obj_clear_flag(ui_MP1V19HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                                lv_obj_clear_flag(ui_MP1V19HScP2SCrown, LV_OBJ_FLAG_HIDDEN);
                                PLAY_PLAYER2WINS_WAV;
                                DEBUG_DEBUG(MODULE_UI,
                                            "Showing crown for Player 2 - Match Play 1v1 9H");
                                break;
                        }
                    }
                    else if (current_hole_mode == EIGHTEEN_HOLES)
                    {
                        switch (winner_index)
                        {
                            case 0:
                                lv_obj_clear_flag(ui_MP1V118HGSP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                lv_obj_clear_flag(ui_MP1V118HScP1SCrown, LV_OBJ_FLAG_HIDDEN);
                                PLAY_PLAYER1WINS_WAV;
                                DEBUG_DEBUG(MODULE_UI,
                                            "Showing crown for Player 1 - Match Play 1v1 18H");
                                break;
                            case 1:
                                lv_obj_clear_flag(ui_MP1V118HGSP2SCrown, LV_OBJ_FLAG_HIDDEN);
                                lv_obj_clear_flag(ui_MP1V118HScP2SCrown, LV_OBJ_FLAG_HIDDEN);
                                PLAY_PLAYER2WINS_WAV;
                                DEBUG_DEBUG(MODULE_UI,
                                            "Showing crown for Player 2 - Match Play 1v1 18H");
                                break;
                        }
                    }
                }
                else if (current_match_play_mode == MATCH_PLAY_MODE_2V2)
                {
                    if (current_hole_mode == NINE_HOLES)
                    {
                        switch (winner_index)
                        {
                            case 0:
                                lv_obj_clear_flag(ui_MP2V29HGST1SCrown, LV_OBJ_FLAG_HIDDEN);
                                lv_obj_clear_flag(ui_MP2V29HScT1SCrown, LV_OBJ_FLAG_HIDDEN);
                                DEBUG_DEBUG(MODULE_UI,
                                            "Showing crown for Team 1 - Match Play 2v2 9H");
                                break;
                            case 1:
                                lv_obj_clear_flag(ui_MP2V29HGST2SCrown, LV_OBJ_FLAG_HIDDEN);
                                lv_obj_clear_flag(ui_MP2V29HScT2SCrown, LV_OBJ_FLAG_HIDDEN);
                                DEBUG_DEBUG(MODULE_UI,
                                            "Showing crown for Team 2 - Match Play 2v2 9H");
                                break;
                        }
                    }
                    else if (current_hole_mode == EIGHTEEN_HOLES)
                    {
                        switch (winner_index)
                        {
                            case 0:
                                lv_obj_clear_flag(ui_MP2V218HGST1SCrown, LV_OBJ_FLAG_HIDDEN);
                                lv_obj_clear_flag(ui_MP2V218HScT1SCrown, LV_OBJ_FLAG_HIDDEN);
                                DEBUG_DEBUG(MODULE_UI,
                                            "Showing crown for Team 1 - Match Play 2v2 18H");
                                break;
                            case 1:
                                lv_obj_clear_flag(ui_MP2V218HGST2SCrown, LV_OBJ_FLAG_HIDDEN);
                                lv_obj_clear_flag(ui_MP2V218HScT2SCrown, LV_OBJ_FLAG_HIDDEN);
                                DEBUG_DEBUG(MODULE_UI,
                                            "Showing crown for Team 2 - Match Play 2v2 18H");
                                break;
                        }
                    }
                }

                // Navigate to scorecard screen based on mode
                if (current_match_play_mode == MATCH_PLAY_MODE_1V1)
                {
                    if (current_hole_mode == NINE_HOLES)
                    {
                        _ui_screen_change(&ui_MP1V19HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_MP1V19HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 1v1 9H scorecard");
                    }
                    else if (current_hole_mode == EIGHTEEN_HOLES)
                    {
                        _ui_screen_change(&ui_MP1V118HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_MP1V118HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 1v1 18H scorecard");
                    }
                }
                else if (current_match_play_mode == MATCH_PLAY_MODE_2V2)
                {
                    if (current_hole_mode == NINE_HOLES)
                    {
                        _ui_screen_change(&ui_MP2V29HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_MP2V29HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 2v2 9H scorecard");
                    }
                    else if (current_hole_mode == EIGHTEEN_HOLES)
                    {
                        _ui_screen_change(&ui_MP2V218HScorecard, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_MP2V218HScorecard_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to 2v2 18H scorecard");
                    }
                }
                break;

            case GAME_MODE_QUOTA:
                DEBUG_DEBUG(MODULE_GAME, "Quota Points mode completed");
                switch (num_players)
                {
                    case 1:
                        DEBUG_INFO(MODULE_GAME,
                                   "1P Quota complete - Score:%d (3pt:%d, 4pt:%d, 5pt:%d)",
                                   players[0].score, players[0].par3_count, players[0].par4_count,
                                   players[0].par5_count);
                        // Navigate back to home screen for 1 player
                        _ui_screen_change(&ui_HScreen, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0,
                                          &ui_HScreen_screen_init);
                        DEBUG_INFO(MODULE_UI, "Navigating to home screen (Quota 1P complete)");
                        break;
                }
                break;

            case GAME_MODE_VEGAS:
                DEBUG_DEBUG(MODULE_GAME, "Vegas mode winner determined - Player %d",
                            winner_index + 1);
                // TODO: Add switch(num_players) logic per game mode
                break;

            default:
                DEBUG_ERROR(MODULE_GAME, "Unknown game mode: %d", current_game_mode);
                break;
        }
    }

    DEBUG_INFO(MODULE_GAME, "Winner determination completed");
}
void logic_update_label_text(int player_index, int current_hole, int score, int detection_count,
                             int nplayers)
{
    DEBUG_TRACE(MODULE_LOGIC, "logic_update_label_text called");
    DEBUG_DEBUG(MODULE_LOGIC, "Player:%d, Hole:%d, Score:%d, Detections:%d, Players:%d",
                player_index + 1, current_hole + 1, score, detection_count, nplayers);

    // Return early if game is completed
    if (update_flag)
    {
        DEBUG_INFO(MODULE_LOGIC, "Update flag set, skipping label update");
        return;
    }

    update_player_highlight(detection_count, current_hole + 1, nplayers);

    // Use a switch statement for nplayers
    switch (current_game_mode)
    {
        case GAME_MODE_MATCH_PLAY:
            DEBUG_TRACE(MODULE_LOGIC, "Match Play mode");
            switch (current_match_play_mode)
            {
                case MATCH_PLAY_MODE_1V1:
                {
                    DEBUG_TRACE(MODULE_LOGIC, "1v1 Match Play");
                    switch (current_hole_mode)
                    {
                        case NINE_HOLES:
                            DEBUG_TRACE(MODULE_LOGIC, "Nine Holes mode");
                            DEBUG_INFO(MODULE_LOGIC, "Player#%d Hole:%d - Ball%d",
                                       current_player_index, current_hole + 1, detection_count);

                            if (players[1].current_hole >= current_hole_mode)
                            {
                                DEBUG_DEBUG(MODULE_LOGIC,
                                            "Condition TRUE - players[1].current_hole (%d) >= "
                                            "current_hole_mode (%d)",
                                            players[1].current_hole, current_hole_mode);
                                lv_label_set_text_fmt(ui_MP1V19HGSHCPText, "%d",
                                                      current_hole_mode + 1);
                                DEBUG_INFO(MODULE_LOGIC, "Set ui_MP1V19HGSHCPText to: %d",
                                           current_hole_mode + 1);
                            }
                            else
                            {
                                DEBUG_DEBUG(MODULE_LOGIC,
                                            "Condition FALSE - players[1].current_hole (%d) < "
                                            "current_hole_mode (%d)",
                                            players[1].current_hole, current_hole_mode);
                                lv_label_set_text_fmt(ui_MP1V19HGSHCPText, "%d",
                                                      players[1].current_hole + 1);
                                DEBUG_INFO(MODULE_LOGIC, "Set ui_MP1V19HGSHCPText to: %d",
                                           players[1].current_hole + 1);
                            }

                            DEBUG_INFO(
                                MODULE_LOGIC,
                                "SCORE DISPLAY: Setting score value to %d on ui_MP1V19HGSHCPText",
                                score);
                            lv_label_set_text_fmt(ui_MP1V19HGSHCPText, "%d", current_hole + 1);

                            DEBUG_INFO(
                                MODULE_LOGIC,
                                "HOLE DISPLAY: Setting current hole to %d on ui_MP1V19HGSBCPText",
                                current_hole + 1);
                            lv_label_set_text_fmt(ui_MP1V19HGSBCPText, "%d", detection_count);
                            break;

                        case EIGHTEEN_HOLES:
                            DEBUG_TRACE(MODULE_LOGIC, "Eighteen Holes mode");
                            DEBUG_INFO(MODULE_LOGIC, "Player#%d Hole:%d - Ball%d",
                                       current_player_index + 1, current_hole + 1, detection_count);

                            // Update hole counter display
                            if (players[1].current_hole >= current_hole_mode)
                            {
                                DEBUG_DEBUG(MODULE_LOGIC,
                                            "Condition TRUE - players[1].current_hole (%d) >= "
                                            "current_hole_mode (%d)",
                                            players[1].current_hole, current_hole_mode);
                                lv_label_set_text_fmt(ui_MP1V118HGSHCPText, "%d",
                                                      current_hole_mode + 1);
                                DEBUG_INFO(MODULE_LOGIC, "Set ui_MP1V118HGSHCPText to: %d",
                                           current_hole_mode + 1);
                            }
                            else
                            {
                                DEBUG_DEBUG(MODULE_LOGIC,
                                            "Condition FALSE - players[1].current_hole (%d) < "
                                            "current_hole_mode (%d)",
                                            players[1].current_hole, current_hole_mode);
                                lv_label_set_text_fmt(ui_MP1V118HGSHCPText, "%d",
                                                      players[1].current_hole + 1);
                                DEBUG_INFO(MODULE_LOGIC, "Set ui_MP1V118HGSHCPText to: %d",
                                           players[1].current_hole + 1);
                            }

                            // Update current hole display
                            DEBUG_INFO(
                                MODULE_LOGIC,
                                "HOLE DISPLAY: Setting current hole to %d on ui_MP1V118HGSHCPText",
                                current_hole + 1);
                            lv_label_set_text_fmt(ui_MP1V118HGSHCPText, "%d", current_hole + 1);

                            // Update ball counter display
                            DEBUG_INFO(
                                MODULE_LOGIC,
                                "BALL DISPLAY: Setting ball count to %d on ui_MP1V118HGSBCPText",
                                detection_count);
                            lv_label_set_text_fmt(ui_MP1V118HGSBCPText, "%d", detection_count);
                            break;
                    }
                    break;
                }
                case MATCH_PLAY_MODE_2V2:
                {
                    DEBUG_TRACE(MODULE_LOGIC, "2v2 Match Play");
                    switch (current_hole_mode)
                    {
                        case NINE_HOLES:
                            DEBUG_TRACE(MODULE_LOGIC, "Nine Holes mode");
                            DEBUG_INFO(MODULE_LOGIC, "Team#%d Hole:%d - Ball%d",
                                       current_player_index + 1, current_hole + 1, detection_count);

                            // Update hole counter display
                            if (players[1].current_hole >= current_hole_mode)
                            {
                                lv_label_set_text_fmt(ui_MP2V29HGSHCPText, "%d",
                                                      current_hole_mode + 1);
                                DEBUG_INFO(MODULE_LOGIC, "Set ui_MP2V29HGSHCPText to: %d",
                                           current_hole_mode + 1);
                            }
                            else
                            {
                                lv_label_set_text_fmt(ui_MP2V29HGSHCPText, "%d",
                                                      players[1].current_hole + 1);
                                DEBUG_INFO(MODULE_LOGIC, "Set ui_MP2V29HGSHCPText to: %d",
                                           players[1].current_hole + 1);
                            }

                            // Update current hole and ball counter
                            lv_label_set_text_fmt(ui_MP2V29HGSHCPText, "%d", current_hole + 1);
                            lv_label_set_text_fmt(ui_MP2V29HGSBCPText, "%d", detection_count);
                            break;

                        case EIGHTEEN_HOLES:
                            DEBUG_TRACE(MODULE_LOGIC, "Eighteen Holes mode - 2v2");
                            DEBUG_INFO(MODULE_LOGIC, "Team#%d Hole:%d - Ball%d",
                                       current_player_index + 1, current_hole + 1, detection_count);

                            // Update hole counter display
                            if (players[1].current_hole >= current_hole_mode)
                            {
                                lv_label_set_text_fmt(ui_MP2V218HGSHCPText, "%d",
                                                      current_hole_mode + 1);
                                DEBUG_INFO(MODULE_LOGIC, "Set ui_MP2V218HGSHCPText to: %d",
                                           current_hole_mode + 1);
                            }
                            else
                            {
                                lv_label_set_text_fmt(ui_MP2V218HGSHCPText, "%d",
                                                      players[1].current_hole + 1);
                                DEBUG_INFO(MODULE_LOGIC, "Set ui_MP2V218HGSHCPText to: %d",
                                           players[1].current_hole + 1);
                            }

                            // Update current hole and ball counter
                            lv_label_set_text_fmt(ui_MP2V218HGSHCPText, "%d", current_hole + 1);
                            lv_label_set_text_fmt(ui_MP2V218HGSBCPText, "%d", detection_count);
                            break;
                    }
                    break;
                }
            }
            break;

        case GAME_MODE_QUOTA:
            DEBUG_TRACE(MODULE_LOGIC, "Quota mode");
            switch (current_hole_mode)
            {
                case NINE_HOLES:
                    switch (nplayers)
                    {
                        case 1:
                            DEBUG_TRACE(MODULE_LOGIC, "Quota 1P 9H");
                            lv_label_set_text_fmt(ui_Q1P9HGSBCPText, "%d", detection_count);
                            lv_label_set_text_fmt(ui_Q1P9HGSPSPar3PText, "%d",
                                                  players[player_index].par3_count);
                            lv_label_set_text_fmt(ui_Q1P9HGSPSPar4PText, "%d",
                                                  players[player_index].par4_count);
                            lv_label_set_text_fmt(ui_Q1P9HGSPSPar5PText, "%d",
                                                  players[player_index].par5_count);
                            DEBUG_INFO(MODULE_LOGIC,
                                       "Quota 1P 9H updated - Balls:%d, 3pt:%d, 4pt:%d, 5pt:%d",
                                       detection_count, players[player_index].par3_count,
                                       players[player_index].par4_count,
                                       players[player_index].par5_count);
                            break;
                        case 2:
                            DEBUG_TRACE(MODULE_LOGIC, "Quota 2P 9H");
                            lv_label_set_text_fmt(ui_Q2P9HGSBCPText, "%d", detection_count);
                            lv_label_set_text_fmt(ui_Q2P9HGSP1SPar3PText, "%d",
                                                  players[0].par3_count);
                            lv_label_set_text_fmt(ui_Q2P9HGSP1SPar4PText, "%d",
                                                  players[0].par4_count);
                            lv_label_set_text_fmt(ui_Q2P9HGSP1SPar5PText, "%d",
                                                  players[0].par5_count);
                            lv_label_set_text_fmt(ui_Q2P9HGSP2SPar3PText, "%d",
                                                  players[1].par3_count);
                            lv_label_set_text_fmt(ui_Q2P9HGSP2SPar4PText, "%d",
                                                  players[1].par4_count);
                            lv_label_set_text_fmt(ui_Q2P9HGSP2SPar5PText, "%d",
                                                  players[1].par5_count);
                            break;
                        case 3:
                            DEBUG_TRACE(MODULE_LOGIC, "Quota 3P 9H");
                            lv_label_set_text_fmt(ui_Q3P9HGSBCPText, "%d", detection_count);
                            lv_label_set_text_fmt(ui_Q3P9HGSP1SPar3PText, "%d",
                                                  players[0].par3_count);
                            lv_label_set_text_fmt(ui_Q3P9HGSP1SPar4PText, "%d",
                                                  players[0].par4_count);
                            lv_label_set_text_fmt(ui_Q3P9HGSP1SPar5PText, "%d",
                                                  players[0].par5_count);
                            lv_label_set_text_fmt(ui_Q3P9HGSP2SPar3PText, "%d",
                                                  players[1].par3_count);
                            lv_label_set_text_fmt(ui_Q3P9HGSP2SPar4PText, "%d",
                                                  players[1].par4_count);
                            lv_label_set_text_fmt(ui_Q3P9HGSP2SPar5PText, "%d",
                                                  players[1].par5_count);
                            lv_label_set_text_fmt(ui_Q3P9HGSP3SPar3PText, "%d",
                                                  players[2].par3_count);
                            lv_label_set_text_fmt(ui_Q3P9HGSP3SPar4PText, "%d",
                                                  players[2].par4_count);
                            lv_label_set_text_fmt(ui_Q3P9HGSP3SPar5PText, "%d",
                                                  players[2].par5_count);
                            break;
                        case 4:
                            DEBUG_TRACE(MODULE_LOGIC, "Quota 4P 9H");
                            lv_label_set_text_fmt(ui_Q4P9HGSBCPText, "%d", detection_count);
                            lv_label_set_text_fmt(ui_Q4P9HGSP1SPar3PText, "%d",
                                                  players[0].par3_count);
                            lv_label_set_text_fmt(ui_Q4P9HGSP1SPar4PText, "%d",
                                                  players[0].par4_count);
                            lv_label_set_text_fmt(ui_Q4P9HGSP1SPar5PText, "%d",
                                                  players[0].par5_count);
                            lv_label_set_text_fmt(ui_Q4P9HGSP2SPar3PText, "%d",
                                                  players[1].par3_count);
                            lv_label_set_text_fmt(ui_Q4P9HGSP2SPar4PText, "%d",
                                                  players[1].par4_count);
                            lv_label_set_text_fmt(ui_Q4P9HGSP2SPar5PText, "%d",
                                                  players[1].par5_count);
                            lv_label_set_text_fmt(ui_Q4P9HGSP3SPar3PText, "%d",
                                                  players[2].par3_count);
                            lv_label_set_text_fmt(ui_Q4P9HGSP3SPar4PText, "%d",
                                                  players[2].par4_count);
                            lv_label_set_text_fmt(ui_Q4P9HGSP3SPar5PText, "%d",
                                                  players[2].par5_count);
                            lv_label_set_text_fmt(ui_Q4P9HGSP4SPar3PText, "%d",
                                                  players[3].par3_count);
                            lv_label_set_text_fmt(ui_Q4P9HGSP4SPar4PText, "%d",
                                                  players[3].par4_count);
                            lv_label_set_text_fmt(ui_Q4P9HGSP4SPar5PText, "%d",
                                                  players[3].par5_count);
                            break;
                    }
                    break;
                case EIGHTEEN_HOLES:
                    switch (nplayers)
                    {
                        case 1:
                            DEBUG_TRACE(MODULE_LOGIC, "Quota 1P 18H");
                            lv_label_set_text_fmt(ui_Q1P18HGSBCPText, "%d", detection_count);
                            lv_label_set_text_fmt(ui_Q1P18HGSPSPar3PText, "%d",
                                                  players[player_index].par3_count);
                            lv_label_set_text_fmt(ui_Q1P18HGSPSPar4PText, "%d",
                                                  players[player_index].par4_count);
                            lv_label_set_text_fmt(ui_Q1P18HGSPSPar5PText, "%d",
                                                  players[player_index].par5_count);
                            DEBUG_INFO(MODULE_LOGIC,
                                       "Quota 1P 18H updated - Balls:%d, 3pt:%d, 4pt:%d, 5pt:%d",
                                       detection_count, players[player_index].par3_count,
                                       players[player_index].par4_count,
                                       players[player_index].par5_count);
                            break;
                        case 2:
                            DEBUG_TRACE(MODULE_LOGIC, "Quota 2P 18H");
                            lv_label_set_text_fmt(ui_Q2P18HGSBCPText, "%d", detection_count);
                            lv_label_set_text_fmt(ui_Q2P9HGSP1SPar3PText1, "%d",
                                                  players[0].par3_count);
                            lv_label_set_text_fmt(ui_Q2P9HGSP1SPar4PText1, "%d",
                                                  players[0].par4_count);
                            lv_label_set_text_fmt(ui_Q2P9HGSP1SPar5PText1, "%d",
                                                  players[0].par5_count);
                            lv_label_set_text_fmt(ui_Q2P18HGSP2SPar3PText, "%d",
                                                  players[1].par3_count);
                            lv_label_set_text_fmt(ui_Q2P18HGSP2SPar4PText, "%d",
                                                  players[1].par4_count);
                            lv_label_set_text_fmt(ui_Q2P18HGSP2SPar5PText, "%d",
                                                  players[1].par5_count);
                            break;
                        case 3:
                            DEBUG_TRACE(MODULE_LOGIC, "Quota 3P 18H");
                            lv_label_set_text_fmt(ui_Q3P18HGSBCPText, "%d", detection_count);
                            lv_label_set_text_fmt(ui_Q3P18HGSP1SPar3PText, "%d",
                                                  players[0].par3_count);
                            lv_label_set_text_fmt(ui_Q3P18HGSP1SPar4PText, "%d",
                                                  players[0].par4_count);
                            lv_label_set_text_fmt(ui_Q3P18HGSP1SPar5PText, "%d",
                                                  players[0].par5_count);
                            lv_label_set_text_fmt(ui_Q3P18HGSP2SPar3PText, "%d",
                                                  players[1].par3_count);
                            lv_label_set_text_fmt(ui_Q3P18HGSP2SPar4PText, "%d",
                                                  players[1].par4_count);
                            lv_label_set_text_fmt(ui_Q3P18HGSP2SPar5PText, "%d",
                                                  players[1].par5_count);
                            lv_label_set_text_fmt(ui_Q3P18HGSP3SPar3PText, "%d",
                                                  players[2].par3_count);
                            lv_label_set_text_fmt(ui_Q3P18HGSP3SPar4PText, "%d",
                                                  players[2].par4_count);
                            lv_label_set_text_fmt(ui_Q3P18HGSP3SPar5PText, "%d",
                                                  players[2].par5_count);
                            break;
                        case 4:
                            DEBUG_TRACE(MODULE_LOGIC, "Quota 4P 18H");
                            lv_label_set_text_fmt(ui_Q4P18HGSBCPText, "%d", detection_count);
                            lv_label_set_text_fmt(ui_Q4P18HGSP1SPar3PText, "%d",
                                                  players[0].par3_count);
                            lv_label_set_text_fmt(ui_Q4P18HGSP1SPar4PText, "%d",
                                                  players[0].par4_count);
                            lv_label_set_text_fmt(ui_Q4P18HGSP1SPar5PText, "%d",
                                                  players[0].par5_count);
                            lv_label_set_text_fmt(ui_Q4P18HGSP2SPar3PText, "%d",
                                                  players[1].par3_count);
                            lv_label_set_text_fmt(ui_Q4P18HGSP2SPar4PText, "%d",
                                                  players[1].par4_count);
                            lv_label_set_text_fmt(ui_Q4P18HGSP2SPar5PText, "%d",
                                                  players[1].par5_count);
                            lv_label_set_text_fmt(ui_Q4P18HGSP3SPar3PText, "%d",
                                                  players[2].par3_count);
                            lv_label_set_text_fmt(ui_Q4P18HGSP3SPar4PText, "%d",
                                                  players[2].par4_count);
                            lv_label_set_text_fmt(ui_Q4P18HGSP3SPar5PText, "%d",
                                                  players[2].par5_count);
                            lv_label_set_text_fmt(ui_Q4P18HGSP4SPar3PText, "%d",
                                                  players[3].par3_count);
                            lv_label_set_text_fmt(ui_Q4P18HGSP4SPar4PText, "%d",
                                                  players[3].par4_count);
                            lv_label_set_text_fmt(ui_Q4P18HGSP4SPar5PText, "%d",
                                                  players[3].par5_count);
                            break;
                    }
                    break;
            }
            break;

        case GAME_MODE_VEGAS:
            DEBUG_TRACE(MODULE_LOGIC, "Vegas Quota mode");
            switch (current_hole_mode)
            {
                case NINE_HOLES:
                    switch (nplayers)
                    {
                        case 1:
                            DEBUG_TRACE(MODULE_LOGIC, "Vegas Quota 1P 9H");
                            lv_label_set_text(ui_VQ1P9HGSBCPText, "-");
                            lv_label_set_text_fmt(ui_VQ1P9HGSPSPar3PText, "%d",
                                                  players[0].par3_count);
                            lv_label_set_text_fmt(ui_VQ1P9HGSPSPar4PText, "%d",
                                                  players[0].par4_count);
                            lv_label_set_text_fmt(ui_VQ1P9HGSPSPar5PText, "%d",
                                                  players[0].par5_count);
                            lv_label_set_text_fmt(ui_VQ1P9HGSPSCText, "%d",
                                                  players[0].score);
                            break;
                        case 2:
                            DEBUG_TRACE(MODULE_LOGIC, "Vegas Quota 2P 9H");
                            lv_label_set_text_fmt(ui_VQ2P9HGSBCPText, "%d",
                                                  SENSORS_PER_TURN - detection_count);
                            lv_label_set_text_fmt(ui_VQ2P9HGSP1SPar3PText, "%d",
                                                  players[0].par3_count);
                            lv_label_set_text_fmt(ui_VQ2P9HGSP1SPar4PText, "%d",
                                                  players[0].par4_count);
                            lv_label_set_text_fmt(ui_Q2P9HGSP1SPar5PText2, "%d",
                                                  players[0].par5_count);
                            lv_label_set_text_fmt(ui_VQ2P9HGSP1ScText, "%d",
                                                  players[0].score);
                            lv_label_set_text_fmt(ui_VQ2P9HGSP2SPar3PText, "%d",
                                                  players[1].par3_count);
                            lv_label_set_text_fmt(ui_VQ2P9HGSP2SPar4PText, "%d",
                                                  players[1].par4_count);
                            lv_label_set_text_fmt(ui_VQ2P9HGSP2SPar5PText, "%d",
                                                  players[1].par5_count);
                            lv_label_set_text_fmt(ui_VQ2P9HGSP2ScText, "%d",
                                                  players[1].score);
                            break;
                        case 3:
                            DEBUG_TRACE(MODULE_LOGIC, "Vegas Quota 3P 9H");
                            lv_label_set_text_fmt(ui_Q3P9HGSBCPText1, "%d",
                                                  SENSORS_PER_TURN - detection_count);
                            lv_label_set_text_fmt(ui_VQ3P9HGSP1SPar3PText, "%d",
                                                  players[0].par3_count);
                            lv_label_set_text_fmt(ui_VQ3P9HGSP1SPar4PText, "%d",
                                                  players[0].par4_count);
                            lv_label_set_text_fmt(ui_VQ3P9HGSP1SPar5PText, "%d",
                                                  players[0].par5_count);
                            lv_label_set_text_fmt(ui_VQ3P9HGSP1ScText, "%d",
                                                  players[0].score);
                            lv_label_set_text_fmt(ui_VQ3P9HGSP2SPar3PText, "%d",
                                                  players[1].par3_count);
                            lv_label_set_text_fmt(ui_VQ3P9HGSP2SPar4PText, "%d",
                                                  players[1].par4_count);
                            lv_label_set_text_fmt(ui_VQ3P9HGSP2SPar5PText, "%d",
                                                  players[1].par5_count);
                            lv_label_set_text_fmt(ui_VQ3P9HGSP2ScText, "%d",
                                                  players[1].score);
                            lv_label_set_text_fmt(ui_VQ3P9HGSP3SPar3PText, "%d",
                                                  players[2].par3_count);
                            lv_label_set_text_fmt(ui_VQ3P9HGSP3SPar4PText, "%d",
                                                  players[2].par4_count);
                            lv_label_set_text_fmt(ui_VQ3P9HGSP3SPar5PText, "%d",
                                                  players[2].par5_count);
                            lv_label_set_text_fmt(ui_VQ3P9HGSP3ScText, "%d",
                                                  players[2].score);
                            break;
                        case 4:
                            DEBUG_TRACE(MODULE_LOGIC, "Vegas Quota 4P 9H");
                            lv_label_set_text_fmt(ui_VQ4P9HGSBCPText, "%d",
                                                  SENSORS_PER_TURN - detection_count);
                            lv_label_set_text_fmt(ui_VQ4P9HGSP1SPar3PText, "%d",
                                                  players[0].par3_count);
                            lv_label_set_text_fmt(ui_VQ4P9HGSP1SPar4PText, "%d",
                                                  players[0].par4_count);
                            lv_label_set_text_fmt(ui_VQ4P9HGSP1SPar5PText, "%d",
                                                  players[0].par5_count);
                            lv_label_set_text_fmt(ui_VQ4P9HGSP1ScText, "%d",
                                                  players[0].score);
                            lv_label_set_text_fmt(ui_VQ4P9HGSP2SPar3PText, "%d",
                                                  players[1].par3_count);
                            lv_label_set_text_fmt(ui_VQ4P9HGSP2SPar4PText, "%d",
                                                  players[1].par4_count);
                            lv_label_set_text_fmt(ui_VQ4P9HGSP2SPar5PText, "%d",
                                                  players[1].par5_count);
                            lv_label_set_text_fmt(ui_VQ4P9HGSP2ScText, "%d",
                                                  players[1].score);
                            lv_label_set_text_fmt(ui_VQ4P9HGSP3SPar3PText, "%d",
                                                  players[2].par3_count);
                            lv_label_set_text_fmt(ui_VQ4P9HGSP3SPar4PText, "%d",
                                                  players[2].par4_count);
                            lv_label_set_text_fmt(ui_VQ4P9HGSP3SPar5PText, "%d",
                                                  players[2].par5_count);
                            lv_label_set_text_fmt(ui_VQ4P9HGSP3ScText, "%d",
                                                  players[2].score);
                            lv_label_set_text_fmt(ui_VQ4P9HGSP4SPar3PText, "%d",
                                                  players[3].par3_count);
                            lv_label_set_text_fmt(ui_VQ4P9HGSP4SPar4PText, "%d",
                                                  players[3].par4_count);
                            lv_label_set_text_fmt(ui_VQ4P9HGSP4SPar5PText, "%d",
                                                  players[3].par5_count);
                            lv_label_set_text_fmt(ui_VQ4P9HGSP4ScText, "%d",
                                                  players[3].score);
                            break;
                    }
                    break;
                case EIGHTEEN_HOLES:
                    switch (nplayers)
                    {
                        case 1:
                            DEBUG_TRACE(MODULE_LOGIC, "Vegas Quota 1P 18H");
                            lv_label_set_text(ui_VQ1P18HGSBCPText, "-");
                            lv_label_set_text_fmt(ui_VQ1P18HGSPSPar3PText, "%d",
                                                  players[0].par3_count);
                            lv_label_set_text_fmt(ui_VQ1P18HGSPSPar4PText, "%d",
                                                  players[0].par4_count);
                            lv_label_set_text_fmt(ui_VQ1P18HGSPSPar5PText, "%d",
                                                  players[0].par5_count);
                            lv_label_set_text_fmt(ui_VQ1P18HGSPSCText, "%d",
                                                  players[0].score);
                            break;
                        case 2:
                            DEBUG_TRACE(MODULE_LOGIC, "Vegas Quota 2P 18H");
                            lv_label_set_text_fmt(ui_VQ2P18HGSBCPText, "%d",
                                                  SENSORS_PER_TURN - detection_count);
                            lv_label_set_text_fmt(ui_VQ2P18HGSP1SPar3PText, "%d",
                                                  players[0].par3_count);
                            lv_label_set_text_fmt(ui_VQ2P18HGSP1SPar4PText, "%d",
                                                  players[0].par4_count);
                            lv_label_set_text_fmt(ui_Q2P18HGSP1SPar5PText, "%d",
                                                  players[0].par5_count);
                            lv_label_set_text_fmt(ui_VQ2P18HGSP1ScText, "%d",
                                                  players[0].score);
                            lv_label_set_text_fmt(ui_VQ2P18HGSP2SPar3PText, "%d",
                                                  players[1].par3_count);
                            lv_label_set_text_fmt(ui_VQ2P18HGSP2SPar4PText, "%d",
                                                  players[1].par4_count);
                            lv_label_set_text_fmt(ui_VQ2P18HGSP2SPar5PText, "%d",
                                                  players[1].par5_count);
                            lv_label_set_text_fmt(ui_VQ2P18HGSP2ScText, "%d",
                                                  players[1].score);
                            break;
                        case 3:
                            DEBUG_TRACE(MODULE_LOGIC, "Vegas Quota 3P 18H");
                            lv_label_set_text_fmt(ui_Q3P9HGSBCPText2, "%d",
                                                  SENSORS_PER_TURN - detection_count);
                            lv_label_set_text_fmt(ui_VQ3P18HGSP1SPar3PText, "%d",
                                                  players[0].par3_count);
                            lv_label_set_text_fmt(ui_VQ3P18HGSP1SPar4PText, "%d",
                                                  players[0].par4_count);
                            lv_label_set_text_fmt(ui_VQ3P18HGSP1SPar5PText, "%d",
                                                  players[0].par5_count);
                            lv_label_set_text_fmt(ui_VQ3P18HGSP1ScText, "%d",
                                                  players[0].score);
                            lv_label_set_text_fmt(ui_VQ3P18HGSP2SPar3PText, "%d",
                                                  players[1].par3_count);
                            lv_label_set_text_fmt(ui_VQ3P18HGSP2SPar4PText, "%d",
                                                  players[1].par4_count);
                            lv_label_set_text_fmt(ui_VQ3P18HGSP2SPar5PText, "%d",
                                                  players[1].par5_count);
                            lv_label_set_text_fmt(ui_VQ3P18HGSP2ScText, "%d",
                                                  players[1].score);
                            lv_label_set_text_fmt(ui_VQ3P18HGSP3SPar3PText, "%d",
                                                  players[2].par3_count);
                            lv_label_set_text_fmt(ui_VQ3P18HGSP3SPar4PText, "%d",
                                                  players[2].par4_count);
                            lv_label_set_text_fmt(ui_VQ3P18HGSP3SPar5PText, "%d",
                                                  players[2].par5_count);
                            lv_label_set_text_fmt(ui_VQ3P18HGSP3ScText, "%d",
                                                  players[2].score);
                            break;
                        case 4:
                            DEBUG_TRACE(MODULE_LOGIC, "Vegas Quota 4P 18H");
                            lv_label_set_text_fmt(ui_VQ4P18HGSBCPText, "%d",
                                                  SENSORS_PER_TURN - detection_count);
                            lv_label_set_text_fmt(ui_VQ4P18HGSP1SPar3PText, "%d",
                                                  players[0].par3_count);
                            lv_label_set_text_fmt(ui_VQ4P18HGSP1SPar4PText, "%d",
                                                  players[0].par4_count);
                            lv_label_set_text_fmt(ui_VQ4P18HGSP1SPar5PText, "%d",
                                                  players[0].par5_count);
                            lv_label_set_text_fmt(ui_VQ4P18HGSP1ScText, "%d",
                                                  players[0].score);
                            lv_label_set_text_fmt(ui_VQ4P18HGSP2SPar3PText, "%d",
                                                  players[1].par3_count);
                            lv_label_set_text_fmt(ui_VQ4P18HGSP2SPar4PText, "%d",
                                                  players[1].par4_count);
                            lv_label_set_text_fmt(ui_VQ4P18HGSP2SPar5PText, "%d",
                                                  players[1].par5_count);
                            lv_label_set_text_fmt(ui_VQ4P18HGSP2ScText, "%d",
                                                  players[1].score);
                            lv_label_set_text_fmt(ui_VQ4P18HGSP3SPar3PText, "%d",
                                                  players[2].par3_count);
                            lv_label_set_text_fmt(ui_VQ4P18HGSP3SPar4PText, "%d",
                                                  players[2].par4_count);
                            lv_label_set_text_fmt(ui_VQ4P18HGSP3SPar5PText, "%d",
                                                  players[2].par5_count);
                            lv_label_set_text_fmt(ui_VQ4P18HGSP3ScText, "%d",
                                                  players[2].score);
                            lv_label_set_text_fmt(ui_VQ4P18HGSP4SPar3PText, "%d",
                                                  players[3].par3_count);
                            lv_label_set_text_fmt(ui_VQ4P18HGSP4SPar4PText, "%d",
                                                  players[3].par4_count);
                            lv_label_set_text_fmt(ui_VQ4P18HGSP4SPar5PText, "%d",
                                                  players[3].par5_count);
                            lv_label_set_text_fmt(ui_VQ4P18HGSP4ScText, "%d",
                                                  players[3].score);
                            break;
                    }
                    break;
            }
            break;

        case GAME_MODE_STROKE_PLAY:
            DEBUG_TRACE(MODULE_LOGIC, "Stroke Play mode");
            switch (current_hole_mode)
            {
                case NINE_HOLES:
                    DEBUG_TRACE(MODULE_LOGIC, "Nine Holes mode");
                    switch (nplayers)
                    {
                        case 1:
                            DEBUG_TRACE(MODULE_LOGIC, "Single Player");
                            DEBUG_INFO(MODULE_LOGIC,
                                       "Updating UI for Single Player Mode - Player %d",
                                       player_index + 1);
                            lv_label_set_text_fmt(ui_SP1P9HGSPSText, "Player %d", player_index + 1);
                            lv_label_set_text_fmt(ui_SP1P9HGSPSPText, "%d", score);
                            lv_label_set_text_fmt(ui_SP1P9HGSHCPText, "%d", current_hole + 1);
                            lv_label_set_text_fmt(ui_SP1P9HGSBCPText, "%d", detection_count);
                            lv_obj_set_style_bg_color(ui_SP1P9HGSPSPanel, lv_color_hex(COLOR_1),
                                                      LV_PART_MAIN | LV_STATE_DEFAULT);
                            DEBUG_INFO(
                                MODULE_LOGIC,
                                "Single Player UI updated - Player:%d, Score:%d, Hole:%d, Balls:%d",
                                player_index + 1, score, current_hole + 1, detection_count);
                            break;

                        case 2:
                            DEBUG_TRACE(MODULE_LOGIC, "Two Players");
                            DEBUG_INFO(MODULE_LOGIC, "Two Player Mode");
                            switch (player_index)
                            {
                                case 0:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 1");
                                    lv_label_set_text_fmt(ui_SP2P9HGSP1SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP2P9HGSP1SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP2P9HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP2P9HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC,
                                               "Player 1 updated - Score:%d, Hole:%d, Balls:%d",
                                               score, current_hole + 1, detection_count);

                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player Two sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERTWO_WAV),
                                                        750);
                                    }
                                    break;

                                case 1:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 2");
                                    lv_label_set_text_fmt(ui_SP2P9HGSP2SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP2P9HGSP2SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP2P9HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP2P9HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC,
                                               "Player 2 updated - Score:%d, Hole:%d, Balls:%d",
                                               score, current_hole + 1, detection_count);
                                    if (detection_count == 2 && current_hole != NINE_HOLES)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player One sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERONE_WAV),
                                                        750);
                                    }
                                    break;
                            }
                            break;

                        case 3:
                            DEBUG_TRACE(MODULE_LOGIC, "Three Players");
                            DEBUG_INFO(MODULE_LOGIC, "Three Player Mode");
                            switch (player_index)
                            {
                                case 0:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 1");
                                    lv_label_set_text_fmt(ui_SP3P9HGSP1SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP3P9HGSP1SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP3P9HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP3P9HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC, "Player 1 updated");
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player Two sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERTWO_WAV),
                                                        750);
                                    }
                                    break;
                                case 1:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 2");
                                    lv_label_set_text_fmt(ui_SP3P9HGSP2SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP3P9HGSP2SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP3P9HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP3P9HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC, "Player 2 updated");
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player Three sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERTHREE_WAV),
                                                        SOUND_DELAY_TURN_SWITCH_MS);
                                    }
                                    break;
                                case 2:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 3");
                                    lv_label_set_text_fmt(ui_SP3P9HGSP3SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP3P9HGSP3SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP3P9HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP3P9HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC, "Player 3 updated");
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player One sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERONE_WAV),
                                                        750);
                                    }
                                    break;
                            }
                            break;

                        case 4:
                            DEBUG_TRACE(MODULE_LOGIC, "Four Players");
                            DEBUG_INFO(MODULE_LOGIC, "Four Player Mode");
                            switch (player_index)
                            {
                                case 0:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 1");
                                    lv_label_set_text_fmt(ui_SP4P9HGSP1SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP4P9HGSP1SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP4P9HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP4P9HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC, "Player 1 updated");
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player Two sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERTWO_WAV),
                                                        750);
                                    }
                                    break;
                                case 1:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 2");
                                    lv_label_set_text_fmt(ui_SP4P9HGSP2SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP4P9HGSP2SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP4P9HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP4P9HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC, "Player 2 updated");
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player Three sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERTHREE_WAV),
                                                        SOUND_DELAY_TURN_SWITCH_MS);
                                    }
                                    break;
                                case 2:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 3");
                                    lv_label_set_text_fmt(ui_SP4P9HGSP3SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP4P9HGSP3SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP4P9HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP4P9HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC, "Player 3 updated");
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player Four sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERFOUR_WAV),
                                                        SOUND_DELAY_TURN_SWITCH_MS);
                                    }
                                    break;
                                case 3:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 4");
                                    lv_label_set_text_fmt(ui_SP4P9HGSP4SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP4P9HGSP4SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP4P9HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP4P9HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC, "Player 4 updated");
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player One sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERONE_WAV),
                                                        750);
                                    }
                                    break;
                            }
                            break;

                        default:
                            DEBUG_ERROR(MODULE_LOGIC, "Invalid number of players: %d", nplayers);
                            break;
                    }
                    break;
                case EIGHTEEN_HOLES:
                    DEBUG_TRACE(MODULE_LOGIC, "Eighteen Holes mode");
                    switch (nplayers)
                    {
                        case 1:
                            DEBUG_TRACE(MODULE_LOGIC, "Single Player 18 Holes");
                            DEBUG_INFO(MODULE_LOGIC,
                                       "Updating UI for Single Player Mode - Player %d",
                                       player_index + 1);
                            lv_label_set_text_fmt(ui_SP1P18HGSPSPText, "Player %d",
                                                  player_index + 1);
                            lv_label_set_text_fmt(ui_SP1P18HGSPSPText, "%d", score);
                            lv_label_set_text_fmt(ui_SP1P18HGSHCPText, "%d", current_hole + 1);
                            lv_label_set_text_fmt(ui_SP1P18HGSBCPText, "%d", detection_count);
                            DEBUG_INFO(MODULE_LOGIC,
                                       "Single Player 18 Holes UI updated - Player:%d, Score:%d, "
                                       "Hole:%d, Balls:%d",
                                       player_index + 1, score, current_hole + 1, detection_count);
                            break;
                        case 2:
                            DEBUG_TRACE(MODULE_LOGIC, "Two Players 18 Holes");
                            DEBUG_INFO(MODULE_LOGIC, "Two Player Mode 18 Holes");
                            switch (player_index)
                            {
                                case 0:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 1 18 Holes");
                                    lv_label_set_text_fmt(ui_SP2P18HGSP1SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP2P18HGSP1SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP2P18HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP2P18HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(
                                        MODULE_LOGIC,
                                        "Player 1 18 Holes updated - Score:%d, Hole:%d, Balls:%d",
                                        score, current_hole + 1, detection_count);
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player Two sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERTWO_WAV),
                                                        750);
                                    }
                                    break;
                                case 1:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 2 18 Holes");
                                    lv_label_set_text_fmt(ui_SP2P18HGSP2SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP2P18HGSP2SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP2P18HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP2P18HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(
                                        MODULE_LOGIC,
                                        "Player 2 18 Holes updated - Score:%d, Hole:%d, Balls:%d",
                                        score, current_hole + 1, detection_count);
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player One sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERONE_WAV),
                                                        750);
                                    }
                                    break;
                            }
                            break;

                        case 3:
                            DEBUG_TRACE(MODULE_LOGIC, "Three Players 18 Holes");
                            DEBUG_INFO(MODULE_LOGIC, "Three Player Mode 18 Holes");
                            switch (player_index)
                            {
                                case 0:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 1 18 Holes");
                                    lv_label_set_text_fmt(ui_SP3P18HGSP1SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP3P18HGSP1SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP3P18HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP3P18HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC, "Player 1 18 Holes updated");
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player Two sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERTWO_WAV),
                                                        750);
                                    }
                                    break;
                                case 1:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 2 18 Holes");
                                    lv_label_set_text_fmt(ui_SP3P18HGSP2SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP3P18HGSP2SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP3P18HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP3P18HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC, "Player 2 18 Holes updated");
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player Three sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERTHREE_WAV),
                                                        SOUND_DELAY_TURN_SWITCH_MS);
                                    }
                                    break;
                                case 2:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 3 18 Holes");
                                    lv_label_set_text_fmt(ui_SP3P18HGSP3SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP3P18HGSP3SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP3P18HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP3P18HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC, "Player 3 18 Holes updated");
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player One sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERONE_WAV),
                                                        750);
                                    }
                                    break;
                            }
                            break;

                        case 4:
                            DEBUG_TRACE(MODULE_LOGIC, "Four Players 18 Holes");
                            DEBUG_INFO(MODULE_LOGIC, "Four Player Mode 18 Holes");
                            switch (player_index)
                            {
                                case 0:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 1 18 Holes");
                                    lv_label_set_text_fmt(ui_SP4P18HGSP1SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP4P18HGSP1SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP4P18HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP4P18HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC, "Player 1 18 Holes updated");
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player Two sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERTWO_WAV),
                                                        750);
                                    }
                                    break;
                                case 1:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 2 18 Holes");
                                    lv_label_set_text_fmt(ui_SP4P18HGSP2SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP4P18HGSP2SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP4P18HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP4P18HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC, "Player 2 18 Holes updated");
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player Three sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERTHREE_WAV),
                                                        SOUND_DELAY_TURN_SWITCH_MS);
                                    }
                                    break;
                                case 2:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 3 18 Holes");
                                    lv_label_set_text_fmt(ui_SP4P18HGSP3SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP4P18HGSP3SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP4P18HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP4P18HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC, "Player 3 18 Holes updated");
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player Four sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERFOUR_WAV),
                                                        SOUND_DELAY_TURN_SWITCH_MS);
                                    }
                                    break;
                                case 3:
                                    DEBUG_TRACE(MODULE_LOGIC, "Player 4 18 Holes");
                                    lv_label_set_text_fmt(ui_SP4P18HGSP4SText, "Player %d",
                                                          player_index + 1);
                                    lv_label_set_text_fmt(ui_SP4P18HGSP4SPText, "%d", score);
                                    lv_label_set_text_fmt(ui_SP4P18HGSHCPText, "%d",
                                                          current_hole + 1);
                                    lv_label_set_text_fmt(ui_SP4P18HGSBCPText, "%d",
                                                          detection_count);
                                    DEBUG_INFO(MODULE_LOGIC, "Player 4 18 Holes updated");
                                    if (detection_count == 2)
                                    {
                                        DEBUG_INFO(MODULE_LOGIC, "Playing Player One sound");
                                        play_sound_once(load_sound_effect(SOUND_PLAYERONE_WAV),
                                                        750);
                                    }
                                    break;
                            }
                            break;

                        default:
                            DEBUG_ERROR(MODULE_LOGIC, "Invalid number of players: %d", nplayers);
                            break;
                    }
                    break;
            }
}

}


/* ============================================================================
 * Event Bus Callbacks
 * Undefine convenience macros so struct member access (gs->players, etc.) works
 * ============================================================================ */
#undef players
#undef num_players
#undef current_player_index

static void ui_on_score_changed(const game_event_t *event)
{
    int pi = event->data.score.player_index;
    int score = event->data.score.score;
    int hole = event->data.score.hole;
    int det = event->data.score.detection_count;
    int np = event->data.score.num_players;

    logic_update_label_text(pi, hole, score, det, np);
    game_state_t *gs = game_state_get();
    update_scoreCard((uint8_t)pi, (uint8_t)gs->players[pi].score, (uint8_t)hole);
}

static void ui_on_player_switched(const game_event_t *event)
{
    (void)event;
    game_state_t *gs = game_state_get();
    update_player_highlight(
        gs->players[gs->current_player_index].detection_count,
        gs->players[gs->current_player_index].current_hole,
        (uint8_t)gs->num_players);
}

static void ui_on_hole_completed(const game_event_t *event)
{
    (void)event;
    /* Scorecard is updated via ui_on_score_changed */
}

static void ui_on_game_completed(const game_event_t *event)
{
    (void)event;
    game_state_t *gs = game_state_get();
    check_all_players_completed(gs->game_mode);
    print_final_scores_and_winner();
}

static void ui_on_game_reset(const game_event_t *event)
{
    (void)event;
    /* reset_scores() handles both data and UI reset */
}

void ui_controller_init(void)
{
    DEBUG_INFO(MODULE_UI, "UI controller initializing - subscribing to events");
    event_bus_subscribe(EVENT_SCORE_CHANGED,    ui_on_score_changed);
    event_bus_subscribe(EVENT_PLAYER_SWITCHED,  ui_on_player_switched);
    event_bus_subscribe(EVENT_HOLE_COMPLETED,   ui_on_hole_completed);
    event_bus_subscribe(EVENT_GAME_COMPLETED,   ui_on_game_completed);
    event_bus_subscribe(EVENT_GAME_RESET,       ui_on_game_reset);
    DEBUG_INFO(MODULE_UI, "UI controller initialized");
}
