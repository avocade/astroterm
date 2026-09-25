#include "drawing.h"

#include <curses.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

// The difference in logic between drawing an ASCII and unicode line differs
// enough that having two different functions is warranted

void draw_line_ASCII(WINDOW *win, int ya, int xa, int yb, int xb)
{
    // A zero-length segment has no direction (and would divide 0 by 0)
    if (ya == yb && xa == xb)
    {
        return;
    }

    // The logic here is not particularly elegant or efficient

    int dy = yb - ya;
    int dx = xb - xa;

    // "Joint"/junction character
    char slope;

    // No intelligence... just choose based on case
    if (dx > 0)
    {
        slope = dy > 0 ? '\\' : '/';
    }
    else
    {
        slope = dy > 0 ? '/' : '\\';
    }

    if (abs(dy) >= abs(dx))
    {
        int y = 0;
        double x = 0.0;

        // Step size
        int sy = (dy > 0) ? 1 : -1;
        double sx = (double)dx / abs(dy);

        while (abs(y) <= abs(dy))
        {
            int curr_y = ya + y;
            int curr_x = xa + (int)round(x);

            int next_y = ya + y + sy;
            int next_x = xa + (int)round(x + sx);

            mvwaddch(win, curr_y, curr_x, '|');

            // Draw slope if we jump a column
            if (next_x != curr_x)
            {
                mvwaddch(win, curr_y, curr_x, slope);
            }

            y += sy;
            x += sx;
        }
    }
    else
    {
        double y = 0.0;
        int x = 0;

        // Step size
        double sy = (double)dy / abs(dx);
        int sx = (dx > 0) ? 1 : -1;

        while (abs(x) <= abs(dx))
        {
            int curr_y = ya + (int)round(y);
            int curr_x = xa + x;

            int next_y = ya + (int)round(y + sy);
            int next_x = xa + x + sx;

            // Edge case where we draw a horizontal line
            char horizontal = ya == yb ? '-' : '_';

            mvwaddch(win, curr_y, curr_x, horizontal);

            // This bit requires a little more logic: drawing '-' characters
            // isn't as smooth as '_' characters. Thus, to draw a good lookin'
            // line, the slope characters must be drawn in a particular way...
            // (remember we're in screen space coordinates and the y-axis is
            // "flipped")

            // Draw slope if we jump a row
            if (next_y != curr_y)
            {
                if (dy > 0)
                {
                    // We're moving "down": add the slope to the next position
                    // Make sure we're not on the last cell first
                    if (curr_y != yb)
                    {
                        mvwaddch(win, next_y, next_x, slope);

                        // Skip drawing the next position the next iteration
                        y += sy;
                        x += sx;
                    }
                }
                else
                {
                    // We're moving "up": just add the slope to the current cell
                    mvwaddch(win, curr_y, curr_x, slope);
                }
            }

            y += sy;
            x += sx;
        }
    }

    // Could add asterisks at beginning and end of segment to "prettify",
    // but not for this application
    // mvwaddch(win, ya, xa, '*');
    // mvwaddch(win, yb, xb, '*');
}

void draw_line_smooth(WINDOW *win, int ya, int xa, int yb, int xb)
{
    // A zero-length segment has no direction (and would divide 0 by 0)
    if (ya == yb && xa == xb)
    {
        return;
    }

    // The logic here is not particularly elegant or efficient

    int dy = yb - ya;
    int dx = xb - xa;

    // "Joint"/junction characters
    char *joint_a;
    char *joint_b;

    if (abs(dy) > abs(dx))
    {
        // No intelligence... just choose based on case
        if (dx > 0)
        {
            joint_a = dy > 0 ? "╰" : "╭";
            joint_b = dy > 0 ? "╮" : "╯";
        }
        else
        {
            joint_a = dy > 0 ? "╯" : "╮";
            joint_b = dy > 0 ? "╭" : "╰";
        }

        int y = 0;
        double x = 0.0;

        // Step size
        int sy = (dy > 0) ? 1 : -1;
        double sx = (double)dx / abs(dy);

        while (abs(y) <= abs(dy))
        {
            int curr_y = ya + y;
            int curr_x = xa + (int)round(x);

            int next_y = ya + y + sy;
            int next_x = xa + (int)round(x + sx);

            mvwaddstr(win, curr_y, curr_x, "│");

            // Draw joint if we jump a column && we're not on the last cell
            if (curr_x != next_x && curr_x != xb)
            {
                mvwaddstr(win, curr_y, curr_x, joint_a);
                mvwaddstr(win, curr_y, next_x, joint_b);
            }

            y += sy;
            x += sx;
        }
    }
    else
    {
        // No intelligence... just choose based on case
        if (dy > 0)
        {
            joint_a = dx > 0 ? "╮" : "╭";
            joint_b = dx > 0 ? "╰" : "╯";
        }
        else
        {
            joint_b = dx > 0 ? "╭" : "╮";
            joint_a = dx > 0 ? "╯" : "╰";
        }

        double y = 0.0;
        int x = 0;

        // Step size
        double sy = (double)dy / abs(dx);
        int sx = (dx > 0) ? 1 : -1;

        while (abs(x) <= abs(dx))
        {
            int curr_y = ya + (int)round(y);
            int curr_x = xa + x;

            int next_y = ya + (int)round(y + sy);
            int next_x = xa + x + sx;

            mvwaddstr(win, curr_y, curr_x, "─");

            // Draw joint if we jump a row && we're not on the last cell
            if (curr_y != next_y && curr_y != yb)
            {
                mvwaddstr(win, curr_y, curr_x, joint_a);
                mvwaddstr(win, next_y, curr_x, joint_b);
            }

            y += sy;
            x += sx;
        }
    }
}

