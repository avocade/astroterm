#include "city.h"
#include "core.h"
#include "core_position.h"
#include "core_render.h"
#include "data/keplerian_elements.h"
#include "feed.h"
#include "omm.h"
#include "satellite.h"
#include "macros.h"
#include "parse_BSC5.h"
#include "sim_clock.h"
#include "stopwatch.h"
#include "term.h"
#include "ui.h"
#include "version.h"

// Embedded data generated during build
#include "bsc5.h"
#include "bsc5_constellations.h"
#include "bsc5_names.h"

// Third party libraries
#ifdef HAVE_ARGTABLE3
#include <argtable3.h>
#elif defined(HAVE_ARGTABLE2)
#include <argtable2.h>
#else
#error "Neither argtable2 nor argtable3 is available. Please install one of them."
#endif
#include <curses.h>

#include <locale.h>
#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

static void catch_winch(int sig);
static void resize_ncurses(void);
static void resize_meta(WINDOW *win);
static void resize_main(WINDOW *win, const struct Conf *config);
static void parse_options(int argc, char *argv[], struct Conf *config);
static void convert_options(struct Conf *config);
static const char *get_timezone(const struct tm *local_time);
static void render_metadata(WINDOW *win, const struct Conf *config);

// Track if we need to resize the curses window
static volatile bool perform_resize = false;
#ifdef _WIN32
// Track console size on windows
static COORD winsize;
#endif

// Track current simulation time (UTC)
// Default to current time in dt_string_utc is NULL
static double julian_date = 0.0;
static double julian_date_start = 0.0; // Note of when we started
static struct SimClock sim_clock;

static const long station_catnrs[] = {CATNR_ISS, CATNR_TIANGONG};

/* Load a cached feed into a catalog (empty when there is no usable data)
 */
static void load_catalog(enum FeedId id, const long *only, int num_only, struct SatCatalog *out)
{
    out->sats = NULL;
    out->count = 0;

    size_t len;
    char *data = feed_read(id, &len);
    if (data == NULL)
    {
        return;
    }
    struct OmmRecord *records = NULL;
    int n = omm_parse_csv(data, len, &records);
    free(data);
    if (n > 0)
    {
        satellite_catalog_build(records, n, only, num_only, out);
    }
    free(records);
}

/* Local clock time of a Julian date, with the weekday when not on `today_jd`
 */
static void format_local_time(double jd, double today_jd, char *buf, size_t len)
{
    time_t t = (time_t)((jd - 2440587.5) * 86400.0);
    time_t today = (time_t)((today_jd - 2440587.5) * 86400.0);
    struct tm when = *localtime(&t);
    struct tm now = *localtime(&today);
    bool same_day = when.tm_year == now.tm_year && when.tm_yday == now.tm_yday;
    strftime(buf, len, same_day ? "%H:%M" : "%a %H:%M", &when);
}

/* The ISS corner line: where it is now, or its next visible pass. Predictions
 * are cached while simulation time stays inside the predicted window, and
 * recomputed at most twice per wall second
 */
static void iss_status(const struct SatCatalog *stations, const struct Conf *config, double jd, char *buf, size_t len)
{
    static struct PassPrediction pass;
    static bool have_pass = false;
    static double last_predict = -1.0;

    buf[0] = '\0';
    const struct Satellite *iss = NULL;
    for (int i = 0; i < stations->count; ++i)
    {
        if (stations->sats[i].catnr == CATNR_ISS)
        {
            iss = &stations->sats[i];
        }
    }
    if (iss == NULL || !config->stations)
    {
        return;
    }

    if (iss->ok && iss->altitude > SAT_PASS_MIN_ALTITUDE)
    {
        snprintf(buf, len, "ISS up: %.0f° %s%s", iss->altitude * 180.0 / M_PI, compass_point(iss->azimuth),
                 iss->sunlit ? "" : ", in shadow");
        return;
    }

    const double horizon_days = 3.0;
    static bool visible = true;
    bool stale = !have_pass || jd < pass.computed_at || (pass.found && jd > pass.end_jd) ||
                 (!pass.found && jd > pass.computed_at + 1.0 / 24.0);
    double mono = clock_monotonic_s();
    if (stale && mono - last_predict >= 0.5)
    {
        have_pass = true;
        last_predict = mono;

        // Prefer a pass you can see (sunlit ISS, dark sky); otherwise say when
        // it next comes over at all, and in what light
        visible = satellite_next_pass(iss, jd, horizon_days, config->latitude, config->longitude, true, &pass);
        if (!visible)
        {
            satellite_next_pass(iss, jd, horizon_days, config->latitude, config->longitude, false, &pass);
        }
    }
    if (!have_pass)
    {
        return;
    }

    if (!pass.found)
    {
        snprintf(buf, len, "ISS: no pass in 72 h");
        return;
    }

    const char *light = "";
    if (!visible)
    {
        double sun = sun_altitude(pass.peak_jd, config->latitude, config->longitude) * 180.0 / M_PI;
        light = sun > -0.833 ? " (daylight)" : " (twilight)";
    }
    char when[32];
    format_local_time(pass.start_jd, jd, when, sizeof(when));
    snprintf(buf, len, "ISS %s %s, max %.0f°%s", when, compass_point(pass.start_azimuth),
             pass.peak_altitude * 180.0 / M_PI, light);
}

