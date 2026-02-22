#ifndef SOUND_LOGIC_EVENT_H
#define SOUND_LOGIC_EVENT_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <limits.h>  // PATH_MAX
#include <stdio.h>
#include <unistd.h>  // For getcwd()

#include "../../ui/ui.h"
#include "../logic/gpio_event.h"

// Define the base path to the sound files
#ifndef SOUND_DIR
#define SOUND_DIR "modules/game_sounds/"
#endif

// Use SOUND_DIR to define full paths
#define SOUND_BACKBTN_WAV SOUND_DIR "backBtn.wav"
#define SOUND_EIGHTEENHOLES_WAV SOUND_DIR "eighteenHoles.wav"
#define SOUND_FOURPLAYERS_WAV SOUND_DIR "fourPlayers.wav"
#define SOUND_HOME_WAV SOUND_DIR "home.wav"
#define SOUND_MAINMENU_WAV SOUND_DIR "mainMenu.wav"
#define SOUND_MATCHPLAY_WAV SOUND_DIR "matchPlay.wav"
#define SOUND_NINEHOLES_WAV SOUND_DIR "nineHoles.wav"
#define SOUND_ONEPLAYER_WAV SOUND_DIR "onePlayer.wav"
#define SOUND_ONEVONE_WAV SOUND_DIR "oneVOne.wav"
#define SOUND_QUOTAPOINTS_WAV SOUND_DIR "quotaPoints.wav"
#define SOUND_SCORECARD_WAV SOUND_DIR "scoreCard.wav"
#define SOUND_STROKEPLAY_WAV SOUND_DIR "strokePlay.wav"
#define SOUND_THREEPLAYERS_WAV SOUND_DIR "threePlayers.wav"
#define SOUND_TWOPLAYERS_WAV SOUND_DIR "twoPlayers.wav"
#define SOUND_TWOVTWO_WAV SOUND_DIR "twoVTwo.wav"
#define SOUND_VEGASQUOTAPOINTS_WAV SOUND_DIR "vegasQuotaPoints.wav"
#define SOUND_THREEPOINTS_WAV SOUND_DIR "threePoints.wav"
#define SOUND_FOURPOINTS_WAV SOUND_DIR "fourPoints.wav"
#define SOUND_FIVEPOINTS_WAV SOUND_DIR "fivePoints.wav"
#define SOUND_ZEROPOINTS_WAV SOUND_DIR "zeroPoints.wav"
#define SOUND_PLAYERONE_WAV SOUND_DIR "playerOne.wav"
#define SOUND_PLAYERTWO_WAV SOUND_DIR "playerTwo.wav"
#define SOUND_PLAYERTHREE_WAV SOUND_DIR "playerThree.wav"
#define SOUND_PLAYERFOUR_WAV SOUND_DIR "playerFour.wav"
#define SOUND_TEAMONE_WAV SOUND_DIR "teamOne.wav"
#define SOUND_TEAMTWO_WAV SOUND_DIR "teamTwo.wav"
#define SOUND_PLAYER1WINS1_WAV SOUND_DIR "player1wins1.wav"
#define SOUND_PLAYER1WINS2_WAV SOUND_DIR "player1wins2.wav"
#define SOUND_PLAYER1WINS3_WAV SOUND_DIR "player1wins3.wav"
#define SOUND_PLAYER1WINS4_WAV SOUND_DIR "player1wins4.wav"
#define SOUND_PLAYER1WINS5_WAV SOUND_DIR "player1wins5.wav"
#define SOUND_PLAYER2WINS1_WAV SOUND_DIR "player2wins1.wav"
#define SOUND_PLAYER2WINS2_WAV SOUND_DIR "player2wins2.wav"
#define SOUND_PLAYER2WINS3_WAV SOUND_DIR "player2wins3.wav"
#define SOUND_PLAYER2WINS4_WAV SOUND_DIR "player2wins4.wav"
#define SOUND_PLAYER2WINS5_WAV SOUND_DIR "player2wins5.wav"
#define SOUND_PLAYER3WINS1_WAV SOUND_DIR "player3wins1.wav"
#define SOUND_PLAYER3WINS2_WAV SOUND_DIR "player3wins2.wav"
#define SOUND_PLAYER3WINS3_WAV SOUND_DIR "player3wins3.wav"
#define SOUND_PLAYER3WINS4_WAV SOUND_DIR "player3wins4.wav"
#define SOUND_PLAYER3WINS5_WAV SOUND_DIR "player3wins5.wav"
#define SOUND_PLAYER4WINS1_WAV SOUND_DIR "player4wins1.wav"
#define SOUND_PLAYER4WINS2_WAV SOUND_DIR "player4wins2.wav"
#define SOUND_PLAYER4WINS3_WAV SOUND_DIR "player4wins3.wav"
#define SOUND_PLAYER4WINS4_WAV SOUND_DIR "player4wins4.wav"
#define SOUND_PLAYER4WINS5_WAV SOUND_DIR "player4wins5.wav"
#define SOUND_TEAM1WINS1_WAV SOUND_DIR "team1wins1.wav"
#define SOUND_TEAM1WINS2_WAV SOUND_DIR "team1wins2.wav"
#define SOUND_TEAM1WINS3_WAV SOUND_DIR "team1wins3.wav"
#define SOUND_TEAM1WINS4_WAV SOUND_DIR "team1wins4.wav"
#define SOUND_TEAM1WINS5_WAV SOUND_DIR "team1wins5.wav"
#define SOUND_TEAM2WINS1_WAV SOUND_DIR "team2wins1.wav"
#define SOUND_TEAM2WINS2_WAV SOUND_DIR "team2wins2.wav"
#define SOUND_TEAM2WINS3_WAV SOUND_DIR "team2wins3.wav"
#define SOUND_TEAM2WINS4_WAV SOUND_DIR "team2wins4.wav"
#define SOUND_TEAM2WINS5_WAV SOUND_DIR "team2wins5.wav"

