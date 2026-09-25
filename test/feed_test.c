#include "feed.h"
#include "sat_layers.h"
#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32

#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

static char root[256];
static char bin[300];
static char log_path[300];
static char body_path[300];

#define HEADER                                                                                                                 \
    "OBJECT_NAME,OBJECT_ID,EPOCH,MEAN_MOTION,ECCENTRICITY,INCLINATION,RA_OF_ASC_NODE,ARG_OF_PERICENTER,MEAN_ANOMALY,"          \
    "EPHEMERIS_TYPE,CLASSIFICATION_TYPE,NORAD_CAT_ID,ELEMENT_SET_NO,REV_AT_EPOCH,BSTAR,MEAN_MOTION_DOT,"                       \
    "MEAN_MOTION_DDOT\n"
#define ROW(n) "SAT,X,2026-09-24T03:24:21.452544,15.49,.0004691,51.63,170.34,174.63,185.47,0,U," #n ",999,1,.18E-3,0,0\n"

/* Write the body the fake curl will serve: `rows` valid rows
 */
static void serve(int rows)
{
    FILE *f = fopen(body_path, "w");
    fputs(HEADER, f);
    for (int i = 0; i < rows; ++i)
    {
        fprintf(f, "SAT,X,2026-09-24T03:24:21.452544,15.49,.0004691,51.63,170.34,174.63,185.47,0,U,%d,999,1,.18E-3,0,0\n",
                1000 + i);
    }
    fclose(f);
}

static int curl_calls(void)
{
    FILE *f = fopen(log_path, "r");
    if (f == NULL)
    {
        return 0;
    }
    int n = 0, c;
    while ((c = fgetc(f)) != EOF)
    {
        n += c == '\n';
    }
    fclose(f);
    return n;
}

static void age_file(const char *name, double hours)
{
    char path[400];
    snprintf(path, sizeof(path), "%s/cache/astroterm/%s", root, name);
    struct utimbuf t;
    t.actime = t.modtime = time(NULL) - (time_t)(hours * 3600.0);
    utime(path, &t);
}

/* Both feeds in use (12 h cadence)
 */
static void refresh_in_use(char *msg, size_t len)
{
    double max_age[NUM_FEEDS] = {FEED_MAX_AGE_HOURS, FEED_MAX_AGE_HOURS};
    feed_refresh(max_age, msg, len);
}

void setUp(void)
{
    snprintf(root, sizeof(root), "/tmp/astroterm_feed_test_XXXXXX");
    TEST_ASSERT_NOT_NULL(mkdtemp(root));
    snprintf(bin, sizeof(bin), "%s/bin", root);
    mkdir(bin, 0700);
    snprintf(log_path, sizeof(log_path), "%s/curl.log", root);
    snprintf(body_path, sizeof(body_path), "%s/body.csv", root);

    // A fake curl: copies body.csv to the -o path, prints $FAKE_STATUS
    char script[400];
    snprintf(script, sizeof(script), "%s/curl", bin);
    FILE *f = fopen(script, "w");
    fprintf(f,
            "#!/bin/sh\n"
            "echo \"$@\" >> '%s'\n"
            "while [ $# -gt 0 ]; do [ \"$1\" = -o ] && out=\"$2\"; shift; done\n"
            "if [ \"${FAKE_EXIT:-0}\" != 0 ]; then printf 000; exit $FAKE_EXIT; fi\n" // Like curl: no reply
            "/bin/cp '%s' \"$out\"\n"
            "printf '%%s' \"${FAKE_STATUS:-200}\"\n",
            log_path, body_path);
    fclose(f);
    chmod(script, 0700);

    char cache[300];
    snprintf(cache, sizeof(cache), "%s/cache", root);
    setenv("XDG_CACHE_HOME", cache, 1);
    setenv("PATH", bin, 1);
    unsetenv("FAKE_STATUS");
    unsetenv("FAKE_EXIT");
    serve(10);
}

void tearDown(void)
{
    char cmd[400];
    snprintf(cmd, sizeof(cmd), "/bin/rm -rf '%s'", root);
    TEST_ASSERT_EQUAL_INT(0, system(cmd));
}

