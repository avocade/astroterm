#include "sat_layers.h"

#include "omm.h"

#include <stdio.h>
#include <stdlib.h>

void sat_catalog_from_feed(enum FeedId id, const long *only, int num_only, struct SatCatalog *out)
{
    out->sats = NULL;
    out->count = 0;

    size_t len;
    char *data = feed_read(id, &len);
    if (data == NULL)
    {
        return;
    }
    struct OmmRecord *records = NULL;
    int n = omm_parse_csv(data, len, &records);
    free(data);
    if (n > 0)
    {
        satellite_catalog_build(records, n, only, num_only, out);
    }
    free(records);
}

static void reload(struct StarlinkLayer *layer)
{
    satellite_catalog_free(&layer->catalog);
    sat_catalog_from_feed(FEED_STARLINK, NULL, 0, &layer->catalog);
    layer->loaded = true;
    layer->updated_at = -1.0e9; // Propagate on the next frame
}

void starlink_enable(struct StarlinkLayer *layer, struct Conf *config, struct UiState *ui, double mono)
{
    if (!layer->loaded)
    {
        reload(layer);
    }

    if (layer->catalog.count > 0)
    {
        // Show what we have at once
        layer->updated_at = -1.0e9;
        layer->report = true;

        double age = feed_age_hours(FEED_STARLINK);
        if (!config->offline && !layer->download.active && age > FEED_BACKGROUND_MAX_AGE_HOURS)
        {
            ui_ask(ui, "Starlink data is %.0f days old. Refresh it now (about 2 MB)? y/N", age / 24.0);
        }
        return;
    }

    if (layer->download.active)
    {
        ui_toast(ui, mono, "Starlink: downloading...");
        return;
    }
    if (config->offline)
    {
        config->starlink = false;
        ui_toast(ui, mono, "Starlink: no data (offline)");
        return;
    }
    ui_ask(ui, "No Starlink data yet. Download it now (about 2 MB)? y/N");
}

void starlink_offer_refresh(struct StarlinkLayer *layer, const struct Conf *config, struct UiState *ui, double mono)
{
    if (config->offline)
    {
        ui_toast(ui, mono, "Offline: no downloads (--offline)");
        return;
    }
    if (layer->download.active)
    {
        ui_toast(ui, mono, "Starlink: already downloading");
        return;
    }

    double age = feed_age_hours(FEED_STARLINK);
    if (age < 0.0)
    {
        ui_ask(ui, "Download Starlink data now (about 2 MB)? y/N");
    }
    else if (age < 48.0)
    {
        ui_ask(ui, "Refresh Starlink data (%.0f hours old, about 2 MB)? y/N", age);
    }
    else
    {
        ui_ask(ui, "Refresh Starlink data (%.0f days old, about 2 MB)? y/N", age / 24.0);
    }
}

void starlink_answer(struct StarlinkLayer *layer, struct Conf *config, struct UiState *ui, bool yes, double mono)
{
    bool have_data = layer->catalog.count > 0;

    if (!yes)
    {
        if (!have_data)
        {
            config->starlink = false;
            ui_toast(ui, mono, "Starlink: off (no data)");
        }
        else
        {
            ui_toast(ui, mono, "Keeping the cached Starlink data");
        }
        return;
    }

    double wait = 0.0;
    const char *problem = NULL;
    switch (feed_download_start(FEED_STARLINK, &layer->download, &wait))
    {
    case FEED_START_OK:
        ui_toast(ui, mono, "Downloading Starlink data in the background...");
        return;
    case FEED_START_TOO_SOON:
        ui_toast(ui, mono, "CelesTrak updates every 2 hours: try again in %.0f min", wait + 0.5);
        break;
    case FEED_START_NO_CURL:
        problem = "curl not found";
        break;
    case FEED_START_UNSUPPORTED:
        problem = "downloads are not supported on this system";
        break;
    case FEED_START_ERROR:
        problem = "could not start the download";
        break;
    }
    if (problem != NULL)
    {
        ui_toast(ui, mono, "Starlink: %s", problem);
    }
    if (!have_data)
    {
        config->starlink = false;
    }
}

void starlink_tick(struct StarlinkLayer *layer, struct Conf *config, struct UiState *ui, double jd, const double sun_dir[3],
                   double mono, double period)
{
    bool updated;
    char problem[96];
    if (feed_download_poll(&layer->download, &updated, problem, sizeof(problem)))
    {
        if (updated)
        {
            reload(layer);
            layer->report = config->starlink;
            ui_toast(ui, mono, "Starlink data updated");
        }
        else
        {
            ui_toast(ui, mono, "Starlink not updated: %s", problem);
            if (layer->catalog.count == 0)
            {
                config->starlink = false;
            }
        }
    }

    if (!config->starlink || mono - layer->updated_at < period)
    {
        return;
    }

    layer->updated_at = mono;
    layer->above = layer->sunlit = 0;
    int usable = 0;
    for (int i = 0; i < layer->catalog.count; ++i)
    {
        struct Satellite *sat = &layer->catalog.sats[i];
        satellite_update(sat, jd, config->latitude, config->longitude, sun_dir, config->vectors);
        usable += sat->ok;
        if (sat->ok && sat->altitude > 0.0)
        {
            layer->above++;
            layer->sunlit += sat->sunlit;
        }
    }

    // Elements more than two weeks from the date shown are hidden, not drawn
    // wrong: say so rather than report an empty sky
    if (layer->report && layer->catalog.count > 0 && usable == 0 && !ui->prompt_open)
    {
        ui_toast(ui, mono, "Starlink: the data does not cover this date (U to refresh)");
        layer->report = false;
        return;
    }

    // Report once there is something to report, without covering a question
    if (layer->report && layer->catalog.count > 0 && !ui->prompt_open)
    {
        double age_days = feed_age_hours(FEED_STARLINK) / 24.0;
        char age[32] = "";
        if (age_days >= 2.0)
        {
            snprintf(age, sizeof(age), " (data %.0f days old)", age_days);
        }
        ui_toast(ui, mono, "Starlink: %d sunlit of %d above the horizon%s", layer->sunlit, layer->above, age);
        layer->report = false;
    }
}

const char *starlink_status(const struct StarlinkLayer *layer)
{
    return layer->download.active ? "Starlink: downloading..." : NULL;
}

void starlink_free(struct StarlinkLayer *layer)
{
    feed_download_cancel(&layer->download);
    satellite_catalog_free(&layer->catalog);
    layer->loaded = false;
}
