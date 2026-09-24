#include "core.h"
#include "core_position.h"
#include "data/keplerian_elements.h"
#include "omm.h"
#include "satellite.h"
#include "sgp4.h"
#include "unity.h"

#include "satellite_vectors.h"

#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void setUp(void)
{
}
void tearDown(void)
{
}

#define NUM_OMM_CASES (sizeof(omm_cases) / sizeof(omm_cases[0]))
#define NUM_PROP_CASES (sizeof(prop_cases) / sizeof(prop_cases[0]))
#define NUM_TOPO_CASES (sizeof(topo_cases) / sizeof(topo_cases[0]))
#define NUM_PASSES (sizeof(iss_passes) / sizeof(iss_passes[0]))

#define DEG (M_PI / 180.0)

// A faithful transcription agrees with the reference to well under a metre;
// anything looser would hide coefficient slips
#define POS_TOL_KM 1.0e-3
#define VEL_TOL_KMS 1.0e-6

/* Parse one case row (with the shared header) into a record
 */
static bool parse_case(int index, struct OmmRecord *out)
{
    char csv[1024];
    snprintf(csv, sizeof(csv), "%s\n%s\n", omm_csv_header, omm_cases[index].row);
    struct OmmRecord *records = NULL;
    int n = omm_parse_csv(csv, strlen(csv), &records);
    if (n == 1)
    {
        *out = records[0];
    }
    free(records);
    return n == 1;
}

static void satellite_from_case(int index, struct Satellite *sat)
{
    struct OmmRecord rec;
    TEST_ASSERT_TRUE(parse_case(index, &rec));
    memset(sat, 0, sizeof(*sat));
    sat->catnr = rec.catnr;
    sat->usable = sgp4_init(&sat->sgp4, &rec.elements) == SGP4_OK;
}

// OMM parsing

void test_omm_parses_every_case(void)
{
    for (size_t i = 0; i < NUM_OMM_CASES; ++i)
    {
        struct OmmRecord rec;
        TEST_ASSERT_TRUE_MESSAGE(parse_case((int)i, &rec), omm_cases[i].row);
        TEST_ASSERT_EQUAL_INT64(omm_cases[i].catnr, rec.catnr);
        TEST_ASSERT_DOUBLE_WITHIN(1.0e-9, omm_cases[i].epoch_jd, rec.elements.epoch_jd);
    }
}

void test_omm_six_digit_catalog_number(void)
{
    // Catalog numbers passed 99999 in July 2026; TLE cannot carry them
    struct OmmRecord rec;
    TEST_ASSERT_TRUE(parse_case(4, &rec));
    TEST_ASSERT_EQUAL_INT64(100465, rec.catnr);
}

void test_omm_ignores_locale(void)
{
    // The app calls setlocale(LC_ALL, ""): under a comma locale strtod() would
    // stop at the '.', silently turning 51.6318 into 51
    const char *locales[] = {"sv_SE.UTF-8", "sv_SE", "de_DE.UTF-8", "de_DE"};
    const char *set = NULL;
    for (unsigned int i = 0; i < 4 && set == NULL; ++i)
    {
        set = setlocale(LC_ALL, locales[i]);
    }

    struct OmmRecord rec;
    TEST_ASSERT_TRUE(parse_case(0, &rec));
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 51.6318 * DEG, rec.elements.inclo);
    setlocale(LC_ALL, "C");

    if (set == NULL)
    {
        TEST_IGNORE_MESSAGE("no comma locale installed");
    }
}

void test_omm_numbers(void)
{
    double v;
    TEST_ASSERT_TRUE(omm_parse_number(".27596078E-3", 12, &v));
    TEST_ASSERT_EQUAL_DOUBLE(0.27596078e-3, v);
    TEST_ASSERT_TRUE(omm_parse_number("-11606E-4", 9, &v));
    TEST_ASSERT_EQUAL_DOUBLE(-1.1606, v);
    TEST_ASSERT_TRUE(omm_parse_number(" 15.49258637 ", 13, &v));
    TEST_ASSERT_EQUAL_DOUBLE(15.49258637, v);

    TEST_ASSERT_FALSE(omm_parse_number("nan", 3, &v));
    TEST_ASSERT_FALSE(omm_parse_number("inf", 3, &v));
    TEST_ASSERT_FALSE(omm_parse_number("", 0, &v));
    TEST_ASSERT_FALSE(omm_parse_number("1.2.3", 5, &v));
    TEST_ASSERT_FALSE(omm_parse_number("1e", 2, &v));
    TEST_ASSERT_FALSE(omm_parse_number("1e999", 5, &v));
}

