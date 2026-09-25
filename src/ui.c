#include "ui.h"
#include "macros.h"
#include "satellite.h"
#include "term.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define KEY_ESCAPE 27
#define ROTATION_STEP (15.0 * M_PI / 180.0)
#define THRESHOLD_STEP 0.5f
#define THRESHOLD_MIN -1.5f
#define THRESHOLD_MAX 8.0f

enum UiCommand
{
    CMD_NONE = 0,
    CMD_COLOR,
    CMD_CONSTELL,
    CMD_GRID,
    CMD_UNICODE,
    CMD_BRAILLE,
    CMD_METADATA,
    CMD_NIGHT,
    CMD_STATIONS,
    CMD_STARLINK,
    CMD_STARLINK_DARK,
    CMD_VECTORS,
    CMD_THRESH_UP,
    CMD_THRESH_DOWN,
    CMD_PAUSE,
    CMD_FASTER,
    CMD_SLOWER,
    CMD_NOW,
    CMD_ROT_LEFT,
    CMD_ROT_RIGHT,
    CMD_RESET_VIEW,
    CMD_ZOOM_IN,
    CMD_ZOOM_OUT,
    CMD_PAN_LEFT,
    CMD_PAN_RIGHT,
    CMD_PAN_UP,
    CMD_PAN_DOWN,
    CMD_QUADRANT,
    CMD_HELP,
    CMD_QUIT,
};

/* One help row: the keys that trigger it and the command for each key
 */
struct KeyRow
{
    const char *keys;
    const char *label;
    int key[8];
    enum UiCommand cmd[8];
};

// The single source of truth for keys, help rows and dispatch
static const struct KeyRow key_rows[] = {
    {"c", "Colors", {'c'}, {CMD_COLOR}},
    {"C", "Constellations", {'C'}, {CMD_CONSTELL}},
    {"g", "Grid", {'g'}, {CMD_GRID}},
    {"u", "Unicode", {'u'}, {CMD_UNICODE}},
    {"b", "Braille lines", {'b'}, {CMD_BRAILLE}},
    {"m", "Metadata panel", {'m'}, {CMD_METADATA}},
    {"r", "Red night vision", {'r'}, {CMD_NIGHT}},
    {"i", "Space stations (ISS)", {'i'}, {CMD_STATIONS}},
    {"x", "Starlink", {'x'}, {CMD_STARLINK}},
    {"X", "Starlink in Earth's shadow", {'X'}, {CMD_STARLINK_DARK}},
    {"v", "Motion vectors", {'v'}, {CMD_VECTORS}},
    {"+ -", "Faintest stars (mag)", {'+', '=', '-'}, {CMD_THRESH_UP, CMD_THRESH_UP, CMD_THRESH_DOWN}},
    {"space", "Pause time", {' '}, {CMD_PAUSE}},
    {"< >", "Speed", {'<', ',', '>'}, {CMD_SLOWER, CMD_SLOWER, CMD_FASTER}},
    {"", "", {'.'}, {CMD_FASTER}}, // Unshifted '>' on most layouts
    {"n", "Back to now", {'n'}, {CMD_NOW}},
    {"z Z", "Zoom in / out", {'z', 'Z'}, {CMD_ZOOM_IN, CMD_ZOOM_OUT}},
    {"hjkl",
     "Move around when zoomed (or arrows)",
     {'h', 'j', 'k', 'l', KEY_LEFT, KEY_DOWN, KEY_UP, KEY_RIGHT},
     {CMD_PAN_LEFT, CMD_PAN_DOWN, CMD_PAN_UP, CMD_PAN_RIGHT, CMD_PAN_LEFT, CMD_PAN_DOWN, CMD_PAN_UP, CMD_PAN_RIGHT}},
    {"1-4", "Look at a quadrant", {'1', '2', '3', '4'}, {CMD_QUADRANT, CMD_QUADRANT, CMD_QUADRANT, CMD_QUADRANT}},
    {"[ ]", "Rotate the dome", {'[', ']'}, {CMD_ROT_LEFT, CMD_ROT_RIGHT}},
    {"R", "Reset view", {'R'}, {CMD_RESET_VIEW}},
    {"?", "This help", {'?'}, {CMD_HELP}},
    {"q ESC", "Quit (ESC closes help)", {'q', KEY_ESCAPE}, {CMD_QUIT, CMD_QUIT}},
};