void test_cache_dir_is_private(void)
{
    char dir[400];
    TEST_ASSERT_TRUE(feed_cache_dir(dir, sizeof(dir)));
    TEST_ASSERT_NOT_NULL(strstr(dir, "/cache/astroterm"));
    struct stat st;
    TEST_ASSERT_EQUAL_INT(0, stat(dir, &st));
    TEST_ASSERT_EQUAL_INT(0700, st.st_mode & 0777);
}

void test_relative_xdg_is_ignored(void)
{
    setenv("XDG_CACHE_HOME", "relative/cache", 1);
    char home[300];
    snprintf(home, sizeof(home), "%s/home", root);
    mkdir(home, 0700);
    setenv("HOME", home, 1);
    char dir[400];
    TEST_ASSERT_TRUE(feed_cache_dir(dir, sizeof(dir)));
    TEST_ASSERT_NOT_NULL(strstr(dir, "/home/.cache/astroterm"));
}

void test_first_run_downloads_both_feeds(void)
{
    char msg[128];
    refresh_in_use(msg, sizeof(msg));
    TEST_ASSERT_EQUAL_STRING("", msg);
    TEST_ASSERT_EQUAL_INT(2, curl_calls());
    TEST_ASSERT_TRUE(feed_age_hours(FEED_STATIONS) >= 0.0);
    TEST_ASSERT_TRUE(feed_age_hours(FEED_STARLINK) >= 0.0);

    size_t len;
    char *data = feed_read(FEED_STARLINK, &len);
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_NOT_NULL(strstr(data, "NORAD_CAT_ID"));
    free(data);
}

void test_fresh_cache_is_not_refetched(void)
{
    char msg[128];
    refresh_in_use(msg, sizeof(msg));
    refresh_in_use(msg, sizeof(msg));
    TEST_ASSERT_EQUAL_INT(2, curl_calls());
}

void test_retry_waits_two_hours(void)
{
    char msg[128];
    refresh_in_use(msg, sizeof(msg));

    // Stale data but a recent attempt: CelesTrak would answer 403
    age_file("stations.csv", 13.0);
    age_file("starlink.csv", 13.0);
    age_file("stations.attempt", 1.0);
    age_file("starlink.attempt", 1.0);
    refresh_in_use(msg, sizeof(msg));
    TEST_ASSERT_EQUAL_INT(2, curl_calls());

    // Once the attempt is old enough, try again
    age_file("stations.attempt", 3.0);
    age_file("starlink.attempt", 3.0);
    refresh_in_use(msg, sizeof(msg));
    TEST_ASSERT_EQUAL_INT(4, curl_calls());
}

void test_rate_limited_keeps_cache(void)
{
    char msg[128];
    refresh_in_use(msg, sizeof(msg));
    age_file("stations.csv", 13.0);
    age_file("stations.attempt", 3.0);
    age_file("starlink.csv", 13.0);
    age_file("starlink.attempt", 3.0);

    serve(0);
    setenv("FAKE_STATUS", "403", 1);
    refresh_in_use(msg, sizeof(msg));
    TEST_ASSERT_NOT_NULL(strstr(msg, "not updated yet"));

    size_t len;
    char *data = feed_read(FEED_STATIONS, &len);
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_NOT_NULL(strstr(data, "1009")); // Still the 10-row feed
    free(data);
}

void test_truncated_download_rejected(void)
{
    char msg[128];
    refresh_in_use(msg, sizeof(msg));
    age_file("starlink.csv", 13.0);
    age_file("starlink.attempt", 3.0);

    serve(3); // Fewer than half of the 10 rows we have
    refresh_in_use(msg, sizeof(msg));
    TEST_ASSERT_NOT_NULL(strstr(msg, "incomplete"));

    size_t len;
    char *data = feed_read(FEED_STARLINK, &len);
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_NOT_NULL(strstr(data, "1009"));
    free(data);
}

