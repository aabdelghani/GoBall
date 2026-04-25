#include "ui_logic.event.h"
#include "../player_name/player_name.h"

uint8_t update_flag                 = 0;  // Flag to indicate if all players have completed the game
uint8_t prev_scores[MAX_PLAYERS][9] = {
    0};  // Previous cumulative scores for each hole for all players
uint8_t individual_scores[MAX_PLAYERS][10] = {
    0};                                   // Individual scores for each hole for all players
uint8_t final_scores[MAX_PLAYERS] = {0};  // Final cumulative scores for all players
uint8_t highliting_counter        = 0;    // Counter for highlighting players
uint8_t sensors_enabled           = 0;    // Flag to check if sensors are enabled
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
    player_name_reset();
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
