#include "feed.h"
#include "omm.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

#define PATH_LEN 1024

static const char *feed_names[NUM_FEEDS] = {"stations", "starlink"};

static const char *feed_urls[NUM_FEEDS] = {
    "https://celestrak.org/NORAD/elements/gp.php?GROUP=stations&FORMAT=csv",
    "https://celestrak.org/NORAD/elements/gp.php?GROUP=starlink&FORMAT=csv",
};

static bool make_dir(const char *path)
{
#ifdef _WIN32
    return _mkdir(path) == 0 || errno == EEXIST;
#else
    return mkdir(path, 0700) == 0 || errno == EEXIST;
#endif
}

bool feed_cache_dir(char *buf, size_t len)
{
#ifdef _WIN32
    const char *base = getenv("LOCALAPPDATA");
    if (base == NULL || snprintf(buf, len, "%s\\astroterm", base) >= (int)len)
    {
        return false;
    }
    return make_dir(buf);
#else
    const char *xdg = getenv("XDG_CACHE_HOME");
    if (xdg != NULL && xdg[0] == '/')
    {
        // Relative values are invalid per the XDG spec and are ignored
        if (snprintf(buf, len, "%s", xdg) >= (int)len || !make_dir(buf))
        {
            return false;
        }
    }
    else
    {
        const char *home = getenv("HOME");
        if (home == NULL || home[0] != '/' || snprintf(buf, len, "%s/.cache", home) >= (int)len || !make_dir(buf))
        {
            return false;
        }
    }
    size_t used = strlen(buf);
    if (snprintf(buf + used, len - used, "/astroterm") >= (int)(len - used))
    {
        return false;
    }
    return make_dir(buf);
#endif
}

static bool feed_file(enum FeedId id, const char *suffix, char *buf, size_t len)
{
    char dir[PATH_LEN];
    return feed_cache_dir(dir, sizeof(dir)) && snprintf(buf, len, "%s/%s%s", dir, feed_names[id], suffix) < (int)len;
}

static double file_age_hours(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0)
    {
        return -1.0;
    }
    return difftime(time(NULL), st.st_mtime) / 3600.0;
}

double feed_age_hours(enum FeedId id)
{
    char path[PATH_LEN];
    return feed_file(id, ".csv", path, sizeof(path)) ? file_age_hours(path) : -1.0;
}

static char *read_file(const char *path, size_t *len_out)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL)
    {
        return NULL;
    }

    size_t cap = 1 << 16;
    size_t len = 0;
    char *buf = malloc(cap);
    while (buf != NULL)
    {
        if (len + 1 >= cap)
        {
            char *bigger = realloc(buf, cap * 2);
            if (bigger == NULL)
            {
                free(buf);
                buf = NULL;
                break;
            }
            buf = bigger;
            cap *= 2;
        }
        size_t n = fread(buf + len, 1, cap - len - 1, f);
        if (n == 0)
        {
            break;
        }
        len += n;
    }
    fclose(f);

    if (buf != NULL)
    {
        buf[len] = '\0';
        *len_out = len;
    }
    return buf;
}

char *feed_read(enum FeedId id, size_t *len_out)
{
    char path[PATH_LEN];
    return feed_file(id, ".csv", path, sizeof(path)) ? read_file(path, len_out) : NULL;
}

/* Number of valid rows in a CSV file, or 0 if unreadable
 */
static int count_rows(const char *path)
{
    size_t len;
    char *data = read_file(path, &len);
    if (data == NULL)
    {
        return 0;
    }
    struct OmmRecord *records = NULL;
    int n = omm_parse_csv(data, len, &records);
    free(records);
    free(data);
    return n > 0 ? n : 0;
}

#ifndef _WIN32

/* Find an executable on PATH, skipping relative entries (so a stray ./curl in
 * the working directory is never run)
 */
static bool find_program(const char *name, char *buf, size_t len)
{
    const char *path = getenv("PATH");
    if (path == NULL)
    {
        return false;
    }
    while (*path != '\0')
    {
        const char *end = strchr(path, ':');
        size_t n = end ? (size_t)(end - path) : strlen(path);
        if (n > 0 && path[0] == '/' && n + strlen(name) + 2 < len)
        {
            snprintf(buf, len, "%.*s/%s", (int)n, path, name);
            if (access(buf, X_OK) == 0)
            {
                return true;
            }
        }
        if (end == NULL)
        {
            break;
        }
        path = end + 1;
    }
    return false;
}

enum FetchResult
{
    FETCH_OK,
    FETCH_NO_CURL,
    FETCH_NETWORK, // DNS, connect or timeout: no point trying the next feed
    FETCH_RATE_LIMITED,
    FETCH_BAD_DATA,
    FETCH_ERROR,
};

/* Download one feed into `tmp_path`. Returns the HTTP status via `http`
 */
static enum FetchResult run_curl(const char *url, const char *tmp_path, int *http)
{
    char curl[PATH_LEN];
    if (!find_program("curl", curl, sizeof(curl)))
    {
        return FETCH_NO_CURL;
    }

