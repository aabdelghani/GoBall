#include "game_videos.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../../ui/ui.h"
#include "../debug/debug.h"

#define MPV_IPC_PATH "/tmp/mpv-ipc"
#define VIDEO_ROW_RATIO  6  /* row1:row2 = 6:1 */

static pid_t                video_pid       = -1;
static int                  _mpv_ipc_fd     = -1;
static game_video_back_cb_t _back_cb        = NULL;
static lv_timer_t          *_ontop_timer    = NULL;

/* Playback state */
static const char *_video_path = NULL;
static int         _video_w = 0, _video_h = 0;
static int         _video_x = 0, _video_y = 0;

/* Deferred-start flag for handle_draw */
static bool _video_started = false;

/* Saved args for async start */
static lv_obj_t            *_pending_panel      = NULL;
static const char          *_pending_video_path = NULL;
static game_video_back_cb_t _pending_back_cb    = NULL;

/* LVGL controls */
static lv_obj_t *_seek_slider = NULL;
static lv_obj_t *_play_btn    = NULL;
static lv_obj_t *_play_label  = NULL;
static bool      _seeking     = false;  /* true while user is dragging slider */
static bool      _eof_reached = false;  /* true when video reached end */

/* ── helpers ──────────────────────────────────────────── */

static int _src_w = 1920, _src_h = 1080;

static void get_video_dimensions(const char *path)
{
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "ffprobe -v quiet -select_streams v:0 "
             "-show_entries stream=width,height -of csv=p=0:s=x '%s' 2>/dev/null",
             path);
    FILE *fp = popen(cmd, "r");
    if (fp)
    {
        int w = 0, h = 0;
        if (fscanf(fp, "%dx%d", &w, &h) == 2 && w > 0 && h > 0)
        {
            _src_w = w;
            _src_h = h;
            DEBUG_INFO(MODULE_VIDEO, "Video dimensions: %dx%d", _src_w, _src_h);
        }
        pclose(fp);
    }
}

/* ── mpv IPC ─────────────────────────────────────────── */

static int mpv_ipc_connect(void)
{
    if (_mpv_ipc_fd >= 0) return 0;

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, MPV_IPC_PATH, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        close(fd);
        return -1;
    }

    /* Set non-blocking so reads don't stall the LVGL thread */
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    _mpv_ipc_fd = fd;
    DEBUG_INFO(MODULE_VIDEO, "Connected to mpv IPC socket");
    return 0;
}

static void mpv_ipc_send(const char *json_cmd)
{
    if (_mpv_ipc_fd < 0 && mpv_ipc_connect() < 0) return;

    ssize_t n = write(_mpv_ipc_fd, json_cmd, strlen(json_cmd));
    if (n < 0)
    {
        DEBUG_DEBUG(MODULE_VIDEO, "IPC write failed, reconnecting: %s", strerror(errno));
        close(_mpv_ipc_fd);
        _mpv_ipc_fd = -1;
    }
    else
    {
        /* Drain any response data so the socket buffer doesn't fill up */
        char buf[256];
        while (read(_mpv_ipc_fd, buf, sizeof(buf)) > 0) {}
    }
}

/**
 * Send a get_property command and parse the numeric response.
 * Returns the value or -1 on failure. Non-blocking.
 */
static double mpv_ipc_get_property_number(const char *property, int req_id)
{
    if (_mpv_ipc_fd < 0 && mpv_ipc_connect() < 0) return -1;

    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "{\"command\":[\"get_property\",\"%s\"],\"request_id\":%d}\n",
             property, req_id);

    ssize_t n = write(_mpv_ipc_fd, cmd, strlen(cmd));
    if (n < 0)
    {
        close(_mpv_ipc_fd);
        _mpv_ipc_fd = -1;
        return -1;
    }

    /* Brief wait for response (non-blocking socket, small delay) */
    usleep(5000); /* 5ms */

    /* Read all available data */
    char buf[1024];
    ssize_t total = 0;
    memset(buf, 0, sizeof(buf));
    while (total < (ssize_t)(sizeof(buf) - 1))
    {
        ssize_t r = read(_mpv_ipc_fd, buf + total, sizeof(buf) - 1 - total);
        if (r <= 0) break;
        total += r;
    }

    if (total <= 0) return -1;

    /* Find the response with our request_id */
    char req_tag[32];
    snprintf(req_tag, sizeof(req_tag), "\"request_id\":%d", req_id);
    char *line = strstr(buf, req_tag);
    if (!line) return -1;

    /* Find "data": value in this line */
    char *data_pos = strstr(line - 80 > buf ? line - 80 : buf, "\"data\":");
    if (!data_pos || data_pos > line + 20) return -1;

    double val = -1;
    if (sscanf(data_pos + 7, "%lf", &val) == 1)
        return val;

    return -1;
}

