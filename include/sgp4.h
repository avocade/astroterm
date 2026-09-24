/* Near-Earth SGP4 orbit propagation.
 *
 * A C transcription of the near-Earth branch of David Vallado's revised SGP4
 * ("Revisiting Spacetrack Report #3", AIAA 2006-6753), using WGS-72 constants
 * and the "improved" operation mode, as in the python-sgp4 reference that the
 * unit tests compare against. Deep-space orbits (period >= 225 minutes) are not
 * supported and are rejected at initialisation.
 *
 * References:  https://celestrak.org/publications/AIAA/2006-6753/
 *              https://pypi.org/project/sgp4/
 */

#ifndef SGP4_H
#define SGP4_H

#include <stdbool.h>

enum Sgp4Error
{
    SGP4_OK = 0,
    SGP4_ERR_ECCENTRICITY = 1, // Mean eccentricity out of range
    SGP4_ERR_MEAN_MOTION = 2,  // Mean motion less than zero
    SGP4_ERR_SEMI_LATUS = 4,   // Semi-latus rectum less than zero
    SGP4_ERR_DECAYED = 6,      // Satellite has decayed
    SGP4_ERR_DEEP_SPACE = 100, // Period >= 225 minutes: needs SDP4, unsupported
};

/* Mean orbital elements as read from a TLE, in SGP4 units
 */
struct Sgp4Elements
{
    double epoch_jd; // Julian date (UTC) of the element set epoch
    double bstar;    // Drag term                            (1/earth radii)
    double ecco;     // Eccentricity
    double inclo;    // Inclination                          (rad)
    double nodeo;    // Right ascension of ascending node    (rad)
    double argpo;    // Argument of perigee                  (rad)
    double mo;       // Mean anomaly                         (rad)
    double no_kozai; // Mean motion                          (rad/min)
};

/* Initialised propagator state. Treat as opaque.
 */
struct Sgp4State
{
    struct Sgp4Elements el;
    bool isimp;
    double no_unkozai, ao, con41, x1mth2, x7thm1, cosio, sinio;
    double eta, cc1, cc4, cc5, d2, d3, d4, delmo, sinmao;
    double mdot, argpdot, nodedot, nodecf, omgcof, xmcof, xlcof, aycof;
    double t2cof, t3cof, t4cof, t5cof;
    int error;
};

/* Initialise a propagator from mean elements. Returns SGP4_OK or an error code
 */
int sgp4_init(struct Sgp4State *state, const struct Sgp4Elements *elements);

/* Propagate to `tsince` minutes from epoch. Writes the TEME position (km) and
 * velocity (km/s). Returns SGP4_OK or an error code, in which case the output
 * must not be used
 */
int sgp4_propagate(const struct Sgp4State *state, double tsince, double r[3], double v[3]);

/* Greenwich mean sidereal time (IAU 1982 model, radians in [0, 2π)) used to
 * rotate TEME into an Earth-fixed frame
 */
double sgp4_gmst(double jd_ut1);

#endif // SGP4_H
