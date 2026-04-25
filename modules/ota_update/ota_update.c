#include "ota_update.h"
#include "../../ui/ui.h"
#include "../debug/debug.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/stat.h>

/* ── constants ───────────────────────────────────────── */

#define OTA_DEFAULT_SERVER   "192.168.50.1"
#define OTA_SERVER_PORT      8000
#define OTA_CONF_PATH        "/etc/goball-agent.conf"
#define OTA_TMP_PATH         "/tmp/goball_update"
#define OTA_POLL_MS          100
#define OTA_VERSION_MAX      32
#define OTA_CHANGELOG_MAX    1024
#define OTA_URL_MAX          256
#define OTA_LINE_MAX         256
#define OTA_SELF_PATH_MAX    512

/* ── state machine ───────────────────────────────────── */

typedef enum {
    OTA_IDLE,
    OTA_CHECKING,
    OTA_CHECK_DONE,
    OTA_UP_TO_DATE,
    OTA_CHECK_ERROR,
    OTA_DOWNLOADING,
    OTA_INSTALLING,
    OTA_DEPLOYING,
    OTA_RESTARTING,
    OTA_ERROR,
} ota_state_t;

/* ── shared state (thread ↔ LVGL timer) ──────────────── */

static struct {
    atomic_int   state;
    char         server_host[OTA_URL_MAX];
    char         current_version[OTA_VERSION_MAX];
    char         available_version[OTA_VERSION_MAX];
    char         changelog[OTA_CHANGELOG_MAX];
    long         firmware_size;
    long         downloaded_bytes;
    char         error_msg[OTA_LINE_MAX];
    char         self_path[OTA_SELF_PATH_MAX];
} _ota;

/* ── UI widgets ──────────────────────────────────────── */

static lv_obj_t   *_overlay      = NULL;
static lv_obj_t   *_panel        = NULL;
static lv_obj_t   *_bar          = NULL;
static lv_obj_t   *_pct_label    = NULL;
static lv_obj_t   *_status_label = NULL;
static lv_obj_t   *_changelog_lbl = NULL;
static lv_obj_t   *_update_btn   = NULL;
static lv_obj_t   *_close_btn    = NULL;
static lv_obj_t   *_ver_cur_lbl  = NULL;
static lv_obj_t   *_ver_avail_lbl = NULL;
static lv_timer_t *_poll_timer   = NULL;

/* ── forward declarations ────────────────────────────── */

static void dismiss_overlay(void);
static void bg_click_cb(lv_event_t *e);
static void close_btn_cb(lv_event_t *e);
static void update_btn_cb(lv_event_t *e);
static void poll_timer_cb(lv_timer_t *timer);
static void *check_thread_fn(void *arg);
static void *download_thread_fn(void *arg);
static void read_server_host(void);
static void read_self_path(void);

/* ── config parsing ──────────────────────────────────── */

static void read_server_host(void)
{
    /* Default */
    strncpy(_ota.server_host, OTA_DEFAULT_SERVER, sizeof(_ota.server_host) - 1);

    FILE *fp = fopen(OTA_CONF_PATH, "r");
    if (!fp) return;

    char line[OTA_LINE_MAX];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n\r")] = '\0';
        if (line[0] == '#' || line[0] == '\0') continue;
        if (strncmp(line, "MQTT_BROKER_HOST=", 17) == 0) {
            const char *val = line + 17;
            if (val[0] != '\0') {
                strncpy(_ota.server_host, val, sizeof(_ota.server_host) - 1);
                _ota.server_host[sizeof(_ota.server_host) - 1] = '\0';
            }
            break;
        }
    }
    fclose(fp);
}

static void read_self_path(void)
{
    ssize_t len = readlink("/proc/self/exe", _ota.self_path, sizeof(_ota.self_path) - 1);
    if (len > 0) {
        _ota.self_path[len] = '\0';
    } else {
        strncpy(_ota.self_path, "/home/q/Desktop/SquareLine_Project/goball",
                sizeof(_ota.self_path) - 1);
    }
}

