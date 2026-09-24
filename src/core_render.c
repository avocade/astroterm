#include "core_render.h"
#include "macros.h"

#include "astro.h"
#include "coord.h"
#include "core.h"
#include "core_position.h"
#include "drawing.h"
#include "term.h"

#include <curses.h>
#include <math.h>
#include <stdlib.h>

void horizontal_to_polar(const struct Conf *config, double azimuth, double altitude, double *radius, double *theta)
{
    double theta_sphere, phi_sphere;
    horizontal_to_spherical(azimuth, altitude, &theta_sphere, &phi_sphere);

    project_stereographic_north(1.0, theta_sphere, phi_sphere, radius, theta);
    *theta += config->rotation;

    return;
}

void render_object_stereo(WINDOW *win, struct ObjectBase *object, const struct Conf *config, enum RenderRole role)
{
    double radius_polar, theta_polar;
    horizontal_to_polar(config, object->azimuth, object->altitude, &radius_polar, &theta_polar);

    // If outside projection (or not a number), ignore
    if (!(fabs(radius_polar) <= 1.0) || !isfinite(theta_polar))
    {
        return;
    }

    int y, x;
    int height, width;
    getmaxyx(win, height, width);
    polar_to_win(radius_polar, theta_polar, height, width, &y, &x);

    attr_t attr = palette_attr(config->night, config->color, role, object->color_pair);

    // Draw object
    wattron(win, attr);
    if (config->unicode)
    {
        mvwaddstr(win, y, x, object->symbol_unicode);
    }
    else
    {
        mvwaddch(win, y, x, object->symbol_ASCII);
    }
    wattroff(win, attr);

    // Draw label (in the object's color by day, the label tier at night)
    if (object->label != NULL)
    {
        attr_t label_attr = config->night ? palette_attr(true, config->color, ROLE_LABEL, 0) : attr;
        wattron(win, label_attr);
        mvwaddstr_truncate(win, y - 1, x + 1, object->label);
        wattroff(win, label_attr);
    }

    return;
}

void render_stars_stereo(WINDOW *win, const struct Conf *config, struct Star *star_table, int num_stars, const int *num_by_mag)
{
    int i;
    for (i = 0; i < num_stars; ++i)
    {
        int catalog_num = num_by_mag[i];
        int table_index = catalog_num - 1;

        struct Star *star = &star_table[table_index];

        if (star->magnitude > config->threshold)
        {
            continue;
        }

        // FIXME: this is hacky
        if (star->magnitude > config->label_thresh)
        {
            star->base.label = NULL;
        }

        render_object_stereo(win, &star->base, config, ROLE_STAR);
    }

    return;
}

void render_constellation(WINDOW *win, const struct Conf *config, struct Constell *constellation, const struct Star *star_table)
{
    unsigned int num_segments = constellation->num_segments;

    // Only render if all stars are visible
    for (unsigned int i = 0; i < num_segments * 2; i += 1)
    {
        int catalog_num = constellation->star_numbers[i];
        int table_index = catalog_num - 1;
        struct Star star = star_table[table_index];
        if (star.magnitude > config->threshold)
        {
            return;
        }
    }

    for (unsigned int i = 0; i < num_segments * 2; i += 2)
    {
        int catalog_num_a = constellation->star_numbers[i];
        int catalog_num_b = constellation->star_numbers[i + 1];

        int table_index_a = catalog_num_a - 1;
        int table_index_b = catalog_num_b - 1;

        struct Star star_a = star_table[table_index_a];
        struct Star star_b = star_table[table_index_b];

        // TODO: Same code as in render_object_stereo... perhaps refactor this
        // or cache coordinates
        double radius_a, theta_a;
        double radius_b, theta_b;
        horizontal_to_polar(config, star_a.base.azimuth, star_a.base.altitude, &radius_a, &theta_a);
        horizontal_to_polar(config, star_b.base.azimuth, star_b.base.altitude, &radius_b, &theta_b);

        // Clip to edge of screen
        if (fabs(radius_a) > 1 && fabs(radius_b) > 1)
        {
            // Segment lies outside of screen
            continue;
        }

        bool a_clipped = false;
        bool b_clipped = false;

        // Clip the segment
        if (fabs(radius_a) > 1)
        {
            a_clipped = true;
            radius_a = 1.0;
        }
        else if (fabs(radius_b) > 1)
        {
            b_clipped = true;
            radius_b = 1.0;
        }

        int height, width;
        getmaxyx(win, height, width);

        int ya, xa;
        int yb, xb;
        polar_to_win(radius_a, theta_a, height, width, &ya, &xa);

        polar_to_win(radius_b, theta_b, height, width, &yb, &xb);

        // TODO: In old version, constrained line length for some reason... not
        // sure why?
        // FIXME: this logic is super verbose/long (any way to cut it down?)
        // FIXME: this clipping doesn't seem to work or no-unicode for some reason?
        if (config->unicode)
        {
            if (config->braille)
            {
                draw_line_braille(win, ya, xa, yb, xb);
            }
            else
            {
                draw_line_smooth(win, ya, xa, yb, xb);
            }
            if (!a_clipped)
            {
                mvwaddstr(win, ya, xa, "\u25CB"); // Unicode circle symbol
            }
            if (!b_clipped)
            {
                mvwaddstr(win, yb, xb, "\u25CB");
            }
        }
        else
        {
            draw_line_ASCII(win, ya, xa, yb, xb);
            if (!a_clipped)
            {
                mvwaddch(win, ya, xa, '+');
            }
            if (!b_clipped)
            {
                mvwaddch(win, yb, xb, '+');
            }
        }
    }
}