void draw_line_dotted(WINDOW *win, int ya, int xa, int yb, int xb)
{
    // A zero-length segment has no direction (and would divide 0 by 0)
    if (ya == yb && xa == xb)
    {
        return;
    }

    // The logic here is not particularly elegant or efficient

    int dy = yb - ya;
    int dx = xb - xa;

    char *fill = "•";

    if (abs(dy) >= abs(dx))
    {
        int y = 0;
        double x = 0.0;

        // Step size
        int sy = (dy > 0) ? 1 : -1;
        double sx = (double)dx / abs(dy);

        while (abs(y) <= abs(dy))
        {
            int curr_y = ya + y;
            int curr_x = xa + (int)round(x);

            mvwaddstr(win, curr_y, curr_x, fill);

            y += sy;
            x += sx;
        }
    }
    else
    {
        double y = 0.0;
        int x = 0;

        // Step size
        double sy = (double)dy / abs(dx);
        int sx = (dx > 0) ? 1 : -1;

        while (abs(x) <= abs(dx))
        {
            int curr_y = ya + (int)round(y);
            int curr_x = xa + x;

            mvwaddstr(win, curr_y, curr_x, fill);

            y += sy;
            x += sx;
        }
    }
}

#define MAX_COLS 1024
#define MAX_ROWS 1024

static unsigned char braille_layer[MAX_ROWS][MAX_COLS];

void clear_braille_lines(void)
{
    memset(braille_layer, 0, sizeof(braille_layer));
}

void draw_braille_cell(WINDOW *win, int y, int x, unsigned char mask)
{
    if (mask == 0 || y < 0 || y >= MAX_ROWS || x < 0 || x >= MAX_COLS)
        return;

    mask |= braille_layer[y][x];
    braille_layer[y][x] = mask;

    unsigned char utf8[4];
    utf8[0] = 0xE2;
    utf8[1] = 0xA0 | (mask >> 6);   // Top 2 bits of mask
    utf8[2] = 0x80 | (mask & 0x3F); // Bottom 6 bits of mask
    utf8[3] = '\0';

    mvwaddstr(win, y, x, (char *)utf8);
}

void draw_line_braille(WINDOW *win, int ya, int xa, int yb, int xb)
{
    // ncurses coordinates
    int curs_xa = xa;
    int curs_ya = ya;

    // braille coordinates
    if (xa < xb)
    {
        xa = xa * 2 + 1;
        xb = xb * 2;
    }
    else if (xa > xb)
    {
        xa = xa * 2;
        xb = xb * 2 + 1;
    }
    else
    {
        xa = xa * 2;
        xb = xb * 2;
    }

    if (ya < yb)
    {
        ya = ya * 4 + 2;
        yb = yb * 4 + 1;
    }
    else if (ya > yb)
    {
        ya = ya * 4 + 1;
        yb = yb * 4 + 2;
    }
    else
    {
        ya = ya * 4 + 1;
        yb = yb * 4 + 1;
    }

    int dx = abs(xb - xa);
    int dy = abs(yb - ya);

    int incx = (xa < xb) ? 1 : -1;
    int incy = (ya < yb) ? 1 : -1;

    int err = ((dx > dy) ? dx : -dy) / 2;
    int e2 = 0;

    unsigned char braille_mask = 0;

    for (;;)
    {
        int curs_x = xa / 2;
        int curs_y = ya / 4;

        if (curs_x != curs_xa || curs_y != curs_ya)
        {
            draw_braille_cell(win, curs_ya, curs_xa, braille_mask);
            braille_mask = 0;
            curs_xa = curs_x;
            curs_ya = curs_y;
        }

        int dot_x = xa % 2;
        int dot_y = ya % 4;

        static const unsigned char dot_map[4][2] = {{0x01, 0x08}, {0x02, 0x10}, {0x04, 0x20}, {0x40, 0x80}};

        braille_mask |= dot_map[dot_y][dot_x];

        if (xa == xb && ya == yb)
        {
            break;
        }

        e2 = err;
        if (e2 > -dx)
        {
            err -= dy;
            xa += incx;
        }
        if (e2 < dy)
        {
            err += dx;
            ya += incy;
        }
    }

    draw_braille_cell(win, curs_ya, curs_xa, braille_mask);
}

