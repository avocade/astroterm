#include "satellite.h"

#include "astro.h"
#include "coord.h"
#include "data/keplerian_elements.h"
#include "macros.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

// WGS-84 ellipsoid for the observer
#define WGS84_A 6378.137
#define WGS84_F (1.0 / 298.257223563)

// Earth radius for the shadow cylinder (km)
#define SHADOW_RADIUS 6378.137

#define SECONDS_PER_DAY 86400.0
#define PASS_STEP_DAYS (20.0 / SECONDS_PER_DAY)
#define PASS_TOLERANCE_DAYS (1.0 / SECONDS_PER_DAY)

static unsigned long long propagation_count = 0;

unsigned long long satellite_propagation_count(void)
{
    return propagation_count;
}

bool satellite_catalog_build(const struct OmmRecord *records, int count, const long *only, int num_only, struct SatCatalog *out)
{
    out->sats = calloc(count > 0 ? count : 1, sizeof(struct Satellite));
    out->count = 0;
    if (out->sats == NULL)
    {
        return false;
    }

    for (int i = 0; i < count; ++i)
    {
        bool wanted = only == NULL;
        for (int k = 0; !wanted && k < num_only; ++k)
        {
            wanted = records[i].catnr == only[k];
        }
        if (!wanted)
        {
            continue;
        }

        struct Satellite *sat = &out->sats[out->count];
        sat->catnr = records[i].catnr;
        sat->usable = sgp4_init(&sat->sgp4, &records[i].elements) == SGP4_OK;
        if (sat->usable)
        {
            out->count++;
        }
    }
    return true;
}

void satellite_catalog_free(struct SatCatalog *catalog)
{
    free(catalog->sats);
    catalog->sats = NULL;
    catalog->count = 0;
}

void sun_direction(double jd, double out[3])
{
    // The Sun's geocentric position is the negated heliocentric position of
    // the Earth-Moon barycenter
    double x, y, z;
    calc_planet_helio_ICRF(&planet_elements[EARTH], &planet_rates[EARTH], NULL, jd, &x, &y, &z);
    double norm = sqrt(x * x + y * y + z * z);
    out[0] = -x / norm;
    out[1] = -y / norm;
    out[2] = -z / norm;
}

double sun_altitude(double jd, double latitude, double longitude)
{
    double s[3];
    sun_direction(jd, s);

    double ra, dec, az, alt;
    equatorial_rectangular_to_spherical(s[0], s[1], s[2], &ra, &dec);
    equatorial_to_horizontal(ra, dec, greenwich_mean_sidereal_time_rad(jd), latitude, longitude, &az, &alt);
    return alt;
}

void teme_to_horizontal(const double r[3], double jd, double latitude, double longitude, double *azimuth, double *altitude)
{
    // TEME is referred to the mean equinox of date, so GMST rotates it into an
    // Earth-fixed frame (polar motion and UT1-UTC are ignored)
    double gmst = sgp4_gmst(jd);

    // Observer position on the WGS-84 ellipsoid, rotated into TEME
    double e2 = WGS84_F * (2.0 - WGS84_F);
    double sin_lat = sin(latitude);
    double n = WGS84_A / sqrt(1.0 - e2 * sin_lat * sin_lat);
    double theta = gmst + longitude;
    double obs[3] = {
        n * cos(latitude) * cos(theta),
        n * cos(latitude) * sin(theta),
        n * (1.0 - e2) * sin_lat,
    };

    // Topocentric direction; the ellipsoid normal has declination = geodetic
    // latitude, so the usual equatorial-to-horizontal formulas apply
    double rho[3] = {r[0] - obs[0], r[1] - obs[1], r[2] - obs[2]};
    double ra, dec;
    equatorial_rectangular_to_spherical(rho[0], rho[1], rho[2], &ra, &dec);
    equatorial_to_horizontal(ra, dec, gmst, latitude, longitude, azimuth, altitude);
}

bool is_sunlit(const double r[3], const double s[3])
{
    double along = r[0] * s[0] + r[1] * s[1] + r[2] * s[2];
    if (along >= 0.0)
    {
        return true; // On the day side of the terminator plane
    }
    double px = r[0] - along * s[0];
    double py = r[1] - along * s[1];
    double pz = r[2] - along * s[2];
    return sqrt(px * px + py * py + pz * pz) > SHADOW_RADIUS;
}

bool satellite_propagate(const struct Satellite *sat, double jd, double r[3], double v[3])
{
    if (!sat->usable || fabs(jd - sat->sgp4.el.epoch_jd) > SAT_MAX_ELEMENT_AGE_DAYS)
    {
        return false;
    }

    propagation_count++;
    double tsince = (jd - sat->sgp4.el.epoch_jd) * 1440.0;
    if (sgp4_propagate(&sat->sgp4, tsince, r, v) != SGP4_OK)
    {
        return false;
    }
    for (int k = 0; k < 3; ++k)
    {
        if (!isfinite(r[k]) || !isfinite(v[k]))
        {
            return false;
        }
    }
    return true;
}

