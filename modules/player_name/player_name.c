#include "player_name.h"
#include "../../ui/ui.h"
#include <stdio.h>
#include <string.h>

/* ── name storage ────────────────────────────────────── */

static const char *default_names[PLAYER_NAME_COUNT] = {
    "Player 1", "Player 2", "Player 3", "Player 4"
};

static char names[PLAYER_NAME_COUNT][PLAYER_NAME_MAX_LEN + 1];

/* ── label registry ──────────────────────────────────── */

#define MAX_LABELS 150

typedef struct {
    lv_obj_t *label;
    int       player_idx;
} label_entry_t;

static label_entry_t registry[MAX_LABELS];
static int registry_count = 0;

/* ── keyboard overlay state ──────────────────────────── */

static lv_obj_t *kb_overlay  = NULL;   /* full-screen dark bg */
static lv_obj_t *kb_textarea = NULL;
static lv_obj_t *kb_keyboard = NULL;
static int       kb_editing_idx = -1;

/* ── forward declarations ────────────────────────────── */

static void label_click_cb(lv_event_t *e);
static void label_delete_cb(lv_event_t *e);
static void show_keyboard(int player_idx);
static void dismiss_keyboard(void);

/* ── public API ──────────────────────────────────────── */

const char *player_name_get(int idx)
{
    if (idx < 0 || idx >= PLAYER_NAME_COUNT) return "???";
    return names[idx];
}

void player_name_set(int idx, const char *name)
{
    if (idx < 0 || idx >= PLAYER_NAME_COUNT) return;
    if (name == NULL || name[0] == '\0') {
        strncpy(names[idx], default_names[idx], PLAYER_NAME_MAX_LEN);
    } else {
        strncpy(names[idx], name, PLAYER_NAME_MAX_LEN);
    }
    names[idx][PLAYER_NAME_MAX_LEN] = '\0';
}

void player_name_reset(void)
{
    dismiss_keyboard();
    for (int i = 0; i < PLAYER_NAME_COUNT; i++) {
        strncpy(names[i], default_names[i], PLAYER_NAME_MAX_LEN);
        names[i][PLAYER_NAME_MAX_LEN] = '\0';
    }
    player_name_apply_all();
    fprintf(stderr, "[PLAYER_NAME] Reset all names to defaults\n");
}