static const unsigned char braille_dot_bits[4][2] = {{0x01, 0x08}, {0x02, 0x10}, {0x04, 0x20}, {0x40, 0x80}};

bool braille_canvas_resize(struct BrailleCanvas *canvas, int rows, int cols)
{
    if (rows != canvas->rows || cols != canvas->cols || canvas->mask == NULL)
    {
        free(canvas->mask);
        canvas->rows = rows > 0 ? rows : 0;
        canvas->cols = cols > 0 ? cols : 0;
        canvas->mask = calloc((size_t)canvas->rows * canvas->cols + 1, 1);
        return canvas->mask != NULL;
    }
    braille_canvas_clear(canvas);
    return true;
}

void braille_canvas_clear(struct BrailleCanvas *canvas)
{
    if (canvas->mask != NULL)
    {
        memset(canvas->mask, 0, (size_t)canvas->rows * canvas->cols);
    }
}

void braille_canvas_free(struct BrailleCanvas *canvas)
{
    free(canvas->mask);
    canvas->mask = NULL;
    canvas->rows = canvas->cols = 0;
}

void braille_canvas_dot(struct BrailleCanvas *canvas, int dot_row, int dot_col)
{
    if (canvas->mask == NULL || dot_row < 0 || dot_col < 0)
    {
        return;
    }
    int row = dot_row / 4;
    int col = dot_col / 2;
    if (row >= canvas->rows || col >= canvas->cols)
    {
        return;
    }
    canvas->mask[row * canvas->cols + col] |= braille_dot_bits[dot_row % 4][dot_col % 2];
}

void braille_canvas_line(struct BrailleCanvas *canvas, int row_a, int col_a, int row_b, int col_b)
{
    int dx = abs(col_b - col_a);
    int dy = -abs(row_b - row_a);
    int sx = col_a < col_b ? 1 : -1;
    int sy = row_a < row_b ? 1 : -1;
    int err = dx + dy;

    // Bresenham; bounded by the line length so bad input cannot spin
    for (int steps = 0; steps <= dx - dy; ++steps)
    {
        braille_canvas_dot(canvas, row_a, col_a);
        if (row_a == row_b && col_a == col_b)
        {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy)
        {
            err += dy;
            col_a += sx;
        }
        if (e2 <= dx)
        {
            err += dx;
            row_a += sy;
        }
    }
}

void braille_canvas_flush(const struct BrailleCanvas *canvas, WINDOW *win)
{
    if (canvas->mask == NULL)
    {
        return;
    }
    for (int row = 0; row < canvas->rows; ++row)
    {
        for (int col = 0; col < canvas->cols; ++col)
        {
            unsigned char mask = canvas->mask[row * canvas->cols + col];
            if (mask == 0)
            {
                continue;
            }
            char utf8[4] = {(char)0xE2, (char)(0xA0 | (mask >> 6)), (char)(0x80 | (mask & 0x3F)), '\0'};
            mvwaddstr(win, row, col, utf8);
        }
    }
}

enum FillType
{
    HORIZONTAL,
    VERTICAL,
    CORNER,
};

// Reference: https://dai.fmph.uniba.sk/upload/0/01/Ellipse.pdf

