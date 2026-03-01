#include "game_videos.h"
#include "../../ui/ui.h"
#include "../debug/debug.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <sys/prctl.h>
#include <errno.h>

static pid_t video_pid = -1;
static game_video_back_cb_t _back_cb = NULL;
static lv_obj_t *controls_bar = NULL;
static lv_obj_t *seek_slider = NULL;
static lv_timer_t *progress_timer = NULL;

/* Playback state */
static const char *_video_path = NULL;
static int _screen_x = 0, _screen_y = 0;
static int _video_w = 0, _video_h = 0;
static double _video_duration = 37.0;  /* seconds — updated by ffprobe */
static double _playback_start_time = 0;
static double _seek_offset = 0;
static bool _paused = false;
static bool _slider_dragging = false;

/* Deferred-start flag for handle_draw */
static bool _video_started = false;

/* Saved args for async start */
static lv_obj_t *_pending_panel = NULL;
static const char *_pending_video_path = NULL;
static game_video_back_cb_t _pending_back_cb = NULL;

/* Height reserved for the LVGL controls area */
#define CONTROLS_HEIGHT 85

/* ── helpers ──────────────────────────────────────────── */

static double get_time_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void get_video_duration(const char *path)
{
    /* Try to get duration via ffprobe — run synchronously, fast */
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "ffprobe -v quiet -show_entries format=duration -of csv=p=0 '%s' 2>/dev/null", path);
    FILE *fp = popen(cmd, "r");
    if (fp) {
        char buf[64];
        if (fgets(buf, sizeof(buf), fp)) {
            double dur = atof(buf);
            if (dur > 0.5) {
                _video_duration = dur;
                DEBUG_INFO(MODULE_VIDEO, "Detected duration: %.1f sec", _video_duration);
            } else {
                DEBUG_WARN(MODULE_VIDEO, "ffprobe returned invalid duration: %s", buf);
            }
        } else {
            DEBUG_WARN(MODULE_VIDEO, "ffprobe returned no output for: %s", path);
        }
        pclose(fp);
    } else {
        DEBUG_ERROR(MODULE_VIDEO, "Failed to run ffprobe (is ffmpeg installed?): %s", strerror(errno));
    }
}

/* Refocus app window after ffplay steals focus (Wayland/labwc).
 * Uses double-fork so grandchild can sleep without blocking LVGL. */
static void refocus_app_async(void)
{
    pid_t p = fork();
    if (p == 0) {
        /* Intermediate child — fork grandchild and exit immediately */
        if (fork() == 0) {
            /* Grandchild: wait for ffplay to appear, then refocus */
            usleep(800000); /* 800ms */
            execlp("wlrctl", "wlrctl", "toplevel", "focus",
                   "SquareLine_Project", NULL);
            _exit(1);
        }
        _exit(0);
    }
    if (p > 0) waitpid(p, NULL, 0); /* reap intermediate child instantly */
}

static void spawn_ffplay(double start_sec)
{
    if (video_pid > 0) {
        kill(video_pid, SIGTERM);
        waitpid(video_pid, NULL, 0);
        video_pid = -1;
    }

    _seek_offset = start_sec;
    _playback_start_time = get_time_sec();
    _paused = false;

    video_pid = fork();
    if (video_pid == 0) {
        /* Auto-kill ffplay when parent process dies */
        prctl(PR_SET_PDEATHSIG, SIGTERM);

        char width_s[16], height_s[16], left_s[16], top_s[16], ss_s[16];
        snprintf(width_s, sizeof(width_s), "%d", _video_w);
        snprintf(height_s, sizeof(height_s), "%d", _video_h);
        snprintf(left_s, sizeof(left_s), "%d", _screen_x);
        snprintf(top_s, sizeof(top_s), "%d", _screen_y);
        snprintf(ss_s, sizeof(ss_s), "%.1f", start_sec);

        if (start_sec > 0.5) {
            execlp("ffplay", "ffplay",
                   "-noborder", "-alwaysontop",
                   "-x", width_s, "-y", height_s,
                   "-left", left_s, "-top", top_s,
                   "-ss", ss_s,
                   "-loop", "0",
                   "-loglevel", "quiet",
                   _video_path, NULL);
        } else {
            execlp("ffplay", "ffplay",
                   "-noborder", "-alwaysontop",
                   "-x", width_s, "-y", height_s,
                   "-left", left_s, "-top", top_s,
                   "-loop", "0",
                   "-loglevel", "quiet",
                   _video_path, NULL);
        }
        /* execlp only returns on error */
        fprintf(stderr, "[VIDEO] ERROR: execlp(ffplay) failed: %s (is ffplay installed?)\n",
                strerror(errno));
        _exit(1);
    } else if (video_pid < 0) {
        DEBUG_ERROR(MODULE_VIDEO, "fork() failed: %s", strerror(errno));
        video_pid = -1;
    } else {
        DEBUG_INFO(MODULE_VIDEO, "ffplay spawned PID %d (seek=%.1fs, pos=%d,%d size=%dx%d)",
                   (int)video_pid, start_sec, _screen_x, _screen_y, _video_w, _video_h);
        /* Refocus app window after delay so LVGL controls respond to first click */
        refocus_app_async();
    }
}