void test_network_failure_stops_early(void)
{
    setenv("FAKE_EXIT", "6", 1); // Could not resolve host
    char msg[128];
    refresh_in_use(msg, sizeof(msg));
    TEST_ASSERT_NOT_NULL(strstr(msg, "no network"));
    TEST_ASSERT_EQUAL_INT(1, curl_calls()); // Did not also try the second feed
}

void test_offline_failure_does_not_block_retry(void)
{
    // Offline: curl never reached CelesTrak, so there is nothing to be polite
    // about, and reconnecting must allow an immediate retry
    setenv("FAKE_EXIT", "6", 1);
    char msg[128];
    refresh_in_use(msg, sizeof(msg));
    TEST_ASSERT_EQUAL_INT(1, curl_calls());

    unsetenv("FAKE_EXIT");
    refresh_in_use(msg, sizeof(msg));
    TEST_ASSERT_EQUAL_STRING("", msg);
    TEST_ASSERT_EQUAL_INT(3, curl_calls());
}

void test_starlink_kept_warm_every_two_weeks(void)
{
    // Stations in use, Starlink off: Starlink is fetched once, then only
    // after two weeks
    double max_age[NUM_FEEDS] = {FEED_MAX_AGE_HOURS, FEED_BACKGROUND_MAX_AGE_HOURS};
    char msg[128];
    feed_refresh(max_age, msg, sizeof(msg));
    TEST_ASSERT_EQUAL_INT(2, curl_calls()); // First run: nothing cached yet

    age_file("stations.csv", 13.0);
    age_file("stations.attempt", 3.0);
    age_file("starlink.csv", 13.0 * 24.0);
    age_file("starlink.attempt", 3.0);
    feed_refresh(max_age, msg, sizeof(msg));
    TEST_ASSERT_EQUAL_INT(3, curl_calls()); // Stations only: Starlink is 13 days old

    age_file("starlink.csv", 15.0 * 24.0);
    age_file("stations.attempt", 3.0);
    age_file("starlink.attempt", 3.0);
    feed_refresh(max_age, msg, sizeof(msg));
    TEST_ASSERT_EQUAL_INT(4, curl_calls()); // Starlink now past two weeks
}

void test_skipped_feed_is_never_fetched(void)
{
    double max_age[NUM_FEEDS] = {FEED_MAX_AGE_HOURS, FEED_SKIP};
    char msg[128];
    feed_refresh(max_age, msg, sizeof(msg));
    TEST_ASSERT_EQUAL_INT(1, curl_calls());
    TEST_ASSERT_TRUE(feed_age_hours(FEED_STARLINK) < 0.0);
}

/* Poll a background download until it finishes (the fake curl is quick)
 */
static bool finish(struct FeedDownload *dl, bool *updated, char *msg, size_t len)
{
    for (int i = 0; i < 500; ++i)
    {
        if (feed_download_poll(dl, updated, msg, len))
        {
            return true;
        }
        usleep(10000);
    }
    return false;
}

void test_background_download(void)
{
    struct FeedDownload dl = {0};
    double wait;
    TEST_ASSERT_EQUAL_INT(FEED_START_OK, feed_download_start(FEED_STARLINK, &dl, &wait));
    TEST_ASSERT_TRUE(dl.active);

    bool updated;
    char msg[96];
    TEST_ASSERT_TRUE(finish(&dl, &updated, msg, sizeof(msg)));
    TEST_ASSERT_TRUE(updated);
    TEST_ASSERT_FALSE(dl.active);
    TEST_ASSERT_TRUE(feed_age_hours(FEED_STARLINK) >= 0.0);

    // CelesTrak answered, so asking again right away would earn a 403
    TEST_ASSERT_EQUAL_INT(FEED_START_TOO_SOON, feed_download_start(FEED_STARLINK, &dl, &wait));
    TEST_ASSERT_TRUE(wait > 100.0 && wait <= 120.0);
}

