/* Satellites: where they are in the observer's sky, whether the Sun lights
 * them, and when the next pass comes.
 *
 * Glossary:
 *  - above horizon: altitude > 0
 *  - sunlit: outside Earth's (cylindrical) shadow
 *  - observer dark: the Sun is below -6 degrees at the observer
 *  - visible pass: above 10 degrees while sunlit and the observer is dark
 *  - element age: |simulation time - element epoch|; sets older than
 *    SAT_MAX_ELEMENT_AGE_DAYS are hidden rather than drawn wrong
 */

#ifndef SATELLITE_H
#define SATELLITE_H

#include "macros.h"
#include "omm.h"
#include "sgp4.h"

#include <stdbool.h>

#define SAT_MAX_ELEMENT_AGE_DAYS 14.0
#define SAT_PASS_MIN_ALTITUDE (10.0 * M_PI / 180.0)
#define SAT_OBSERVER_DARK_SUN_ALT (-6.0 * M_PI / 180.0)
#define SAT_LOOKAHEAD_SECONDS 10.0

#define CATNR_ISS 25544
#define CATNR_TIANGONG 48274

struct Satellite
{
    long catnr;
    struct Sgp4State sgp4;
    bool usable; // Initialised without error

    // Updated by satellite_update()
    bool ok;              // Position valid at the last update (propagated, age within limit)
    double azimuth;       // Radians, from North through East
    double altitude;      // Radians
    bool sunlit;          // Outside Earth's shadow
    double ahead_azimuth; // Apparent position SAT_LOOKAHEAD_SECONDS later
    double ahead_altitude;
};

struct SatCatalog
{
    struct Satellite *sats;
    int count;
};

struct PassPrediction
{
    bool found;
    double start_jd;      // Start of the visible window (or rise when geometric)
    double start_azimuth; // Radians
    double peak_jd;
    double peak_altitude; // Radians
    double end_jd;
    double computed_at; // Simulation time the prediction was made from
};

/* Build a catalog from parsed records, keeping only the catalog numbers in
 * `only` (NULL keeps all). Returns false on allocation failure
 */
bool satellite_catalog_build(const struct OmmRecord *records, int count, const long *only, int num_only,
                             struct SatCatalog *out);

void satellite_catalog_free(struct SatCatalog *catalog);

/* Geocentric unit vector toward the Sun (equatorial frame) at a Julian date
 */
void sun_direction(double jd, double out[3]);

/* Sun altitude (radians) for an observer
 */
double sun_altitude(double jd, double latitude, double longitude);

/* Topocentric azimuth/altitude of a TEME position (km) for an observer on the
 * WGS-84 ellipsoid at sea level
 */
void teme_to_horizontal(const double r[3], double jd, double latitude, double longitude, double *azimuth, double *altitude);

/* True when a geocentric position (km) is outside Earth's cylindrical shadow
 */
bool is_sunlit(const double r[3], const double sun_dir[3]);

/* Propagate one satellite to `jd`: TEME position/velocity. Returns false on a
 * propagation error, non-finite output, or element age over the limit
 */
bool satellite_propagate(const struct Satellite *sat, double jd, double r[3], double v[3]);

/* Update position, sunlit state and (when `ahead`) the look-ahead position
 */
void satellite_update(struct Satellite *sat, double jd, double latitude, double longitude, const double sun_dir[3], bool ahead);

/* Number of SGP4 propagations performed so far (for cost tests)
 */
unsigned long long satellite_propagation_count(void);

/* Find the next pass starting after `jd_from` within `horizon_days`. With
 * `visible`, only a window where the satellite is sunlit, above the minimum
 * altitude and the observer is dark counts; otherwise the geometric pass above
 * the minimum altitude
 */
bool satellite_next_pass(const struct Satellite *sat, double jd_from, double horizon_days, double latitude, double longitude,
                         bool visible, struct PassPrediction *out);

/* Eight-point compass name for an azimuth, e.g. "NW"
 */
const char *compass_point(double azimuth);

#endif // SATELLITE_H
