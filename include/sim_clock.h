/* Simulation clock.
 *
 * Simulation time is derived from the system's realtime clock instead of being
 * accumulated frame by frame, so slow frames, a suspended process or a sleeping
 * laptop cannot make the sky fall behind (see upstream issue #77):
 *
 *     jd = anchor_jd + (wall - anchor_wall) * speed / 86400
 *
 * Every change of speed or pause state re-anchors at the current instant. All
 * functions take the wall time as a parameter so they can be tested without
 * waiting.
 */

#ifndef SIM_CLOCK_H
#define SIM_CLOCK_H

#include <stdbool.h>

struct SimClock
{
    double anchor_jd;   // Simulation Julian date at the anchor
    double anchor_wall; // Wall time (seconds, Unix epoch) at the anchor
    double speed;       // Simulation seconds per wall second
    bool paused;
};

/* Seconds since the Unix epoch from the system realtime clock
 */
double clock_realtime_s(void);

/* Seconds from an arbitrary origin on a monotonic clock, for timers
 */
double clock_monotonic_s(void);

/* Julian date of a Unix time in seconds
 */
double unix_to_julian_date(double unix_s);

void sim_clock_init(struct SimClock *clock, double jd, double wall, double speed);

/* Simulation Julian date at wall time `wall`
 */
double sim_clock_jd(const struct SimClock *clock, double wall);

void sim_clock_set_paused(struct SimClock *clock, bool paused, double wall);

/* Step to the next faster (direction > 0) or slower (direction < 0) rung of the
 * speed ladder 1, 10, 60, 600, 3600. A speed off the ladder moves to the next
 * rung in that direction
 */
void sim_clock_step_speed(struct SimClock *clock, int direction, double wall);

/* Jump to the present at realtime speed and unpause
 */
void sim_clock_now(struct SimClock *clock, double wall);

#endif // SIM_CLOCK_H