    int pipe_fd[2];
    if (pipe(pipe_fd) != 0)
    {
        return FETCH_ERROR;
    }

    // -q first: ignore ~/.curlrc. HTTPS only, even across redirects. The body
    // goes to the temporary file, the status code to our pipe
    char *argv[] = {curl,
                    "-q",
                    "-sS",
                    "-L",
                    "--proto",
                    "=https",
                    "--proto-redir",
                    "=https",
                    "--max-filesize",
                    "20000000",
                    "--connect-timeout",
                    "5",
                    "--max-time",
                    "60",
                    "-A",
                    "astroterm",
                    "-o",
                    (char *)tmp_path,
                    "-w",
                    "%{http_code}",
                    (char *)url,
                    NULL};

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_adddup2(&actions, pipe_fd[1], STDOUT_FILENO);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);
    posix_spawn_file_actions_addclose(&actions, pipe_fd[0]);

    pid_t pid;
    int spawn_err = posix_spawn(&pid, curl, &actions, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    close(pipe_fd[1]);

    if (spawn_err != 0)
    {
        close(pipe_fd[0]);
        return FETCH_NO_CURL;
    }

    char status[16] = "";
    ssize_t n = read(pipe_fd[0], status, sizeof(status) - 1);
    status[n > 0 ? n : 0] = '\0';
    close(pipe_fd[0]);

    int wstatus = 0;
    while (waitpid(pid, &wstatus, 0) < 0 && errno == EINTR)
    {
    }
    *http = atoi(status);

    if (!WIFEXITED(wstatus))
    {
        return FETCH_ERROR;
    }
    switch (WEXITSTATUS(wstatus))
    {
    case 0:
        break;
    case 127:
        return FETCH_NO_CURL;
    case 6:  // Could not resolve host
    case 7:  // Could not connect
    case 28: // Timeout
        return FETCH_NETWORK;
    default:
        return FETCH_ERROR;
    }

    if (*http == 403)
    {
        return FETCH_RATE_LIMITED;
    }
    return *http == 200 ? FETCH_OK : FETCH_ERROR;
}

static void touch(const char *path)
{
    FILE *f = fopen(path, "w");
    if (f != NULL)
    {
        fclose(f);
    }
}

static enum FetchResult refresh_one(enum FeedId id)
{
    char path[PATH_LEN], marker[PATH_LEN], tmp[PATH_LEN];
    if (!feed_file(id, ".csv", path, sizeof(path)) || !feed_file(id, ".attempt", marker, sizeof(marker)) ||
        !feed_file(id, ".csv.XXXXXX", tmp, sizeof(tmp)))
    {
        return FETCH_ERROR;
    }

    double age = file_age_hours(path);
    double since_attempt = file_age_hours(marker);
    if ((age >= 0.0 && age < FEED_MAX_AGE_HOURS) || (since_attempt >= 0.0 && since_attempt < FEED_RETRY_HOURS))
    {
        return FETCH_OK; // Fresh enough, or asked recently
    }

    int fd = mkstemp(tmp);
    if (fd < 0)
    {
        return FETCH_ERROR;
    }
    close(fd);
    touch(marker);

    fprintf(stderr, "astroterm: updating satellite data (%s)...\n", feed_names[id]);

    int http = 0;
    enum FetchResult result = run_curl(feed_urls[id], tmp, &http);
    if (result == FETCH_OK)
    {
        // Accept only a feed that parses, and that is not suspiciously smaller
        // than the one it replaces (a truncated download)
        int rows = count_rows(tmp);
        int old_rows = count_rows(path);
        if (rows < 1 || rows < old_rows / 2 || rename(tmp, path) != 0)
        {
            result = FETCH_BAD_DATA;
        }
    }
    unlink(tmp);
    return result;
}

void feed_refresh_all(char *message, size_t len)
{
    message[0] = '\0';
    for (int id = 0; id < NUM_FEEDS; ++id)
    {
        enum FetchResult result = refresh_one((enum FeedId)id);
        const char *problem = NULL;
        switch (result)
        {
        case FETCH_OK:
            break;
        case FETCH_NO_CURL:
            problem = "curl not found";
            break;
        case FETCH_NETWORK:
            problem = "no network";
            break;
        case FETCH_RATE_LIMITED:
            problem = "CelesTrak has not updated yet";
            break;
        case FETCH_BAD_DATA:
            problem = "download was incomplete";
            break;
        case FETCH_ERROR:
            problem = "download failed";
            break;
        }
        if (problem != NULL)
        {
            snprintf(message, len, "Satellite data not refreshed: %s", problem);
            if (result == FETCH_NETWORK || result == FETCH_NO_CURL)
            {
                return; // The next feed would fail the same way
            }
        }
    }
}

#else // _WIN32

void feed_refresh_all(char *message, size_t len)
{
    snprintf(message, len, "Satellite downloads are not supported on Windows");
}

#endif // _WIN32
