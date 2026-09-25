#include "sim_clock.h"
#include "unity.h"

void setUp(void)
{
}
void tearDown(void)
{
}

#define JD0 2461308.0
#define SEC (1.0 / 86400.0)

void test_realtime_tracks_wall_clock(void)
{
    struct SimClock clock;
    sim_clock_init(&clock, JD0, 1000.0, 1.0);

    // 100 frames of 200 ms each at a nominal 24 fps: time is taken from the
    // wall clock, so the frame rate is irrelevant
    double wall = 1000.0;
    for (int i = 0; i < 100; ++i)
    {
        wall += 0.2;
    }
    TEST_ASSERT_DOUBLE_WITHIN(1e-6 * SEC, JD0 + 20.0 * SEC, sim_clock_jd(&clock, wall));
}

void test_suspended_process_catches_up(void)
{
    struct SimClock clock;
    sim_clock_init(&clock, JD0, 1000.0, 1.0);

    // No frames at all for an hour (SIGSTOP, laptop asleep)
    TEST_ASSERT_DOUBLE_WITHIN(1e-6 * SEC, JD0 + 3600.0 * SEC, sim_clock_jd(&clock, 4600.0));
}

void test_pause_and_resume(void)
{
    struct SimClock clock;
    sim_clock_init(&clock, JD0, 0.0, 60.0);

    sim_clock_set_paused(&clock, true, 10.0); // 600 sim seconds in
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, JD0 + 600.0 * SEC, sim_clock_jd(&clock, 10.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, JD0 + 600.0 * SEC, sim_clock_jd(&clock, 50.0));

    sim_clock_set_paused(&clock, false, 50.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, JD0 + 660.0 * SEC, sim_clock_jd(&clock, 51.0));
}

void test_speed_ladder(void)
{
    struct SimClock clock;
    sim_clock_init(&clock, JD0, 0.0, 1.0);

    sim_clock_step_speed(&clock, 1, 0.0);
    sim_clock_step_speed(&clock, 1, 0.0);
    TEST_ASSERT_EQUAL_DOUBLE(60.0, clock.speed);

    sim_clock_step_speed(&clock, 1, 0.0);
    sim_clock_step_speed(&clock, 1, 0.0);
    sim_clock_step_speed(&clock, 1, 0.0);
    TEST_ASSERT_EQUAL_DOUBLE(3600.0, clock.speed); // Top of the ladder holds

    for (int i = 0; i < 10; ++i)
    {
        sim_clock_step_speed(&clock, -1, 0.0);
    }
    TEST_ASSERT_EQUAL_DOUBLE(1.0, clock.speed); // Bottom holds too: no reverse
}

void test_speed_change_keeps_time_continuous(void)
{
    struct SimClock clock;
    sim_clock_init(&clock, JD0, 0.0, 1.0);

    double before = sim_clock_jd(&clock, 100.0);
    sim_clock_step_speed(&clock, 1, 100.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, before, sim_clock_jd(&clock, 100.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, before + 10.0 * SEC, sim_clock_jd(&clock, 101.0));
}

void test_off_ladder_speed_moves_to_next_rung(void)
{
    struct SimClock clock;
    sim_clock_init(&clock, JD0, 0.0, 30.0); // e.g. --speed 30

    sim_clock_step_speed(&clock, 1, 0.0);
    TEST_ASSERT_EQUAL_DOUBLE(60.0, clock.speed);

    sim_clock_init(&clock, JD0, 0.0, 30.0);
    sim_clock_step_speed(&clock, -1, 0.0);
    TEST_ASSERT_EQUAL_DOUBLE(10.0, clock.speed);
}

void test_negative_speed_keeps_direction(void)
{
    struct SimClock clock;
    sim_clock_init(&clock, JD0, 0.0, -600.0); // e.g. --speed -600

    sim_clock_step_speed(&clock, 1, 0.0);
    TEST_ASSERT_EQUAL_DOUBLE(-3600.0, clock.speed); // Faster, still backwards

    for (int i = 0; i < 10; ++i)
    {
        sim_clock_step_speed(&clock, -1, 0.0);
    }
    TEST_ASSERT_EQUAL_DOUBLE(-1.0, clock.speed);
}

void test_jump_to_now(void)
{
    struct SimClock clock;
    sim_clock_init(&clock, JD0, 0.0, 3600.0);
    sim_clock_set_paused(&clock, true, 5.0);

    double wall = 1790000000.0; // 2026-09-21T21:33:20Z
    sim_clock_now(&clock, wall);
    TEST_ASSERT_FALSE(clock.paused);
    TEST_ASSERT_EQUAL_DOUBLE(1.0, clock.speed);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2440587.5 + wall / 86400.0, sim_clock_jd(&clock, wall));
}

void test_realtime_clock_is_sane(void)
{
    // Between 2020 and 2100
    double now = clock_realtime_s();
    TEST_ASSERT_TRUE(now > 1577836800.0 && now < 4102444800.0);

    double a = clock_monotonic_s();
    double b = clock_monotonic_s();
    TEST_ASSERT_TRUE(b >= a);
}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_realtime_tracks_wall_clock);
    RUN_TEST(test_suspended_process_catches_up);
    RUN_TEST(test_pause_and_resume);
    RUN_TEST(test_speed_ladder);
    RUN_TEST(test_speed_change_keeps_time_continuous);
    RUN_TEST(test_off_ladder_speed_moves_to_next_rung);
    RUN_TEST(test_negative_speed_keeps_direction);
    RUN_TEST(test_jump_to_now);
    RUN_TEST(test_realtime_clock_is_sane);

    return UNITY_END();
}