#define NUM_KEY_ROWS (sizeof(key_rows) / sizeof(key_rows[0]))

static enum UiCommand lookup(int ch)
{
    for (unsigned int i = 0; i < NUM_KEY_ROWS; ++i)
    {
        for (int k = 0; k < 8; ++k)
        {
            if (key_rows[i].cmd[k] != CMD_NONE && key_rows[i].key[k] == ch)
            {
                return key_rows[i].cmd[k];
            }
        }
    }
    return CMD_NONE;
}

static const char *on_off(bool value)
{
    return value ? "on" : "off";
}

void ui_toast(struct UiState *ui, double mono, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(ui->toast, sizeof(ui->toast), fmt, args);
    va_end(args);
    ui->toast_until = mono + UI_TOAST_SECONDS;
}

const char *ui_current_toast(const struct UiState *ui, double mono)
{
    if (ui->toast[0] == '\0' || mono >= ui->toast_until)
    {
        return NULL;
    }
    return ui->toast;
}

void ui_speed_text(const struct SimClock *clock, char *buf, size_t len)
{
    if (clock->paused)
    {
        snprintf(buf, len, "paused (%gx)", clock->speed);
    }
    else
    {
        snprintf(buf, len, "%gx", clock->speed);
    }
}

void ui_view_text(const struct Conf *config, char *buf, size_t len)
{
    int tiles = config->zoom > 1 ? config->zoom : 1;
    if (tiles == 1)
    {
        snprintf(buf, len, "Zoom: whole sky");
        return;
    }

    // What the centre of the tile looks at: invert the stereographic projection
    double cx = -1.0 + (2.0 * config->tile_x + 1.0) / tiles;
    double cy = 1.0 - (2.0 * config->tile_y + 1.0) / tiles;
    double radius = fmin(1.0, hypot(cx, cy));
    double azimuth = atan2(cy, cx) - config->rotation - M_PI / 2.0;
    double altitude = M_PI / 2.0 - 2.0 * atan(radius);

    // A minimap: a quadrant block at 2x, a 4x4 grid of braille dots at 4x
    char map[16];
    if (!config->unicode)
    {
        snprintf(map, sizeof(map), "(%d,%d)", config->tile_x + 1, config->tile_y + 1);
    }
    else if (tiles == 2)
    {
        static const char *quadrants[4] = {"▘", "▝", "▖", "▗"};
        snprintf(map, sizeof(map), "%s", quadrants[config->tile_y * 2 + config->tile_x]);
    }
    else
    {
        static const unsigned char dots[4][2] = {{0x01, 0x08}, {0x02, 0x10}, {0x04, 0x20}, {0x40, 0x80}};
        unsigned char cells[2] = {0, 0};
        cells[config->tile_x / 2] = dots[config->tile_y % 4][config->tile_x % 2];
        int n = 0;
        map[n++] = '[';
        for (int c = 0; c < 2; ++c)
        {
            map[n++] = (char)0xE2;
            map[n++] = (char)(0xA0 | (cells[c] >> 6));
            map[n++] = (char)(0x80 | (cells[c] & 0x3F));
        }
        map[n++] = ']';
        map[n] = '\0';
    }

    if (altitude < 1.0 * M_PI / 180.0)
    {
        snprintf(buf, len, "Zoom %dx %s %s horizon", tiles, map, compass_point(azimuth));
    }
    else
    {
        snprintf(buf, len, "Zoom %dx %s %s %.0f° up", tiles, map, compass_point(azimuth), altitude * 180.0 / M_PI);
    }
}

static double rotation_degrees(const struct Conf *config)
{
    double deg = fmod(config->rotation * 180.0 / M_PI, 360.0);
    return deg < 0 ? deg + 360.0 : deg;
}

