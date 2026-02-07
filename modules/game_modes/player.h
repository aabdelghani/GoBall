#ifndef PLAYER_H
#define PLAYER_H

typedef struct Player {
    int score;
    int current_hole;
    int detection_count;
    int holes_won;
    char match_status[10];
    int holes_halved;
    signed char upAndDown; 
    int round_total_score;
} Player;


#endif // PLAYER_H