// ============================================================================
// Sound Timing Configuration (milliseconds)
// ============================================================================
// Game start announcements (when entering a game screen)
#define SOUND_DELAY_PLAYER_ANNOUNCE_MS 750  // Player 1/2/3/4 announcement delay
#define SOUND_DELAY_TEAM_ANNOUNCE_MS 750    // Team 1/2 announcement delay
#define SOUND_DELAY_PLAYER_WINS_MS 750      // Player wins announcement delay
#define SOUND_DELAY_TEAM_WINS_MS 750        // Team wins announcement delay

// Turn switch announcements (during gameplay)
#define SOUND_DELAY_TURN_SWITCH_MS 500  // Player/Team turn switch delay

// Function declarations
int        init_audio_system();
void       print_audio_driver_info();
void       print_current_working_dir();
Mix_Chunk* load_sound_effect(const char* file_path);
void       play_sound_effect(Mix_Chunk* sound);
void       cleanup_audio_system();

// Audio control functions
void stop_all_sounds();
void mute_audio();
void unmute_audio();
int  is_audio_muted();  // Add this declaration

// Timer functions
lv_timer_t* play_sound_once(Mix_Chunk* sound, uint32_t delay_ms);
extern void play_sound_timer_cb(lv_timer_t* timer);
extern void init_sound_timer();

#ifdef _GO_BALL_RASPBERY_PI_UI_H

// Update all macros to use the function instead of direct variable access
#define PLAY_BACKBTN_SOUND \
    if (!is_audio_muted()) play_sound_effect(load_sound_effect(SOUND_BACKBTN_WAV));

#define PLAY_EIGHTEENHOLES_SOUND \
    if (!is_audio_muted()) play_sound_effect(load_sound_effect(SOUND_EIGHTEENHOLES_WAV));

#define PLAY_FOURPLAYERS_SOUND \
    if (!is_audio_muted()) play_sound_effect(load_sound_effect(SOUND_FOURPLAYERS_WAV));

#define PLAY_HOME_SOUND \
    if (!is_audio_muted()) play_sound_effect(load_sound_effect(SOUND_HOME_WAV));

#define PLAY_MAINMENU_SOUND \
    if (!is_audio_muted()) play_sound_effect(load_sound_effect(SOUND_MAINMENU_WAV));

#define PLAY_MATCHPLAY_SOUND \
    if (!is_audio_muted()) play_sound_effect(load_sound_effect(SOUND_MATCHPLAY_WAV));

