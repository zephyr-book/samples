/**
 * @file clock_sync.h
 * @brief Wall clock from SNTP, for the title bar.
 *
 *        This board has no RTC, so the wall clock is the kernel's monotonic
 *        time plus an offset that SNTP establishes once the device comes
 *        online. Nothing here is available in a build without networking; the
 *        clock then simply never becomes valid and the title bar stays clear
 *        of it.
 *
 * @copyright Copyright (c) Centro de Inovacao EDGE
 */

#ifndef BRINGUP_CLOCK_SYNC_H_
#define BRINGUP_CLOCK_SYNC_H_

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Start watching for the device coming online.
 *
 * On a build with networking this asks Wi-Fi to connect using the credentials
 * stored in flash, then syncs from SNTP whenever the link comes up and
 * periodically after that. On a build without, it does nothing.
 */
void clock_sync_init(void);

/**
 * @brief Whether the clock has been set at least once.
 *
 * @return False until the first successful SNTP query.
 */
bool clock_sync_is_valid(void);

/**
 * @brief Format the current local time as "HH:MM".
 *
 * @param[out] buffer Receives the text, or an empty string if the clock has
 *                    never been set.
 * @param      size   Size of @p buffer. Six bytes is enough.
 *
 * @return True if a time was written.
 */
bool clock_sync_format(char *buffer, size_t size);

/**
 * @brief One-line sync state, for a status display.
 *
 * @return A string such as "synced", "no link" or "not in build". Never NULL.
 */
const char *clock_sync_state(void);

#endif /* BRINGUP_CLOCK_SYNC_H_ */