void render_constells(WINDOW *win, const struct Conf *config, struct Constell **constell_table, int num_const,
                      const struct Star *star_table)
{
    attr_t attr = palette_attr(config->night, config->color, ROLE_LINE, 0);
    wattron(win, attr);
    clear_braille_lines();
    for (int i = 0; i < num_const; ++i)
    {
        struct Constell *constellation = &((*constell_table)[i]);
        render_constellation(win, config, constellation, star_table);
    }
    wattroff(win, attr);
}

void render_planets_stereo(WINDOW *win, const struct Conf *config, const struct Planet *planet_table)
{
    // Render planets so that closest are drawn on top
    int i;
    for (i = NUM_PLANETS - 1; i >= 0; --i)
    {
        // Skip rendering the Earth--we're on the Earth! The geocentric
        // coordinates of the Earth are (0.0, 0.0, 0.0) and plotting the "Earth"
        // simply traces along the ecliptic at the approximate hour angle
        if (i == EARTH)
        {
            continue;
        }

        struct Planet planet_data = planet_table[i];
        render_object_stereo(win, &planet_data.base, config, ROLE_BODY);
    }

    return;
}

void render_moon_stereo(WINDOW *win, const struct Conf *config, struct Moon moon_object)
{
    // Moon phase emoji are drawn in full color by most terminals whatever the
    // color pair, so night vision uses text glyphs that can be tinted red
    if (config->night)
    {
        static const char *text_phases[8] = {"○", "☽", "◑", "◑", "●", "◐", "◐", "☾"};
        int phase = moon_object.phase;
        if (!moon_object.northern && phase != 0)
        {
            phase = 8 - phase;
        }
        moon_object.base.symbol_unicode = text_phases[phase & 7];
    }
    render_object_stereo(win, &moon_object.base, config, ROLE_BODY);

    return;
}

void render_stations(WINDOW *win, const struct Conf *config, const struct SatCatalog *stations)
{
    for (int i = 0; i < stations->count; ++i)
    {
        const struct Satellite *sat = &stations->sats[i];
        if (!sat->ok || sat->altitude <= 0.0)
        {
            continue;
        }

        struct ObjectBase base = {
            .azimuth = sat->azimuth,
            .altitude = sat->altitude,
            .color_pair = 0,
            .symbol_ASCII = '#',
            .symbol_unicode = "⌖",
            .label = sat->catnr == CATNR_ISS ? "ISS" : "Tiangong",
        };
        render_object_stereo(win, &base, config, sat->sunlit ? ROLE_STATION : ROLE_STATION_DARK);
    }
}

