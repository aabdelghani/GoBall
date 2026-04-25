#ifndef RADIO_MANAGER_H
#define RADIO_MANAGER_H

#include <stdbool.h>

/* Default config path; override via env GOBALL_RADIO_CONF if needed. */
#define RADIO_CONF_PATH_DEFAULT "/etc/goball-radio.conf"

/* Load config (if present), spawn mpv if ENABLED=1. Idempotent. */
void radio_init(void);

/* Toggle. Persists to disk and spawns/kills mpv. */
void radio_set_enabled(bool on);

/* Read current state (true = mpv running and ENABLED=1 in conf). */
bool radio_is_enabled(void);

/* Stop mpv on shutdown. Safe to call repeatedly. */
void radio_stop(void);

#endif /* RADIO_MANAGER_H */