/* ── timer callback for slider progress ───────────────── */

static void progress_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (_slider_dragging || _paused || video_pid <= 0 || seek_slider == NULL) return;

    double elapsed = get_time_sec() - _playback_start_time + _seek_offset;
    /* Handle looping */
    if (elapsed >= _video_duration) {
        elapsed = 0;
        _seek_offset = 0;
        _playback_start_time = get_time_sec();
    }

    int pct = (int)((elapsed / _video_duration) * 100);
    if (pct > 100) pct = 100;
    if (pct < 0) pct = 0;
    lv_slider_set_value(seek_slider, pct, LV_ANIM_OFF);
}

/* ── control callbacks ────────────────────────────────── */

static void btn_pause_cb(lv_event_t *e)
{
    (void)e;
    if (video_pid > 0) {
        if (_paused) {
            DEBUG_DEBUG(MODULE_VIDEO, "Resume (PID %d, offset=%.1fs)", (int)video_pid, _seek_offset);
            kill(video_pid, SIGCONT);
            _playback_start_time = get_time_sec();
            _paused = false;
        } else {
            _seek_offset += get_time_sec() - _playback_start_time;
            DEBUG_DEBUG(MODULE_VIDEO, "Pause (PID %d, position=%.1fs)", (int)video_pid, _seek_offset);
            kill(video_pid, SIGSTOP);
            _paused = true;
        }
    }
}

static void slider_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED) {
        _slider_dragging = true;
    } else if (code == LV_EVENT_RELEASED) {
        _slider_dragging = false;
        int val = lv_slider_get_value(seek_slider);
        double seek_to = (_video_duration * val) / 100.0;
        DEBUG_INFO(MODULE_VIDEO, "Seek to %.1f/%.1f sec (%d%%)", seek_to, _video_duration, val);
        spawn_ffplay(seek_to);
    }
}

static lv_obj_t *create_control_btn(lv_obj_t *parent, const char *text, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 140, 50);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2D2D2D), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x00F46A), 0);
    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_radius(btn, 8, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(0x00F46A), 0);
    lv_obj_set_style_text_font(label, &ui_font_Unitblock_48, 0);
    lv_obj_center(label);

    return btn;
}

/* ── public API ───────────────────────────────────────── */

