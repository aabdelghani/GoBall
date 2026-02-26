#define _POSIX_C_SOURCE 199309L

#include "gpio_driver.h"
#include "game_state.h"
#include "event_bus.h"
#include "debug.h"

#include <gpiod.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#define NUM_SENSORS 4

static struct gpiod_chip     *chip = NULL;
static struct gpiod_line_bulk lines;
static timer_t               debounce_timers[NUM_SENSORS];
static volatile sig_atomic_t sensor_debouncing[NUM_SENSORS];

static int pin_to_sensor_index(unsigned int pin)
{
    game_state_t *gs = game_state_get();
    for (int i = 0; i < NUM_SENSORS; i++) {
        if (gs->sensor_pins[i] == pin) return i;
    }
    return -1;
}

static void debounce_timer_handler(int sig, siginfo_t *si, void *uc)
{
    (void)sig;
    (void)uc;
    int sensor_index = si->si_value.sival_int;
    if (sensor_index >= 0 && sensor_index < NUM_SENSORS) {
        sensor_debouncing[sensor_index] = 0;
        DEBUG_TRACE(MODULE_GPIO, "Debounce timer expired for sensor %d", sensor_index);
    }
}

static int init_debounce_timers(void)
{
    struct sigaction sa;
    struct sigevent sev;

    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = debounce_timer_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(DEBOUNCE_SIGNAL, &sa, NULL) == -1) {
        DEBUG_ERROR(MODULE_GPIO, "Failed to set up debounce signal handler");
        return -1;
    }

    for (int i = 0; i < NUM_SENSORS; i++) {
        sensor_debouncing[i] = 0;

        sev.sigev_notify = SIGEV_SIGNAL;
        sev.sigev_signo = DEBOUNCE_SIGNAL;
        sev.sigev_value.sival_int = i;

        if (timer_create(CLOCK_MONOTONIC, &sev, &debounce_timers[i]) == -1) {
            DEBUG_ERROR(MODULE_GPIO, "Failed to create debounce timer for sensor %d", i);
            return -1;
        }
        DEBUG_TRACE(MODULE_GPIO, "Created debounce timer for sensor %d", i);
    }

    DEBUG_INFO(MODULE_GPIO, "Debounce timers initialized (%d ms)", DEBOUNCE_TIME_MS);
    return 0;
}

static void start_debounce_timer(int sensor_index)
{
    if (sensor_index < 0 || sensor_index >= NUM_SENSORS) return;

    struct itimerspec its;
    its.it_value.tv_sec     = DEBOUNCE_TIME_MS / 1000;
    its.it_value.tv_nsec    = (DEBOUNCE_TIME_MS % 1000) * 1000000;
    its.it_interval.tv_sec  = 0;
    its.it_interval.tv_nsec = 0;

    sensor_debouncing[sensor_index] = 1;

    if (timer_settime(debounce_timers[sensor_index], 0, &its, NULL) == -1) {
        DEBUG_ERROR(MODULE_GPIO, "Failed to start debounce timer for sensor %d", sensor_index);
        sensor_debouncing[sensor_index] = 0;
    } else {
        DEBUG_TRACE(MODULE_GPIO, "Started debounce timer for sensor %d (%d ms)",
                    sensor_index, DEBOUNCE_TIME_MS);
    }
}

int gpio_driver_init(void)
{
    DEBUG_INFO(MODULE_GPIO, "Initializing GPIO driver");

    if (init_debounce_timers() < 0) {
        DEBUG_ERROR(MODULE_GPIO, "Failed to initialize debounce timers");
        return -1;
    }

    game_state_t *gs = game_state_get();

    chip = gpiod_chip_open("/dev/gpiochip0");
    if (!chip) {
        DEBUG_ERROR(MODULE_GPIO, "Failed to open GPIO chip: /dev/gpiochip0");
        return -1;
    }

    if (gpiod_chip_get_lines(chip, gs->sensor_pins, NUM_SENSORS, &lines) < 0) {
        DEBUG_ERROR(MODULE_GPIO, "Failed to get GPIO lines");
        gpiod_chip_close(chip);
        return -1;
    }

    if (gpiod_line_request_bulk_falling_edge_events(&lines, "goball-sensors") < 0) {
        DEBUG_ERROR(MODULE_GPIO, "Failed to request falling edge events");
        gpiod_line_release_bulk(&lines);
        gpiod_chip_close(chip);
        return -1;
    }

    DEBUG_INFO(MODULE_GPIO, "GPIO driver initialized - %d sensors configured", NUM_SENSORS);
    return 0;
}

void gpio_driver_poll(void)
{
    game_state_t *gs = game_state_get();
    struct gpiod_line_bulk event_lines;
    struct gpiod_line_event event;

    if (gpiod_line_event_wait_bulk(&lines, &(struct timespec){0, 0}, &event_lines) <= 0)
        return;

    for (unsigned int i = 0; i < gpiod_line_bulk_num_lines(&event_lines); i++) {
        struct gpiod_line *line = gpiod_line_bulk_get_line(&event_lines, i);
        if (!line) continue;

        if (gpiod_line_event_read(line, &event) != 0)
            continue;

        if (!gs->sensors_enabled)
            continue;

        uint8_t pin_offset = gpiod_line_offset(line);
        int sensor_index = pin_to_sensor_index(pin_offset);
        if (sensor_index < 0) {
            DEBUG_ERROR(MODULE_GPIO, "Unknown GPIO pin: %d", pin_offset);
            continue;
        }

        if (sensor_debouncing[sensor_index])
            continue;

        start_debounce_timer(sensor_index);
        DEBUG_DEBUG(MODULE_GPIO, "Valid sensor event - pin: %d, sensor: %d", pin_offset, sensor_index);

        /* Publish event — game controller will handle scoring */
        game_event_t ge = { .type = EVENT_SCORE_CHANGED };
        ge.data.score.pin = pin_offset;
        ge.data.score.player_index = gs->current_player_index;
        ge.data.score.score = gs->players[gs->current_player_index].score;
        ge.data.score.hole = gs->players[gs->current_player_index].current_hole;
        ge.data.score.detection_count = gs->players[gs->current_player_index].detection_count;
        ge.data.score.num_players = gs->num_players;
        event_bus_publish(&ge);
    }
}

void gpio_driver_cleanup(int signal)
{
    DEBUG_INFO(MODULE_GPIO, "Caught signal %d - cleaning up GPIO", signal);

    if (chip) {
        gpiod_line_release_bulk(&lines);
        gpiod_chip_close(chip);
        chip = NULL;
        DEBUG_INFO(MODULE_GPIO, "GPIO resources released");
    }
}
