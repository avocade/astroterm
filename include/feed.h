/* Satellite data feeds from CelesTrak, cached per user.
 *
 * Feeds are refreshed before the UI starts, by running `curl` (no shell, no
 * link dependency) into a unique temporary file that replaces the cache only
 * after it validates. Politeness rules: a feed is fetched only when older than
 * FEED_MAX_AGE_HOURS, and never more often than FEED_RETRY_HOURS, because
 * CelesTrak answers a repeat download of unchanged data with HTTP 403.
 */

#ifndef FEED_H
#define FEED_H

#include <stdbool.h>
#include <stddef.h>

#define FEED_MAX_AGE_HOURS 12.0
#define FEED_RETRY_HOURS 2.0

enum FeedId
{
    FEED_STATIONS = 0,
    FEED_STARLINK,
    NUM_FEEDS
};

/* Resolve (and create, mode 0700) the cache directory:
 * $XDG_CACHE_HOME/astroterm if absolute, else ~/.cache/astroterm
 */
bool feed_cache_dir(char *buf, size_t len);

/* Age of a cached feed in hours, or a negative value if there is none
 */
double feed_age_hours(enum FeedId id);

/* Read a cached feed. Returns a malloc'd, NUL-terminated buffer or NULL
 */
char *feed_read(enum FeedId id, size_t *len_out);

/* Refresh every stale feed (blocking; call before curses starts). Progress
 * goes to stderr; a one-line summary for the UI goes to `message` (empty when
 * there is nothing to report)
 */
void feed_refresh_all(char *message, size_t len);

#endif // FEED_H