void test_omm_skips_bad_rows_and_reads_quotes(void)
{
    const char *csv = "OBJECT_NAME,EPOCH,MEAN_MOTION,ECCENTRICITY,INCLINATION,RA_OF_ASC_NODE,ARG_OF_PERICENTER,"
                      "MEAN_ANOMALY,NORAD_CAT_ID,BSTAR\r\n"
                      "\"QUOTED, NAME \"\"X\"\"\",2026-09-24T03:24:21.452544,15.49258637,.0004691,51.6318,170.3464,"
                      "174.6338,185.4701,25544,.18116E-3\r\n"
                      "BAD ECC,2026-09-24T03:24:21.452544,15.49,nan,51.6,170.3,174.6,185.4,25545,.18E-3\r\n"
                      "BAD EPOCH,2026-13-24T03:24:21,15.49,.0004,51.6,170.3,174.6,185.4,25546,.18E-3\r\n"
                      "HYPERBOLIC,2026-09-24T03:24:21,15.49,1.5,51.6,170.3,174.6,185.4,25547,.18E-3\r\n"
                      "SHORT,2026-09-24T03:24:21\r\n";
    struct OmmRecord *records = NULL;
    int n = omm_parse_csv(csv, strlen(csv), &records);
    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_INT64(25544, records[0].catnr);
    free(records);

    // No header: not a feed
    TEST_ASSERT_EQUAL_INT(-1, omm_parse_csv("<html>blocked</html>\n", 21, &records));
}

// SGP4 against python-sgp4, including Vallado's verification cases

void test_sgp4_matches_reference(void)
{
    char msg[160];
    for (size_t i = 0; i < NUM_PROP_CASES; ++i)
    {
        const struct PropCase *pc = &prop_cases[i];
        struct OmmRecord rec;
        TEST_ASSERT_TRUE(parse_case(pc->sat, &rec));

        struct Sgp4State state;
        sgp4_init(&state, &rec.elements);

        double r[3], v[3];
        int err = sgp4_propagate(&state, pc->tsince, r, v);

        snprintf(msg, sizeof(msg), "case %d (%.14s) at tsince %.4f", pc->sat, omm_cases[pc->sat].row, pc->tsince);
        TEST_ASSERT_EQUAL_INT_MESSAGE(pc->error, err, msg);
        if (pc->error != SGP4_OK && pc->error != SGP4_ERR_DECAYED)
        {
            continue;
        }
        for (int k = 0; k < 3; ++k)
        {
            TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(POS_TOL_KM, pc->r[k], r[k], msg);
            TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(VEL_TOL_KMS, pc->v[k], v[k], msg);
        }
    }
}

void test_deep_space_rejected(void)
{
    struct Sgp4Elements geo = {
        .epoch_jd = 2461308.0, .bstar = 0.0, .ecco = 0.0002, .inclo = 0.1 * DEG, .nodeo = 0.0, .argpo = 0.0,
        .mo = 0.0, .no_kozai = 1.0027 * 2.0 * M_PI / 1440.0, // ~1 rev/day
    };
    struct Sgp4State state;
    TEST_ASSERT_EQUAL_INT(SGP4_ERR_DEEP_SPACE, sgp4_init(&state, &geo));
    double r[3], v[3];
    TEST_ASSERT_EQUAL_INT(SGP4_ERR_DEEP_SPACE, sgp4_propagate(&state, 0.0, r, v));
}

void test_gmst_at_j2000(void)
{
    // GMST at 2000-01-01 12:00 UT1 is 18h 41m 50.54841s = 280.46061837 degrees
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 280.46061837 * DEG, sgp4_gmst(2451545.0));
}

// Topocentric position against Skyfield

void test_topocentric_matches_skyfield(void)
{
    char msg[96];
    for (size_t i = 0; i < NUM_TOPO_CASES; ++i)
    {
        const struct TopoCase *tc = &topo_cases[i];
        struct Satellite sat;
        satellite_from_case(tc->sat, &sat);

        double r[3], v[3], az, alt;
        TEST_ASSERT_TRUE(satellite_propagate(&sat, tc->jd_utc, r, v));
        teme_to_horizontal(r, tc->jd_utc, obs_lat_deg * DEG, obs_lon_deg * DEG, &az, &alt);

        snprintf(msg, sizeof(msg), "sat %d at jd %.6f", tc->sat, tc->jd_utc);
        TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(0.05, tc->alt_deg, alt / DEG, msg);

        double daz = fmod(fabs(az / DEG - tc->az_deg), 360.0);
        daz = daz > 180.0 ? 360.0 - daz : daz;
        TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(0.05, 0.0, daz * cos(alt), msg); // Azimuth error on the sky
    }
}