enum UiAction ui_handle_key(int ch, struct Conf *config, struct UiState *ui, struct SimClock *clock,
                            const struct UiContext *ctx)
{
    if (config->quit_on_any)
    {
        return UI_QUIT;
    }

    enum UiCommand cmd = lookup(ch);
    char speed[64];

    switch (cmd)
    {
    case CMD_NONE:
        return UI_NONE;

    case CMD_COLOR:
        if (!ctx->has_colors)
        {
            ui_toast(ui, ctx->mono, "No color support in this terminal");
            return UI_NONE;
        }
        config->color = !config->color;
        ui_toast(ui, ctx->mono, "Colors: %s", on_off(config->color));
        return UI_NONE;

    case CMD_CONSTELL:
        config->constell = !config->constell;
        ui_toast(ui, ctx->mono, "Constellations: %s", on_off(config->constell));
        return UI_NONE;

    case CMD_GRID:
        config->grid = !config->grid;
        ui_toast(ui, ctx->mono, "Grid: %s", on_off(config->grid));
        return UI_NONE;

    case CMD_UNICODE:
        config->unicode = !config->unicode;
        ui_toast(ui, ctx->mono, "Unicode: %s", on_off(config->unicode));
        return UI_NONE;

    case CMD_BRAILLE:
        config->braille = !config->braille;
        if (config->braille && !config->unicode)
        {
            ui_toast(ui, ctx->mono, "Braille lines: on (needs Unicode: press u)");
        }
        else
        {
            ui_toast(ui, ctx->mono, "Braille lines: %s", on_off(config->braille));
        }
        return UI_NONE;

    case CMD_METADATA:
        config->metadata = !config->metadata;
        ui_toast(ui, ctx->mono, "Metadata: %s", on_off(config->metadata));
        return UI_NONE;

    case CMD_NIGHT:
        if (!ctx->has_colors)
        {
            ui_toast(ui, ctx->mono, "Night vision needs a color terminal");
            return UI_NONE;
        }
        config->night = !config->night;
        ui_toast(ui, ctx->mono, "Night vision: %s", on_off(config->night));
        return UI_NONE;

    case CMD_STATIONS:
        config->stations = !config->stations;
        if (config->stations && ctx->stations_count == 0)
        {
            ui_toast(ui, ctx->mono, "Stations: no data (run once online)");
        }
        else
        {
            ui_toast(ui, ctx->mono, "Stations: %s", on_off(config->stations));
        }
        return UI_NONE;

    case CMD_STARLINK:
        config->starlink = !config->starlink;
        if (!config->starlink)
        {
            ui_toast(ui, ctx->mono, "Starlink: off");
            return UI_NONE;
        }
        ui_toast(ui, ctx->mono, "Starlink: on");
        return UI_STARLINK;

    case CMD_STARLINK_DARK:
        config->starlink_dark = !config->starlink_dark;
        ui_toast(ui, ctx->mono, "Starlink in shadow: %s", config->starlink_dark ? "shown (dim)" : "hidden");
        return UI_NONE;

    case CMD_VECTORS:
        config->vectors = !config->vectors;
        if (config->vectors)
        {
            ui_toast(ui, ctx->mono, "Vectors: satellites 10 s ahead, Sun/Moon/planets 1 day vs stars");
            return UI_SATELLITES;
        }
        ui_toast(ui, ctx->mono, "Vectors: off");
        return UI_NONE;

    case CMD_THRESH_UP:
        config->threshold = MIN(THRESHOLD_MAX, config->threshold + THRESHOLD_STEP);
        ui_toast(ui, ctx->mono, "Faintest stars: mag %.1f", config->threshold);
        return UI_NONE;

    case CMD_THRESH_DOWN:
        config->threshold = MAX(THRESHOLD_MIN, config->threshold - THRESHOLD_STEP);
        ui_toast(ui, ctx->mono, "Faintest stars: mag %.1f", config->threshold);
        return UI_NONE;

    case CMD_PAUSE:
        sim_clock_set_paused(clock, !clock->paused, ctx->wall);
        ui_speed_text(clock, speed, sizeof(speed));
        ui_toast(ui, ctx->mono, clock->paused ? "Time: %s" : "Time: running (%s)", speed);
        return UI_NONE;

    case CMD_FASTER:
    case CMD_SLOWER:
        sim_clock_step_speed(clock, cmd == CMD_FASTER ? 1 : -1, ctx->wall);
        ui_speed_text(clock, speed, sizeof(speed));
        ui_toast(ui, ctx->mono, "Speed: %s", speed);
        return UI_NONE;

    case CMD_NOW:
        sim_clock_now(clock, ctx->wall);
        ui_toast(ui, ctx->mono, "Now, realtime");
        return UI_NONE;

    case CMD_ROT_LEFT:
    case CMD_ROT_RIGHT:
        config->rotation += cmd == CMD_ROT_RIGHT ? ROTATION_STEP : -ROTATION_STEP;
        config->rotation = fmod(config->rotation, 2.0 * M_PI);
        ui_toast(ui, ctx->mono, "Rotation: %.0f°", rotation_degrees(config));
        return UI_NONE;

    case CMD_RESET_VIEW:
        config->rotation = 0.0;
        config->zoom = 1;
        config->tile_x = config->tile_y = 0;
        ui_toast(ui, ctx->mono, "View reset: whole sky, north up");
        return UI_NONE;

    case CMD_ZOOM_IN:
        if (config->zoom >= 4)
        {
            ui_toast(ui, ctx->mono, "Zoom: 4x is the closest");
            return UI_NONE;
        }
        if (config->zoom <= 1)
        {
            // All four quadrants meet at the zenith: start bottom-left
            config->zoom = 2;
            config->tile_x = 0;
            config->tile_y = 1;
        }
        else
        {
            // Keep the part of the quadrant nearest the zenith
            config->zoom = 4;
            config->tile_x = 2 * config->tile_x + (1 - config->tile_x);
            config->tile_y = 2 * config->tile_y + (1 - config->tile_y);
        }
        ui_view_text(config, speed, sizeof(speed));
        ui_toast(ui, ctx->mono, "%s", speed);
        return UI_NONE;

    case CMD_ZOOM_OUT:
        if (config->zoom <= 1)
        {
            ui_toast(ui, ctx->mono, "Zoom: whole sky");
            return UI_NONE;
        }
        config->zoom /= 2;
        config->tile_x /= 2;
        config->tile_y /= 2;
        if (config->zoom <= 1)
        {
            config->tile_x = config->tile_y = 0;
            ui_toast(ui, ctx->mono, "Zoom: whole sky");
            return UI_NONE;
        }
        ui_view_text(config, speed, sizeof(speed));
        ui_toast(ui, ctx->mono, "%s", speed);
        return UI_NONE;

    case CMD_PAN_LEFT:
    case CMD_PAN_RIGHT:
    case CMD_PAN_UP:
    case CMD_PAN_DOWN: {
        if (config->zoom <= 1)
        {
            ui_toast(ui, ctx->mono, "Zoom in with z to move around");
            return UI_NONE;
        }
        int x = config->tile_x + (cmd == CMD_PAN_RIGHT) - (cmd == CMD_PAN_LEFT);
        int y = config->tile_y + (cmd == CMD_PAN_DOWN) - (cmd == CMD_PAN_UP);
        if (x < 0 || y < 0 || x >= config->zoom || y >= config->zoom)
        {
            ui_toast(ui, ctx->mono, "Edge of the sky");
            return UI_NONE;
        }
        config->tile_x = x;
        config->tile_y = y;
        ui_view_text(config, speed, sizeof(speed));
        ui_toast(ui, ctx->mono, "%s", speed);
        return UI_NONE;
    }

    case CMD_QUADRANT:
        config->zoom = 2;
        config->tile_x = (ch - '1') % 2;
        config->tile_y = (ch - '1') / 2;
        ui_view_text(config, speed, sizeof(speed));
        ui_toast(ui, ctx->mono, "%s", speed);
        return UI_NONE;

    case CMD_HELP:
        ui->help_open = !ui->help_open;
        return UI_NONE;

    case CMD_QUIT:
        if (ch == KEY_ESCAPE && ui->help_open)
        {
            ui->help_open = false;
            return UI_NONE;
        }
        return UI_QUIT;
    }

    return UI_NONE;
}