int main(int argc, char *argv[])
{
    // Default config
    struct Conf config = {
        .longitude = 0.0,
        .latitude = 0.0,
        .dt_string_utc = NULL,
        .threshold = 5.0f,
        .label_thresh = 0.25f,
        .fps = 24,
        .speed = 1.0f,
        .aspect_ratio = 0.0,
        .quit_on_any = false,
        .unicode = false,
        .braille = false,
        .color = false,
        .grid = false,
        .constell = false,
        .metadata = false,
        .stations = true,
    };

    // Parse command line args and convert to internal representations
    parse_options(argc, argv, &config);
    convert_options(&config);

    // Time for each frame in microseconds
    unsigned long dt = (unsigned long)(1.0 / config.fps * 1.0E6);

    // Initialize data structs
    unsigned int num_stars, num_const;

    struct Entry *BSC5_entries = NULL;
    struct StarName *name_table = NULL;
    struct Constell *constell_table = NULL;
    struct Star *star_table = NULL;
    struct Planet *planet_table = NULL;
    struct Moon moon_object;
    int *num_by_mag = NULL;

    // Track success of functions
    bool s = true;

    // Generated BSC5 data during build in bsc5_xxx.h:
    //
    // uint8_t bsc5_xxx[];
    // size_t bsc5_xxx_len;

    s = s && parse_entries(bsc5, bsc5_len, &BSC5_entries, &num_stars);
    s = s && generate_name_table(bsc5_names, bsc5_names_len, &name_table, num_stars);
    s = s && generate_constell_table(bsc5_constellations, bsc5_constellations_len, &constell_table, &num_const);
    s = s && generate_star_table(&star_table, BSC5_entries, name_table, num_stars);
    s = s && generate_planet_table(&planet_table, planet_elements, planet_rates, planet_extras);
    s = s && generate_moon_object(&moon_object, &moon_elements, &moon_rates);
    s = s && star_numbers_by_magnitude(&num_by_mag, star_table, num_stars);

    if (!s)
    {
        // At least one of the above functions failed, exit
        exit(EXIT_FAILURE);
    }

    // This memory is no longer needed
    free(BSC5_entries);

    // Satellite data is refreshed before the UI starts (see feed.h)
    char data_message[128] = "";
    if ((config.stations || config.starlink) && !config.offline)
    {
        // Both feeds, so the cache is warm before you leave signal behind
        feed_refresh_all(data_message, sizeof(data_message));
    }
    struct SatCatalog stations, starlink;
    load_catalog(FEED_STATIONS, station_catnrs, 2, &stations);
    load_catalog(FEED_STARLINK, NULL, 0, &starlink);

    // Starlink is propagated at most this often (wall clock), at any speed
    const double starlink_period = 0.25;
    double starlink_updated = -1.0e9;
    bool starlink_toast = false;
    int starlink_above = 0;
    int starlink_sunlit = 0;
    struct BrailleCanvas starlink_lit = {0};
    struct BrailleCanvas starlink_dark = {0};

    // Terminal/System settings
    setlocale(LC_ALL, ""); // Required for unicode rendering
#ifndef _WIN32
    signal(SIGWINCH, catch_winch); // Capture window resizes
#endif
    tzset(); // Initialize timezone information

    // Ncurses initialization
    ncurses_init(config.color);

    // Main (projection) window
    WINDOW *main_win = newwin(0, 0, 0, 0);
    resize_main(main_win, &config);

    // Metadata window
    WINDOW *metadata_win = newwin(0, 0, 0, 0); // Position at top left
    resize_meta(metadata_win);

    // Simulation time is derived from the realtime clock each frame, so slow
    // frames or a suspended process never make the sky fall behind
    sim_clock_init(&sim_clock, julian_date_start, clock_realtime_s(), config.speed);

    struct UiState ui = {0};
    double data_age = feed_age_hours(FEED_STATIONS);
    if (data_message[0] != '\0')
    {
        ui_toast(&ui, clock_monotonic_s(), "%s", data_message);
    }
    else if (config.stations && stations.count == 0)
    {
        ui_toast(&ui, clock_monotonic_s(), "No satellite data yet: run once online");
    }
    else if (config.stations && data_age > 48.0)
    {
        ui_toast(&ui, clock_monotonic_s(), "Satellite data is %.0f days old", data_age / 24.0);
    }
    else if (config.latitude == 0.0 && config.longitude == 0.0)
    {
        ui_toast(&ui, clock_monotonic_s(), "Location 0°, 0°: use -i <city> or -a/-o");
    }
    else
    {
        ui_toast(&ui, clock_monotonic_s(), "? for keys");
    }

    // Window backgrounds follow night vision (applied when it changes)
    bool background_night = !config.night;

    // Render loop
    bool quit = false;
    while (!quit)
    {
        struct SwTimestamp frame_begin;
        sw_gettime(&frame_begin);

        julian_date = sim_clock_jd(&sim_clock, clock_realtime_s());

#ifdef _WIN32
        // Use this function to catch console resizes on Windows
        perform_resize = check_console_window_resize_event(&winsize);
#endif

        if (perform_resize)
        {
            resize_ncurses();
            resize_main(main_win, &config);
            resize_meta(metadata_win);

            perform_resize = false;
        }

        if (background_night != config.night)
        {
            attr_t background = palette_background(config.night);
            wbkgd(stdscr, background);
            wbkgd(main_win, background);
            wbkgd(metadata_win, background);
            background_night = config.night;
        }

        // Everything is redrawn every frame; ncurses only sends what changed.
        // Erasing stdscr clears whatever a closed modal, panel or toast left
        // outside the sky window
        werase(stdscr);
        werase(metadata_win);
        werase(main_win);

        // Update object positions
        update_star_positions(star_table, num_stars, julian_date, config.latitude, config.longitude);
        update_planet_positions(planet_table, julian_date, config.latitude, config.longitude);
        update_moon_position(&moon_object, julian_date, config.latitude, config.longitude);
        update_moon_phase(&moon_object, julian_date, config.latitude);

        double sun_dir[3];
        sun_direction(julian_date, sun_dir);
        for (int i = 0; config.stations && i < stations.count; ++i)
        {
            satellite_update(&stations.sats[i], julian_date, config.latitude, config.longitude, sun_dir, false);
        }

        double mono_now = clock_monotonic_s();
        if (config.starlink && mono_now - starlink_updated >= starlink_period)
        {
            starlink_updated = mono_now;
            starlink_above = starlink_sunlit = 0;
            for (int i = 0; i < starlink.count; ++i)
            {
                struct Satellite *sat = &starlink.sats[i];
                satellite_update(sat, julian_date, config.latitude, config.longitude, sun_dir, false);
                if (sat->ok && sat->altitude > 0.0)
                {
                    starlink_above++;
                    starlink_sunlit += sat->sunlit;
                }
            }
            if (starlink_toast)
            {
                ui_toast(&ui, mono_now, "Starlink: %d sunlit of %d above the horizon", starlink_sunlit, starlink_above);
                starlink_toast = false;
            }
        }

        // Render objects, bottom layer first
        if (config.starlink)
        {
            render_starlink(main_win, &config, &starlink, &starlink_lit, &starlink_dark);
        }
        render_stars_stereo(main_win, &config, star_table, num_stars, num_by_mag);
        if (config.constell)
        {
            render_constells(main_win, &config, &constell_table, num_const, star_table);
        }
        render_planets_stereo(main_win, &config, planet_table);
        render_moon_stereo(main_win, &config, moon_object);
        if (config.stations)
        {
            render_stations(main_win, &config, &stations);
        }
        if (config.grid)
        {
            render_azimuthal_grid(main_win, &config);
        }
        else
        {
            render_cardinal_directions(main_win, &config);
        }

        // Render metadata
        if (config.metadata)
        {
            render_metadata(metadata_win, &config);
        }

        // Toast in the bottom-left corner, off the dome where possible
        double mono = clock_monotonic_s();
        char iss_line[64];
        iss_status(&stations, &config, julian_date, iss_line, sizeof(iss_line));
        const char *corner[2] = {iss_line, ui_current_toast(&ui, mono)};
        ui_draw_corner(main_win, corner, 2, palette_background(config.night));

        // Queue windows bottom to top, then draw once to avoid flickering
        wnoutrefresh(stdscr);
        wnoutrefresh(main_win);
        if (config.metadata)
        {
            wnoutrefresh(metadata_win);
        }
        if (ui.help_open)
        {
            ui_draw_help(&config, &sim_clock, palette_background(config.night));
        }
        doupdate();

        // Read input only after the frame is on screen, so getch()'s implicit
        // refresh of stdscr has nothing left to paint
        int ch;
        while (!quit && (ch = getch()) != ERR)
        {
            struct UiContext ctx = {
                .wall = clock_realtime_s(),
                .mono = clock_monotonic_s(),
                .has_colors = has_colors(),
                .stations_count = stations.count,
                .starlink_count = starlink.count,
            };
            enum UiAction action = ui_handle_key(ch, &config, &ui, &sim_clock, &ctx);
            if (action == UI_QUIT)
            {
                quit = true;
            }
            else if (action == UI_STARLINK)
            {
                // Update now and report what is up
                starlink_updated = -1.0e9;
                starlink_toast = true;
            }
        }

        // Determine time it took to update positions and render to screen
        struct SwTimestamp frame_end;
        sw_gettime(&frame_end);

        unsigned long long frame_time;
        sw_timediff_usec(frame_end, frame_begin, &frame_time);

        // If updating the frame took less time than the time between frames,
        // wait the rest of the time
        if (frame_time < dt)
        {
            sw_sleep(dt - frame_time);
        }
    }

    // Clean up

    ncurses_kill();

    free_constells(constell_table, num_const);
    free_stars(star_table, num_stars);
    free_planets(planet_table, NUM_PLANETS);
    free_moon_object(moon_object);
    free_star_names(name_table, num_stars);
    satellite_catalog_free(&stations);
    satellite_catalog_free(&starlink);
    braille_canvas_free(&starlink_lit);
    braille_canvas_free(&starlink_dark);

    return EXIT_SUCCESS;
}