// Passes against Skyfield's find_events (geometric, above 10 degrees)

void test_passes_match_skyfield(void)
{
    struct Satellite iss;
    satellite_from_case(0, &iss);

    double from = omm_cases[0].epoch_jd;
    for (size_t i = 0; i < NUM_PASSES; ++i)
    {
        struct PassPrediction p;
        TEST_ASSERT_TRUE(satellite_next_pass(&iss, from, 3.0, obs_lat_deg * DEG, obs_lon_deg * DEG, false, &p));
        TEST_ASSERT_DOUBLE_WITHIN(20.0 / 86400.0, iss_passes[i].rise_jd, p.start_jd);
        TEST_ASSERT_DOUBLE_WITHIN(20.0 / 86400.0, iss_passes[i].culminate_jd, p.peak_jd);
        TEST_ASSERT_DOUBLE_WITHIN(20.0 / 86400.0, iss_passes[i].set_jd, p.end_jd);
        TEST_ASSERT_DOUBLE_WITHIN(0.5, iss_passes[i].culminate_alt_deg, p.peak_altitude / DEG);
        from = p.end_jd;
    }
}

void test_visible_pass_is_dark_and_sunlit(void)
{
    struct Satellite iss;
    satellite_from_case(0, &iss);

    struct PassPrediction p;
    double lat = obs_lat_deg * DEG, lon = obs_lon_deg * DEG;
    if (!satellite_next_pass(&iss, omm_cases[0].epoch_jd, 3.0, lat, lon, true, &p))
    {
        TEST_IGNORE_MESSAGE("no visible pass in the window");
    }

    double mid = 0.5 * (p.start_jd + p.end_jd);
    double r[3], v[3], s[3];
    TEST_ASSERT_TRUE(satellite_propagate(&iss, mid, r, v));
    sun_direction(mid, s);
    TEST_ASSERT_TRUE(is_sunlit(r, s));
    TEST_ASSERT_TRUE(sun_altitude(mid, lat, lon) < SAT_OBSERVER_DARK_SUN_ALT);
    TEST_ASSERT_TRUE(p.peak_altitude > SAT_PASS_MIN_ALTITUDE);
}

// Shadow, element age and cost

void test_shadow(void)
{
    double sun[3] = {1.0, 0.0, 0.0};
    double day[3] = {6928.0, 0.0, 0.0};
    double night[3] = {-6928.0, 0.0, 0.0};       // 550 km on the anti-solar line
    double beside[3] = {-6928.0, 6500.0, 0.0};   // Behind Earth but outside the cylinder
    double terminator[3] = {0.0, 0.0, 6928.0};   // Over the pole at the terminator
    TEST_ASSERT_TRUE(is_sunlit(day, sun));
    TEST_ASSERT_FALSE(is_sunlit(night, sun));
    TEST_ASSERT_TRUE(is_sunlit(beside, sun));
    TEST_ASSERT_TRUE(is_sunlit(terminator, sun));
}

void test_stale_elements_hidden(void)
{
    struct Satellite iss;
    satellite_from_case(0, &iss);
    double r[3], v[3];
    double epoch = omm_cases[0].epoch_jd;
    TEST_ASSERT_TRUE(satellite_propagate(&iss, epoch + 13.0, r, v));
    TEST_ASSERT_FALSE(satellite_propagate(&iss, epoch + 15.0, r, v));
    TEST_ASSERT_FALSE(satellite_propagate(&iss, epoch - 30.0, r, v)); // --datetime before launch data
}

