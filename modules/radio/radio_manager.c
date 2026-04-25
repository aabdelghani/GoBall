#include "radio_manager.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../../ui/ui.h"
#include "../debug/debug.h"

#define RADIO_DEFAULT_URL "https://video-distribution.pgatourhq.com/pgatour-radio/pgatour-radio_1.m3u8"
#define RADIO_DEFAULT_REFERER "Referer: https://www.pgatour.com/"
#define RADIO_DEFAULT_VOLUME 60

static pid_t _radio_pid = -1;
static bool  _radio_enabled = false;
static char  _radio_url[1024]      = RADIO_DEFAULT_URL;
static char  _radio_referer[256]   = RADIO_DEFAULT_REFERER;
static int   _radio_volume         = RADIO_DEFAULT_VOLUME;

static const char *conf_path(void)
{
    const char *p = getenv("GOBALL_RADIO_CONF");
    return (p && *p) ? p : RADIO_CONF_PATH_DEFAULT;
}

/* Trim leading/trailing whitespace + strip surrounding quotes */
static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r' || s[n-1] == '\n')) s[--n] = 0;
    if (n >= 2 && ((s[0] == '"' && s[n-1] == '"') || (s[0] == '\'' && s[n-1] == '\''))) {
        s[n-1] = 0; s++;
    }
    return s;
}

/* Read /etc/goball-radio.conf (key=value, comments with #). Missing file = defaults. */
static void load_config(void)
{
    FILE *fp = fopen(conf_path(), "r");
    if (!fp) {
        DEBUG_INFO(MODULE_RADIO, "config %s not found, using defaults", conf_path());
        return;
    }
    char line[1280];
    while (fgets(line, sizeof(line), fp)) {
        char *s = trim(line);
        if (*s == '#' || *s == 0) continue;
        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = 0;
        char *k = trim(s);
        char *v = trim(eq + 1);
        if (strcmp(k, "ENABLED") == 0) {
            _radio_enabled = (strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0);
        } else if (strcmp(k, "URL") == 0) {
            strncpy(_radio_url, v, sizeof(_radio_url) - 1);
            _radio_url[sizeof(_radio_url) - 1] = 0;
        } else if (strcmp(k, "REFERER") == 0) {
            snprintf(_radio_referer, sizeof(_radio_referer), "Referer: %s", v);
        } else if (strcmp(k, "VOLUME") == 0) {
            int n = atoi(v);
            if (n >= 0 && n <= 300) _radio_volume = n;  /* matches mpv --volume-max=300 */
        }
    }
    fclose(fp);
    DEBUG_INFO(MODULE_RADIO, "config: ENABLED=%d VOL=%d URL=%.80s",
               _radio_enabled, _radio_volume, _radio_url);
}

static void save_config(void)
{
    FILE *fp = fopen(conf_path(), "w");
    if (!fp) {
        DEBUG_WARN(MODULE_RADIO, "cannot write config %s: %s", conf_path(), strerror(errno));
        return;
    }
    fprintf(fp,
        "# GoBall radio config (managed by radio_manager)\n"
        "ENABLED=%d\n"
        "URL=%s\n"
        "VOLUME=%d\n",
        _radio_enabled ? 1 : 0, _radio_url, _radio_volume);
    fclose(fp);
}

static bool mpv_running(void)
{
    if (_radio_pid <= 0) return false;
    int status;
    pid_t r = waitpid(_radio_pid, &status, WNOHANG);
    if (r == _radio_pid) {
        DEBUG_INFO(MODULE_RADIO, "mpv exited (pid %d)", (int)_radio_pid);
        _radio_pid = -1;
        return false;
    }
    return true;
}

