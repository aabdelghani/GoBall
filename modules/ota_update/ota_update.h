#ifndef OTA_UPDATE_H
#define OTA_UPDATE_H

/**
 * Initialize the OTA update module (no-op for now, future use).
 */
void ota_update_init(void);

/**
 * Show the OTA firmware update popup overlay.
 * Checks the fleet server for a newer version and presents
 * a download/install UI if an update is available.
 */
void ota_update_show(void);

#endif /* OTA_UPDATE_H */