void test_background_download_cancel(void)
{
    // A download that would take a minute, cancelled when the user quits
    char script[400];
    snprintf(script, sizeof(script), "%s/curl", bin);
    FILE *f = fopen(script, "w");
    fprintf(f, "#!/bin/sh\nsleep 60\n");
    fclose(f);

    struct FeedDownload dl = {0};
    double wait;
    TEST_ASSERT_EQUAL_INT(FEED_START_OK, feed_download_start(FEED_STARLINK, &dl, &wait));
    bool updated;
    char msg[96];
    TEST_ASSERT_FALSE(feed_download_poll(&dl, &updated, msg, sizeof(msg))); // Still running

    feed_download_cancel(&dl);
    TEST_ASSERT_FALSE(dl.active);
    TEST_ASSERT_EQUAL_INT(-1, access(dl.tmp, F_OK)); // Temporary file gone
    TEST_ASSERT_TRUE(feed_age_hours(FEED_STARLINK) < 0.0);
}

// Starlink is never downloaded without asking

static void write_cache(const char *name, int rows, double age_hours)
{
    char path[400];
    snprintf(path, sizeof(path), "%s/cache/astroterm", root);
    mkdir(path, 0700); // May already exist
    snprintf(path, sizeof(path), "%s/cache/astroterm/%s", root, name);
    FILE *f = fopen(path, "w");
    fputs(HEADER, f);
    for (int i = 0; i < rows; ++i)
    {
        fprintf(f, "SAT,X,2026-09-24T03:24:21.452544,15.49,.0004691,51.63,170.34,174.63,185.47,0,U,%d,999,1,.18E-3,0,0\n",
                1000 + i);
    }
    fclose(f);
    age_file(name, age_hours);
}

void test_starlink_cached_shown_without_asking(void)
{
    char dir[400];
    TEST_ASSERT_TRUE(feed_cache_dir(dir, sizeof(dir)));
    write_cache("starlink.csv", 5, 3.0 * 24.0);

    struct StarlinkLayer layer = {0};
    struct Conf config = {.starlink = true};
    struct UiState ui = {0};
    starlink_enable(&layer, &config, &ui, 0.0);
    TEST_ASSERT_EQUAL_INT(5, layer.catalog.count);
    TEST_ASSERT_FALSE(ui.prompt_open);
    TEST_ASSERT_EQUAL_INT(0, curl_calls());
    starlink_free(&layer);
}

void test_starlink_old_cache_asks_and_no_keeps_it(void)
{
    char dir[400];
    TEST_ASSERT_TRUE(feed_cache_dir(dir, sizeof(dir)));
    write_cache("starlink.csv", 5, 16.0 * 24.0);

    struct StarlinkLayer layer = {0};
    struct Conf config = {.starlink = true};
    struct UiState ui = {0};
    starlink_enable(&layer, &config, &ui, 0.0);
    TEST_ASSERT_EQUAL_INT(5, layer.catalog.count); // Shown at once
    TEST_ASSERT_TRUE(ui.prompt_open);
    TEST_ASSERT_NOT_NULL(strstr(ui.prompt, "16 days old"));

    starlink_answer(&layer, &config, &ui, false, 0.0);
    TEST_ASSERT_TRUE(config.starlink);
    TEST_ASSERT_EQUAL_INT(0, curl_calls());
    starlink_free(&layer);
}

void test_starlink_no_cache_asks_then_downloads(void)
{
    struct StarlinkLayer layer = {0};
    struct Conf config = {.starlink = true};
    struct UiState ui = {0};
    starlink_enable(&layer, &config, &ui, 0.0);
    TEST_ASSERT_TRUE(ui.prompt_open);
    TEST_ASSERT_EQUAL_INT(0, curl_calls());

    starlink_answer(&layer, &config, &ui, true, 0.0);
    TEST_ASSERT_TRUE(layer.download.active);
    bool updated;
    char msg[96];
    TEST_ASSERT_TRUE(finish(&layer.download, &updated, msg, sizeof(msg)));
    TEST_ASSERT_TRUE(updated);
    TEST_ASSERT_EQUAL_INT(1, curl_calls());
    starlink_free(&layer);
}