static void state_text(enum UiCommand cmd, const struct Conf *config, const struct SimClock *clock, char *buf, size_t len)
{
    buf[0] = '\0';
    switch (cmd)
    {
    case CMD_COLOR:
        snprintf(buf, len, "%s", on_off(config->color));
        break;
    case CMD_CONSTELL:
        snprintf(buf, len, "%s", on_off(config->constell));
        break;
    case CMD_GRID:
        snprintf(buf, len, "%s", on_off(config->grid));
        break;
    case CMD_UNICODE:
        snprintf(buf, len, "%s", on_off(config->unicode));
        break;
    case CMD_BRAILLE:
        snprintf(buf, len, "%s", on_off(config->braille));
        break;
    case CMD_METADATA:
        snprintf(buf, len, "%s", on_off(config->metadata));
        break;
    case CMD_NIGHT:
        snprintf(buf, len, "%s", on_off(config->night));
        break;
    case CMD_STATIONS:
        snprintf(buf, len, "%s", on_off(config->stations));
        break;
    case CMD_STARLINK:
        snprintf(buf, len, "%s", on_off(config->starlink));
        break;
    case CMD_STARLINK_DARK:
        snprintf(buf, len, "%s", config->starlink_dark ? "shown" : "hidden");
        break;
    case CMD_VECTORS:
        snprintf(buf, len, "%s", on_off(config->vectors));
        break;
    case CMD_THRESH_UP:
        snprintf(buf, len, "%.1f", config->threshold);
        break;
    case CMD_PAUSE:
        snprintf(buf, len, "%s", clock->paused ? "paused" : "running");
        break;
    case CMD_SLOWER:
        ui_speed_text(clock, buf, len);
        break;
    case CMD_ROT_LEFT:
        snprintf(buf, len, "%.0f°", rotation_degrees(config));
        break;
    case CMD_ZOOM_IN:
        snprintf(buf, len, "%dx", config->zoom > 1 ? config->zoom : 1);
        break;
    default:
        break;
    }
}