#define PLAY_NINEHOLES_SOUND \
    if (!is_audio_muted()) play_sound_effect(load_sound_effect(SOUND_NINEHOLES_WAV));

#define PLAY_ONEPLAYER_SOUND \
    if (!is_audio_muted()) play_sound_effect(load_sound_effect(SOUND_ONEPLAYER_WAV));

#define PLAY_ONEVONE_SOUND \
    if (!is_audio_muted()) play_sound_effect(load_sound_effect(SOUND_ONEVONE_WAV));

#define PLAY_QUOTAPOINTS_SOUND \
    if (!is_audio_muted()) play_sound_effect(load_sound_effect(SOUND_QUOTAPOINTS_WAV));

#define PLAY_SCORECARD_SOUND \
    if (!is_audio_muted()) play_sound_effect(load_sound_effect(SOUND_SCORECARD_WAV));

#define PLAY_STROKEPLAY_SOUND \
    if (!is_audio_muted()) play_sound_effect(load_sound_effect(SOUND_STROKEPLAY_WAV));

#define PLAY_THREEPLAYERS_SOUND \
    if (!is_audio_muted()) play_sound_effect(load_sound_effect(SOUND_THREEPLAYERS_WAV));

#define PLAY_TWOPLAYERS_SOUND \
    if (!is_audio_muted()) play_sound_effect(load_sound_effect(SOUND_TWOPLAYERS_WAV));

#define PLAY_TWOVTWO_SOUND \
    if (!is_audio_muted()) play_sound_effect(load_sound_effect(SOUND_TWOVTWO_WAV));

#define PLAY_VEGASQUOTAPOINTS_SOUND \
    if (!is_audio_muted()) play_sound_effect(load_sound_effect(SOUND_VEGASQUOTAPOINTS_WAV));

#define PLAY_ZEROPOINTS_WAV \
    if (!is_audio_muted()) play_sound_effect(load_sound_effect(SOUND_ZEROPOINTS_WAV));

#define PLAY_FIVEPOINTS_WAV \
    if (!is_audio_muted()) play_sound_effect(load_sound_effect(SOUND_FIVEPOINTS_WAV));

#define PLAY_FOURPOINTS_WAV \
    if (!is_audio_muted()) play_sound_effect(load_sound_effect(SOUND_FOURPOINTS_WAV));

#define PLAY_THREEPOINTS_WAV \
    if (!is_audio_muted()) play_sound_effect(load_sound_effect(SOUND_THREEPOINTS_WAV));

#define PLAY_PLAYERONE_WAV \
    if (!is_audio_muted()) play_sound_effect(load_sound_effect(SOUND_PLAYERONE_WAV));

#define PLAY_PLAYERTWO_WAV \
    if (!is_audio_muted()) play_sound_effect(load_sound_effect(SOUND_PLAYERTWO_WAV));

#define PLAY_PLAYERTHREE_WAV \
    if (!is_audio_muted()) play_sound_effect(load_sound_effect(SOUND_PLAYERTHREE_WAV));

#define PLAY_PLAYERFOUR_WAV \
    if (!is_audio_muted()) play_sound_effect(load_sound_effect(SOUND_PLAYERFOUR_WAV));

#define PLAY_TEAMONE_WAV \
    if (!is_audio_muted()) play_sound_effect(load_sound_effect(SOUND_TEAMONE_WAV));

#define PLAY_TEAMTWO_WAV \
    if (!is_audio_muted()) play_sound_effect(load_sound_effect(SOUND_TEAMTWO_WAV));

#define PLAY_PLAYER1WINS_WAV                                                        \
    do                                                                              \
    {                                                                               \
        if (!is_audio_muted())                                                      \
        {                                                                           \
            const char* sounds[] = {SOUND_PLAYER1WINS1_WAV, SOUND_PLAYER1WINS2_WAV, \
                                    SOUND_PLAYER1WINS3_WAV, SOUND_PLAYER1WINS4_WAV, \
                                    SOUND_PLAYER1WINS5_WAV};                        \
            int         idx      = rand() % 5;                                      \
            Mix_Chunk*  sound    = load_sound_effect(sounds[idx]);                  \
            if (sound) play_sound_once(sound, SOUND_DELAY_PLAYER_WINS_MS);          \
        }                                                                           \
    } while (0)

