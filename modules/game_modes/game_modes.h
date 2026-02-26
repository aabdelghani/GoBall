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

/* GPIO pin assignments for scoring sensors */
#define PIN_THREE_POINTS_HOLE 17
#define PIN_FOUR_POINTS_HOLE  26
#define PIN_FIVE_POINTS_HOLE  27
#define PIN_ZERO_POINTS_HOLE  24

/* Score values per sensor */
#define SCORE_THREE_POINTS 3
#define SCORE_FOUR_POINTS  4
#define SCORE_FIVE_POINTS  5
#define SCORE_ZERO_POINTS  0

#endif // GAME_MODES_H