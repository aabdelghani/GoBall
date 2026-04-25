#ifndef PLAYER_NAME_H
#define PLAYER_NAME_H

#include "lvgl/lvgl.h"

#define PLAYER_NAME_MAX_LEN 15
#define PLAYER_NAME_COUNT   4

/* Call once after ui_init() — registers all player name labels */
void player_name_init(void);

/* Reset all names to defaults ("Player 1" – "Player 4") */
void player_name_reset(void);

/* Get the current display name for player idx (0-3) */
const char *player_name_get(int idx);

/* Set a custom name for player idx (0-3) */
void player_name_set(int idx, const char *name);

/* Register an LVGL label to track a player's name.
 * Adds clickable flag + click-to-edit callback.
 * Auto-unregisters when the label is deleted. */
void player_name_register_label(lv_obj_t *label, int player_idx);

/* Update all registered labels with current names */
void player_name_apply_all(void);

#endif /* PLAYER_NAME_H */