#define PLAY_PLAYER2WINS_WAV                                                        \
    do                                                                              \
    {                                                                               \
        if (!is_audio_muted())                                                      \
        {                                                                           \
            const char* sounds[] = {SOUND_PLAYER2WINS1_WAV, SOUND_PLAYER2WINS2_WAV, \
                                    SOUND_PLAYER2WINS3_WAV, SOUND_PLAYER2WINS4_WAV, \
                                    SOUND_PLAYER2WINS5_WAV};                        \
            int         idx      = rand() % 5;                                      \
            Mix_Chunk*  sound    = load_sound_effect(sounds[idx]);                  \
            if (sound) play_sound_once(sound, SOUND_DELAY_PLAYER_WINS_MS);          \
        }                                                                           \
    } while (0)

#define PLAY_PLAYER3WINS_WAV                                                        \
    do                                                                              \
    {                                                                               \
        if (!is_audio_muted())                                                      \
        {                                                                           \
            const char* sounds[] = {SOUND_PLAYER3WINS1_WAV, SOUND_PLAYER3WINS2_WAV, \
                                    SOUND_PLAYER3WINS3_WAV, SOUND_PLAYER3WINS4_WAV, \
                                    SOUND_PLAYER3WINS5_WAV};                        \
            int         idx      = rand() % 5;                                      \
            Mix_Chunk*  sound    = load_sound_effect(sounds[idx]);                  \
            if (sound) play_sound_once(sound, SOUND_DELAY_PLAYER_WINS_MS);          \
        }                                                                           \
    } while (0)

#define PLAY_PLAYER4WINS_WAV                                                        \
    do                                                                              \
    {                                                                               \
        if (!is_audio_muted())                                                      \
        {                                                                           \
            const char* sounds[] = {SOUND_PLAYER4WINS1_WAV, SOUND_PLAYER4WINS2_WAV, \
                                    SOUND_PLAYER4WINS3_WAV, SOUND_PLAYER4WINS4_WAV, \
                                    SOUND_PLAYER4WINS5_WAV};                        \
            int         idx      = rand() % 5;                                      \
            Mix_Chunk*  sound    = load_sound_effect(sounds[idx]);                  \
            if (sound) play_sound_once(sound, SOUND_DELAY_PLAYER_WINS_MS);          \
        }                                                                           \
    } while (0)

#define PLAY_TEAM1WINS_WAV                                                          \
    do                                                                              \
    {                                                                               \
        if (!is_audio_muted())                                                      \
        {                                                                           \
            const char* sounds[] = {SOUND_TEAM1WINS1_WAV, SOUND_TEAM1WINS2_WAV,     \
                                    SOUND_TEAM1WINS3_WAV, SOUND_TEAM1WINS4_WAV,     \
                                    SOUND_TEAM1WINS5_WAV};                          \
            int         idx      = rand() % 5;                                      \
            Mix_Chunk*  sound    = load_sound_effect(sounds[idx]);                  \
            if (sound) play_sound_once(sound, SOUND_DELAY_TEAM_WINS_MS);            \
        }                                                                           \
    } while (0)

#define PLAY_TEAM2WINS_WAV                                                          \
    do                                                                              \
    {                                                                               \
        if (!is_audio_muted())                                                      \
        {                                                                           \
            const char* sounds[] = {SOUND_TEAM2WINS1_WAV, SOUND_TEAM2WINS2_WAV,     \
                                    SOUND_TEAM2WINS3_WAV, SOUND_TEAM2WINS4_WAV,     \
                                    SOUND_TEAM2WINS5_WAV};                          \
            int         idx      = rand() % 5;                                      \
            Mix_Chunk*  sound    = load_sound_effect(sounds[idx]);                  \
            if (sound) play_sound_once(sound, SOUND_DELAY_TEAM_WINS_MS);            \
        }                                                                           \
    } while (0)

// Update the delayed sound macro too
#define PLAY_SOUND_DELAYED(sound_path, delay_ms)              \
    do                                                        \
    {                                                         \
        if (!is_audio_muted())                                \
        {                                                     \
            Mix_Chunk* sound = load_sound_effect(sound_path); \
            if (sound) play_sound_once(sound, delay_ms);      \
        }                                                     \
    } while (0)

#else
#endif

#endif  // SOUND_LOGIC_EVENT_H