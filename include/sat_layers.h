/* Satellite layers: turning cached feeds into catalogs, and the Starlink
 * layer's life cycle.
 *
 * Starlink is never downloaded without the user's consent: turning it on shows
 * the cached data at once, and asks before refreshing it (only when the cache
 * is older than FEED_BACKGROUND_MAX_AGE_HOURS) or downloading it for the first
 * time. An accepted download runs in the background, so a slow link never
 * freezes the sky.
 */

#ifndef SAT_LAYERS_H
#define SAT_LAYERS_H

#include "core.h"
#include "feed.h"
#include "satellite.h"
#include "ui.h"

#include <stdbool.h>

struct StarlinkLayer
{
    struct SatCatalog catalog;
    bool loaded;
    struct FeedDownload download;
    double updated_at; // Monotonic seconds of the last propagation
    int above;         // Above the horizon at the last update
    int sunlit;        // ... and in sunlight
    bool report;       // Toast the counts after the next update
};

/* Load a cached feed into a catalog, keeping only `only` (NULL: all). The
 * catalog is empty when there is no usable data
 */
void sat_catalog_from_feed(enum FeedId id, const long *only, int num_only, struct SatCatalog *out);

/* The layer was turned on (x or --starlink): load and show the cache, and ask
 * about downloading when it is missing or old
 */
void starlink_enable(struct StarlinkLayer *layer, struct Conf *config, struct UiState *ui, double mono);

/* U: ask to refresh the data now
 */
void starlink_offer_refresh(struct StarlinkLayer *layer, const struct Conf *config, struct UiState *ui, double mono);

/* The answer to the question asked above
 */
void starlink_answer(struct StarlinkLayer *layer, struct Conf *config, struct UiState *ui, bool yes, double mono);

/* Per frame: finish a background download, and propagate at most every
 * `period` seconds while the layer is on
 */
void starlink_tick(struct StarlinkLayer *layer, struct Conf *config, struct UiState *ui, double jd, const double sun_dir[3],
                   double mono, double period);

/* A status line while downloading, or NULL
 */
const char *starlink_status(const struct StarlinkLayer *layer);

/* Cancel any download and free the catalog
 */
void starlink_free(struct StarlinkLayer *layer);

#endif // SAT_LAYERS_H
