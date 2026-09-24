/* Core functions for rendering
 */

#ifndef CORE_RENDER_H
#define CORE_RENDER_H

#include "core.h"
#include "drawing.h"
#include "satellite.h"
#include "term.h"

#include <curses.h>

/* Render stars to the screen using a stereographic projection
 */
void render_stars_stereo(WINDOW *win, const struct Conf *config, struct Star *star_table, int num_stars, const int *num_by_mag);

/* Render the Sun and planets to the screen using a stereographic projection
 */
void render_planets_stereo(WINDOW *win, const struct Conf *config, const struct Planet *planet_table);

/* Render the Moon to the screen using a stereographic projection
 */
void render_moon_stereo(WINDOW *win, const struct Conf *config, struct Moon moon_object);

/* Render constellations
 */
void render_constells(WINDOW *win, const struct Conf *config, struct Constell **constell_table, int num_const,
                      const struct Star *star_table);

/* Render space stations (ISS, Tiangong) above the horizon, dimmed in shadow
 */
void render_stations(WINDOW *win, const struct Conf *config, const struct SatCatalog *stations);

/* Project a horizontal position to fractional braille-dot coordinates (4 dot
 * rows and 2 dot columns per cell). Returns false outside the dome
 */
bool horizontal_to_dots(WINDOW *win, const struct Conf *config, double azimuth, double altitude, int *dot_row,
                        int *dot_col);

/* Render Starlink satellites above the horizon as single braille dots (Unicode)
 * or ',' (ASCII). Sunlit ones always; eclipsed ones when starlink_dark is on.
 * Drawn first, so every other layer covers them
 */
void render_starlink(WINDOW *win, const struct Conf *config, const struct SatCatalog *starlink,
                     struct BrailleCanvas *lit, struct BrailleCanvas *dark);

/* Render motion vectors: satellites to where they will be SAT_LOOKAHEAD_SECONDS
 * later (apparent motion), the Sun, Moon and planets to where they will be
 * among the stars a day later (their own motion; the daily rotation every star
 * shares is left out). Dim, beneath every glyph, capped at 30 degrees of arc
 */
void render_vectors(WINDOW *win, const struct Conf *config, double julian_date, const struct Planet *planet_table,
                    const struct Moon *moon_object, const struct SatCatalog *stations, const struct SatCatalog *starlink,
                    struct BrailleCanvas *canvas);

/* Render an azimuthal grid on a stereographic projection
 */
void render_azimuthal_grid(WINDOW *win, const struct Conf *config);

/* Render cardinal direction indicators for the Northern, Eastern, Southern, and
 * Western horizons
 */
void render_cardinal_directions(WINDOW *win, const struct Conf *config);

#endif // CORE_RENDER_H
