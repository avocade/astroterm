/* Interactive controls: keybindings, the help modal and toasts.
 *
 * One table defines every key, its help row and its toast text. Key handling
 * (ui_handle_key) is a pure state transition over the display config, the UI
 * state and the simulation clock: it never draws, so it can be unit tested.
 * Everything is redrawn every frame, so most keys need nothing more; the few
 * side effects that do are named by the returned action.
 */

#ifndef UI_H
#define UI_H

#include "core.h"
#include "sim_clock.h"

#include <curses.h>
#include <stdbool.h>

#define UI_TOAST_SECONDS 2.0
#define UI_TOAST_LEN 128

enum UiAction
{
    UI_NONE = 0,
    UI_QUIT,
    UI_STARLINK,   // Starlink turned on: load if needed, update now, report counts
    UI_SATELLITES, // Satellite state needs recomputing now (e.g. vectors on)
};

struct UiState
{
    bool help_open;
    char toast[UI_TOAST_LEN];
    double toast_until; // Monotonic seconds
};

/* Read-only facts key handling may need
 */
struct UiContext
{
    double wall; // Realtime seconds (for the sim clock)
    double mono; // Monotonic seconds (for toasts)
    bool has_colors;
    int stations_count; // Stations with usable data
};

/* Apply one keypress. Returns the side effect the caller must perform
 */
enum UiAction ui_handle_key(int ch, struct Conf *config, struct UiState *ui, struct SimClock *clock,
                            const struct UiContext *ctx);

/* Show a toast for UI_TOAST_SECONDS
 */
void ui_toast(struct UiState *ui, double mono, const char *fmt, ...);

/* Current toast text, or NULL if none is showing
 */
const char *ui_current_toast(const struct UiState *ui, double mono);

/* Draw the help modal centered on the screen (creates and frees its own
 * window after queueing it with wnoutrefresh)
 */
void ui_draw_help(const struct Conf *config, const struct SimClock *clock, attr_t attr);

/* Draw short status lines in the bottom-left corner: on stdscr when the sky
 * window leaves room beside it, otherwise inside the sky window's corner
 */
void ui_draw_corner(WINDOW *sky_win, const char *const *lines, int num_lines, attr_t attr);

/* Describe the zoomed view, e.g. "Zoom 2x ▖ SE 38° up" (or "Zoom: whole sky")
 */
void ui_view_text(const struct Conf *config, char *buf, size_t len);

/* Human readable speed, e.g. "60x" or "paused (60x)"
 */
void ui_speed_text(const struct SimClock *clock, char *buf, size_t len);

#endif // UI_H