void test_starlink_offline_never_asks(void)
{
    struct StarlinkLayer layer = {0};
    struct Conf config = {.starlink = true, .offline = true};
    struct UiState ui = {0};
    starlink_enable(&layer, &config, &ui, 0.0);
    TEST_ASSERT_FALSE(ui.prompt_open);
    TEST_ASSERT_FALSE(config.starlink);
    TEST_ASSERT_EQUAL_INT(0, curl_calls());
}

void test_abandoned_temp_files_removed(void)
{
    char dir[400], old_tmp[500], new_tmp[500], other[500];
    TEST_ASSERT_TRUE(feed_cache_dir(dir, sizeof(dir)));
    snprintf(old_tmp, sizeof(old_tmp), "%s/stations.csv.AbC123", dir);
    snprintf(new_tmp, sizeof(new_tmp), "%s/starlink.csv.XyZ789", dir);
    snprintf(other, sizeof(other), "%s/notes.csv.AbC123", dir);
    fclose(fopen(old_tmp, "w"));
    fclose(fopen(new_tmp, "w"));
    fclose(fopen(other, "w"));
    age_file("stations.csv.AbC123", 1.0);
    age_file("notes.csv.AbC123", 1.0);

    double max_age[NUM_FEEDS] = {FEED_SKIP, FEED_SKIP};
    char msg[128];
    feed_refresh(max_age, msg, sizeof(msg));
    TEST_ASSERT_EQUAL_INT(-1, access(old_tmp, F_OK)); // Abandoned: removed
    TEST_ASSERT_EQUAL_INT(0, access(new_tmp, F_OK));  // Maybe in use: kept
    TEST_ASSERT_EQUAL_INT(0, access(other, F_OK));    // Not ours: kept
}

void test_curl_missing(void)
{
    setenv("PATH", "/nonexistent", 1);
    char msg[128];
    refresh_in_use(msg, sizeof(msg));
    TEST_ASSERT_NOT_NULL(strstr(msg, "curl not found"));
}

void test_curl_args_are_hardened(void)
{
    char msg[128];
    refresh_in_use(msg, sizeof(msg));
    FILE *f = fopen(log_path, "r");
    char line[1024];
    TEST_ASSERT_NOT_NULL(fgets(line, sizeof(line), f));
    fclose(f);
    TEST_ASSERT_EQUAL_INT(0, strncmp(line, "-q ", 3)); // ~/.curlrc ignored
    TEST_ASSERT_NOT_NULL(strstr(line, "--proto =https --proto-redir =https"));
    TEST_ASSERT_NOT_NULL(strstr(line, "--max-filesize"));
    TEST_ASSERT_NOT_NULL(strstr(line, "FORMAT=csv"));
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0); // Keep progress visible if a test crashes
    UNITY_BEGIN();

    RUN_TEST(test_cache_dir_is_private);
    RUN_TEST(test_relative_xdg_is_ignored);
    RUN_TEST(test_first_run_downloads_both_feeds);
    RUN_TEST(test_fresh_cache_is_not_refetched);
    RUN_TEST(test_retry_waits_two_hours);
    RUN_TEST(test_rate_limited_keeps_cache);
    RUN_TEST(test_truncated_download_rejected);
    RUN_TEST(test_network_failure_stops_early);
    RUN_TEST(test_offline_failure_does_not_block_retry);
    RUN_TEST(test_starlink_kept_warm_every_two_weeks);
    RUN_TEST(test_skipped_feed_is_never_fetched);
    RUN_TEST(test_background_download);
    RUN_TEST(test_background_download_cancel);
    RUN_TEST(test_starlink_cached_shown_without_asking);
    RUN_TEST(test_starlink_old_cache_asks_and_no_keeps_it);
    RUN_TEST(test_starlink_no_cache_asks_then_downloads);
    RUN_TEST(test_starlink_offline_never_asks);
    RUN_TEST(test_abandoned_temp_files_removed);
    RUN_TEST(test_curl_missing);
    RUN_TEST(test_curl_args_are_hardened);

    return UNITY_END();
}

#else

void setUp(void)
{
}
void tearDown(void)
{
}

int main(void)
{
    UNITY_BEGIN();
    return UNITY_END();
}

#endif