void test_lookahead_uses_no_extra_propagation(void)
{
    struct Satellite iss;
    satellite_from_case(0, &iss);
    double sun[3];
    sun_direction(topo_cases[0].jd_utc, sun);

    unsigned long long before = satellite_propagation_count();
    satellite_update(&iss, topo_cases[0].jd_utc, obs_lat_deg * DEG, obs_lon_deg * DEG, sun, true);
    TEST_ASSERT_EQUAL_UINT64(before + 1, satellite_propagation_count());
    TEST_ASSERT_TRUE(iss.ok);

    // The look-ahead matches a real propagation 10 s later to a few hundredths
    // of a degree
    double r[3], v[3], az, alt;
    double later = topo_cases[0].jd_utc + SAT_LOOKAHEAD_SECONDS / 86400.0;
    TEST_ASSERT_TRUE(satellite_propagate(&iss, later, r, v));
    teme_to_horizontal(r, later, obs_lat_deg * DEG, obs_lon_deg * DEG, &az, &alt);
    TEST_ASSERT_DOUBLE_WITHIN(0.05 * DEG, alt, iss.ahead_altitude);
}

// Body vectors show motion against the stars

static double separation(double ra0, double dec0, double ra1, double dec1)
{
    return acos(sin(dec0) * sin(dec1) + cos(dec0) * cos(dec1) * cos(ra1 - ra0));
}

void test_moon_drifts_east_about_13_degrees_a_day(void)
{
    struct Moon moon;
    TEST_ASSERT_TRUE(generate_moon_object(&moon, &moon_elements, &moon_rates));

    double jd = 2461308.0; // 2026-09-24 12:00 UTC
    double ra0, dec0, ra1, dec1;
    moon_equatorial(&moon, jd, &ra0, &dec0);
    moon_equatorial(&moon, jd + 1.0, &ra1, &dec1);

    double sep = separation(ra0, dec0, ra1, dec1) / DEG;
    TEST_ASSERT_TRUE_MESSAGE(sep > 11.0 && sep < 15.5, "Moon moves 11-15 degrees a day against the stars");
    TEST_ASSERT_TRUE(sin(ra1 - ra0) > 0.0); // Eastward: right ascension grows
}

void test_saturn_is_retrograde_before_opposition(void)
{
    // Saturn reaches opposition on 2026-10-04 and moves westward around it
    struct Planet *planets = NULL;
    TEST_ASSERT_TRUE(generate_planet_table(&planets, planet_elements, planet_rates, planet_extras));

    double jd = 2461308.0;
    double ra0, dec0, ra1, dec1;
    planet_equatorial(planets, SATURN, jd, &ra0, &dec0);
    planet_equatorial(planets, SATURN, jd + 1.0, &ra1, &dec1);
    TEST_ASSERT_TRUE(sin(ra1 - ra0) < 0.0);

    // ...while the Sun moves east about a degree a day
    planet_equatorial(planets, SUN, jd, &ra0, &dec0);
    planet_equatorial(planets, SUN, jd + 1.0, &ra1, &dec1);
    TEST_ASSERT_DOUBLE_WITHIN(0.1, 0.99, separation(ra0, dec0, ra1, dec1) / DEG);
    TEST_ASSERT_TRUE(sin(ra1 - ra0) > 0.0);
    free_planets(planets, NUM_PLANETS);
}

void test_compass(void)
{
    TEST_ASSERT_EQUAL_STRING("N", compass_point(0.0));
    TEST_ASSERT_EQUAL_STRING("NE", compass_point(45.0 * DEG));
    TEST_ASSERT_EQUAL_STRING("W", compass_point(270.0 * DEG));
    TEST_ASSERT_EQUAL_STRING("N", compass_point(350.0 * DEG));
    TEST_ASSERT_EQUAL_STRING("NW", compass_point(-45.0 * DEG));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_omm_parses_every_case);
    RUN_TEST(test_omm_six_digit_catalog_number);
    RUN_TEST(test_omm_ignores_locale);
    RUN_TEST(test_omm_numbers);
    RUN_TEST(test_omm_skips_bad_rows_and_reads_quotes);
    RUN_TEST(test_sgp4_matches_reference);
    RUN_TEST(test_deep_space_rejected);
    RUN_TEST(test_gmst_at_j2000);
    RUN_TEST(test_topocentric_matches_skyfield);
    RUN_TEST(test_passes_match_skyfield);
    RUN_TEST(test_visible_pass_is_dark_and_sunlit);
    RUN_TEST(test_shadow);
    RUN_TEST(test_stale_elements_hidden);
    RUN_TEST(test_lookahead_uses_no_extra_propagation);
    RUN_TEST(test_moon_drifts_east_about_13_degrees_a_day);
    RUN_TEST(test_saturn_is_retrograde_before_opposition);
    RUN_TEST(test_compass);

    return UNITY_END();
}
