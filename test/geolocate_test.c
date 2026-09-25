#include "geolocate.h"
#include "sim_clock.h"
#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32

#include <sys/stat.h>
#include <unistd.h>

static char root[256];
static char bin[300];

/* Install a fake CoreLocationCLI running `body` (a shell snippet)
 */
static void fake_locator(const char *body)
{
    char path[400];
    snprintf(path, sizeof(path), "%s/CoreLocationCLI", bin);
    FILE *f = fopen(path, "w");
    fprintf(f, "#!/bin/sh\n%s\n", body);
    fclose(f);
    chmod(path, 0700);
}

void setUp(void)
{
    snprintf(root, sizeof(root), "/tmp/astroterm_geo_test_XXXXXX");
    TEST_ASSERT_NOT_NULL(mkdtemp(root));
    snprintf(bin, sizeof(bin), "%s/bin", root);
    mkdir(bin, 0700);
    char cache[300];
    snprintf(cache, sizeof(cache), "%s/cache", root);
    setenv("XDG_CACHE_HOME", cache, 1);
    setenv("PATH", bin, 1);
}

void tearDown(void)
{
    char cmd[400];
    snprintf(cmd, sizeof(cmd), "/bin/rm -rf '%s'", root);
    TEST_ASSERT_EQUAL_INT(0, system(cmd));
}

void test_parse(void)
{
    double lat, lon;
    TEST_ASSERT_TRUE(geolocate_parse("56.512421 16.598629\n", &lat, &lon));
    TEST_ASSERT_EQUAL_DOUBLE(56.512421, lat);
    TEST_ASSERT_EQUAL_DOUBLE(16.598629, lon); // East is positive, whatever the tool's help says
    TEST_ASSERT_TRUE(geolocate_parse("-33.8688 151.2093", &lat, &lon));

    TEST_ASSERT_FALSE(geolocate_parse("", &lat, &lon));
    TEST_ASSERT_FALSE(geolocate_parse("56.5", &lat, &lon));
    TEST_ASSERT_FALSE(geolocate_parse("91.0 10.0", &lat, &lon));
    TEST_ASSERT_FALSE(geolocate_parse("Location services are disabled", &lat, &lon));
}

void test_fresh_fix_is_remembered(void)
{
    fake_locator("echo '56.512421 16.598629'");
    struct GeoFix fix;
    TEST_ASSERT_TRUE(geolocate(&fix));
    TEST_ASSERT_FALSE(fix.remembered);
    TEST_ASSERT_EQUAL_DOUBLE(56.512421, fix.latitude);

    // The locator is gone (offline, uninstalled): the last fix is used
    fake_locator("exit 1");
    TEST_ASSERT_TRUE(geolocate(&fix));
    TEST_ASSERT_TRUE(fix.remembered);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 16.598629, fix.longitude);
    TEST_ASSERT_TRUE(fix.age_hours < 1.0);
}

void test_nothing_known(void)
{
    struct GeoFix fix;
    TEST_ASSERT_FALSE(geolocate(&fix)); // No locator on PATH, nothing remembered
}

void test_slow_locator_times_out(void)
{
    fake_locator("/bin/sleep 30; echo '56.5 16.6'");
    double start = clock_monotonic_s();
    struct GeoFix fix;
    TEST_ASSERT_FALSE(geolocate(&fix));
    TEST_ASSERT_TRUE(clock_monotonic_s() - start < GEOLOCATE_TIMEOUT_SECONDS + 1.0);
}

void test_garbage_is_ignored(void)
{
    fake_locator("echo 'kCLErrorDomain error 1'");
    struct GeoFix fix;
    TEST_ASSERT_FALSE(geolocate(&fix));
}

void test_nearest_city_and_description(void)
{
    char name[64];
    double km;
    TEST_ASSERT_TRUE(nearest_city(59.33, 18.07, name, sizeof(name), &km));
    TEST_ASSERT_EQUAL_STRING("Stockholm", name);
    TEST_ASSERT_TRUE(km < 5.0);

    struct GeoFix fix = {.latitude = 59.33, .longitude = 18.07};
    char text[128];
    geolocate_describe(&fix, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("Location: near Stockholm (59.3° N, 18.1° E)", text);

    // Mid-ocean: no city nearby, so no name
    struct GeoFix ocean = {.latitude = -40.0, .longitude = -120.0, .remembered = true, .age_hours = 72.0};
    geolocate_describe(&ocean, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("Location: last known 3 days ago, (40.0° S, 120.0° W)", text);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    UNITY_BEGIN();

    RUN_TEST(test_parse);
    RUN_TEST(test_fresh_fix_is_remembered);
    RUN_TEST(test_nothing_known);
    RUN_TEST(test_slow_locator_times_out);
    RUN_TEST(test_garbage_is_ignored);
    RUN_TEST(test_nearest_city_and_description);

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
