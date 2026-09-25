#include "geolocate.h"

#include "city.h"
#include "feed.h"
#include "macros.h"
#include "omm.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifndef _WIN32
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

bool geolocate_parse(const char *text, double *latitude, double *longitude)
{
    // Two whitespace-separated numbers; parsed without the C locale
    const char *p = text;
    double values[2];
    for (int i = 0; i < 2; ++i)
    {
        while (*p == ' ' || *p == '\t')
        {
            p++;
        }
        const char *start = p;
        while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
        {
            p++;
        }
        if (p == start || !omm_parse_number(start, (size_t)(p - start), &values[i]))
        {
            return false;
        }
    }
    if (fabs(values[0]) > 90.0 || fabs(values[1]) > 180.0)
    {
        return false;
    }
    *latitude = values[0];
    *longitude = values[1];
    return true;
}

static bool location_file(char *buf, size_t len)
{
    char dir[1024];
    return feed_cache_dir(dir, sizeof(dir)) && snprintf(buf, len, "%s/location", dir) < (int)len;
}

static void remember(double latitude, double longitude)
{
    char path[1100];
    if (!location_file(path, sizeof(path)))
    {
        return;
    }
    FILE *f = fopen(path, "w");
    if (f != NULL)
    {
        // Fixed notation with '.' regardless of locale (we run before setlocale)
        fprintf(f, "%.6f %.6f\n", latitude, longitude);
        fclose(f);
    }
}

static bool recall(struct GeoFix *fix)
{
    char path[1100];
    if (!location_file(path, sizeof(path)))
    {
        return false;
    }
    FILE *f = fopen(path, "r");
    if (f == NULL)
    {
        return false;
    }
    char line[128] = "";
    bool ok = fgets(line, sizeof(line), f) != NULL && geolocate_parse(line, &fix->latitude, &fix->longitude);
    fclose(f);

    struct stat st;
    if (!ok || stat(path, &st) != 0)
    {
        return false;
    }
    fix->remembered = true;
    fix->age_hours = difftime(time(NULL), st.st_mtime) / 3600.0;
    return true;
}

#ifndef _WIN32

/* Run CoreLocationCLI, giving up after GEOLOCATE_TIMEOUT_SECONDS
 */
static bool ask_location_services(double *latitude, double *longitude)
{
    // Absolute PATH entries only, as for curl
    char tool[1024] = "";
    const char *path = getenv("PATH");
    while (path != NULL && *path != '\0' && tool[0] == '\0')
    {
        const char *end = strchr(path, ':');
        size_t n = end ? (size_t)(end - path) : strlen(path);
        if (n > 0 && path[0] == '/' && n + 17 < sizeof(tool))
        {
            snprintf(tool, sizeof(tool), "%.*s/CoreLocationCLI", (int)n, path);
            if (access(tool, X_OK) != 0)
            {
                tool[0] = '\0';
            }
        }
        path = end ? end + 1 : NULL;
    }
    if (tool[0] == '\0')
    {
        return false;
    }

    int pipe_fd[2];
    if (pipe(pipe_fd) != 0)
    {
        return false;
    }
    char *argv[] = {tool, "--format", "%latitude %longitude", NULL};
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_adddup2(&actions, pipe_fd[1], STDOUT_FILENO);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addclose(&actions, pipe_fd[0]);
    pid_t pid;
    int err = posix_spawn(&pid, tool, &actions, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    close(pipe_fd[1]);
    if (err != 0)
    {
        close(pipe_fd[0]);
        return false;
    }

    // Wait in short steps, and never longer than the timeout
    int status = 0;
    bool exited = false;
    for (int waited_ms = 0; waited_ms < (int)(GEOLOCATE_TIMEOUT_SECONDS * 1000); waited_ms += 20)
    {
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid || (r < 0 && errno != EINTR))
        {
            exited = r == pid;
            break;
        }
        usleep(20000);
    }
    if (!exited)
    {
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
        close(pipe_fd[0]);
        return false;
    }

    char out[128] = "";
    ssize_t n = read(pipe_fd[0], out, sizeof(out) - 1);
    out[n > 0 ? n : 0] = '\0';
    close(pipe_fd[0]);

    return WIFEXITED(status) && WEXITSTATUS(status) == 0 && geolocate_parse(out, latitude, longitude);
}

#else

static bool ask_location_services(double *latitude, double *longitude)
{
    (void)latitude;
    (void)longitude;
    return false;
}

#endif

bool geolocate(struct GeoFix *fix)
{
    memset(fix, 0, sizeof(*fix));
    if (ask_location_services(&fix->latitude, &fix->longitude))
    {
        remember(fix->latitude, fix->longitude);
        return true;
    }
    return recall(fix);
}

struct Nearest
{
    double latitude, longitude; // Radians
    double best_km;
    char name[64];
};

static void consider_city(const CityData *city, void *data)
{
    struct Nearest *n = data;
    double lat = city->latitude * TO_RAD;
    double lon = city->longitude * TO_RAD;

    // Haversine distance on a 6371 km sphere
    double dlat = lat - n->latitude;
    double dlon = lon - n->longitude;
    double a = sin(dlat / 2) * sin(dlat / 2) + cos(n->latitude) * cos(lat) * sin(dlon / 2) * sin(dlon / 2);
    double km = 2.0 * 6371.0 * asin(fmin(1.0, sqrt(a)));
    if (km < n->best_km)
    {
        n->best_km = km;
        snprintf(n->name, sizeof(n->name), "%s", city->city_name);
    }
}

bool nearest_city(double latitude, double longitude, char *name, size_t len, double *distance_km)
{
    struct Nearest n = {.latitude = latitude * TO_RAD, .longitude = longitude * TO_RAD, .best_km = 1.0e9};
    iter_cities(consider_city, &n);
    if (n.name[0] == '\0')
    {
        return false;
    }
    snprintf(name, len, "%s", n.name);
    *distance_km = n.best_km;
    return true;
}

void geolocate_describe(const struct GeoFix *fix, char *buf, size_t len)
{
    char city[64];
    double km;
    char where[96] = "";
    if (nearest_city(fix->latitude, fix->longitude, city, sizeof(city), &km) && km < 150.0)
    {
        snprintf(where, sizeof(where), "near %s ", city);
    }

    char coords[48];
    snprintf(coords, sizeof(coords), "%.1f° %c, %.1f° %c", fabs(fix->latitude), fix->latitude >= 0 ? 'N' : 'S',
             fabs(fix->longitude), fix->longitude >= 0 ? 'E' : 'W');

    if (!fix->remembered)
    {
        snprintf(buf, len, "Location: %s(%s)", where, coords);
    }
    else if (fix->age_hours < 48.0)
    {
        snprintf(buf, len, "Location: last known, %s(%s)", where, coords);
    }
    else
    {
        snprintf(buf, len, "Location: last known %.0f days ago, %s(%s)", fix->age_hours / 24.0, where, coords);
    }
}
