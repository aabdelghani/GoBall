#include "wifi_manager.h"
#include "../../ui/ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── constants ───────────────────────────────────────── */

#define MAX_NETWORKS    32
#define SSID_MAX_LEN    64
#define PASS_MAX_LEN    63
#define WIFI_POLL_MS    5000  /* check connection status every 5s */

/* ── network entry ───────────────────────────────────── */

typedef struct {
    char ssid[SSID_MAX_LEN + 1];
    int  signal;
    char security[32];
} wifi_entry_t;

/* ── overlay state ───────────────────────────────────── */

static lv_obj_t *_overlay    = NULL;
static lv_obj_t *_list       = NULL;
static lv_obj_t *_kb_overlay = NULL;
static lv_obj_t *_kb_ta      = NULL;
static lv_obj_t *_kb         = NULL;
static lv_obj_t *_status_lbl = NULL;

/* WiFi status indicator on home screen */
static lv_obj_t   *_wifi_icon  = NULL;
static lv_timer_t *_poll_timer = NULL;

static char _selected_ssid[SSID_MAX_LEN + 1];
static char _current_ssid[SSID_MAX_LEN + 1];  /* currently connected SSID */
static bool _wifi_connected = false;

/* ── forward declarations ────────────────────────────── */

static void logo_click_cb(lv_event_t *e);
static void dismiss_overlay(void);
static void dismiss_password(void);
static void show_password_dialog(const char *ssid);
static void network_item_cb(lv_event_t *e);
static void bg_click_cb(lv_event_t *e);
static void kb_ready_cb(lv_event_t *e);
static void kb_cancel_cb(lv_event_t *e);
static void close_btn_cb(lv_event_t *e);
static void back_btn_cb(lv_event_t *e);
static void disconnect_btn_cb(lv_event_t *e);
static int  scan_networks(wifi_entry_t *out, int max_count);
static void check_wifi_status(void);
static void wifi_poll_timer_cb(lv_timer_t *timer);
static void update_wifi_icon(void);

/* ── WiFi status checking ────────────────────────────── */

static void check_wifi_status(void)
{
    _wifi_connected = false;
    _current_ssid[0] = '\0';

    FILE *fp = popen("iw dev wlan0 link 2>/dev/null", "r");
    if (!fp) return;

    char line[256];
    bool got_connected = false;
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        if (strncmp(line, "Connected to ", 13) == 0) {
            got_connected = true;
        } else if (strncmp(line, "\tSSID: ", 7) == 0) {
            strncpy(_current_ssid, line + 7, SSID_MAX_LEN);
            _current_ssid[SSID_MAX_LEN] = '\0';
        }
    }
    pclose(fp);

    if (got_connected && _current_ssid[0] != '\0')
        _wifi_connected = true;
}