void satellite_update(struct Satellite *sat, double jd, double latitude, double longitude, const double sun_dir[3], bool ahead)
{
    double r[3], v[3];
    sat->ok = satellite_propagate(sat, jd, r, v);
    if (!sat->ok)
    {
        return;
    }

    teme_to_horizontal(r, jd, latitude, longitude, &sat->azimuth, &sat->altitude);
    sat->sunlit = is_sunlit(r, sun_dir);

    if (ahead)
    {
        // Linear extrapolation along the velocity: accurate to well under a
        // cell over a few seconds, and costs no second propagation
        double r2[3];
        for (int k = 0; k < 3; ++k)
        {
            r2[k] = r[k] + v[k] * SAT_LOOKAHEAD_SECONDS;
        }
        teme_to_horizontal(r2, jd + SAT_LOOKAHEAD_SECONDS / SECONDS_PER_DAY, latitude, longitude, &sat->ahead_azimuth,
                           &sat->ahead_altitude);
    }
}

/* Altitude at jd, or -pi/2 when the position is unavailable
 */
static double altitude_at(const struct Satellite *sat, double jd, double lat, double lon, double r_out[3])
{
    double r[3], v[3], az, alt;
    if (!satellite_propagate(sat, jd, r, v))
    {
        return -M_PI / 2.0;
    }
    teme_to_horizontal(r, jd, lat, lon, &az, &alt);
    if (r_out != NULL)
    {
        memcpy(r_out, r, sizeof(r));
    }
    return alt;
}

static bool pass_condition(const struct Satellite *sat, double jd, double lat, double lon, bool visible)
{
    double r[3];
    if (altitude_at(sat, jd, lat, lon, r) <= SAT_PASS_MIN_ALTITUDE)
    {
        return false;
    }
    if (!visible)
    {
        return true;
    }
    double s[3];
    sun_direction(jd, s);
    return is_sunlit(r, s) && sun_altitude(jd, lat, lon) < SAT_OBSERVER_DARK_SUN_ALT;
}

/* Bisect the instant the condition flips between a (value a_state) and b
 */
static double bisect(const struct Satellite *sat, double a, double b, bool a_state, double lat, double lon, bool visible)
{
    while (b - a > PASS_TOLERANCE_DAYS)
    {
        double mid = 0.5 * (a + b);
        if (pass_condition(sat, mid, lat, lon, visible) == a_state)
        {
            a = mid;
        }
        else
        {
            b = mid;
        }
    }
    return 0.5 * (a + b);
}

bool satellite_next_pass(const struct Satellite *sat, double jd_from, double horizon_days, double lat, double lon, bool visible,
                         struct PassPrediction *out)
{
    memset(out, 0, sizeof(*out));
    out->computed_at = jd_from;

    double t = jd_from;
    bool prev = pass_condition(sat, t, lat, lon, visible);
    double stop = jd_from + horizon_days;

    while (t < stop)
    {
        double t2 = t + PASS_STEP_DAYS;
        bool curr = pass_condition(sat, t2, lat, lon, visible);
        if (!prev && curr)
        {
            double start = bisect(sat, t, t2, false, lat, lon, visible);

            // Walk to the end of the window
            double e = t2;
            while (pass_condition(sat, e + PASS_STEP_DAYS, lat, lon, visible) && e < stop + 1.0)
            {
                e += PASS_STEP_DAYS;
            }
            double end = bisect(sat, e, e + PASS_STEP_DAYS, true, lat, lon, visible);

            // Altitude is unimodal within a pass: ternary search for its peak
            double lo = start, hi = end;
            while (hi - lo > PASS_TOLERANCE_DAYS)
            {
                double m1 = lo + (hi - lo) / 3.0;
                double m2 = hi - (hi - lo) / 3.0;
                if (altitude_at(sat, m1, lat, lon, NULL) < altitude_at(sat, m2, lat, lon, NULL))
                {
                    lo = m1;
                }
                else
                {
                    hi = m2;
                }
            }

            double r[3], v[3], az, alt;
            out->found = true;
            out->start_jd = start;
            out->end_jd = end;
            out->peak_jd = 0.5 * (lo + hi);
            out->peak_altitude = altitude_at(sat, out->peak_jd, lat, lon, NULL);
            if (satellite_propagate(sat, start, r, v))
            {
                teme_to_horizontal(r, start, lat, lon, &az, &alt);
                out->start_azimuth = az;
            }
            return true;
        }
        prev = curr;
        t = t2;
    }
    return false;
}

const char *compass_point(double azimuth)
{
    static const char *points[8] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    double deg = fmod(azimuth * 180.0 / M_PI, 360.0);
    if (deg < 0.0)
    {
        deg += 360.0;
    }
    return points[(int)floor((deg + 22.5) / 45.0) % 8];
}