bool horizontal_to_dots(WINDOW *win, const struct Conf *config, double azimuth, double altitude, int *dot_row, int *dot_col)
{
    double radius, theta;
    horizontal_to_polar(config, azimuth, altitude, &radius, &theta);
    if (!(fabs(radius) <= 1.0) || !isfinite(theta))
    {
        return false;
    }

    // Same mapping as polar_to_win, kept fractional
    int height, width;
    getmaxyx(win, height, width);
    double rad_y = (height - 1) / 2.0;
    double rad_x = (width - 1) / 2.0;
    double row = radius * -rad_y * sin(theta) + rad_y;
    double col = radius * rad_x * cos(theta) + rad_x;

    // Cell c spans [c - 0.5, c + 0.5): 4 dot rows, 2 dot columns
    *dot_row = (int)floor((row + 0.5) * 4.0);
    *dot_col = (int)floor((col + 0.5) * 2.0);
    return true;
}

void render_starlink(WINDOW *win, const struct Conf *config, const struct SatCatalog *starlink, struct BrailleCanvas *lit,
                     struct BrailleCanvas *dark)
{
    int height, width;
    getmaxyx(win, height, width);
    braille_canvas_resize(lit, height, width);
    braille_canvas_resize(dark, height, width);

    attr_t lit_attr = palette_attr(config->night, config->color, ROLE_SAT_LIT, 0);
    attr_t dark_attr = palette_attr(config->night, config->color, ROLE_SAT_DARK, 0);

    for (int i = 0; i < starlink->count; ++i)
    {
        const struct Satellite *sat = &starlink->sats[i];
        if (!sat->ok || sat->altitude <= 0.0 || (!sat->sunlit && !config->starlink_dark))
        {
            continue;
        }

        if (config->unicode)
        {
            int dot_row, dot_col;
            if (horizontal_to_dots(win, config, sat->azimuth, sat->altitude, &dot_row, &dot_col))
            {
                braille_canvas_dot(sat->sunlit ? lit : dark, dot_row, dot_col);
            }
        }
        else
        {
            // ',' rather than '.', which faint stars use
            struct ObjectBase base = {
                .azimuth = sat->azimuth,
                .altitude = sat->altitude,
                .symbol_ASCII = ',',
                .symbol_unicode = ",",
            };
            render_object_stereo(win, &base, config, sat->sunlit ? ROLE_SAT_LIT : ROLE_SAT_DARK);
        }
    }

    // Sunlit dots win a shared cell: they are the ones you can see
    wattron(win, dark_attr);
    braille_canvas_flush(dark, win);
    wattroff(win, dark_attr);
    wattron(win, lit_attr);
    braille_canvas_flush(lit, win);
    wattroff(win, lit_attr);
}

#define VECTOR_MAX_ARC (30.0 * M_PI / 180.0)
#define BODY_LOOKAHEAD_DAYS 1.0

static void horizontal_to_unit(double azimuth, double altitude, double u[3])
{
    u[0] = cos(altitude) * cos(azimuth);
    u[1] = cos(altitude) * sin(azimuth);
    u[2] = sin(altitude);
}

static void unit_to_horizontal(const double u[3], double *azimuth, double *altitude)
{
    *azimuth = atan2(u[1], u[0]);
    *altitude = asin(fmax(-1.0, fmin(1.0, u[2])));
}

/* Normalised projected position (dome radius 1)
 */
static void project_xy(const struct Conf *config, double azimuth, double altitude, double *x, double *y)
{
    double radius, theta;
    horizontal_to_polar(config, azimuth, altitude, &radius, &theta);
    *x = radius * cos(theta);
    *y = radius * sin(theta);
}

static void draw_vector(WINDOW *win, const struct Conf *config, double az0, double alt0, double az1, double alt1,
                        struct BrailleCanvas *canvas)
{
    if (alt0 <= 0.0 || !isfinite(az1) || !isfinite(alt1))
    {
        return;
    }

    // Cap the arc on the sphere, before projecting
    double u0[3], u1[3];
    horizontal_to_unit(az0, alt0, u0);
    horizontal_to_unit(az1, alt1, u1);
    double arc = acos(fmax(-1.0, fmin(1.0, u0[0] * u1[0] + u0[1] * u1[1] + u0[2] * u1[2])));
    if (arc > VECTOR_MAX_ARC)
    {
        double f = VECTOR_MAX_ARC / arc;
        double s0 = sin((1.0 - f) * arc) / sin(arc);
        double s1 = sin(f * arc) / sin(arc);
        for (int k = 0; k < 3; ++k)
        {
            u1[k] = s0 * u0[k] + s1 * u1[k];
        }
        unit_to_horizontal(u1, &az1, &alt1);
    }

