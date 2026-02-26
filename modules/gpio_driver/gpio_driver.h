#ifndef GPIO_DRIVER_H
#define GPIO_DRIVER_H

#define DEBOUNCE_TIME_MS 3000
#define DEBOUNCE_SIGNAL  SIGRTMIN

/**
 * Initialize GPIO chip, lines, and debounce timers.
 * @return 0 on success, -1 on failure.
 */
int gpio_driver_init(void);

/**
 * Poll for GPIO events (non-blocking).
 * Publishes EVENT_SCORE_CHANGED via event bus when a valid debounced event occurs.
 */
void gpio_driver_poll(void);

/**
 * Cleanup GPIO resources.
 */
void gpio_driver_cleanup(int signal);

#endif // GPIO_DRIVER_H
