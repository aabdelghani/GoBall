#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <gpiod.h>  // Include libgpiod

#define DEBOUNCE_TIME 3  // Time to prevent multiple sensor readings (in seconds)
#define NUM_PLAYERS 2
#define MAX_HOLES 8
#define SENSORS_PER_TURN 2

// Define sensor pins - RPI 5 -
int sensor_pins[] = {17, 26, 27, 24};
#define NUM_SENSORS 4  // Number of sensor pins

// Player data
typedef struct {
    int score;
    int current_hole;
    int detection_count;
} Player;

Player players[NUM_PLAYERS];
int current_player_index = 0;
time_t last_activation_times[NUM_SENSORS];

// GPIO chip and line handles
struct gpiod_chip *chip;
struct gpiod_line *sensor_lines[NUM_SENSORS];

// Function to initialize player data
void initialize_players() {
    for (int i = 0; i < NUM_PLAYERS; i++) {
        players[i].score = 0;
        players[i].current_hole = 0;
        players[i].detection_count = 0;
    }
}

// Function to print the current game state
void print_game_state(int player_index, int current_hole, int score, int detection_count) {
    printf("Player %d - Hole %d - Score: %d - Detection Count: %d\n",
           player_index + 1, current_hole + 1, score, detection_count);
}

// Function to handle sensor events
void handle_sensor_event(int pin_index) {
    time_t current_time = time(NULL);

    if (current_time - last_activation_times[pin_index] > DEBOUNCE_TIME) {
        last_activation_times[pin_index] = current_time;
        Player *player = &players[current_player_index];

        if (player->detection_count < SENSORS_PER_TURN) {
            if (pin_index == 0) {
                player->score += 3;
            } else if (pin_index == 1) {
                player->score += 4;
            } else if (pin_index == 2) {
                player->score += 5;
            } else if (pin_index == 3) {
                player->score += 0;
            }

            player->detection_count++;
            print_game_state(current_player_index, player->current_hole, player->score, player->detection_count);

            if (player->detection_count == SENSORS_PER_TURN) {
                player->current_hole++;
                player->detection_count = 0;
                current_player_index = (current_player_index + 1) % NUM_PLAYERS;

                if (player->current_hole > MAX_HOLES) {
                    printf("All players have completed the game.\n");
                    exit(0);
                }
            }
        }
    }
}

// Signal handler for sensor events
void sensor_event_handler(int signum) {
    (void)signum;
    for (int i = 0; i < NUM_SENSORS; i++) {
        struct gpiod_line_event event;
        if (gpiod_line_event_read(sensor_lines[i], &event) == 0) {
            if (event.event_type == GPIOD_LINE_EVENT_FALLING_EDGE) {
                handle_sensor_event(i);
            }
        }
    }
}

// Function to initialize GPIO sensors
void initialize_sensors() {
    // Open the GPIO chip
    chip = gpiod_chip_open_by_name("gpiochip0");
    if (!chip) {
        perror("Failed to open GPIO chip");
        exit(1);
    }

    // Get GPIO lines for sensor pins
    for (int i = 0; i < NUM_SENSORS; i++) {
        sensor_lines[i] = gpiod_chip_get_line(chip, sensor_pins[i]);
        if (!sensor_lines[i]) {
            perror("Failed to get GPIO line");
            for (int j = 0; j < i; j++) {
                gpiod_line_release(sensor_lines[j]);
            }
            gpiod_chip_close(chip);
            exit(1);
        }

        // Request the line for falling edge events
        if (gpiod_line_request_falling_edge_events(sensor_lines[i], "sensor") < 0) {
            perror("Failed to request GPIO line for events");
            for (int j = 0; j <= i; j++) {
                gpiod_line_release(sensor_lines[j]);
            }
            gpiod_chip_close(chip);
            exit(1);
        }
    }

    // Set up signal handler for GPIO events
    signal(SIGIO, sensor_event_handler);

    // Enable asynchronous I/O for the GPIO chip
    for (int i = 0; i < NUM_SENSORS; i++) {
if (gpiod_line_set_flags(sensor_lines[i], GPIOD_LINE_REQUEST_FLAG_ACTIVE_LOW) < 0) {
    perror("Failed to set GPIO line flags");
    printf("Error occurred on GPIO line %d\n", sensor_pins[i]);
    for (int j = 0; j <= i; j++) {
        gpiod_line_release(sensor_lines[j]);
    }
    gpiod_chip_close(chip);
    exit(1);
}

    }
}

// Function to release GPIO resources
void cleanup_resources() {
    for (int i = 0; i < NUM_SENSORS; i++) {
        gpiod_line_release(sensor_lines[i]);
    }
    gpiod_chip_close(chip);
}

int main() {
    // Initialize players
    initialize_players();

    // Initialize sensors
    initialize_sensors();

    // Initialize last activation times
    for (int i = 0; i < NUM_SENSORS; i++) {
        last_activation_times[i] = 0;
    }

    printf("Game started. Waiting for sensor inputs...\n");

    // Main loop (can be empty or used for other tasks)
    while (1) {
        // You can add other tasks here or leave it empty
        sleep(1);  // Sleep to avoid busy-waiting
    }

    // Cleanup (unreachable in this example)
    cleanup_resources();

    return 0;
}