void print_city_name_quoted(const CityData *city, void *unused)
{
    const char *s = city->city_name;

    putchar('\'');
    // Single-quote wrapping with escaping--needed for bash completions to
    // handle city names single quotes in them (e.g. "St. John's")
    while (*s)
    {
        if (*s == '\'')
        {
            printf("'\\''"); // close, escape, reopen
        }
        else
        {
            putchar(*s);
        }
        s++;
    }
    putchar('\'');
    putchar('\n');
}

static void print_short_option(const char *short_name)
{
    if (short_name != NULL)
    {
        printf("    -%s\n", short_name);
    }
}

void parse_options(int argc, char *argv[], struct Conf *config)
{
#define INCLUDE_ARG_DEFINITION_DBL0(token, short_name, long_name, datatype, glossary)                                          \
    struct arg_dbl *token = arg_dbl0(short_name, long_name, datatype, glossary);
#define INCLUDE_ARG_DEFINITION_STR0(token, short_name, long_name, datatype, glossary)                                          \
    struct arg_str *token = arg_str0(short_name, long_name, datatype, glossary);
#define INCLUDE_ARG_DEFINITION_LIT0(token, short_name, long_name, glossary)                                                    \
    struct arg_lit *token = arg_lit0(short_name, long_name, glossary);
#define INCLUDE_ARG_DEFINITION_INT0(token, short_name, long_name, datatype, glossary)                                          \
    struct arg_int *token = arg_int0(short_name, long_name, datatype, glossary);
#include "arg_definitions.h"
    struct arg_end *end = arg_end(20);

    void *argtable[] = {latitude_arg, longitude_arg, datetime_arg,    threshold_arg, label_arg,   fps_arg,  speed_arg,
                        color_arg,    constell_arg,  grid_arg,        unicode_arg,   braille_arg, quit_arg, meta_arg,
                        ratio_arg,    help_arg,      completions_arg, city_arg,      version_arg, night_arg,
                        offline_arg,  starlink_arg,  end};

    int nerrors = arg_parse(argc, argv, argtable);

    if (help_arg->count > 0)
    {
        printf("View stars, planets, and more, right in your terminal! ✨🪐\n\n");
        printf("Usage: astroterm [OPTION]...\n\n");
        arg_print_glossary_gnu(stdout, argtable);
        exit(EXIT_SUCCESS);
    }

    if (completions_arg->count > 0)
    {
        // Print bash completions
        printf("# Bash completions for astroterm\n");
        printf("ASTROTERM_OPTIONS=(\n");
#define INCLUDE_ARG_DEFINITION_DBL0(token, short_name, long_name, datatype, glossary)                                          \
    printf("    -%s\n", short_name);                                                                                           \
    printf("    --%s\n", long_name);
#define INCLUDE_ARG_DEFINITION_STR0(token, short_name, long_name, datatype, glossary)                                          \
    printf("    -%s\n", short_name);                                                                                           \
    printf("    --%s\n", long_name);
#define INCLUDE_ARG_DEFINITION_LIT0(token, short_name, long_name, glossary)                                                    \
    print_short_option(short_name);                                                                                            \
    printf("    --%s\n", long_name);
#define INCLUDE_ARG_DEFINITION_INT0(token, short_name, long_name, datatype, glossary)                                          \
    printf("    -%s\n", short_name);                                                                                           \
    printf("    --%s\n", long_name);
#include "arg_definitions.h"
        printf(")\n\n");
        printf("ASTROTERM_CITIES=(\n");
        iter_cities(&print_city_name_quoted, NULL);
        printf(")\n");
        // Warning: this is a vibe code modified version from PR #80 in order to
        // get things working on bash 3.2. The cleaning of the input word seems
        // necessary to prevent some weird escaping issues that cause the completions
        // to not work
        printf("_astroterm_completions() {\n");
        printf("    local word=\"${COMP_WORDS[COMP_CWORD]}\"\n");
        printf("    local prev=\"${COMP_WORDS[COMP_CWORD-1]}\"\n");
        printf("    case \"$prev\" in\n");
        printf("        -i|--city)\n");
        printf("            local clean_word=\"${word//\\\\/}\"\n");
        printf("            clean_word=\"${clean_word//\\\"/}\"\n");
        printf("            clean_word=\"${clean_word//\\'/}\"\n");
        printf("            COMPREPLY=()\n");
        printf("            for city in \"${ASTROTERM_CITIES[@]}\"; do\n");
        printf("                if [[ \"$city\" == \"${clean_word}\"* ]]; then\n");
        printf("                    printf -v city_esc '%%q ' \"$city\"\n");
        printf("                    COMPREPLY+=(\"$city_esc\")\n");
        printf("                fi\n");
        printf("            done\n");
        printf("            ;;\n");
        printf("        *)\n");
        printf("            COMPREPLY=( $(compgen -W \"${ASTROTERM_OPTIONS[*]}\" -- \"${word}\") )\n");
        printf("            ;;\n");
        printf("    esac\n");
        printf("}\n");
        printf("complete -o nospace -F _astroterm_completions astroterm\n");
        exit(EXIT_SUCCESS);
    }

    if (nerrors > 0)
    {
        arg_print_errors(stderr, end, argv[0]);
        printf("Try '--help' for more information.\n");
        exit(EXIT_FAILURE);
    }

    if (version_arg->count > 0)
    {
        printf("%s %s\n", PROJ_NAME, PROJ_VERSION);
        exit(EXIT_SUCCESS);
    }

    if (latitude_arg->count > 0)
    {
        config->latitude = latitude_arg->dval[0];
        if (config->latitude < -90 || config->latitude > 90)
        {
            fprintf(stderr, "ERROR: Latitude out of range [-90°, 90°]\n");
            exit(EXIT_FAILURE);
        }
    }

    if (longitude_arg->count > 0)
    {
        config->longitude = longitude_arg->dval[0];
        if (config->longitude < -180 || config->longitude > 180)
        {
            fprintf(stderr, "ERROR: Longitude out of range [-180°, 180°]\n");
            exit(EXIT_FAILURE);
        }
    }

    if (datetime_arg->count > 0)
    {
        config->dt_string_utc = datetime_arg->sval[0];
    }

    if (threshold_arg->count > 0)
    {
        config->threshold = (float)threshold_arg->dval[0];
    }

    if (label_arg->count > 0)
    {
        config->label_thresh = (float)label_arg->dval[0];
    }

    if (fps_arg->count > 0)
    {
        config->fps = fps_arg->ival[0];
        if (config->fps < 1)
        {
            fprintf(stderr, "ERROR: FPS must be greater than or equal to 1\n");
            exit(EXIT_FAILURE);
        }
    }

    if (speed_arg->count > 0)
    {
        config->speed = (float)speed_arg->dval[0];
    }

    if (color_arg->count > 0)
    {
        config->color = true;
    }

    if (constell_arg->count > 0)
    {
        config->constell = true;
    }

    if (meta_arg->count > 0)
    {
        config->metadata = true;
    }

    if (grid_arg->count > 0)
    {
        config->grid = true;
    }

    if (unicode_arg->count > 0)
    {
        config->unicode = true;
    }

    if (braille_arg->count > 0)
    {
        config->braille = true;
    }

    if (night_arg->count > 0)
    {
        config->night = true;
    }

    if (offline_arg->count > 0)
    {
        config->offline = true;
    }

    if (starlink_arg->count > 0)
    {
        config->starlink = true;
    }

    if (quit_arg->count > 0)
    {
        config->quit_on_any = true;
    }

    if (ratio_arg->count > 0)
    {
        config->aspect_ratio = ratio_arg->dval[0];
    }

    if (city_arg->count > 0)
    {
        const char *city_name = city_arg->sval[0];
        CityData *city = get_city(city_name);

        if (!city)
        {
            fprintf(stderr, "ERROR: Could not find city \"%s\"\n", city_name);
            exit(EXIT_FAILURE);
        }

        config->latitude = city->latitude;
        config->longitude = city->longitude;
        free_city(city);
    }

    // Free Argtable resources
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
}

