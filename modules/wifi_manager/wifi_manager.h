#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "lvgl.h"

/**
 * Initialize the WiFi manager.
 * Makes the G logo on the home screen clickable to open the WiFi selector.
 */
void wifi_manager_init(void);

/**
 * Show the WiFi network selector overlay.
 */
void wifi_manager_show(void);

#endif /* WIFI_MANAGER_H */