void game_video_play(lv_obj_t *parent, const char *video_path,
                     game_video_back_cb_t back_cb)
{
    DEBUG_INFO(MODULE_VIDEO, "game_video_play() called");
    DEBUG_INFO(MODULE_VIDEO, "  video path: %s", video_path);

    /* Check if video file exists */
    if (access(video_path, F_OK) != 0) {
        DEBUG_ERROR(MODULE_VIDEO, "Video file not found: %s", video_path);
        return;
    }
    if (access(video_path, R_OK) != 0) {
        DEBUG_ERROR(MODULE_VIDEO, "Video file not readable (permission denied): %s", video_path);
        return;
    }

    if (video_pid > 0) {
        DEBUG_DEBUG(MODULE_VIDEO, "Stopping previous video (PID %d) before starting new one", (int)video_pid);
        game_video_stop();
    }

    _back_cb = back_cb;
    _video_path = video_path;

    /* Get video duration */
    get_video_duration(video_path);

    /* Get SDL window position on screen */
    int win_x = 0, win_y = 0;
    SDL_Window *sdl_win = SDL_GetWindowFromID(1);
    if (sdl_win) {
        SDL_GetWindowPosition(sdl_win, &win_x, &win_y);
        DEBUG_DEBUG(MODULE_VIDEO, "SDL window position: %d, %d", win_x, win_y);
    } else {
        DEBUG_WARN(MODULE_VIDEO, "SDL_GetWindowFromID(1) returned NULL — using position 0,0");
    }

    /* Panel inner area: 1320x552 centered on 2560x720 screen */
    int panel_x = 620;
    int panel_y = 84;
    int panel_w = 1320;
    int panel_h = 552;

    _screen_x = win_x + panel_x;
    _screen_y = win_y + panel_y;
    _video_w = panel_w;
    _video_h = panel_h - CONTROLS_HEIGHT;

    DEBUG_INFO(MODULE_VIDEO, "Video rect: screen(%d,%d) size(%dx%d)",
               _screen_x, _screen_y, _video_w, _video_h);

    /* Set up parent layout — controls at bottom */
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, 5, 0);
    lv_obj_set_style_pad_row(parent, 3, 0);

    /* Controls container */
    controls_bar = lv_obj_create(parent);
    lv_obj_remove_style_all(controls_bar);
    lv_obj_set_size(controls_bar, lv_pct(100), CONTROLS_HEIGHT - 5);
    lv_obj_set_flex_flow(controls_bar, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(controls_bar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(controls_bar, 5, 0);

    /* Seek slider */
    seek_slider = lv_slider_create(controls_bar);
    lv_obj_set_size(seek_slider, lv_pct(90), 10);
    lv_slider_set_range(seek_slider, 0, 100);
    lv_slider_set_value(seek_slider, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(seek_slider, lv_color_hex(0x555555), LV_PART_MAIN);
    lv_obj_set_style_bg_color(seek_slider, lv_color_hex(0x00F46A), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(seek_slider, lv_color_hex(0x00F46A), LV_PART_KNOB);
    lv_obj_set_style_pad_all(seek_slider, 3, LV_PART_KNOB);
    lv_obj_add_event_cb(seek_slider, slider_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(seek_slider, slider_event_cb, LV_EVENT_RELEASED, NULL);

    /* Buttons row */
    lv_obj_t *btn_row = lv_obj_create(controls_bar);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_size(btn_row, lv_pct(100), 55);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btn_row, 30, 0);

    create_control_btn(btn_row, "Pause", btn_pause_cb);

    /* Start progress timer (update slider every 500ms) */
    progress_timer = lv_timer_create(progress_timer_cb, 500, NULL);

    /* Spawn ffplay */
    spawn_ffplay(0);

    DEBUG_INFO(MODULE_VIDEO, "Playback started (duration=%.1fs, controls enabled)", _video_duration);
}

void game_video_stop(void)
{
    DEBUG_INFO(MODULE_VIDEO, "game_video_stop() called (pid=%d)", (int)video_pid);

    if (progress_timer != NULL) {
        lv_timer_delete(progress_timer);
        progress_timer = NULL;
    }

    if (video_pid > 0) {
        DEBUG_DEBUG(MODULE_VIDEO, "Sending SIGTERM to ffplay PID %d", (int)video_pid);
        kill(video_pid, SIGCONT);  /* Resume first in case paused */
        kill(video_pid, SIGTERM);
        int status;
        waitpid(video_pid, &status, 0);
        if (WIFEXITED(status)) {
            DEBUG_DEBUG(MODULE_VIDEO, "ffplay exited with code %d", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            DEBUG_DEBUG(MODULE_VIDEO, "ffplay killed by signal %d", WTERMSIG(status));
        }
        video_pid = -1;
    }

    if (controls_bar != NULL) {
        lv_obj_delete(controls_bar);
        controls_bar = NULL;
    }
    seek_slider = NULL;

    _back_cb = NULL;
    _video_path = NULL;
    _slider_dragging = false;
    _paused = false;
    _seek_offset = 0;
    DEBUG_DEBUG(MODULE_VIDEO, "game_video_stop() complete");
}

bool game_video_is_playing(void)
{
    if (video_pid > 0) {
        int status;
        pid_t result = waitpid(video_pid, &status, WNOHANG);
        if (result == video_pid) {
            if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                DEBUG_WARN(MODULE_VIDEO, "ffplay exited unexpectedly with code %d", WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                DEBUG_WARN(MODULE_VIDEO, "ffplay killed by signal %d", WTERMSIG(status));
            }
            video_pid = -1;
            return false;
        }
        return true;
    }
    return false;
}

/* ── reusable screen helpers ─────────────────────────── */

void game_video_prepare(void)
{
    _video_started = false;
    DEBUG_DEBUG(MODULE_VIDEO, "game_video_prepare() - ready for next screen");
}

static void _deferred_play(void *data)
{
    (void)data;
    DEBUG_DEBUG(MODULE_VIDEO, "_deferred_play() - starting video playback");
    game_video_play(_pending_panel, _pending_video_path, _pending_back_cb);
}

void game_video_handle_draw(lv_obj_t *panel, const char *video_path,
                            game_video_back_cb_t back_cb)
{
    if (!_video_started) {
        DEBUG_DEBUG(MODULE_VIDEO, "handle_draw() - deferring video start for: %s", video_path);
        _video_started = true;
        _pending_panel = panel;
        _pending_video_path = video_path;
        _pending_back_cb = back_cb;
        lv_async_call(_deferred_play, NULL);
    }
}
