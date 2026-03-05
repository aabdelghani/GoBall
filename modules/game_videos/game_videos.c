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

static pid_t                video_pid       = -1;
static int                  _mpv_ipc_fd     = -1;
static game_video_back_cb_t _back_cb        = NULL;
static lv_timer_t          *_ontop_timer    = NULL;

/* Playback state */
static const char *_video_path = NULL;
static int         _video_w = 0, _video_h = 0;

/* Deferred-start flag for handle_draw */
static bool _video_started = false;

/* Saved args for async start */
static lv_obj_t            *_pending_panel      = NULL;
static const char          *_pending_video_path = NULL;
static game_video_back_cb_t _pending_back_cb    = NULL;

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

static void mpv_ipc_close(void)
{
    if (_mpv_ipc_fd >= 0)
    {
        close(_mpv_ipc_fd);
        _mpv_ipc_fd = -1;
    }
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

    /* Force ontop every tick */
    mpv_ipc_send("{\"command\":[\"set_property\",\"ontop\",true]}\n");
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

        char geom_arg[48];
        snprintf(geom_arg, sizeof(geom_arg), "--geometry=%dx%d", _video_w, _video_h);

        execlp("mpv", "mpv",
               "--no-border", "--ontop",
               "--loop=yes", "--osc=yes",
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

    /* Query LVGL panel position */
    lv_area_t panel_area;
    lv_obj_get_coords(parent, &panel_area);
    int panel_w = lv_area_get_width(&panel_area);
    int panel_h = lv_area_get_height(&panel_area);

    /* Inner area (account for border) */
    int border  = 12;
    int inner_w = panel_w - 2 * border;
    int inner_h = panel_h - 2 * border;
    if (inner_w < 1) inner_w = 1;
    if (inner_h < 1) inner_h = 1;

    /* Fit video maintaining aspect ratio */
    int fit_w = inner_h * _src_w / _src_h;
    int fit_h = inner_h;
    if (fit_w > inner_w)
    {
        fit_w = inner_w;
        fit_h = inner_w * _src_h / _src_w;
    }
    _video_w = fit_w;
    _video_h = fit_h;

    DEBUG_INFO(MODULE_VIDEO, "Video: size(%dx%d) panel(%dx%d) src(%dx%d)",
               _video_w, _video_h, panel_w, panel_h, _src_w, _src_h);

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

    /* Start ontop enforcer timer — re-asserts ontop every 500ms */
    if (video_pid > 0 && _ontop_timer == NULL)
    {
        _ontop_timer = lv_timer_create(ontop_timer_cb, 500, NULL);
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