void print_chars_ellipse_ASCII(WINDOW *win, int center_y, int center_x, int y, int x, int fill)
{
    switch (fill)
    {
    case CORNER:
        mvwaddch(win, center_y - y, center_x + x, '\\'); // Quad I
        mvwaddch(win, center_y - y, center_x - x, '/');  // Quad II
        mvwaddch(win, center_y + y, center_x - x, '\\'); // Quad III
        mvwaddch(win, center_y + y, center_x + x, '/');  // Quad IV
        break;

    case VERTICAL:
        mvwaddch(win, center_y - y, center_x + x, '|');
        mvwaddch(win, center_y - y, center_x - x, '|');
        mvwaddch(win, center_y + y, center_x - x, '|');
        mvwaddch(win, center_y + y, center_x + x, '|');
        break;

    case HORIZONTAL:
        mvwaddch(win, center_y - y, center_x + x, '-');
        mvwaddch(win, center_y - y, center_x - x, '-');
        mvwaddch(win, center_y + y, center_x - x, '-');
        mvwaddch(win, center_y + y, center_x + x, '-');
        break;
    }
}

void print_chars_ellipse_unicode(WINDOW *win, int center_y, int center_x, int y, int x, int fill)
{
    // TODO: def not correct
    switch (fill)
    {
    case CORNER:
        // Quad I
        mvwaddstr(win, center_y - y - 1, center_x + x, "╮");
        mvwaddstr(win, center_y - y, center_x + x, "╰");
        // Quad II
        mvwaddstr(win, center_y - y - 1, center_x - x, "╭");
        mvwaddstr(win, center_y - y, center_x - x, "╯");
        // Quad III
        mvwaddstr(win, center_y + y - 1, center_x - x, "╮");
        mvwaddstr(win, center_y + y, center_x - x, "╰");
        // Quad IV
        mvwaddstr(win, center_y + y - 1, center_x + x, "╭");
        mvwaddstr(win, center_y + y, center_x + x, "╯");
        break;

    case VERTICAL:
        mvwaddstr(win, center_y - y, center_x + x, "│");
        mvwaddstr(win, center_y - y, center_x - x, "│");
        mvwaddstr(win, center_y + y, center_x - x, "│");
        mvwaddstr(win, center_y + y, center_x + x, "│");
        break;

    case HORIZONTAL:
        mvwaddstr(win, center_y - y, center_x + x, "─");
        mvwaddstr(win, center_y - y, center_x - x, "─");
        mvwaddstr(win, center_y + y, center_x - x, "─");
        mvwaddstr(win, center_y + y, center_x + x, "─");
        break;
    }

    return;
}

int ellipse_error(int y, int x, int rad_y, int rad_x)
{
    return (rad_x * rad_x + x * x) + (rad_y * rad_y + y * y) - (rad_x * rad_x * rad_y * rad_y);
}

void draw_ellipse(WINDOW *win, int center_y, int center_x, int rad_y, int rad_x, bool no_unicode)
{
    int y = 0;
    int x = rad_x;

    int y_next = y;
    int x_next = x;

    // Point where slope = -1
    int magicY = (int)sqrt(pow((double)rad_y, 4.0) / (rad_x * rad_x + rad_y * rad_y));

    // Print first part of first quadrant: slope is > -1
    while (y_next > magicY)
    {
        // If outside ellipse, move inward
        y_next = y + 1;
        x_next = (ellipse_error(y_next, x_next, rad_y, rad_x) > 0) ? x - 1 : x;

        bool corner = y_next > y && x_next < x;
        bool vertical = y_next > y && x_next == x;

        char fill = corner ? CORNER : vertical ? VERTICAL : HORIZONTAL;

        if (no_unicode)
        {
            print_chars_ellipse_ASCII(win, center_y, center_x, y, x, fill);
        }
        else
        {
            print_chars_ellipse_unicode(win, center_y, center_x, y, x, fill);
        }

        y = y_next;
        x = x_next;
    }

    // Print second part of first quadrant: slope is < -1
    while (x_next > 0)
    {
        // If inside ellipse, move outward
        y_next = (ellipse_error(y_next, x_next, rad_y, rad_x) < 0) ? y + 1 : y;
        x_next = x - 1;

        bool corner = y_next > y && x_next < x;
        bool vertical = y_next > y && x_next == x;

        char fill = corner ? CORNER : vertical ? VERTICAL : HORIZONTAL;

        if (no_unicode)
        {
            print_chars_ellipse_ASCII(win, center_y, center_x, y, x, fill);
        }
        else
        {
            print_chars_ellipse_unicode(win, center_y, center_x, y, x, fill);
        }

        y = y_next;
        x = x_next;
    }

    return;
}