    double x0, y0, x1, y1;
    project_xy(config, az0, alt0, &x0, &y0);
    project_xy(config, az1, alt1, &x1, &y1);

    // Clip the far end at the horizon circle
    double dx = x1 - x0, dy = y1 - y0;
    if (x1 * x1 + y1 * y1 > 1.0)
    {
        double a = dx * dx + dy * dy;
        double b = 2.0 * (x0 * dx + y0 * dy);
        double c = x0 * x0 + y0 * y0 - 1.0;
        double t = (-b + sqrt(fmax(0.0, b * b - 4.0 * a * c))) / (2.0 * a);
        x1 = x0 + t * dx;
        y1 = y0 + t * dy;
    }

    int height, width;
    getmaxyx(win, height, width);
    double rad_y = (height - 1) / 2.0;
    double rad_x = (width - 1) / 2.0;

    if (config->unicode)
    {
        int r0 = (int)floor((rad_y - y0 * rad_y + 0.5) * 4.0);
        int c0 = (int)floor((rad_x + x0 * rad_x + 0.5) * 2.0);
        int r1 = (int)floor((rad_y - y1 * rad_y + 0.5) * 4.0);
        int c1 = (int)floor((rad_x + x1 * rad_x + 0.5) * 2.0);
        if (r0 != r1 || c0 != c1)
        {
            braille_canvas_line(canvas, r0, c0, r1, c1);
        }
    }
    else
    {
        int r0 = (int)round(rad_y - y0 * rad_y), c0 = (int)round(rad_x + x0 * rad_x);
        int r1 = (int)round(rad_y - y1 * rad_y), c1 = (int)round(rad_x + x1 * rad_x);
        if (r0 != r1 || c0 != c1)
        {
            draw_line_ASCII(win, r0, c0, r1, c1);
        }
    }
}

void render_vectors(WINDOW *win, const struct Conf *config, double julian_date, const struct Planet *planet_table,
                    const struct Moon *moon_object, const struct SatCatalog *stations, const struct SatCatalog *starlink,
                    struct BrailleCanvas *canvas)
{
    int height, width;
    getmaxyx(win, height, width);
    braille_canvas_resize(canvas, height, width);

    attr_t attr = palette_attr(config->night, config->color, ROLE_VECTOR, 0);
    wattron(win, attr);

    // Bodies: their position among the stars a day later, seen at today's
    // sidereal time
    double gmst = greenwich_mean_sidereal_time_rad(julian_date);
    double later = julian_date + BODY_LOOKAHEAD_DAYS;
    for (int i = SUN; i < NUM_PLANETS; ++i)
    {
        if (i == EARTH)
        {
            continue;
        }
        double ra, dec, az, alt;
        planet_equatorial(planet_table, i, later, &ra, &dec);
        equatorial_to_horizontal(ra, dec, gmst, config->latitude, config->longitude, &az, &alt);
        draw_vector(win, config, planet_table[i].base.azimuth, planet_table[i].base.altitude, az, alt, canvas);
    }
    {
        double ra, dec, az, alt;
        moon_equatorial(moon_object, later, &ra, &dec);
        equatorial_to_horizontal(ra, dec, gmst, config->latitude, config->longitude, &az, &alt);
        draw_vector(win, config, moon_object->base.azimuth, moon_object->base.altitude, az, alt, canvas);
    }

    // Satellites: apparent motion over the next few seconds
    const struct SatCatalog *catalogs[2] = {config->stations ? stations : NULL, config->starlink ? starlink : NULL};
    for (int c = 0; c < 2; ++c)
    {
        for (int i = 0; catalogs[c] != NULL && i < catalogs[c]->count; ++i)
        {
            const struct Satellite *sat = &catalogs[c]->sats[i];
            if (!sat->ok || (c == 1 && !sat->sunlit && !config->starlink_dark))
            {
                continue;
            }
            draw_vector(win, config, sat->azimuth, sat->altitude, sat->ahead_azimuth, sat->ahead_altitude, canvas);
        }
    }

    braille_canvas_flush(canvas, win);
    wattroff(win, attr);
}

