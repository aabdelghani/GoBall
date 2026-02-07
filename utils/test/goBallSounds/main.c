#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>
#include <unistd.h> // For getcwd()

#define WAV_FILE "strokePlay.wav"

int main() {
    SDL_setenv("SLD_AUDIODRIVER", "alsa", 1);
    // Initialize SDL
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        fprintf(stderr, "Could not initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    // Initialize SDL_mixer
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
        fprintf(stderr, "Could not initialize SDL_mixer: %s\n", Mix_GetError());
        SDL_Quit();
        return 1;
    }
    // Print the current audio driver
    const char* audioDriver = SDL_GetCurrentAudioDriver();
    if (audioDriver) {
        printf("Current SDL audio driver: %s\n", audioDriver);
    } else {
        printf("No audio driver is currently in use.\n");
    }

    // Debug: Print the current working directory
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd))) {
        printf("Current working directory: %s\n", cwd);
    } else {
        perror("getcwd() error");
    }

    // Load the WAV file
    Mix_Chunk *sound = Mix_LoadWAV(WAV_FILE);
    if (!sound) {
        printf("Failed to load sound: %s\n", Mix_GetError());
        printf("Looking for file: %s in directory: %s\n", WAV_FILE, cwd);
        //return 1;
    } else {
        printf("Sound loaded successfully!\n");
    }

    // Play the sound
    if (sound) {
        printf("Playing sound...\n");
        Mix_PlayChannel(-1, sound, 0);

        // Wait for the sound to finish playing
        while (Mix_Playing(-1)) {
            SDL_Delay(100);
        }

        // Free the sound
        Mix_FreeChunk(sound);
    }

    // Clean up
    Mix_CloseAudio();
    SDL_Quit();

    return 0;
}
