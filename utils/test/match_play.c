#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define GPIO_CHIP "/dev/gpiochip0"
#define SENSOR_COUNT 4
#define SENSES_PER_TURN 2
#define COOLDOWN_TIME_MS 5000 // 5 seconds
#define POLL_INTERVAL_MS 50   // 50 millACCAZZiseconds
#define TOTAL_HOLES 9

int sensor_pins[SENSOR_COUNT] = {17, 24, 26, 27};
int sensor_points[SENSOR_COUNT] = {3, 4, 5, 0};

int last_sensor_state[SENSOR_COUNT] = {0};
long last_trigger_time[SENSOR_COUNT] = {0};

long get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int main() {
    struct gpiod_chip *chip;
    struct gpiod_line *lines[SENSOR_COUNT];
    int player_scores[2];
    int holes_won[2] = {0, 0};
    int holes_halved = 0;

    chip = gpiod_chip_open(GPIO_CHIP);
    if (!chip) {
        perror("Failed to open GPIO chip");
        return EXIT_FAILURE;
    }

    for (int i = 0; i < SENSOR_COUNT; i++) {
        lines[i] = gpiod_chip_get_line(chip, sensor_pins[i]);
        if (!lines[i] || gpiod_line_request_input(lines[i], "IR_Sensor") < 0) {
            fprintf(stderr, "Failed to configure GPIO %d\n", sensor_pins[i]);
            lines[i] = NULL;
        }
    }

    printf("\nMatch Play: Player 1 vs. Player 2 - 9 Holes\n");

    for (int hole = 1; hole <= TOTAL_HOLES; hole++) {
        printf("\nHole %d starts\n", hole);
        player_scores[0] = 0;
        player_scores[1] = 0;

        for (int player = 0; player < 2; player++) {
            printf("Player %d's turn:\n", player + 1);
            int senses = 0;

            while (senses < SENSES_PER_TURN) {
                for (int i = 0; i < SENSOR_COUNT; i++) {
                    if (!lines[i]) continue;
                    int sensor_value = gpiod_line_get_value(lines[i]);
                    long now = get_time_ms();

                    if (sensor_value == 1) {
                        last_sensor_state[i] = 1;
                    } else if (sensor_value == 0 && last_sensor_state[i] == 1) {
                        if (now - last_trigger_time[i] >= COOLDOWN_TIME_MS) {
                            last_trigger_time[i] = now;
                            last_sensor_state[i] = 0;

                            int points = sensor_points[i];
                            player_scores[player] += points;
                            senses++;

                            printf("  Sense %d/%d | GPIO %d | Points: %d\n", senses, SENSES_PER_TURN, sensor_pins[i], points);
                        }
                    }
                }
                usleep(POLL_INTERVAL_MS * 1000);
            }
        }

        // Evaluate hole result
        if (player_scores[0] > player_scores[1]) {
            holes_won[0]++;
            printf("Player 1 wins Hole %d!\n", hole);
        } else if (player_scores[1] > player_scores[0]) {
            holes_won[1]++;
            printf("Player 2 wins Hole %d!\n", hole);
        } else {
            holes_halved++;
            printf("Hole %d is halved.\n", hole);
        }

        // Check for early win
        int lead = holes_won[0] - holes_won[1];
        int holes_remaining = TOTAL_HOLES - hole;

        // Show match status for both players
        if (lead > 0)
            printf("Match status: Player 1: %dUP | Player 2: %dDN\n", lead, lead);
        else if (lead < 0)
            printf("Match status: Player 1: %dDN | Player 2: %dUP\n", -lead, -lead);
        else
            printf("Match status: All Square (E)\n");

        if (lead > holes_remaining) {
            printf("\nGame ends early: Player 1 wins %d&%d\n", lead, holes_remaining);
            break;
        } else if (-lead > holes_remaining) {
            printf("\nGame ends early: Player 2 wins %d&%d\n", -lead, holes_remaining);
            break;
        }
    }

    // Final outcome
    int final_lead = holes_won[0] - holes_won[1];
    printf("\nMatch Summary:\n");
    printf(" - Player 1 won %d holes\n", holes_won[0]);
    printf(" - Player 2 won %d holes\n", holes_won[1]);
    printf(" - Halved holes: %d\n", holes_halved);

    if (final_lead > 0)
        printf("Final Result: Player 1 wins %dUP\n", final_lead);
    else if (final_lead < 0)
        printf("Final Result: Player 2 wins %dUP\n", -final_lead);
    else
        printf("Final Result: Match is All Square (Draw)\n");

    for (int i = 0; i < SENSOR_COUNT; i++) {
        if (lines[i]) gpiod_line_release(lines[i]);
    }
    gpiod_chip_close(chip);
    return EXIT_SUCCESS;
}