int gcd(int a, int b)
{
    while (b != 0)
    {
        int temp = a % b;

        a = b;
        b = temp;
    }
    return a;
}

int compare_angles(const void *a, const void *b)
{
    int x = *(int *)a;
    int y = *(int *)b;
    return (90 / gcd(x, 90)) < (90 / gcd(y, 90));
}

void render_azimuthal_grid(WINDOW *win, const struct Conf *config)
{
    const double to_rad = M_PI / 180.0;

    int height, width;
    getmaxyx(win, height, width);
    int maxy = height - 1;
    int maxx = width - 1;

    int rad_vertical = round(maxy / 2.0);
    int rad_horizontal = round(maxx / 2.0);

    // Possible step sizes in degrees (multiples of 5 and factors of 90)
    int step_sizes[5] = {10, 15, 30, 45, 90};
    int length = sizeof(step_sizes) / sizeof(step_sizes[0]);

    // Minimum number of rows separating grid line (at end of window)
    int min_height = 10;

    int inc = step_sizes[length - 1]; // Fallback to large delta on small windows
    for (int i = 0; i < length; ++i)
    {
        // Vertical offset (rows) of the angle at edge of window -- approximately the "arc length" for small angles
        int vertical_offset = (int)round(rad_vertical * sin(step_sizes[i] * to_rad));
        if (vertical_offset >= min_height)
        {
            inc = step_sizes[i];
            break;
        }
    }

    // Sort grid angles in the first quadrant by rendering priority
    int number_angles = 90 / inc + 1;
    int *angles = malloc(number_angles * sizeof(int));
    if (angles == NULL)
    {
        return;
    }

    for (int i = 0; i < number_angles; ++i)
    {
        angles[i] = inc * i;
    }
    qsort(angles, number_angles, sizeof(int), compare_angles);

    attr_t attr = palette_attr(config->night, config->color, ROLE_LINE, 0);
    wattron(win, attr);

    // Draw angles in all four quadrants
    int quad;
    for (quad = 0; quad < 4; ++quad)
    {
        for (int i = 0; i < number_angles; ++i)
        {
            int angle = angles[i] + 90 * quad;
            double drawn = angle * to_rad + config->rotation;

            int y = rad_vertical - round(rad_vertical * sin(drawn));
            int x = rad_horizontal + round(rad_horizontal * cos(drawn));

            if (config->unicode)
            {
                draw_line_smooth(win, y, x, rad_vertical, rad_horizontal);
            }
            else
            {
                draw_line_ASCII(win, y, x, rad_vertical, rad_horizontal);
            }

            int str_len = snprintf(NULL, 0, "%d", angle);
            char *label = malloc(str_len + 1);
            if (label == NULL)
            {
                continue;
            }

            snprintf(label, str_len + 1, "%d", angle);

            // Offset to avoid truncating string
            int x_off = (x < rad_horizontal) ? 0 : -(str_len - 1);

            mvwaddstr(win, y, x + x_off, label);

            free(label);
        }
    }
    wattroff(win, attr);
    free(angles);

    // while (angle <= 90.0)
    // {
    //     int rad_x = rad_horizontal * angle / 90.0;
    //     int rad_x = rad_vertical * angle / 90.0;
    //     // draw_ellipse(win, win->_maxy/2, win->_maxx/2, 20, 20,
    //     ascii); angle += inc;
    // }
}

void render_cardinal_directions(WINDOW *win, const struct Conf *config)
{
    // Render horizon directions

    attr_t attr = palette_attr(config->night, config->color, ROLE_UI, 5);
    wattron(win, attr);

    int height, width;
    getmaxyx(win, height, width);
    int maxy = height - 1;
    int maxx = width - 1;

    // Place the letters on the horizon through the same projection as every
    // object, so they follow the dome's rotation
    const char letters[4] = {'N', 'E', 'S', 'W'};
    for (int i = 0; i < 4; ++i)
    {
        double radius, theta;
        horizontal_to_polar(config, i * M_PI / 2.0, 0.0, &radius, &theta);

        int y, x;
        polar_to_win(radius, theta, height, width, &y, &x);
        y = MAX(0, MIN(maxy, y));
        x = MAX(0, MIN(maxx, x));
        mvwaddch(win, y, x, letters[i]);
    }

    wattroff(win, attr);
}