void player_name_register_label(lv_obj_t *label, int player_idx)
{
    if (label == NULL || player_idx < 0 || player_idx >= PLAYER_NAME_COUNT) return;

    /* Skip duplicates */
    for (int i = 0; i < registry_count; i++) {
        if (registry[i].label == label) return;
    }

    if (registry_count >= MAX_LABELS) {
        fprintf(stderr, "[PLAYER_NAME] WARNING: registry full (%d)\n", MAX_LABELS);
        return;
    }

    registry[registry_count].label      = label;
    registry[registry_count].player_idx = player_idx;
    registry_count++;

    /* Make label clickable */
    lv_obj_add_flag(label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(label, label_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(label, label_delete_cb, LV_EVENT_DELETE, NULL);
}

void player_name_apply_all(void)
{
    for (int i = 0; i < registry_count; i++) {
        if (registry[i].label != NULL) {
            lv_label_set_text(registry[i].label, names[registry[i].player_idx]);
        }
    }
}

/* ── screen load callback ────────────────────────────── */

static void screen_loaded_cb(lv_event_t *e)
{
    (void)e;
    player_name_apply_all();
}

/* ── registration helper macro ───────────────────────── */

#define REG(obj, pidx) player_name_register_label(obj, pidx)

void player_name_init(void)
{
    /* Initialize names to defaults */
    for (int i = 0; i < PLAYER_NAME_COUNT; i++) {
        strncpy(names[i], default_names[i], PLAYER_NAME_MAX_LEN);
        names[i][PLAYER_NAME_MAX_LEN] = '\0';
    }

    registry_count = 0;

    /* ── Stroke Play game screens ── */

    /* 1P */
    REG(ui_SP1P9HGSPSText, 0);
    REG(ui_SP1P18HGSPSText, 0);

    /* 2P */
    REG(ui_SP2P9HGSP1SText, 0);    REG(ui_SP2P9HGSP2SText, 1);
    REG(ui_SP2P18HGSP1SText, 0);   REG(ui_SP2P18HGSP2SText, 1);

    /* 3P */
    REG(ui_SP3P9HGSP1SText, 0);    REG(ui_SP3P9HGSP2SText, 1);    REG(ui_SP3P9HGSP3SText, 2);
    REG(ui_SP3P18HGSP1SText, 0);   REG(ui_SP3P18HGSP2SText, 1);   REG(ui_SP3P18HGSP3SText, 2);

    /* 4P */
    REG(ui_SP4P9HGSP1SText, 0);    REG(ui_SP4P9HGSP2SText, 1);    REG(ui_SP4P9HGSP3SText, 2);    REG(ui_SP4P9HGSP4SText, 3);
    REG(ui_SP4P18HGSP1SText, 0);   REG(ui_SP4P18HGSP2SText, 1);   REG(ui_SP4P18HGSP3SText, 2);   REG(ui_SP4P18HGSP4SText, 3);

    /* ── Stroke Play scorecards ── */

    /* 1P */
    REG(ui_SP1P18HScPText, 0);

    /* 2P */
    REG(ui_SP2P9HScP1Text, 0);     REG(ui_SP2P9HScP2Text, 1);
    REG(ui_SP2P18HScP1Text, 0);    REG(ui_SP2P18HScP2Text, 1);

    /* 3P */
    REG(ui_SP3P9HScP1Text, 0);     REG(ui_SP3P9HScP2Text, 1);     REG(ui_SP3P9HScP3Text, 2);
    REG(ui_SP3P18HScP1Text, 0);    REG(ui_SP3P18HScP2Text, 1);    REG(ui_SP3P18HScP3Text, 2);

    /* 4P */
    REG(ui_SP4P9HScP1Text, 0);     REG(ui_SP4P9HScP2Text, 1);     REG(ui_SP4P9HScP3Text, 2);     REG(ui_SP4P9HScP4Text, 3);
    REG(ui_SP4P18HScP1Text, 0);    REG(ui_SP4P18HScP2Text, 1);    REG(ui_SP4P18HScP3Text, 2);    REG(ui_SP4P18HScP4Text, 3);

    /* ── Match Play 1v1 ── */

    REG(ui_MP1V19HGSP1SText, 0);   REG(ui_MP1V19HGSP2SText, 1);
    REG(ui_MP1V118HGSP1SText, 0);  REG(ui_MP1V118HGSP2SText, 1);

    REG(ui_MP1V19HScP1Text, 0);    REG(ui_MP1V19HScP2Text, 1);
    REG(ui_MP1V118HScP1Text, 0);   REG(ui_MP1V118HScP2Text, 1);

    /* ── Match Play 2v2 (Team 1 → idx 0, Team 2 → idx 1) ── */

    REG(ui_MP2V29HGST1SText, 0);   REG(ui_MP2V29HGST2SText, 1);
    REG(ui_MP2V218HGST1SText, 0);  REG(ui_MP2V218HGST2SText, 1);

    REG(ui_MP2V29HScT1Text, 0);    REG(ui_MP2V29HScT2Text, 1);
    REG(ui_MP2V218HScT1Text, 0);   REG(ui_MP2V218HScT2Text, 1);

    /* ── Quota Points game screens ── */

    /* 1P */
    REG(ui_Q1P9HGSPSText, 0);      REG(ui_Q1P18HGSPSText, 0);

    /* 2P */
    REG(ui_Q2P9HGSP1SText, 0);     REG(ui_Q2P9HGSP2SText, 1);
    REG(ui_Q2P18HGSP1SText, 0);    REG(ui_Q2P18HGSP2SText, 1);

    /* 3P */
    REG(ui_Q3P9HGSP1SText, 0);     REG(ui_Q3P9HGSP2SText, 1);     REG(ui_Q3P9HGSP3SText, 2);
    REG(ui_Q3P18HGSP1SText, 0);    REG(ui_Q3P18HGSP2SText, 1);    REG(ui_Q3P18HGSP3SText, 2);

    /* 4P */
    REG(ui_Q4P9HGSP1SText, 0);     REG(ui_Q4P9HGSP2SText, 1);     REG(ui_Q4P9HGSP3SText, 2);     REG(ui_Q4P9HGSP4SText, 3);
    REG(ui_Q4P18HGSP1SText, 0);    REG(ui_Q4P18HGSP2SText, 1);    REG(ui_Q4P18HGSP3SText, 2);    REG(ui_Q4P18HGSP4SText, 3);

    /* ── Vegas Quota Points game screens ── */

    /* 1P */
    REG(ui_VQ1P9HGSPSText, 0);     REG(ui_VQ1P18HGSPSText, 0);

    /* 2P */
    REG(ui_VQ2P9HGSP1SText, 0);    REG(ui_VQ2P9HGSP2SText, 1);
    REG(ui_VQ2P18HGSP1SText, 0);   REG(ui_VQ2P18HGSP2SText, 1);

    /* 3P */
    REG(ui_VQ3P9HGSP1SText, 0);    REG(ui_VQ3P9HGSP2SText, 1);    REG(ui_VQ3P9HGSP3SText, 2);
    REG(ui_VQ3P18HGSP1SText, 0);   REG(ui_VQ3P18HGSP2SText, 1);   REG(ui_VQ3P18HGSP3SText, 2);

    /* 4P */
    REG(ui_VQ4P9HGSP1SText, 0);    REG(ui_VQ4P9HGSP2SText, 1);    REG(ui_VQ4P9HGSP3SText, 2);    REG(ui_VQ4P9HGSP4SText, 3);
    REG(ui_VQ4P18HGSP1SText, 0);   REG(ui_VQ4P18HGSP2SText, 1);   REG(ui_VQ4P18HGSP3SText, 2);   REG(ui_VQ4P18HGSP4SText, 3);

    /* ── Hook screen-loaded event to refresh names ── */

    lv_display_t *disp = lv_display_get_default();
    if (disp) {
        lv_display_add_event_cb(disp, screen_loaded_cb, LV_EVENT_SCREEN_LOADED, NULL);
    }

    fprintf(stderr, "[PLAYER_NAME] Initialized with %d labels registered\n", registry_count);
}

/* ── internal callbacks ──────────────────────────────── */

static void label_click_cb(lv_event_t *e)
{
    lv_obj_t *label = lv_event_get_target(e);

    /* Find which player this label belongs to */
    for (int i = 0; i < registry_count; i++) {
        if (registry[i].label == label) {
            show_keyboard(registry[i].player_idx);
            return;
        }
    }
}

static void label_delete_cb(lv_event_t *e)
{
    lv_obj_t *label = lv_event_get_target(e);

    /* Remove from registry */
    for (int i = 0; i < registry_count; i++) {
        if (registry[i].label == label) {
            registry[i] = registry[registry_count - 1];
            registry_count--;
            return;
        }
    }
}

/* ── keyboard overlay ────────────────────────────────── */

static void kb_bg_click_cb(lv_event_t *e)
{
    (void)e;
    dismiss_keyboard();
}

static void kb_ready_cb(lv_event_t *e)
{
    (void)e;
    if (kb_textarea == NULL || kb_editing_idx < 0) return;

    const char *text = lv_textarea_get_text(kb_textarea);
    player_name_set(kb_editing_idx, text);
    player_name_apply_all();

    fprintf(stderr, "[PLAYER_NAME] Player %d renamed to \"%s\"\n",
            kb_editing_idx + 1, names[kb_editing_idx]);

    dismiss_keyboard();
}

static void kb_cancel_cb(lv_event_t *e)
{
    (void)e;
    dismiss_keyboard();
}

static void show_keyboard(int player_idx)
{
    /* Don't open if already open */
    if (kb_overlay != NULL) {
        dismiss_keyboard();
    }

    kb_editing_idx = player_idx;

    /* Dark semi-transparent background covering entire screen */
    kb_overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(kb_overlay);
    lv_obj_set_size(kb_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(kb_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(kb_overlay, LV_OPA_70, 0);
    lv_obj_add_flag(kb_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(kb_overlay, kb_bg_click_cb, LV_EVENT_CLICKED, NULL);

    /* Text area for name input */
    kb_textarea = lv_textarea_create(kb_overlay);
    lv_obj_set_size(kb_textarea, 500, 70);
    lv_obj_align(kb_textarea, LV_ALIGN_TOP_MID, 0, 80);
    lv_textarea_set_max_length(kb_textarea, PLAYER_NAME_MAX_LEN);
    lv_textarea_set_one_line(kb_textarea, true);
    lv_textarea_set_text(kb_textarea, names[player_idx]);
    lv_obj_set_style_text_font(kb_textarea, &ui_font_Unitblock_48, 0);
    lv_obj_set_style_text_color(kb_textarea, lv_color_hex(0x00F46A), 0);
    lv_obj_set_style_bg_color(kb_textarea, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_border_color(kb_textarea, lv_color_hex(0x00F46A), 0);
    lv_obj_set_style_border_width(kb_textarea, 2, 0);

    /* Keyboard */
    kb_keyboard = lv_keyboard_create(kb_overlay);
    lv_obj_set_size(kb_keyboard, LV_PCT(100), 350);
    lv_obj_align(kb_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(kb_keyboard, kb_textarea);
    lv_obj_set_style_bg_color(kb_keyboard, lv_color_hex(0x2D2D2D), 0);
    lv_obj_set_style_bg_color(kb_keyboard, lv_color_hex(0x444444), LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb_keyboard, lv_color_hex(0xFFFFFF), LV_PART_ITEMS);
    lv_obj_add_event_cb(kb_keyboard, kb_ready_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(kb_keyboard, kb_cancel_cb, LV_EVENT_CANCEL, NULL);

    /* Select all text so user can just start typing */
    lv_textarea_set_cursor_pos(kb_textarea, 0);

    fprintf(stderr, "[PLAYER_NAME] Keyboard opened for Player %d\n", player_idx + 1);
}

static void dismiss_keyboard(void)
{
    if (kb_overlay != NULL) {
        lv_obj_delete(kb_overlay);
        kb_overlay  = NULL;
        kb_textarea = NULL;
        kb_keyboard = NULL;
        kb_editing_idx = -1;
        fprintf(stderr, "[PLAYER_NAME] Keyboard dismissed\n");
    }
}