/**
 * Get boolean property (like "pause"). Returns 0/1 or -1 on failure.
 */
static int mpv_ipc_get_property_bool(const char *property, int req_id)
{
    if (_mpv_ipc_fd < 0 && mpv_ipc_connect() < 0) return -1;

    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "{\"command\":[\"get_property\",\"%s\"],\"request_id\":%d}\n",
             property, req_id);

    ssize_t n = write(_mpv_ipc_fd, cmd, strlen(cmd));
    if (n < 0)
    {
        close(_mpv_ipc_fd);
        _mpv_ipc_fd = -1;
        return -1;
    }

    usleep(5000);

    char buf[1024];
    ssize_t total = 0;
    memset(buf, 0, sizeof(buf));
    while (total < (ssize_t)(sizeof(buf) - 1))
    {
        ssize_t r = read(_mpv_ipc_fd, buf + total, sizeof(buf) - 1 - total);
        if (r <= 0) break;
        total += r;
    }

    if (total <= 0) return -1;

    char req_tag[32];
    snprintf(req_tag, sizeof(req_tag), "\"request_id\":%d", req_id);
    char *line = strstr(buf, req_tag);
    if (!line) return -1;

    /* Check for "data":true or "data":false */
    char *data_pos = strstr(line - 80 > buf ? line - 80 : buf, "\"data\":");
    if (!data_pos || data_pos > line + 20) return -1;

    if (strstr(data_pos, "true")) return 1;
    if (strstr(data_pos, "false")) return 0;
    return -1;
}

static void mpv_ipc_close(void)
{
    if (_mpv_ipc_fd >= 0)
    {
        close(_mpv_ipc_fd);
        _mpv_ipc_fd = -1;
    }
}

/* ── LVGL control callbacks ──────────────────────────── */

static void play_btn_cb(lv_event_t *e)
{
    (void)e;
    if (_eof_reached)
    {
        /* Restart from beginning */
        mpv_ipc_send("{\"command\":[\"seek\",0,\"absolute\"]}\n");
        mpv_ipc_send("{\"command\":[\"set_property\",\"pause\",false]}\n");
        _eof_reached = false;
    }
    else
    {
        mpv_ipc_send("{\"command\":[\"cycle\",\"pause\"]}\n");
    }
}

static void slider_pressed_cb(lv_event_t *e)
{
    (void)e;
    _seeking = true;
}

static void slider_released_cb(lv_event_t *e)
{
    (void)e;
    _seeking = false;
    if (!_seek_slider) return;

    int32_t val = lv_slider_get_value(_seek_slider);
    double pct = val / 10.0;

    char cmd[128];
    snprintf(cmd, sizeof(cmd),
             "{\"command\":[\"seek\",%.1f,\"absolute-percent\",\"exact\"]}\n", pct);
    mpv_ipc_send(cmd);
}

/* ── LVGL controls creation ──────────────────────────── */

