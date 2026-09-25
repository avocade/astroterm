/* Best-effort observer location when none is given on the command line.
 *
 * Asks the operating system (macOS Location Services via the `CoreLocationCLI`
 * tool, when installed) with a short timeout, remembers each fix in the cache
 * directory, and falls back to the last remembered fix. Upstream astroterm
 * used 0°, 0° (the Gulf of Guinea) whenever no location was given.
 */

#ifndef GEOLOCATE_H
#define GEOLOCATE_H

#include <stdbool.h>
#include <stddef.h>

#define GEOLOCATE_TIMEOUT_SECONDS 3.0

struct GeoFix
{
    double latitude;  // Degrees north
    double longitude; // Degrees east
    bool remembered;  // From the cache rather than a fresh fix
    double age_hours; // Age of a remembered fix
};

/* Find the observer: a fresh fix if possible, else the remembered one. Returns
 * false when neither exists
 */
bool geolocate(struct GeoFix *fix);

/* Parse "<latitude> <longitude>" (degrees, east positive) as printed by
 * CoreLocationCLI. Returns false unless both are present and in range
 */
bool geolocate_parse(const char *text, double *latitude, double *longitude);

/* Name of the built-in city nearest to a location, and its distance
 */
bool nearest_city(double latitude, double longitude, char *name, size_t len, double *distance_km);

/* One line describing a fix for the launch toast, e.g.
 * "Location: near Kalmar (56.5° N, 16.6° E)"
 */
void geolocate_describe(const struct GeoFix *fix, char *buf, size_t len);

#endif // GEOLOCATE_H