void convert_options(struct Conf *config)
{
    // Convert longitude and latitude to radians
    config->longitude *= M_PI / 180.0;
    config->latitude *= M_PI / 180.0;

    // Convert Gregorian calendar date to Julian date
    if (config->dt_string_utc == NULL)
    {
        // Set julian date to current time (sub-second precision)
        julian_date_start = unix_to_julian_date(clock_realtime_s());
        julian_date = julian_date_start;
    }
    else
    {
        struct tm datetime;
        bool parse_success = string_to_time(config->dt_string_utc, &datetime);
        if (!parse_success)
        {
            printf("ERROR: Unable to parse datetime string '%s'\nDatetimes "
                   "must be in form <yyyy-mm-ddThh:mm:ss>\n",
                   config->dt_string_utc);
            exit(EXIT_FAILURE);
        }
        julian_date_start = datetime_to_julian_date(&datetime);
        julian_date = julian_date_start;
    }

    return;
}

void catch_winch(int sig)
{
    (void)sig;
    perform_resize = true;
}

void resize_ncurses(void)
{
    // Resize ncurses internal terminal
    int y;
    int x;
    term_size(&y, &x);

#ifdef _WIN32
    resize_term(winsize.Y, winsize.X);
#else
    resize_term(y, x);
#endif
}