static void create_controls(lv_obj_t *parent)
{
    /* Get parent inner dimensions to calculate row 2 height */
    int pw = lv_obj_get_width(parent);
    int ph = lv_obj_get_height(parent);
    int row2_h = ph / (VIDEO_ROW_RATIO + 1);  /* 1/7 of panel height */
    int btn_sz = (row2_h - 4) * 80 / 100;         /* square button, 20% smaller */

    /* Row 2, Col 1: Play/Pause button */
    _play_btn = lv_obj_create(parent);
    lv_obj_remove_style_all(_play_btn);
    lv_obj_set_size(_play_btn, btn_sz, btn_sz);
    lv_obj_align(_play_btn, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(_play_btn, lv_color_hex(0x00F46A), 0);
    lv_obj_set_style_bg_opa(_play_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(_play_btn, 6, 0);
    lv_obj_add_flag(_play_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_play_btn, play_btn_cb, LV_EVENT_CLICKED, NULL);

    _play_label = lv_label_create(_play_btn);
    lv_label_set_text(_play_label, LV_SYMBOL_PAUSE);
    lv_obj_set_style_text_color(_play_label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(_play_label, &lv_font_montserrat_32, 0);
    lv_obj_center(_play_label);

    /* Row 2, Col 2: Seekbar slider (fills remaining width) */
    int slider_w = pw - btn_sz - 10;  /* panel width minus button minus gap */
    _seek_slider = lv_slider_create(parent);
    lv_obj_set_height(_seek_slider, 10);
    lv_obj_set_width(_seek_slider, slider_w);
    lv_obj_align_to(_seek_slider, _play_btn, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    lv_slider_set_range(_seek_slider, 0, 1000);
    lv_slider_set_value(_seek_slider, 0, LV_ANIM_OFF);

    /* Style: green indicator, dark track */
    lv_obj_set_style_bg_color(_seek_slider, lv_color_hex(0x444444), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_seek_slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(_seek_slider, 4, LV_PART_MAIN);

    lv_obj_set_style_bg_color(_seek_slider, lv_color_hex(0x00F46A), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(_seek_slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(_seek_slider, 4, LV_PART_INDICATOR);

    lv_obj_set_style_bg_color(_seek_slider, lv_color_hex(0x00F46A), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(_seek_slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_pad_all(_seek_slider, 6, LV_PART_KNOB);

    lv_obj_add_event_cb(_seek_slider, slider_pressed_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(_seek_slider, slider_released_cb, LV_EVENT_RELEASED, NULL);
}

static void destroy_controls(void)
{
    if (_seek_slider) { lv_obj_delete(_seek_slider); _seek_slider = NULL; }
    if (_play_btn)    { lv_obj_delete(_play_btn);    _play_btn = NULL; }
    _play_label = NULL;
}

/* ── ontop enforcer timer ────────────────────────────── */

static void ontop_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (video_pid <= 0) return;

    /* Check if mpv is still alive */
    int   status;
    pid_t result = waitpid(video_pid, &status, WNOHANG);
    if (result == video_pid)
    {
        DEBUG_INFO(MODULE_VIDEO, "mpv exited (detected by ontop timer)");
        video_pid = -1;
        mpv_ipc_close();
        return;
    }

    /* Poll playback position and update slider */
    if (_seek_slider && !_seeking)
    {
        double pct = mpv_ipc_get_property_number("percent-pos", 1);
        if (pct >= 0)
        {
            lv_slider_set_value(_seek_slider, (int32_t)(pct * 10), LV_ANIM_OFF);
        }
    }

    /* Poll EOF state */
    int eof = mpv_ipc_get_property_bool("eof-reached", 3);
    if (eof == 1)
        _eof_reached = true;
    else if (eof == 0)
        _eof_reached = false;

    /* Update button label: rewind if EOF, play/pause otherwise */
    if (_play_label)
    {
        if (_eof_reached)
            lv_label_set_text(_play_label, LV_SYMBOL_REFRESH);
        else
        {
            int paused = mpv_ipc_get_property_bool("pause", 2);
            if (paused == 1)
                lv_label_set_text(_play_label, LV_SYMBOL_PLAY);
            else if (paused == 0)
                lv_label_set_text(_play_label, LV_SYMBOL_PAUSE);
        }
    }
}

/* ── mpv spawn ────────────────────────────────────────── */

static void spawn_mpv(void)
{
    if (video_pid > 0)
    {
        kill(video_pid, SIGTERM);
        waitpid(video_pid, NULL, 0);
        video_pid = -1;
    }

    mpv_ipc_close();
    unlink(MPV_IPC_PATH);

    DEBUG_INFO(MODULE_VIDEO, "Spawning mpv: file=%s size=%dx%d", _video_path, _video_w, _video_h);

    video_pid = fork();
    if (video_pid == 0)
    {
        prctl(PR_SET_PDEATHSIG, SIGTERM);

        char geom_arg[64];
        snprintf(geom_arg, sizeof(geom_arg), "--geometry=%dx%d+%d+%d",
                 _video_w, _video_h, _video_x, _video_y);

        execlp("mpv", "mpv",
               "--no-border", "--ontop", "--force-window-position",
               "--no-osc", "--keep-open=yes",
               "--no-window-dragging",
               "--input-ipc-server=" MPV_IPC_PATH,
               geom_arg,
               "--really-quiet",
               _video_path, NULL);

        fprintf(stderr, "[VIDEO] ERROR: execlp(mpv) failed: %s\n", strerror(errno));
        _exit(1);
    }
    else if (video_pid < 0)
    {
        DEBUG_ERROR(MODULE_VIDEO, "fork() failed: %s", strerror(errno));
        video_pid = -1;
    }
    else
    {
        DEBUG_INFO(MODULE_VIDEO, "mpv PID %d (size=%dx%d)", (int)video_pid, _video_w, _video_h);
    }
}

/* ── public API ───────────────────────────────────────── */

void game_video_play(lv_obj_t *parent, const char *video_path, game_video_back_cb_t back_cb)
{
    DEBUG_INFO(MODULE_VIDEO, "game_video_play(): %s", video_path);

    if (access(video_path, R_OK) != 0)
    {
        DEBUG_ERROR(MODULE_VIDEO, "Video not accessible: %s (%s)", video_path, strerror(errno));
        return;
    }

    if (video_pid > 0) game_video_stop();

    _back_cb    = back_cb;
    _video_path = video_path;

    get_video_dimensions(video_path);

    /* Remove panel padding so controls sit at edges */
    lv_obj_set_style_pad_all(parent, 0, 0);
    /* Shift panel down 20px */
    lv_obj_set_y(parent, lv_obj_get_y(parent) - 22);
    lv_obj_update_layout(parent);

    /* Query LVGL panel position */
    lv_area_t panel_area;
    lv_obj_get_coords(parent, &panel_area);
    int panel_w = lv_area_get_width(&panel_area);
    int panel_h = lv_area_get_height(&panel_area);

    int border  = 12;
    int inner_w = panel_w - 2 * border;
    int inner_h = panel_h;
    if (inner_w < 1) inner_w = 1;
    if (inner_h < 1) inner_h = 1;

    /* Row 1 = video (6/7), Row 2 = controls (1/7) */
    int row2_h = inner_h / (VIDEO_ROW_RATIO + 1);
    int row1_h = inner_h - row2_h;

    /* Video fills row 1 */
    _video_w = inner_w;
    _video_h = row1_h;

    /* Position video relative to screen (not panel) */
    _video_x = panel_area.x1 + border;
    _video_y = 0;  /* absolute screen Y */

    DEBUG_INFO(MODULE_VIDEO, "Video: size(%dx%d) pos(%d,%d) panel(%dx%d@%d,%d) inner(%dx%d) src(%dx%d)",
               _video_w, _video_h, _video_x, _video_y,
               panel_w, panel_h, panel_area.x1, panel_area.y1,
               inner_w, inner_h, _src_w, _src_h);

    spawn_mpv();

    /* Verify mpv started */
    usleep(100000);
    if (video_pid > 0)
    {
        int   status;
        pid_t result = waitpid(video_pid, &status, WNOHANG);
        if (result == video_pid)
        {
            DEBUG_ERROR(MODULE_VIDEO, "mpv died immediately (exit=%d, signal=%d)",
                        WIFEXITED(status) ? WEXITSTATUS(status) : -1,
                        WIFSIGNALED(status) ? WTERMSIG(status) : -1);
            video_pid = -1;
        }
    }

    /* Create LVGL controls inside panel */
    if (video_pid > 0)
    {
        create_controls(parent);
    }

    /* Start ontop enforcer + position poll timer */
    if (video_pid > 0 && _ontop_timer == NULL)
    {
        _ontop_timer = lv_timer_create(ontop_timer_cb, 10, NULL);
        DEBUG_INFO(MODULE_VIDEO, "Ontop enforcer timer started");
    }

    DEBUG_INFO(MODULE_VIDEO, "Playback started (pid=%d)", (int)video_pid);
}

void game_video_stop(void)
{
    DEBUG_INFO(MODULE_VIDEO, "game_video_stop() (pid=%d)", (int)video_pid);

    if (_ontop_timer != NULL)
    {
        lv_timer_delete(_ontop_timer);
        _ontop_timer = NULL;
    }

    destroy_controls();

    mpv_ipc_close();

    if (video_pid > 0)
    {
        kill(video_pid, SIGTERM);
        int status;
        waitpid(video_pid, &status, 0);
        video_pid = -1;
    }

    unlink(MPV_IPC_PATH);
    _back_cb    = NULL;
    _video_path = NULL;
}

bool game_video_is_playing(void)
{
    if (video_pid > 0)
    {
        int   status;
        pid_t result = waitpid(video_pid, &status, WNOHANG);
        if (result == video_pid)
        {
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
}

static void _deferred_play(void *data)
{
    (void)data;
    game_video_play(_pending_panel, _pending_video_path, _pending_back_cb);
}

void game_video_handle_draw(lv_obj_t *panel, const char *video_path, game_video_back_cb_t back_cb)
{
    if (!_video_started)
    {
        _video_started      = true;
        _pending_panel      = panel;
        _pending_video_path = video_path;
        _pending_back_cb    = back_cb;
        lv_async_call(_deferred_play, NULL);
    }
}
