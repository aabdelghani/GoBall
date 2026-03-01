#ifndef GAME_VIDEOS_H
#define GAME_VIDEOS_H

#include "lvgl/lvgl.h"
#include <stdbool.h>

/* Video file paths — add new videos here */
#ifdef YOCTO_BUILD
#define GAME_VIDEO_VISUALIZE_TIP "/opt/goball/videos/visualize_tip.mp4"
#else
#define GAME_VIDEO_VISUALIZE_TIP "/home/q/Desktop/SquareLine_Project/modules/game_videos/visualize_tip.mp4"
#endif

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

/* Call when navigating TO a video screen — resets the start flag so
 * game_video_handle_draw() will trigger playback on the first draw. */
void game_video_prepare(void);

/* Call from a panel's LV_EVENT_DRAW_MAIN handler. On first draw after
 * game_video_prepare(), starts video playback asynchronously.
 * Subsequent draws are ignored (prevents re-trigger during fade-out). */
void game_video_handle_draw(lv_obj_t *panel, const char *video_path,
                            game_video_back_cb_t back_cb);

#endif /* GAME_VIDEOS_H */