void ui_draw_help(const struct Conf *config, const struct SimClock *clock, attr_t attr)
{
    const int width = 50;
    int height = 4; // Border, title, blank line, border
    for (unsigned int i = 0; i < NUM_KEY_ROWS; ++i)
    {
        height += key_rows[i].keys[0] != '\0';
    }

    int h = MIN(height, LINES);
    int w = MIN(width, COLS);
    if (h < 3 || w < 10)
    {
        return;
    }

    WINDOW *win = newwin(h, w, (LINES - h) / 2, (COLS - w) / 2);
    if (win == NULL)
    {
        return;
    }
    wbkgd(win, attr);
    werase(win);
    box(win, 0, 0);
    mvwaddstr_truncate(win, 1, 2, "astroterm keys");

    int row = 3;
    for (unsigned int i = 0; i < NUM_KEY_ROWS && row < h - 1; ++i)
    {
        const struct KeyRow *kr = &key_rows[i];
        if (kr->keys[0] == '\0')
        {
            continue;
        }
        char state[32];
        state_text(kr->cmd[0], config, clock, state, sizeof(state));

        char line[96];
        snprintf(line, sizeof(line), "%-7s %-26s %s", kr->keys, kr->label, state);
        mvwaddstr_truncate(win, row++, 2, line);
    }

    wnoutrefresh(win);
    delwin(win);
}

void ui_draw_corner(WINDOW *sky_win, const char *const *lines, int num_lines, attr_t attr)
{
    int widest = 0;
    for (int i = 0; i < num_lines; ++i)
    {
        if (lines[i] != NULL)
        {
            widest = MAX(widest, (int)strlen(lines[i]));
        }
    }
    if (widest == 0)
    {
        return;
    }

    // Prefer the margin beside the (square) sky window; fall back to its
    // bottom-left corner, which lies outside the circular dome
    int sky_x = getbegx(sky_win);
    WINDOW *target = sky_x > widest ? stdscr : sky_win;
    int bottom = getmaxy(target) - 1;

    wattron(target, attr);
    int row = bottom;
    for (int i = num_lines - 1; i >= 0 && row >= 0; --i)
    {
        if (lines[i] != NULL && lines[i][0] != '\0')
        {
            mvwaddstr_truncate(target, row--, 0, lines[i]);
        }
    }
    wattroff(target, attr);
}
