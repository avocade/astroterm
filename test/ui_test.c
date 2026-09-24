#include "ui.h"
#include "unity.h"

#include <math.h>
#include <string.h>

static struct Conf config;
static struct UiState ui;
static struct SimClock clock_;
static struct UiContext ctx;

void setUp(void)
{
    memset(&config, 0, sizeof(config));
    config.threshold = 5.0f;
    memset(&ui, 0, sizeof(ui));
    sim_clock_init(&clock_, 2461308.0, 0.0, 1.0);
    ctx = (struct UiContext){.wall = 0.0, .mono = 100.0, .has_colors = true};
}
void tearDown(void)
{
}

static enum UiAction press(int ch)
{
    return ui_handle_key(ch, &config, &ui, &clock_, &ctx);
}

void test_display_toggles_flip_and_toast(void)
{
    struct
    {
        int key;
        bool *field;
        const char *toast_on;
    } cases[] = {
        {'c', &config.color, "Colors: on"},       {'C', &config.constell, "Constellations: on"},
        {'g', &config.grid, "Grid: on"},          {'u', &config.unicode, "Unicode: on"},
        {'m', &config.metadata, "Metadata: on"},  {'r', &config.night, "Night vision: on"},
    };

    for (unsigned int i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        TEST_ASSERT_FALSE(*cases[i].field);
        press(cases[i].key);
        TEST_ASSERT_TRUE(*cases[i].field);
        TEST_ASSERT_EQUAL_STRING(cases[i].toast_on, ui_current_toast(&ui, ctx.mono));
        press(cases[i].key);
        TEST_ASSERT_FALSE(*cases[i].field);
    }
}

void test_actions_request_side_effects(void)
{
    TEST_ASSERT_EQUAL_INT(UI_PALETTE, press('c'));
    TEST_ASSERT_EQUAL_INT(UI_LAYOUT, press('m'));
    TEST_ASSERT_EQUAL_INT(UI_NONE, press('g'));
    TEST_ASSERT_EQUAL_INT(UI_NONE, press('Z')); // Unbound key
}

void test_braille_needs_unicode(void)
{
    press('b');
    TEST_ASSERT_TRUE(config.braille);
    TEST_ASSERT_NOT_NULL(strstr(ui_current_toast(&ui, ctx.mono), "needs Unicode"));
}

void test_colors_need_a_color_terminal(void)
{
    ctx.has_colors = false;
    TEST_ASSERT_EQUAL_INT(UI_NONE, press('c'));
    TEST_ASSERT_FALSE(config.color);
    TEST_ASSERT_EQUAL_INT(UI_NONE, press('r'));
    TEST_ASSERT_FALSE(config.night);
    TEST_ASSERT_EQUAL_STRING("Night vision needs a color terminal", ui_current_toast(&ui, ctx.mono));
}

void test_threshold_bounds(void)
{
    for (int i = 0; i < 20; ++i)
    {
        press('+');
    }
    TEST_ASSERT_EQUAL_FLOAT(8.0f, config.threshold);
    press('=');
    TEST_ASSERT_EQUAL_FLOAT(8.0f, config.threshold);

    for (int i = 0; i < 40; ++i)
    {
        press('-');
    }
    TEST_ASSERT_EQUAL_FLOAT(-1.5f, config.threshold);
}

void test_time_keys(void)
{
    press('>');
    press('.');
    TEST_ASSERT_EQUAL_DOUBLE(60.0, clock_.speed);
    TEST_ASSERT_EQUAL_STRING("Speed: 60x", ui_current_toast(&ui, ctx.mono));

    press(',');
    TEST_ASSERT_EQUAL_DOUBLE(10.0, clock_.speed);

    press(' ');
    TEST_ASSERT_TRUE(clock_.paused);
    press(' ');
    TEST_ASSERT_FALSE(clock_.paused);

    ctx.wall = 1790000000.0;
    press('n');
    TEST_ASSERT_EQUAL_DOUBLE(1.0, clock_.speed);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2440587.5 + ctx.wall / 86400.0, sim_clock_jd(&clock_, ctx.wall));
}

void test_rotation(void)
{
    for (int i = 0; i < 12; ++i)
    {
        press(KEY_RIGHT);
    }
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, M_PI, config.rotation);
    TEST_ASSERT_EQUAL_STRING("Rotation: 180°", ui_current_toast(&ui, ctx.mono));

    press(KEY_LEFT);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, M_PI - M_PI / 12.0, config.rotation);

    press(KEY_DOWN);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, config.rotation);
}

void test_arrow_keys_do_not_quit(void)
{
    TEST_ASSERT_NOT_EQUAL(UI_QUIT, press(KEY_UP));
    TEST_ASSERT_NOT_EQUAL(UI_QUIT, press(KEY_LEFT));
}

void test_help_and_escape(void)
{
    TEST_ASSERT_EQUAL_INT(UI_LAYOUT, press('?'));
    TEST_ASSERT_TRUE(ui.help_open);

    // Other keys act and keep the modal open
    press('g');
    TEST_ASSERT_TRUE(ui.help_open);
    TEST_ASSERT_TRUE(config.grid);

    // ESC closes the modal first, then quits
    TEST_ASSERT_EQUAL_INT(UI_LAYOUT, press(27));
    TEST_ASSERT_FALSE(ui.help_open);
    TEST_ASSERT_EQUAL_INT(UI_QUIT, press(27));
    TEST_ASSERT_EQUAL_INT(UI_QUIT, press('q'));
}

void test_stations_toggle(void)
{
    config.stations = true;
    ctx.stations_count = 2;
    press('i');
    TEST_ASSERT_FALSE(config.stations);
    ctx.stations_count = 0;
    press('i');
    TEST_ASSERT_TRUE(config.stations);
    TEST_ASSERT_EQUAL_STRING("Stations: no data (run once online)", ui_current_toast(&ui, ctx.mono));
}

void test_quit_on_any(void)
{
    config.quit_on_any = true;
    TEST_ASSERT_EQUAL_INT(UI_QUIT, press('g'));
    TEST_ASSERT_FALSE(config.grid);
}

void test_toast_expires(void)
{
    press('g');
    TEST_ASSERT_NOT_NULL(ui_current_toast(&ui, ctx.mono + 1.9));
    TEST_ASSERT_NULL(ui_current_toast(&ui, ctx.mono + 2.0));
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_display_toggles_flip_and_toast);
    RUN_TEST(test_actions_request_side_effects);
    RUN_TEST(test_braille_needs_unicode);
    RUN_TEST(test_colors_need_a_color_terminal);
    RUN_TEST(test_threshold_bounds);
    RUN_TEST(test_time_keys);
    RUN_TEST(test_rotation);
    RUN_TEST(test_arrow_keys_do_not_quit);
    RUN_TEST(test_help_and_escape);
    RUN_TEST(test_stations_toggle);
    RUN_TEST(test_quit_on_any);
    RUN_TEST(test_toast_expires);

    return UNITY_END();
}
