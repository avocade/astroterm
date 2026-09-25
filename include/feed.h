/* Satellite data feeds from CelesTrak, cached per user.
 *
 * Feeds are refreshed before the UI starts, by running `curl` (no shell, no
 * link dependency) into a unique temporary file that replaces the cache only
 * after it validates. A feed in use is refreshed when older than
 * FEED_MAX_AGE_HOURS; a feed that is merely kept warm for later (Starlink when
 * its layer is off) only when older than FEED_BACKGROUND_MAX_AGE_HOURS. Never
 * more often than FEED_RETRY_HOURS, because CelesTrak answers a repeat download
 * of unchanged data with HTTP 403.
 */

#ifndef FEED_H
#define FEED_H

#include <stdbool.h>
#include <stddef.h>

#define FEED_MAX_AGE_HOURS 12.0
#define FEED_BACKGROUND_MAX_AGE_HOURS (14.0 * 24.0) // Matches the 14-day element-age limit
#define FEED_SKIP (-1.0)
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

/* Refresh each feed older than its maximum age in hours (FEED_SKIP: never).
 * Blocking; call before curses starts. Progress goes to stderr; a one-line
 * summary for the UI goes to `message` (empty when there is nothing to report)
 */
void feed_refresh(const double max_age_hours[NUM_FEEDS], char *message, size_t len);

/* A download running in the background (for a layer turned on mid-session)
 */
struct FeedDownload
{
    bool active;
    enum FeedId id;
    long pid;
    int status_fd;
    char tmp[1024];
};

enum FeedStart
{
    FEED_START_OK,
    FEED_START_TOO_SOON, // Asked within FEED_RETRY_HOURS: CelesTrak would answer 403
    FEED_START_NO_CURL,
    FEED_START_ERROR,
    FEED_START_UNSUPPORTED, // Windows
};

/* Start downloading a feed without blocking. On FEED_START_TOO_SOON,
 * *minutes_to_wait says when to try again
 */
enum FeedStart feed_download_start(enum FeedId id, struct FeedDownload *download, double *minutes_to_wait);

/* Check on a background download. Returns true once it has finished; then
 * *updated tells whether the cache was replaced, and `message` holds the
 * problem if not
 */
bool feed_download_poll(struct FeedDownload *download, bool *updated, char *message, size_t len);

/* Stop a background download and remove its temporary file
 */
void feed_download_cancel(struct FeedDownload *download);

#endif // FEED_H