static void update_wifi_icon(void)
{
    if (!_wifi_icon) return;

    if (_wifi_connected) {
        lv_label_set_text(_wifi_icon, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(_wifi_icon, lv_color_hex(0x00F46A), 0);
    } else {
        lv_label_set_text(_wifi_icon, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(_wifi_icon, lv_color_hex(0xFF4444), 0);
    }
}

static void wifi_poll_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    check_wifi_status();
    update_wifi_icon();
}

/* ── public API ──────────────────────────────────────── */

void wifi_manager_init(void)
{
    /* Make G logo clickable */
    lv_obj_add_flag(ui_HSLogo, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui_HSLogo, logo_click_cb, LV_EVENT_CLICKED, NULL);

    /* Create WiFi status icon on home screen (top-right area) */
    _wifi_icon = lv_label_create(ui_HScreen);
    lv_label_set_text(_wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(_wifi_icon, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(_wifi_icon, lv_color_hex(0xFF4444), 0);
    lv_obj_align(_wifi_icon, LV_ALIGN_BOTTOM_RIGHT, -80, -10);

    /* Initial status check */
    check_wifi_status();
    update_wifi_icon();

    /* Start polling timer */
    _poll_timer = lv_timer_create(wifi_poll_timer_cb, WIFI_POLL_MS, NULL);

    fprintf(stderr, "[WIFI] WiFi manager initialized (connected=%d, ssid='%s')\n",
            _wifi_connected, _current_ssid);
}

void wifi_manager_show(void)
{
    if (_overlay) dismiss_overlay();

    /* Refresh status before showing */
    check_wifi_status();
    update_wifi_icon();

    /* Dark overlay on layer_top */
    _overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(_overlay);
    lv_obj_set_size(_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(_overlay, LV_OPA_80, 0);
    lv_obj_add_flag(_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_overlay, bg_click_cb, LV_EVENT_CLICKED, NULL);

    /* Container panel */
    lv_obj_t *panel = lv_obj_create(_overlay);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, 800, 650);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel, 12, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x00F46A), 0);
    lv_obj_set_style_border_width(panel, 2, 0);
    lv_obj_set_style_pad_all(panel, 20, 0);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);

    /* Title */
    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, "WiFi Networks");
    lv_obj_set_style_text_color(title, lv_color_hex(0x00F46A), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    /* Current connection status at top */
    lv_obj_t *conn_lbl = lv_label_create(panel);
    if (_wifi_connected) {
        char conn_text[128];
        snprintf(conn_text, sizeof(conn_text), LV_SYMBOL_WIFI "  Connected to: %s", _current_ssid);
        lv_label_set_text(conn_lbl, conn_text);
        lv_obj_set_style_text_color(conn_lbl, lv_color_hex(0x00F46A), 0);
    } else {
        lv_label_set_text(conn_lbl, LV_SYMBOL_WIFI "  Not connected");
        lv_obj_set_style_text_color(conn_lbl, lv_color_hex(0xFF4444), 0);
    }
    lv_obj_set_style_text_font(conn_lbl, &lv_font_montserrat_32, 0);
    lv_obj_align(conn_lbl, LV_ALIGN_TOP_LEFT, 0, 40);

    /* Disconnect button (only shown when connected) */
    if (_wifi_connected) {
        lv_obj_t *disc_btn = lv_obj_create(panel);
        lv_obj_remove_style_all(disc_btn);
        lv_obj_set_size(disc_btn, 160, 38);
        lv_obj_align(disc_btn, LV_ALIGN_TOP_RIGHT, 0, 38);
        lv_obj_set_style_bg_color(disc_btn, lv_color_hex(0xFF4444), 0);
        lv_obj_set_style_bg_opa(disc_btn, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(disc_btn, 6, 0);
        lv_obj_add_flag(disc_btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(disc_btn, disconnect_btn_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *disc_lbl = lv_label_create(disc_btn);
        lv_label_set_text(disc_lbl, "Disconnect");
        lv_obj_set_style_text_color(disc_lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(disc_lbl, &lv_font_montserrat_32, 0);
        lv_obj_center(disc_lbl);
    }

    /* Scan networks */
    wifi_entry_t networks[MAX_NETWORKS];
    int count = scan_networks(networks, MAX_NETWORKS);

    /* Scrollable list */
    _list = lv_obj_create(panel);
    lv_obj_remove_style_all(_list);
    lv_obj_set_size(_list, 740, 440);
    lv_obj_align(_list, LV_ALIGN_TOP_MID, 0, 85);
    lv_obj_set_style_bg_color(_list, lv_color_hex(0x222222), 0);
    lv_obj_set_style_bg_opa(_list, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(_list, 8, 0);
    lv_obj_set_style_pad_all(_list, 5, 0);
    lv_obj_set_flex_flow(_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(_list, 4, 0);
    lv_obj_add_flag(_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(_list, LV_DIR_VER);

    /* Scrollbar style (green, thin) */
    lv_obj_set_scrollbar_mode(_list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_bg_color(_list, lv_color_hex(0x00F46A), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(_list, LV_OPA_70, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(_list, 6, LV_PART_SCROLLBAR);

    if (count == 0) {
        lv_obj_t *empty = lv_label_create(_list);
        lv_label_set_text(empty, "No networks found");
        lv_obj_set_style_text_color(empty, lv_color_hex(0x888888), 0);
        lv_obj_set_style_text_font(empty, &lv_font_montserrat_32, 0);
        lv_obj_set_width(empty, LV_PCT(100));
    }

    for (int i = 0; i < count; i++) {
        lv_obj_t *row = lv_obj_create(_list);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, LV_PCT(100), 50);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x333333), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(row, 6, 0);
        lv_obj_set_style_pad_left(row, 15, 0);
        lv_obj_set_style_pad_right(row, 15, 0);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);

        /* Highlight currently connected network */
        bool is_current = _wifi_connected && strcmp(networks[i].ssid, _current_ssid) == 0;
        if (is_current) {
            lv_obj_set_style_border_color(row, lv_color_hex(0x00F46A), 0);
            lv_obj_set_style_border_width(row, 2, 0);
        }

        /* Pressed style */
        lv_obj_set_style_bg_color(row, lv_color_hex(0x00F46A), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(row, LV_OPA_30, LV_STATE_PRESSED);

        /* SSID label */
        lv_obj_t *ssid_lbl = lv_label_create(row);
        char ssid_text[SSID_MAX_LEN + 16];
        if (is_current)
            snprintf(ssid_text, sizeof(ssid_text), "%s " LV_SYMBOL_OK, networks[i].ssid);
        else
            snprintf(ssid_text, sizeof(ssid_text), "%s", networks[i].ssid);
        lv_label_set_text(ssid_lbl, ssid_text);
        lv_obj_set_style_text_color(ssid_lbl, is_current ? lv_color_hex(0x00F46A) : lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(ssid_lbl, &lv_font_montserrat_32, 0);
        lv_obj_align(ssid_lbl, LV_ALIGN_LEFT_MID, 0, 0);

        /* Signal + security label */
        char info[48];
        snprintf(info, sizeof(info), "%s %d%%",
                 networks[i].security[0] ? LV_SYMBOL_EYE_CLOSE : "",
                 networks[i].signal);
        lv_obj_t *info_lbl = lv_label_create(row);
        lv_label_set_text(info_lbl, info);
        lv_obj_set_style_text_color(info_lbl, lv_color_hex(0x00F46A), 0);
        lv_obj_set_style_text_font(info_lbl, &lv_font_montserrat_32, 0);
        lv_obj_align(info_lbl, LV_ALIGN_RIGHT_MID, 0, 0);

        /* Store SSID in user data */
        char *ssid_copy = malloc(SSID_MAX_LEN + 1);
        if (ssid_copy) {
            strncpy(ssid_copy, networks[i].ssid, SSID_MAX_LEN);
            ssid_copy[SSID_MAX_LEN] = '\0';
            lv_obj_set_user_data(row, ssid_copy);
        }
        lv_obj_add_event_cb(row, network_item_cb, LV_EVENT_CLICKED, NULL);
    }

    /* Close button */
    lv_obj_t *close_btn = lv_obj_create(panel);
    lv_obj_remove_style_all(close_btn);
    lv_obj_set_size(close_btn, 120, 45);
    lv_obj_align(close_btn, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0x00F46A), 0);
    lv_obj_set_style_bg_opa(close_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(close_btn, 6, 0);
    lv_obj_add_flag(close_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(close_btn, close_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *close_lbl = lv_label_create(close_btn);
    lv_label_set_text(close_lbl, "Close");
    lv_obj_set_style_text_color(close_lbl, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(close_lbl, &lv_font_montserrat_32, 0);
    lv_obj_center(close_lbl);

    fprintf(stderr, "[WIFI] Showing %d networks\n", count);
}

/* ── network scanning ────────────────────────────────── */

static int scan_networks(wifi_entry_t *out, int max_count)
{
    /* Trigger a fresh scan (ignore errors — results come from cache if scan busy) */
    system("iw dev wlan0 scan trigger 2>/dev/null");

    /* Small delay to let scan results populate */
    struct timespec ts = {0, 500000000}; /* 500ms */
    nanosleep(&ts, NULL);

    FILE *fp = popen(
        "iw dev wlan0 scan dump 2>/dev/null", "r");
    if (!fp) return 0;

    char line[512];
    int count = 0;
    char cur_ssid[SSID_MAX_LEN + 1] = "";
    int  cur_signal = -100;
    char cur_security[32] = "";

    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';

        /* New BSS entry — save previous if valid */
        if (strncmp(line, "BSS ", 4) == 0) {
            if (cur_ssid[0] != '\0' && count < max_count) {
                /* Convert dBm to percentage (rough: -30=100%, -90=0%) */
                int pct = (cur_signal + 90) * 100 / 60;
                if (pct > 100) pct = 100;
                if (pct < 0) pct = 0;

                /* Check for duplicate SSID, keep strongest */
                int dup = 0;
                for (int i = 0; i < count; i++) {
                    if (strcmp(out[i].ssid, cur_ssid) == 0) {
                        if (pct > out[i].signal) out[i].signal = pct;
                        dup = 1;
                        break;
                    }
                }
                if (!dup) {
                    strncpy(out[count].ssid, cur_ssid, SSID_MAX_LEN);
                    out[count].ssid[SSID_MAX_LEN] = '\0';
                    out[count].signal = pct;
                    strncpy(out[count].security, cur_security,
                            sizeof(out[count].security) - 1);
                    out[count].security[sizeof(out[count].security) - 1] = '\0';
                    count++;
                }
            }
            cur_ssid[0] = '\0';
            cur_signal = -100;
            cur_security[0] = '\0';
            continue;
        }

        /* Parse fields (lines are tab-indented) */
        char *p = line;
        while (*p == '\t' || *p == ' ') p++;

        if (strncmp(p, "SSID: ", 6) == 0) {
            strncpy(cur_ssid, p + 6, SSID_MAX_LEN);
            cur_ssid[SSID_MAX_LEN] = '\0';
        } else if (strncmp(p, "signal: ", 8) == 0) {
            cur_signal = atoi(p + 8); /* dBm value, e.g. -45 */
        } else if (strncmp(p, "RSN:", 4) == 0 || strncmp(p, "WPA:", 4) == 0) {
            strncpy(cur_security, "WPA", sizeof(cur_security) - 1);
        }
    }

    /* Don't forget last BSS entry */
    if (cur_ssid[0] != '\0' && count < max_count) {
        int pct = (cur_signal + 90) * 100 / 60;
        if (pct > 100) pct = 100;
        if (pct < 0) pct = 0;

        int dup = 0;
        for (int i = 0; i < count; i++) {
            if (strcmp(out[i].ssid, cur_ssid) == 0) {
                if (pct > out[i].signal) out[i].signal = pct;
                dup = 1;
                break;
            }
        }
        if (!dup) {
            strncpy(out[count].ssid, cur_ssid, SSID_MAX_LEN);
            out[count].ssid[SSID_MAX_LEN] = '\0';
            out[count].signal = pct;
            strncpy(out[count].security, cur_security,
                    sizeof(out[count].security) - 1);
            out[count].security[sizeof(out[count].security) - 1] = '\0';
            count++;
        }
    }

    pclose(fp);

    /* Sort by signal strength (descending) */
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (out[j].signal > out[i].signal) {
                wifi_entry_t tmp = out[i];
                out[i] = out[j];
                out[j] = tmp;
            }
        }
    }

    return count;
}

/* ── password dialog ─────────────────────────────────── */

static void show_password_dialog(const char *ssid)
{
    strncpy(_selected_ssid, ssid, SSID_MAX_LEN);
    _selected_ssid[SSID_MAX_LEN] = '\0';

    _kb_overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(_kb_overlay);
    lv_obj_set_size(_kb_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(_kb_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(_kb_overlay, LV_OPA_80, 0);
    lv_obj_add_flag(_kb_overlay, LV_OBJ_FLAG_CLICKABLE);

    /* Back button (top-left) */
    lv_obj_t *back_btn = lv_obj_create(_kb_overlay);
    lv_obj_remove_style_all(back_btn);
    lv_obj_set_size(back_btn, 140, 45);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 20, 20);
    lv_obj_set_style_bg_color(back_btn, lv_color_hex(0x444444), 0);
    lv_obj_set_style_bg_opa(back_btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(back_btn, 6, 0);
    lv_obj_add_flag(back_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(back_btn, back_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_lbl = lv_label_create(back_btn);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_color(back_lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_32, 0);
    lv_obj_center(back_lbl);

    /* SSID label */
    lv_obj_t *ssid_lbl = lv_label_create(_kb_overlay);
    char title[128];
    snprintf(title, sizeof(title), "Connect to: %s", ssid);
    lv_label_set_text(ssid_lbl, title);
    lv_obj_set_style_text_color(ssid_lbl, lv_color_hex(0x00F46A), 0);
    lv_obj_set_style_text_font(ssid_lbl, &lv_font_montserrat_32, 0);
    lv_obj_align(ssid_lbl, LV_ALIGN_TOP_MID, 0, 30);

    /* Password text area */
    _kb_ta = lv_textarea_create(_kb_overlay);
    lv_obj_set_size(_kb_ta, 600, 60);
    lv_obj_align(_kb_ta, LV_ALIGN_TOP_MID, 0, 80);
    lv_textarea_set_max_length(_kb_ta, PASS_MAX_LEN);
    lv_textarea_set_one_line(_kb_ta, true);
    lv_textarea_set_placeholder_text(_kb_ta, "Enter password...");
    lv_textarea_set_password_mode(_kb_ta, true);
    lv_obj_set_style_text_font(_kb_ta, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(_kb_ta, lv_color_hex(0x00F46A), 0);
    lv_obj_set_style_bg_color(_kb_ta, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_border_color(_kb_ta, lv_color_hex(0x00F46A), 0);
    lv_obj_set_style_border_width(_kb_ta, 2, 0);

    /* Status label */
    _status_lbl = lv_label_create(_kb_overlay);
    lv_label_set_text(_status_lbl, "");
    lv_obj_set_style_text_color(_status_lbl, lv_color_hex(0x00F46A), 0);
    lv_obj_set_style_text_font(_status_lbl, &lv_font_montserrat_32, 0);
    lv_obj_align(_status_lbl, LV_ALIGN_TOP_MID, 0, 150);

    /* Keyboard */
    _kb = lv_keyboard_create(_kb_overlay);
    lv_obj_set_size(_kb, LV_PCT(100), 450);
    lv_obj_align(_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(_kb, _kb_ta);
    lv_obj_set_style_bg_color(_kb, lv_color_hex(0x2D2D2D), 0);
    lv_obj_set_style_bg_color(_kb, lv_color_hex(0x444444), LV_PART_ITEMS);
    lv_obj_set_style_text_color(_kb, lv_color_hex(0xFFFFFF), LV_PART_ITEMS);
    lv_obj_set_style_text_font(_kb, &lv_font_montserrat_32, LV_PART_ITEMS);
    lv_obj_add_event_cb(_kb, kb_ready_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(_kb, kb_cancel_cb, LV_EVENT_CANCEL, NULL);

    fprintf(stderr, "[WIFI] Password dialog for '%s'\n", ssid);
}

/* ── auto-dismiss timer ──────────────────────────────── */

static void auto_dismiss_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    /* Refresh status after connection attempt */
    check_wifi_status();
    update_wifi_icon();
    dismiss_password();
    dismiss_overlay();
    lv_timer_delete(timer);
}

/* ── callbacks ───────────────────────────────────────── */

static void logo_click_cb(lv_event_t *e)
{
    (void)e;
    wifi_manager_show();
}

static void bg_click_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    if (target == _overlay) {
        dismiss_overlay();
    }
}

static void close_btn_cb(lv_event_t *e)
{
    (void)e;
    dismiss_overlay();
}

static void back_btn_cb(lv_event_t *e)
{
    (void)e;
    dismiss_password();
}

static void disconnect_btn_cb(lv_event_t *e)
{
    (void)e;
    if (!_wifi_connected || _current_ssid[0] == '\0') return;

    fprintf(stderr, "[WIFI] Disconnecting from '%s'...\n", _current_ssid);
    system("nmcli device disconnect wlan0 2>/dev/null");

    check_wifi_status();
    update_wifi_icon();

    /* Refresh the overlay to reflect new state */
    dismiss_overlay();
    wifi_manager_show();
}

static void network_item_cb(lv_event_t *e)
{
    lv_obj_t *row = lv_event_get_current_target(e);
    char *ssid = (char *)lv_obj_get_user_data(row);
    if (ssid) {
        show_password_dialog(ssid);
    }
}

static void kb_ready_cb(lv_event_t *e)
{
    (void)e;
    if (!_kb_ta) return;

    const char *pass = lv_textarea_get_text(_kb_ta);
    if (!pass || pass[0] == '\0') return;

    if (_status_lbl) {
        lv_label_set_text(_status_lbl, "Connecting...");
    }

    if (_kb) lv_obj_add_flag(_kb, LV_OBJ_FLAG_HIDDEN);
    if (_kb_ta) lv_obj_add_flag(_kb_ta, LV_OBJ_FLAG_HIDDEN);

    fprintf(stderr, "[WIFI] Connecting to '%s'...\n", _selected_ssid);

    char cmd[512];
    int success = 0;

    /* Use nmcli to connect (creates/updates connection profile automatically) */
    snprintf(cmd, sizeof(cmd),
             "nmcli device wifi connect '%s' password '%s' ifname wlan0 2>/dev/null",
             _selected_ssid, pass);
    int ret = system(cmd);

    if (ret == 0) {
        /* Wait a moment for association */
        struct timespec ts = {3, 0};
        nanosleep(&ts, NULL);

        check_wifi_status();
        if (_wifi_connected && strcmp(_current_ssid, _selected_ssid) == 0)
            success = 1;
    }

    if (_status_lbl) {
        if (success) {
            lv_label_set_text(_status_lbl, "Connected!");
            lv_obj_set_style_text_color(_status_lbl, lv_color_hex(0x00F46A), 0);
        } else {
            lv_label_set_text(_status_lbl, "Connection failed");
            lv_obj_set_style_text_color(_status_lbl, lv_color_hex(0xFF4444), 0);
        }
    }

    fprintf(stderr, "[WIFI] Connection %s\n", success ? "succeeded" : "failed");

    lv_timer_create(auto_dismiss_timer_cb, 2000, NULL);
}

static void kb_cancel_cb(lv_event_t *e)
{
    (void)e;
    dismiss_password();
}

/* ── cleanup ─────────────────────────────────────────── */

static void dismiss_password(void)
{
    if (_kb_overlay) {
        lv_obj_delete(_kb_overlay);
        _kb_overlay = NULL;
        _kb_ta      = NULL;
        _kb         = NULL;
        _status_lbl = NULL;
    }
}

static void dismiss_overlay(void)
{
    dismiss_password();

    if (_overlay) {
        if (_list) {
            uint32_t cnt = lv_obj_get_child_count(_list);
            for (uint32_t i = 0; i < cnt; i++) {
                lv_obj_t *child = lv_obj_get_child(_list, i);
                char *data = (char *)lv_obj_get_user_data(child);
                if (data) free(data);
            }
        }

        lv_obj_delete(_overlay);
        _overlay = NULL;
        _list    = NULL;
        fprintf(stderr, "[WIFI] Overlay dismissed\n");
    }
}
