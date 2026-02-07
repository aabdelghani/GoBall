// game_MODES.h
#ifndef GAME_MODES_H
#define GAME_MODES_H
#define NINE_HOLES 8
#define EIGHTEEN_HOLES 17
typedef enum {
    GAME_MODE_STROKE_PLAY,
    GAME_MODE_MATCH_PLAY,
    GAME_MODE_QUOTA,
    GAME_MODE_VEGAS,
    NUM_GAME_MODES
} GameMode;

// Add these:
typedef enum {
    HOLES_9 = NINE_HOLES , // 0-8 ? 9 holes
    HOLES_18 = EIGHTEEN_HOLES// 0-17 ? 18 holes
} HoleMode;

typedef enum{
    MATCH_PLAY_MODE_1V1,          // One vs One mode 
    MATCH_PLAY_MODE_2V2,          // Two vs Two mode
} MatchPlayMode;
#endif // GAME_MODES_H