/* ── public API ──────────────────────────────────────── */

void ota_update_init(void)
{
    atomic_store(&_ota.state, OTA_IDLE);
    strncpy(_ota.current_version, GOBALL_VERSION, OTA_VERSION_MAX - 1);
    read_server_host();
    read_self_path();
    DEBUG_INFO(MODULE_OTA, "Initialized (server=%s, version=%s, path=%s)",
               _ota.server_host, _ota.current_version, _ota.self_path);
}

void ota_update_show(void)
{
    if (_overlay) dismiss_overlay();

    /* Re-read config in case it changed */
    read_server_host();
    read_self_path();

    /* ── Dark overlay on layer_top ─── */
    _overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(_overlay);
    lv_obj_set_size(_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(_overlay, LV_OPA_80, 0);
    lv_obj_add_flag(_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_overlay, bg_click_cb, LV_EVENT_CLICKED, NULL);

    /* ── Panel ─── */
    _panel = lv_obj_create(_overlay);
    lv_obj_remove_style_all(_panel);
    lv_obj_set_size(_panel, 900, 620);
    lv_obj_align(_panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(_panel, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_bg_opa(_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(_panel, 12, 0);
    lv_obj_set_style_border_color(_panel, lv_color_hex(0x00F46A), 0);
    lv_obj_set_style_border_width(_panel, 2, 0);
    lv_obj_set_style_pad_all(_panel, 30, 0);
    lv_obj_add_flag(_panel, LV_OBJ_FLAG_CLICKABLE);

    /* ── Title ─── */
    lv_obj_t *title = lv_label_create(_panel);
    lv_label_set_text(title, "Firmware Update");
    lv_obj_set_style_text_color(title, lv_color_hex(0x00F46A), 0);
    lv_obj_set_style_text_font(title, &ui_font_Unitblock_60, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    /* ── Current version ─── */
    _ver_cur_lbl = lv_label_create(_panel);
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "Current:   v%s", _ota.current_version);
        lv_label_set_text(_ver_cur_lbl, buf);
    }
    lv_obj_set_style_text_color(_ver_cur_lbl, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(_ver_cur_lbl, &lv_font_montserrat_32, 0);
    lv_obj_align(_ver_cur_lbl, LV_ALIGN_TOP_LEFT, 0, 80);

    /* ── Available version (hidden until check completes) ─── */
    _ver_avail_lbl = lv_label_create(_panel);
    lv_label_set_text(_ver_avail_lbl, "Available: checking...");
    lv_obj_set_style_text_color(_ver_avail_lbl, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(_ver_avail_lbl, &lv_font_montserrat_32, 0);
    lv_obj_align(_ver_avail_lbl, LV_ALIGN_TOP_LEFT, 0, 120);

    /* ── Changelog label ─── */
    _changelog_lbl = lv_label_create(_panel);
    lv_label_set_text(_changelog_lbl, "");
    lv_obj_set_style_text_color(_changelog_lbl, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(_changelog_lbl, &lv_font_montserrat_32, 0);
    lv_obj_set_width(_changelog_lbl, 820);
    lv_label_set_long_mode(_changelog_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_align(_changelog_lbl, LV_ALIGN_TOP_LEFT, 0, 175);

    /* ── Progress bar ─── */
    _bar = lv_bar_create(_panel);
    lv_obj_set_size(_bar, 720, 25);
    lv_obj_align(_bar, LV_ALIGN_BOTTOM_LEFT, 0, -100);
    lv_bar_set_range(_bar, 0, 100);
    lv_bar_set_value(_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(_bar, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(_bar, lv_color_hex(0x00F46A), LV_PART_INDICATOR);
    lv_obj_set_style_radius(_bar, 6, LV_PART_MAIN);
    lv_obj_set_style_radius(_bar, 6, LV_PART_INDICATOR);
    lv_obj_add_flag(_bar, LV_OBJ_FLAG_HIDDEN);

    /* ── Percentage label ─── */
    _pct_label = lv_label_create(_panel);
    lv_label_set_text(_pct_label, "");
    lv_obj_set_style_text_color(_pct_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(_pct_label, &lv_font_montserrat_32, 0);
    lv_obj_align_to(_pct_label, _bar, LV_ALIGN_OUT_RIGHT_MID, 15, 0);
    lv_obj_add_flag(_pct_label, LV_OBJ_FLAG_HIDDEN);

    /* ── Status label ─── */
    _status_label = lv_label_create(_panel);
    lv_label_set_text(_status_label, "Checking for updates...");
    lv_obj_set_style_text_color(_status_label, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_font(_status_label, &lv_font_montserrat_32, 0);
    lv_obj_align(_status_label, LV_ALIGN_BOTTOM_MID, 0, -65);

    /* ── Update Now button (hidden until update available) ─── */
    _update_btn = lv_obj_create(_panel);
    lv_obj_remove_style_all(_update_btn);
    lv_obj_set_size(_update_btn, 220, 50);
    lv_obj_align(_update_btn, LV_ALIGN_BOTTOM_LEFT, 140, -5);
    lv_obj_set_style_bg_color(_update_btn, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_bg_opa(_update_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(_update_btn, lv_color_hex(0x00F46A), 0);
    lv_obj_set_style_border_width(_update_btn, 2, 0);
    lv_obj_set_style_radius(_update_btn, 8, 0);
    lv_obj_add_flag(_update_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_update_btn, update_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(_update_btn, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *upd_lbl = lv_label_create(_update_btn);
    lv_label_set_text(upd_lbl, "Update Now");
    lv_obj_set_style_text_color(upd_lbl, lv_color_hex(0x00F46A), 0);
    lv_obj_set_style_text_font(upd_lbl, &lv_font_montserrat_32, 0);
    lv_obj_center(upd_lbl);

    /* ── Close button ─── */
    _close_btn = lv_obj_create(_panel);
    lv_obj_remove_style_all(_close_btn);
    lv_obj_set_size(_close_btn, 160, 50);
    lv_obj_align(_close_btn, LV_ALIGN_BOTTOM_RIGHT, -140, -5);
    lv_obj_set_style_bg_color(_close_btn, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_bg_opa(_close_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(_close_btn, lv_color_hex(0x00F46A), 0);
    lv_obj_set_style_border_width(_close_btn, 2, 0);
    lv_obj_set_style_radius(_close_btn, 8, 0);
    lv_obj_add_flag(_close_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_close_btn, close_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *cls_lbl = lv_label_create(_close_btn);
    lv_label_set_text(cls_lbl, "Close");
    lv_obj_set_style_text_color(cls_lbl, lv_color_hex(0x00F46A), 0);
    lv_obj_set_style_text_font(cls_lbl, &lv_font_montserrat_32, 0);
    lv_obj_center(cls_lbl);

    /* ── Start poll timer ─── */
    _poll_timer = lv_timer_create(poll_timer_cb, OTA_POLL_MS, NULL);

    /* ── Start check thread ─── */
    atomic_store(&_ota.state, OTA_CHECKING);
    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, check_thread_fn, NULL);
    pthread_attr_destroy(&attr);
}

/* ── background: check for update ────────────────────── */

static void *check_thread_fn(void *arg)
{
    (void)arg;
    char url[OTA_URL_MAX * 2];
    snprintf(url, sizeof(url),
             "wget -q -O - --timeout=10 "
             "http://%s:%d/api/firmware/latest",
             _ota.server_host, OTA_SERVER_PORT);

    FILE *fp = popen(url, "r");
    if (!fp) {
        snprintf(_ota.error_msg, sizeof(_ota.error_msg), "Failed to connect to server");
        atomic_store(&_ota.state, OTA_CHECK_ERROR);
        return NULL;
    }

    char buf[2048] = {0};
    size_t total = 0;
    size_t n;
    while ((n = fread(buf + total, 1, sizeof(buf) - total - 1, fp)) > 0)
        total += n;
    buf[total] = '\0';
    int rc = pclose(fp);

    if (rc != 0 || total == 0) {
        snprintf(_ota.error_msg, sizeof(_ota.error_msg), "Server unreachable or returned error");
        atomic_store(&_ota.state, OTA_CHECK_ERROR);
        return NULL;
    }

    /* Minimal JSON parsing — find "version", "changelog", "size", "available"
     * For string values the pattern is:  "key":"value"
     * We find the key, skip to ':', then find the opening '"' of the value. */

    /* Helper: find value after "key": — returns pointer to char after opening quote */
    #define JSON_FIND_STR(key) do { \
        char *_p = strstr(buf, "\"" key "\""); \
        if (_p) { _p = strchr(_p + strlen(key) + 2, ':'); \
            if (_p) { _p = strchr(_p, '"'); if (_p) _p++; } } \
        p = _p; } while(0)

    char *p;

    /* "version":"1.8.0" */
    JSON_FIND_STR("version");
    if (p) {
        char *end = strchr(p, '"');
        if (end) {
            size_t len = (size_t)(end - p);
            if (len >= OTA_VERSION_MAX) len = OTA_VERSION_MAX - 1;
            memcpy(_ota.available_version, p, len);
            _ota.available_version[len] = '\0';
        }
    }

    /* "changelog":"- Fleet monitoring..." */
    JSON_FIND_STR("changelog");
    if (p) {
        char *dst = _ota.changelog;
        char *dst_end = _ota.changelog + OTA_CHANGELOG_MAX - 1;
        while (*p && *p != '"' && dst < dst_end) {
            if (*p == '\\' && *(p + 1) == 'n') {
                *dst++ = '\n'; p += 2;
            } else if (*p == '\\' && *(p + 1)) {
                *dst++ = *(p + 1); p += 2;
            } else {
                *dst++ = *p++;
            }
        }
        *dst = '\0';
    }

    #undef JSON_FIND_STR

    /* "size":12345 */
    p = strstr(buf, "\"size\"");
    if (p) {
        p = strchr(p + 6, ':');
        if (p) {
            _ota.firmware_size = strtol(p + 1, NULL, 10);
        }
    }

    /* Check if available is false */
    p = strstr(buf, "\"available\"");
    if (p) {
        p = strchr(p + 11, ':');
        if (p && strstr(p, "false")) {
            atomic_store(&_ota.state, OTA_UP_TO_DATE);
            return NULL;
        }
    }

    /* Compare versions — simple string compare */
    if (_ota.available_version[0] == '\0' ||
        strcmp(_ota.available_version, _ota.current_version) == 0) {
        atomic_store(&_ota.state, OTA_UP_TO_DATE);
    } else {
        atomic_store(&_ota.state, OTA_CHECK_DONE);
    }

    return NULL;
}

/* ── background: download + install ──────────────────── */

static void *download_thread_fn(void *arg)
{
    (void)arg;

    /* Remove old temp file */
    unlink(OTA_TMP_PATH);

    /* Download */
    atomic_store(&_ota.state, OTA_DOWNLOADING);
    _ota.downloaded_bytes = 0;

    char cmd[OTA_URL_MAX * 2];
    snprintf(cmd, sizeof(cmd),
             "wget -q -O %s --timeout=300 "
             "http://%s:%d/api/firmware/download",
             OTA_TMP_PATH, _ota.server_host, OTA_SERVER_PORT);

    int rc = system(cmd);
    if (rc != 0) {
        snprintf(_ota.error_msg, sizeof(_ota.error_msg), "Download failed (wget exit %d)", rc);
        atomic_store(&_ota.state, OTA_ERROR);
        return NULL;
    }

    /* Verify downloaded file exists and has reasonable size */
    struct stat st;
    if (stat(OTA_TMP_PATH, &st) != 0 || st.st_size < 1000) {
        snprintf(_ota.error_msg, sizeof(_ota.error_msg), "Downloaded file missing or too small");
        atomic_store(&_ota.state, OTA_ERROR);
        return NULL;
    }

    /* Verify ELF header */
    FILE *fp = fopen(OTA_TMP_PATH, "rb");
    if (fp) {
        unsigned char hdr[4];
        if (fread(hdr, 1, 4, fp) == 4) {
            if (hdr[0] != 0x7f || hdr[1] != 'E' || hdr[2] != 'L' || hdr[3] != 'F') {
                fclose(fp);
                snprintf(_ota.error_msg, sizeof(_ota.error_msg), "Downloaded file is not a valid ELF binary");
                atomic_store(&_ota.state, OTA_ERROR);
                return NULL;
            }
        }
        fclose(fp);
    }

    /* Backup current binary */
    atomic_store(&_ota.state, OTA_INSTALLING);
    {
        char bak_cmd[OTA_SELF_PATH_MAX * 2 + 32];
        snprintf(bak_cmd, sizeof(bak_cmd), "cp '%s' '%s.bak'", _ota.self_path, _ota.self_path);
        rc = system(bak_cmd);
        if (rc != 0) {
            snprintf(_ota.error_msg, sizeof(_ota.error_msg), "Failed to create backup (exit %d)", rc);
            atomic_store(&_ota.state, OTA_ERROR);
            return NULL;
        }
    }

    /* Deploy: move downloaded binary to self path */
    atomic_store(&_ota.state, OTA_DEPLOYING);
    {
        char mv_cmd[OTA_SELF_PATH_MAX * 2 + 64];
        snprintf(mv_cmd, sizeof(mv_cmd),
                 "mv '%s' '%s' && chmod +x '%s'",
                 OTA_TMP_PATH, _ota.self_path, _ota.self_path);
        rc = system(mv_cmd);
        if (rc != 0) {
            snprintf(_ota.error_msg, sizeof(_ota.error_msg), "Failed to install binary (exit %d)", rc);
            atomic_store(&_ota.state, OTA_ERROR);
            return NULL;
        }
    }

    /* Reboot */
    atomic_store(&_ota.state, OTA_RESTARTING);
    sleep(5);
    system("reboot");

    return NULL;
}

/* ── LVGL poll timer — updates UI from shared state ──── */

static void poll_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!_overlay) {
        if (_poll_timer) {
            lv_timer_delete(_poll_timer);
            _poll_timer = NULL;
        }
        return;
    }

    int state = atomic_load(&_ota.state);

    switch (state) {
    case OTA_CHECKING:
        lv_label_set_text(_status_label, "Checking for updates...");
        break;

    case OTA_CHECK_DONE: {
        char buf[128];
        snprintf(buf, sizeof(buf), "Available: v%s", _ota.available_version);
        lv_label_set_text(_ver_avail_lbl, buf);

        if (_ota.changelog[0] != '\0') {
            char cl_buf[OTA_CHANGELOG_MAX + 32];
            snprintf(cl_buf, sizeof(cl_buf), "What's new:\n%s", _ota.changelog);
            lv_label_set_text(_changelog_lbl, cl_buf);
        }

        lv_label_set_text(_status_label, "Update available!");
        lv_obj_set_style_text_color(_status_label, lv_color_hex(0x00F46A), 0);
        lv_obj_clear_flag(_update_btn, LV_OBJ_FLAG_HIDDEN);
        break;
    }

    case OTA_UP_TO_DATE:
        lv_label_set_text(_ver_avail_lbl, "");
        lv_label_set_text(_status_label, "Firmware is up to date");
        lv_obj_set_style_text_color(_status_label, lv_color_hex(0x00F46A), 0);
        break;

    case OTA_CHECK_ERROR:
    case OTA_ERROR:
        lv_label_set_text(_status_label, _ota.error_msg);
        lv_obj_set_style_text_color(_status_label, lv_color_hex(0xFF4444), 0);
        break;

    case OTA_DOWNLOADING: {
        lv_obj_clear_flag(_bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(_pct_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_update_btn, LV_OBJ_FLAG_HIDDEN);

        /* Check download progress by file size */
        struct stat st;
        int pct = 0;
        if (stat(OTA_TMP_PATH, &st) == 0 && _ota.firmware_size > 0) {
            pct = (int)(st.st_size * 60 / _ota.firmware_size);
            if (pct > 60) pct = 60;
            _ota.downloaded_bytes = st.st_size;
        }
        lv_bar_set_value(_bar, pct, LV_ANIM_ON);
        char pct_buf[16];
        snprintf(pct_buf, sizeof(pct_buf), "%d%%", pct);
        lv_label_set_text(_pct_label, pct_buf);
        lv_label_set_text(_status_label, "Downloading...");
        break;
    }

    case OTA_INSTALLING:
        lv_bar_set_value(_bar, 70, LV_ANIM_ON);
        lv_label_set_text(_pct_label, "70%");
        lv_label_set_text(_status_label, "Creating backup...");
        break;

    case OTA_DEPLOYING:
        lv_bar_set_value(_bar, 85, LV_ANIM_ON);
        lv_label_set_text(_pct_label, "85%");
        lv_label_set_text(_status_label, "Installing firmware...");
        break;

    case OTA_RESTARTING:
        lv_bar_set_value(_bar, 100, LV_ANIM_ON);
        lv_label_set_text(_pct_label, "100%");
        lv_label_set_text(_status_label, "Restarting in 5 seconds...");
        lv_obj_set_style_text_color(_status_label, lv_color_hex(0x00F46A), 0);
        /* Hide close button during restart */
        if (_close_btn) lv_obj_add_flag(_close_btn, LV_OBJ_FLAG_HIDDEN);
        break;

    default:
        break;
    }
}

/* ── callbacks ───────────────────────────────────────── */

static void bg_click_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    if (target == _overlay) {
        int state = atomic_load(&_ota.state);
        /* Don't dismiss during active operations */
        if (state >= OTA_DOWNLOADING && state <= OTA_RESTARTING) return;
        dismiss_overlay();
    }
}

static void close_btn_cb(lv_event_t *e)
{
    (void)e;
    int state = atomic_load(&_ota.state);
    if (state >= OTA_DOWNLOADING && state <= OTA_RESTARTING) return;
    dismiss_overlay();
}

static void update_btn_cb(lv_event_t *e)
{
    (void)e;
    int state = atomic_load(&_ota.state);
    if (state != OTA_CHECK_DONE) return;

    /* Disable buttons and start download */
    lv_obj_add_flag(_update_btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(_pct_label, LV_OBJ_FLAG_HIDDEN);

    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, download_thread_fn, NULL);
    pthread_attr_destroy(&attr);
}

/* ── cleanup ─────────────────────────────────────────── */

static void dismiss_overlay(void)
{
    if (_poll_timer) {
        lv_timer_delete(_poll_timer);
        _poll_timer = NULL;
    }

    if (_overlay) {
        lv_obj_delete(_overlay);
        _overlay       = NULL;
        _panel         = NULL;
        _bar           = NULL;
        _pct_label     = NULL;
        _status_label  = NULL;
        _changelog_lbl = NULL;
        _update_btn    = NULL;
        _close_btn     = NULL;
        _ver_cur_lbl   = NULL;
        _ver_avail_lbl = NULL;
        DEBUG_DEBUG(MODULE_OTA, "Overlay dismissed");
    }

    /* Reset state if not in the middle of an operation */
    int state = atomic_load(&_ota.state);
    if (state < OTA_DOWNLOADING || state == OTA_ERROR || state == OTA_CHECK_ERROR) {
        atomic_store(&_ota.state, OTA_IDLE);
    }
}