static void spawn_mpv(void)
{
    if (mpv_running()) return;

    DEBUG_INFO(MODULE_RADIO, "spawning mpv for %.80s", _radio_url);

    char vol_arg[32], hdr_arg[320];
    snprintf(vol_arg, sizeof(vol_arg), "--volume=%d", _radio_volume);
    snprintf(hdr_arg, sizeof(hdr_arg), "--http-header-fields=%s", _radio_referer);
    /* mpv's default --volume-max is 130; raise so VOLUME up to 300 in radio.conf works */
    const char *vmax_arg = "--volume-max=300";

    pid_t pid = fork();
    if (pid < 0) {
        DEBUG_ERROR(MODULE_RADIO, "fork failed: %s", strerror(errno));
        return;
    }
    if (pid == 0) {
        /* Child — die when parent exits */
        prctl(PR_SET_PDEATHSIG, SIGTERM);
        /* Detach from controlling tty so SIGINT to goball doesn't kill mpv directly */
        setsid();

        execlp("mpv", "mpv",
               "--no-video",
               "--no-terminal",
               "--no-input-default-bindings",
               "--really-quiet",
               "--loop=inf",
               "--cache=yes",
               "--cache-secs=15",
               "--audio-display=no",
               vmax_arg,
               vol_arg,
               hdr_arg,
               _radio_url,
               (char *)NULL);
        fprintf(stderr, "[RADIO] execlp(mpv) failed: %s\n", strerror(errno));
        _exit(127);
    }
    _radio_pid = pid;
    DEBUG_INFO(MODULE_RADIO, "mpv PID %d", (int)pid);
}

static void kill_mpv(void)
{
    if (_radio_pid <= 0) return;
    DEBUG_INFO(MODULE_RADIO, "stopping mpv (pid %d)", (int)_radio_pid);
    kill(_radio_pid, SIGTERM);
    int status;
    /* Brief blocking wait — mpv exits cleanly on SIGTERM */
    for (int i = 0; i < 20; i++) {
        if (waitpid(_radio_pid, &status, WNOHANG) == _radio_pid) {
            _radio_pid = -1;
            return;
        }
        usleep(100000);  /* 100 ms */
    }
    /* Force */
    kill(_radio_pid, SIGKILL);
    waitpid(_radio_pid, &status, 0);
    _radio_pid = -1;
}

/* ── UI: tap-to-toggle icon on home screen ─────────────────────── */

static lv_obj_t *_radio_icon = NULL;

static void update_icon(void)
{
    if (!_radio_icon) return;
    bool on = _radio_enabled;
    /* Green when on, dim grey when off */
    lv_obj_set_style_text_color(_radio_icon,
        lv_color_hex(on ? 0x00F46A : 0x888888), 0);
}

static void icon_clicked_cb(lv_event_t *e)
{
    (void)e;
    radio_set_enabled(!_radio_enabled);
    update_icon();
}

static void install_ui_toggle(void)
{
    if (_radio_icon) return;
    _radio_icon = lv_label_create(ui_HScreen);
    lv_label_set_text(_radio_icon, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_font(_radio_icon, &lv_font_montserrat_32, 0);
    /* Position: just left of the WiFi icon (which is at -80, -10) */
    lv_obj_align(_radio_icon, LV_ALIGN_BOTTOM_RIGHT, -130, -10);
    lv_obj_add_flag(_radio_icon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_radio_icon, icon_clicked_cb, LV_EVENT_CLICKED, NULL);
    update_icon();
    DEBUG_INFO(MODULE_RADIO, "icon installed on home screen");
}

/* ── Public API ─────────────────────────────────────────────────── */

void radio_init(void)
{
    load_config();
    install_ui_toggle();
    if (_radio_enabled) {
        spawn_mpv();
    } else {
        DEBUG_INFO(MODULE_RADIO, "radio disabled in config - not starting");
    }
}

void radio_set_enabled(bool on)
{
    if (on == _radio_enabled && (on == false || mpv_running())) return;
    _radio_enabled = on;
    save_config();
    if (on) spawn_mpv();
    else    kill_mpv();
}

bool radio_is_enabled(void)
{
    /* Reflect actual subprocess state, not just the saved flag */
    return _radio_enabled && mpv_running();
}

void radio_stop(void)
{
    kill_mpv();
}