void resize_main(WINDOW *win, const struct Conf *config)
{
    // Clear the window before resizing
    werase(win);
#ifndef _WIN32
    wnoutrefresh(win);
#endif

    // Check cell ratio
    float aspect;
    if (config->aspect_ratio)
    {
        aspect = config->aspect_ratio;
    }
    else
    {
        aspect = get_cell_aspect_ratio();
    }

    // Resize/position application window
    win_resize_square(win, aspect);
    win_position_center(win);
#ifdef _WIN32
    wnoutrefresh(win);
#endif
}

void resize_meta(WINDOW *win)
{
    // Clear the window before resizing
    werase(win);
#ifndef _WIN32
    wnoutrefresh(win);
#endif

    const int meta_lines = 7; // Allows for 7 rows
    const int meta_cols = 48; // Set to allow enough room for longest line (elapsed time)

    wresize(win, MIN(LINES, meta_lines), MIN(COLS, meta_cols));
#ifdef _WIN32
    wnoutrefresh(win);
#endif
}

const char *get_timezone(const struct tm *local_time)
{
#ifdef _WIN32
    // Windows-specific code
    TIME_ZONE_INFORMATION tz_info;
    GetTimeZoneInformation(&tz_info);

    static char tzbuf[8];
    char sign = tz_info.Bias > 0 ? '-' : '+';
    long hours = labs(tz_info.Bias) / 60;
    long minutes = labs(tz_info.Bias) % 60;
    snprintf(tzbuf, sizeof(tzbuf), "%c%02ld:%02ld", sign, hours, minutes);
    return tzbuf;
#else
    // Unix-like systems (Linux/macOS) code
    extern char *tzname[2]; // tzname[0] is standard, tzname[1] is DST
    return local_time->tm_isdst > 0 ? tzname[1] : tzname[0];
#endif
}

