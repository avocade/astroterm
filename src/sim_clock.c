#include "sim_clock.h"

#include <math.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define SECONDS_PER_DAY 86400.0
#define UNIX_EPOCH_JD 2440587.5

static const double speed_ladder[] = {1.0, 10.0, 60.0, 600.0, 3600.0};
#define LADDER_LEN (sizeof(speed_ladder) / sizeof(speed_ladder[0]))

double clock_realtime_s(void)
{
#ifdef _WIN32
    // 100 ns intervals since 1601-01-01
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    unsigned long long ticks = ((unsigned long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return (double)(ticks - 116444736000000000ULL) / 1.0e7;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (double)ts.tv_sec + ts.tv_nsec / 1.0e9;
#endif
}

double clock_monotonic_s(void)
{
#ifdef _WIN32
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec / 1.0e9;
#endif
}

double unix_to_julian_date(double unix_s)
{
    return UNIX_EPOCH_JD + unix_s / SECONDS_PER_DAY;
}

static void reanchor(struct SimClock *clock, double wall)
{
    clock->anchor_jd = sim_clock_jd(clock, wall);
    clock->anchor_wall = wall;
}

void sim_clock_init(struct SimClock *clock, double jd, double wall, double speed)
{
    clock->anchor_jd = jd;
    clock->anchor_wall = wall;
    clock->speed = speed;
    clock->paused = false;
}

double sim_clock_jd(const struct SimClock *clock, double wall)
{
    if (clock->paused)
    {
        return clock->anchor_jd;
    }
    return clock->anchor_jd + (wall - clock->anchor_wall) * clock->speed / SECONDS_PER_DAY;
}

void sim_clock_set_paused(struct SimClock *clock, bool paused, double wall)
{
    reanchor(clock, wall);
    clock->paused = paused;
}

void sim_clock_step_speed(struct SimClock *clock, int direction, double wall)
{
    reanchor(clock, wall);

    // Step the magnitude, keeping the direction (--speed may be negative)
    double sign = clock->speed < 0.0 ? -1.0 : 1.0;
    double speed = fabs(clock->speed);
    if (direction > 0)
    {
        for (unsigned int i = 0; i < LADDER_LEN; ++i)
        {
            if (speed_ladder[i] > speed)
            {
                clock->speed = sign * speed_ladder[i];
                return;
            }
        }
    }
    else if (direction < 0)
    {
        for (int i = (int)LADDER_LEN - 1; i >= 0; --i)
        {
            if (speed_ladder[i] < speed)
            {
                clock->speed = sign * speed_ladder[i];
                return;
            }
        }
    }
}

void sim_clock_now(struct SimClock *clock, double wall)
{
    sim_clock_init(clock, unix_to_julian_date(wall), wall, 1.0);
}
