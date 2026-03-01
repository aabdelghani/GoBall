#include "sound_logic_event.h"
// Global variable to track mute state
static int audio_muted = 0;

int init_audio_system()
{
    SDL_setenv("SDL_AUDIODRIVER", "pulseaudio", 1);
    DEBUG_INFO(MODULE_SOUND, "Initializing audio system (driver=pulseaudio, rate=44100, channels=2, buffer=2048)");
    if (SDL_Init(SDL_INIT_AUDIO) < 0)
    {
        DEBUG_ERROR(MODULE_SOUND, "Could not initialize SDL: %s", SDL_GetError());
        return 0;  // Failure
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
    {
        DEBUG_ERROR(MODULE_SOUND, "Could not initialize SDL_mixer: %s", Mix_GetError());
        SDL_Quit();
        return 0;  // Failure
    }

    /* Log actual audio format after opening */
    int freq, channels;
    Uint16 format;
    if (Mix_QuerySpec(&freq, &format, &channels)) {
        DEBUG_INFO(MODULE_SOUND, "Audio opened: freq=%dHz, format=0x%04X, channels=%d",
                   freq, format, channels);
    }
    int num_channels = Mix_AllocateChannels(-1);  /* query without changing */
    DEBUG_INFO(MODULE_SOUND, "Mixing channels available: %d", num_channels);

    return 1;  // Success
}

void print_audio_driver_info()
{
    const char* audioDriver = SDL_GetCurrentAudioDriver();
    if (audioDriver)
    {
        DEBUG_INFO(MODULE_SOUND, "Current SDL audio driver: %s", audioDriver);
    }
    else
    {
        DEBUG_WARN(MODULE_SOUND, "No audio driver is currently in use");
    }
}

void print_current_working_dir(void)
{
    char cwd[1024];

    if (!getcwd(cwd, sizeof(cwd)))
    {
        DEBUG_ERROR(MODULE_SOUND, "getcwd() failed");
        return;
    }

    DEBUG_DEBUG(MODULE_SOUND, "Current working directory: %s", cwd);

    // Build "cwd/SOUND_DIR"
    char combined[2048];
    int  n = snprintf(combined, sizeof(combined), "%s/%s", cwd, SOUND_DIR);

    if (n < 0 || (size_t)n >= sizeof(combined))
    {
        DEBUG_ERROR(MODULE_SOUND, "Audio path too long");
        return;
    }

    // Let realpath allocate safely
    char* resolved = realpath(combined, NULL);
    if (resolved)
    {
        DEBUG_DEBUG(MODULE_SOUND, "Final audio fetching directory: %s", resolved);
        free(resolved);
    }
    else
    {
        DEBUG_WARN(MODULE_SOUND, "Audio fetching directory (unresolved): %s", combined);
    }
}
Mix_Chunk* load_sound_effect(const char* file_path)
{
    DEBUG_DEBUG(MODULE_SOUND, "Attempting to load sound: %s", file_path);

    /* Check file existence before trying to load */
    if (access(file_path, F_OK) != 0)
    {
        DEBUG_ERROR(MODULE_SOUND, "Sound file not found: %s", file_path);
        return NULL;
    }
    if (access(file_path, R_OK) != 0)
    {
        DEBUG_ERROR(MODULE_SOUND, "Sound file not readable (permission denied): %s", file_path);
        return NULL;
    }

    Mix_Chunk* sound = Mix_LoadWAV(file_path);
    if (!sound)
    {
        DEBUG_ERROR(MODULE_SOUND, "Failed to load sound '%s': %s", file_path, Mix_GetError());
        return NULL;
    }
    DEBUG_DEBUG(MODULE_SOUND, "Sound loaded: %s (length=%u bytes)", file_path, sound->alen);
    return sound;
}

void play_sound_effect(Mix_Chunk* sound)
{
    if (!sound) {
        DEBUG_WARN(MODULE_SOUND, "play_sound_effect() called with NULL sound");
        return;
    }

    int channel = Mix_PlayChannel(-1, sound, 0);
    if (channel == -1) {
        DEBUG_ERROR(MODULE_SOUND, "Failed to play sound: %s", Mix_GetError());
    } else {
        DEBUG_TRACE(MODULE_SOUND, "Playing sound on channel %d (length=%u bytes)", channel, sound->alen);
    }

    /* // Wait for the sound to finish playing
     while (Mix_Playing(-1)) {
         SDL_Delay(100);
     }*/
}

// NEW: Stop all currently playing sounds
void stop_all_sounds()
{
    DEBUG_DEBUG(MODULE_SOUND, "Stopping all sounds and cancelling pending audio");
    Mix_HaltChannel(-1);  // -1 means all channels

    // You might want to add timer cancellation here if you have
    // a way to track sound timers
}

// NEW: Mute audio (sounds won't play but can be resumed)
void mute_audio()
{
    DEBUG_INFO(MODULE_SOUND, "Audio muted");
    audio_muted = 1;
    stop_all_sounds();  // Also stop any currently playing sounds
}

// NEW: Unmute audio
void unmute_audio()
{
    DEBUG_INFO(MODULE_SOUND, "Audio unmuted");
    audio_muted = 0;
}

// NEW: Check if audio is muted
int is_audio_muted()
{
    return audio_muted;
}
void cleanup_audio_system()
{
    DEBUG_INFO(MODULE_SOUND, "Cleaning up audio system");
    Mix_CloseAudio();
    SDL_Quit();
    DEBUG_INFO(MODULE_SOUND, "Audio system shutdown complete");
}

// Callback for one-shot timer
static void sound_timer_cb(lv_timer_t* timer)
{
    Mix_Chunk* sound = (Mix_Chunk*)timer->user_data;
    play_sound_effect(sound);
    lv_timer_del(timer);  // Auto-delete
}

// One-shot sound player
lv_timer_t* play_sound_once(Mix_Chunk* sound, uint32_t delay_ms)
{
    DEBUG_INFO(MODULE_SOUND, "*** play_sound_once called - delay_ms: %u, sound: %p ***",
               delay_ms, (void*)sound);
    lv_timer_t* timer = lv_timer_create(sound_timer_cb, delay_ms, sound);
    lv_timer_set_repeat_count(timer, 1);
    DEBUG_INFO(MODULE_SOUND, "*** Timer created: %p ***", (void*)timer);
    return timer;
}

void play_sound_timer_cb(lv_timer_t* timer)
{
    DEBUG_INFO(MODULE_SOUND, "*** play_sound_timer_cb CALLED - timer: %p ***", (void*)timer);
    (void)timer;  // Unused parameter
    // Note: This callback was previously hardcoded to play PLAY_PLAYERONE_WAV
    // which caused unwanted player announcements after game end.
    // The callback is now empty to prevent this issue.
    // If player announcements are needed, they should be handled explicitly
    // in the game logic, not through this generic timer callback.

    // Delete the timer after execution (to prevent repeats)
    lv_timer_del(timer);
    DEBUG_INFO(MODULE_SOUND, "*** Timer deleted ***");
}