void render_metadata(WINDOW *win, const struct Conf *config)
{
    // Gregorian Date (local time)

    // Convert sim julian date (UTC) to local time
    const double JULIAN_DATE_EPOCH = 2440587.5;
    time_t utc_time = (time_t)((julian_date - JULIAN_DATE_EPOCH) * 86400);
    const struct tm *local_time = localtime(&utc_time);
    if (local_time == NULL)
    {
        // Default to UTC if conversion fails
        local_time = gmtime(&utc_time);
    }

    int year = local_time->tm_year + 1900; // tm_year is years since 1900
    int month = local_time->tm_mon + 1;    // tm_mon is months since January (0-11)
    int day = local_time->tm_mday;         // Day of the month
    int hour = local_time->tm_hour;        // Hour (0-23)
    int minute = local_time->tm_min;       // Minute (0-59)

    const char *timezone = get_timezone(local_time);
    mvwprintw(win, 0, 0, "Date (%s): \t%02d-%02d-%04d %02d:%02d", timezone, day, month, year, hour, minute);

    // Zodiac
    const char *zodiac_name = get_zodiac_sign(month, day);
    const char *zodiac_symbol = get_zodiac_symbol(month, day);
    if (config->unicode)
    {
        mvwprintw(win, 1, 0, "Zodiac: \t%s %s", zodiac_name, zodiac_symbol);
    }
    else
    {
        mvwprintw(win, 1, 0, "Zodiac: \t%s", zodiac_name);
    }

    // Lunar phase
    double age = calc_moon_age(julian_date);
    enum MoonPhase phase = moon_age_to_phase(age);
    const char *lunar_phase = get_moon_phase_name(phase);
    mvwprintw(win, 2, 0, "Lunar Phase: \t%s", lunar_phase);

    // Lat and Lon (convert back to degrees)
    int deg, min;
    double sec;
    decimal_to_dms(config->latitude * 180 / M_PI, &deg, &min, &sec);
    mvwprintw(win, 3, 0, "Latitude: \t%d° %d' %.2f\"", deg, min, sec);

    // Longitude
    decimal_to_dms(config->longitude * 180 / M_PI, &deg, &min, &sec);
    mvwprintw(win, 4, 0, "Longitude: \t%d° %d' %.2f\"", deg, min, sec);

    // Elapsed time
    int eyears, edays, ehours, emins, esecs;
    elapsed_time_to_components(julian_date - julian_date_start, &eyears, &edays, &ehours, &emins, &esecs);
    const char *year_label = (eyears == 1) ? " year" : "years";
    const char *day_label = (edays == 1) ? " day" : "days";

    // Display elapsed time with proper labels
    mvwprintw(win, 5, 0, "Elapsed Time: \t%03d %s, %03d %s, %02d:%02d:%02d", eyears, year_label, edays, day_label, ehours,
              emins, esecs);

    char speed[32];
    ui_speed_text(&sim_clock, speed, sizeof(speed));
    mvwprintw(win, 6, 0, "Speed: \t%s", speed);

    return;
}
