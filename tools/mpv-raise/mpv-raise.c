/*
 * mpv-raise: Keeps mpv window always on top using wlr-foreign-toplevel-management.
 * Connects to Wayland, finds mpv toplevel, and activates it periodically.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <wayland-client.h>
#include "wlr-foreign-toplevel-management-unstable-v1-client-protocol.h"

static struct wl_display *display = NULL;
static struct wl_seat *seat = NULL;
static struct zwlr_foreign_toplevel_manager_v1 *toplevel_manager = NULL;

/* Track mpv toplevel handle */
static struct zwlr_foreign_toplevel_handle_v1 *mpv_handle = NULL;
static bool mpv_found = false;

/* ── toplevel handle callbacks ─────────────────────────── */

static void handle_title(void *data,
                         struct zwlr_foreign_toplevel_handle_v1 *handle,
                         const char *title)
{
    (void)data; (void)handle; (void)title;
}

static void handle_app_id(void *data,
                          struct zwlr_foreign_toplevel_handle_v1 *handle,
                          const char *app_id)
{
    (void)data;
    if (app_id && strcmp(app_id, "mpv") == 0)
    {
        mpv_handle = handle;
        mpv_found = true;
    }
}

static void handle_output_enter(void *data,
                                struct zwlr_foreign_toplevel_handle_v1 *handle,
                                struct wl_output *output)
{
    (void)data; (void)handle; (void)output;
}

static void handle_output_leave(void *data,
                                struct zwlr_foreign_toplevel_handle_v1 *handle,
                                struct wl_output *output)
{
    (void)data; (void)handle; (void)output;
}

static void handle_state(void *data,
                         struct zwlr_foreign_toplevel_handle_v1 *handle,
                         struct wl_array *state)
{
    (void)data; (void)handle; (void)state;
}

static void handle_done(void *data,
                        struct zwlr_foreign_toplevel_handle_v1 *handle)
{
    (void)data; (void)handle;
}

static void handle_closed(void *data,
                          struct zwlr_foreign_toplevel_handle_v1 *handle)
{
    (void)data;
    if (handle == mpv_handle)
    {
        mpv_handle = NULL;
        mpv_found = false;
    }
    zwlr_foreign_toplevel_handle_v1_destroy(handle);
}

static void handle_parent(void *data,
                          struct zwlr_foreign_toplevel_handle_v1 *handle,
                          struct zwlr_foreign_toplevel_handle_v1 *parent)
{
    (void)data; (void)handle; (void)parent;
}

static const struct zwlr_foreign_toplevel_handle_v1_listener toplevel_handle_listener = {
    .title = handle_title,
    .app_id = handle_app_id,
    .output_enter = handle_output_enter,
    .output_leave = handle_output_leave,
    .state = handle_state,
    .done = handle_done,
    .closed = handle_closed,
    .parent = handle_parent,
};

/* ── manager callbacks ─────────────────────────────────── */

static void manager_toplevel(void *data,
                             struct zwlr_foreign_toplevel_manager_v1 *manager,
                             struct zwlr_foreign_toplevel_handle_v1 *handle)
{
    (void)data; (void)manager;
    zwlr_foreign_toplevel_handle_v1_add_listener(handle, &toplevel_handle_listener, NULL);
}

static void manager_finished(void *data,
                             struct zwlr_foreign_toplevel_manager_v1 *manager)
{
    (void)data; (void)manager;
    toplevel_manager = NULL;
}

static const struct zwlr_foreign_toplevel_manager_v1_listener manager_listener = {
    .toplevel = manager_toplevel,
    .finished = manager_finished,
};

/* ── registry ──────────────────────────────────────────── */

static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface, uint32_t version)
{
    (void)data;
    if (strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0)
    {
        toplevel_manager = wl_registry_bind(registry, name,
                                            &zwlr_foreign_toplevel_manager_v1_interface,
                                            version < 3 ? version : 3);
        zwlr_foreign_toplevel_manager_v1_add_listener(toplevel_manager, &manager_listener, NULL);
    }
    else if (strcmp(interface, "wl_seat") == 0)
    {
        seat = wl_registry_bind(registry, name, &wl_seat_interface, 1);
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
    (void)data; (void)registry; (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

/* ── main ──────────────────────────────────────────────── */

int main(void)
{
    display = wl_display_connect(NULL);
    if (!display)
    {
        fprintf(stderr, "mpv-raise: cannot connect to Wayland display\n");
        return 1;
    }

    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);

    if (!toplevel_manager)
    {
        fprintf(stderr, "mpv-raise: compositor does not support wlr-foreign-toplevel-management\n");
        wl_display_disconnect(display);
        return 1;
    }

    if (!seat)
    {
        fprintf(stderr, "mpv-raise: no wl_seat found\n");
        wl_display_disconnect(display);
        return 1;
    }

    /* Initial roundtrip to discover existing toplevels */
    wl_display_roundtrip(display);

    fprintf(stderr, "mpv-raise: running (activate mpv every 500ms)\n");

    while (1)
    {
        /* Process events (new toplevels, closed, etc.) */
        wl_display_roundtrip(display);

        if (mpv_handle && seat)
        {
            zwlr_foreign_toplevel_handle_v1_activate(mpv_handle, seat);
            wl_display_flush(display);
        }

        usleep(500000); /* 500ms */
    }

    wl_display_disconnect(display);
    return 0;
}
