#ifndef GAME_VIDEOS_H
#define GAME_VIDEOS_H

#include "lvgl/lvgl.h"
#include <stdbool.h>

#define GAME_VIDEO_VISUALIZE_TIP "/home/q/Desktop/SquareLine_Project/modules/game_videos/visualize_tip.mp4"

/* Callback invoked when user presses Back on video controls */
typedef void (*game_video_back_cb_t)(void);

/* Start video playback using external ffplay process at panel position.
 * back_cb is called when the user presses Back (can be NULL). */
void game_video_play(lv_obj_t *parent, const char *video_path,
                     game_video_back_cb_t back_cb);

/* Stop and clean up video playback. */
void game_video_stop(void);

/* Returns true if a video is currently playing. */
bool game_video_is_playing(void);

#endif /* GAME_VIDEOS_H */
