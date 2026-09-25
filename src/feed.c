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
#include <signal.h>
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

/* Start curl downloading `url` into `tmp_path`, with the HTTP status on a pipe
 */
static enum FetchResult spawn_curl(const char *url, const char *tmp_path, pid_t *pid_out, int *status_fd)
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
                    "120",
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

    int spawn_err = posix_spawn(pid_out, curl, &actions, NULL, argv, environ);
    posix_spawn_file_actions_destroy(&actions);
    close(pipe_fd[1]);

    if (spawn_err != 0)
    {
        close(pipe_fd[0]);
        return FETCH_NO_CURL;
    }
    *status_fd = pipe_fd[0];
    return FETCH_OK;
}

/* Wait for (or, without `block`, check on) a spawned curl. Sets *done; when
 * done, returns the outcome and the HTTP status (0 if the server never answered)
 */
static enum FetchResult collect_curl(pid_t pid, int status_fd, bool block, bool *done, int *http)
{
    int wstatus = 0;
    pid_t waited;
    while ((waited = waitpid(pid, &wstatus, block ? 0 : WNOHANG)) < 0 && errno == EINTR)
    {
    }
    *done = waited != 0;
    if (!*done)
    {
        return FETCH_OK;
    }

    // curl has exited, so its status line is complete in the pipe
    char status[16] = "";
    ssize_t n = read(status_fd, status, sizeof(status) - 1);
    status[n > 0 ? n : 0] = '\0';
    close(status_fd);
    *http = atoi(status);

    if (waited < 0 || !WIFEXITED(wstatus))
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

/* Record the attempt and, if the download is good, move it into place
 */
static enum FetchResult install(enum FeedId id, const char *tmp, enum FetchResult result, int http)
{
    char path[PATH_LEN], marker[PATH_LEN];
    if (!feed_file(id, ".csv", path, sizeof(path)) || !feed_file(id, ".attempt", marker, sizeof(marker)))
    {
        unlink(tmp);
        return FETCH_ERROR;
    }

    // The two-hour wait is politeness toward CelesTrak, so it only starts once
    // CelesTrak has actually answered (not when we were offline)
    if (http != 0)
    {
        touch(marker);
    }
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

/* Hours until CelesTrak may be asked again, or 0 when it may be asked now
 */
static double hours_until_retry(enum FeedId id)
{
    char marker[PATH_LEN];
    if (!feed_file(id, ".attempt", marker, sizeof(marker)))
    {
        return 0.0;
    }
    double since = file_age_hours(marker);
    return since >= 0.0 && since < FEED_RETRY_HOURS ? FEED_RETRY_HOURS - since : 0.0;
}

static bool make_temp(enum FeedId id, char *tmp, size_t len)
{
    if (!feed_file(id, ".csv.XXXXXX", tmp, len))
    {
        return false;
    }
    int fd = mkstemp(tmp);
    if (fd < 0)
    {
        return false;
    }
    close(fd);
    return true;
}

static enum FetchResult refresh_one(enum FeedId id, double max_age_hours)
{
    double age = feed_age_hours(id);
    if (max_age_hours < 0.0 || (age >= 0.0 && age < max_age_hours) || hours_until_retry(id) > 0.0)
    {
        return FETCH_OK; // Not wanted, fresh enough, or asked recently
    }

    char tmp[PATH_LEN];
    if (!make_temp(id, tmp, sizeof(tmp)))
    {
        return FETCH_ERROR;
    }

    fprintf(stderr, "astroterm: updating satellite data (%s)...\n", feed_names[id]);

    pid_t pid;
    int status_fd;
    int http = 0;
    enum FetchResult result = spawn_curl(feed_urls[id], tmp, &pid, &status_fd);
    if (result == FETCH_OK)
    {
        bool done;
        result = collect_curl(pid, status_fd, true, &done, &http);
    }
    return install(id, tmp, result, http);
}

static const char *problem_text(enum FetchResult result)
{
    switch (result)
    {
    case FETCH_OK:
        return NULL;
    case FETCH_NO_CURL:
        return "curl not found";
    case FETCH_NETWORK:
        return "no network";
    case FETCH_RATE_LIMITED:
        return "CelesTrak has not updated yet";
    case FETCH_BAD_DATA:
        return "download was incomplete";
    case FETCH_ERROR:
        return "download failed";
    }
    return "download failed";
}

void feed_refresh(const double max_age_hours[NUM_FEEDS], char *message, size_t len)
{
    message[0] = '\0';
    for (int id = 0; id < NUM_FEEDS; ++id)
    {
        enum FetchResult result = refresh_one((enum FeedId)id, max_age_hours[id]);
        const char *problem = problem_text(result);
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

enum FeedStart feed_download_start(enum FeedId id, struct FeedDownload *download, double *minutes_to_wait)
{
    *minutes_to_wait = hours_until_retry(id) * 60.0;
    if (*minutes_to_wait > 0.0)
    {
        return FEED_START_TOO_SOON;
    }
    if (!make_temp(id, download->tmp, sizeof(download->tmp)))
    {
        return FEED_START_ERROR;
    }

    pid_t pid;
    if (spawn_curl(feed_urls[id], download->tmp, &pid, &download->status_fd) != FETCH_OK)
    {
        unlink(download->tmp);
        return FEED_START_NO_CURL;
    }
    download->pid = (long)pid;
    download->id = id;
    download->active = true;
    return FEED_START_OK;
}

bool feed_download_poll(struct FeedDownload *download, bool *updated, char *message, size_t len)
{
    *updated = false;
    if (!download->active)
    {
        return false;
    }

    bool done;
    int http = 0;
    enum FetchResult result = collect_curl((pid_t)download->pid, download->status_fd, false, &done, &http);
    if (!done)
    {
        return false;
    }

    download->active = false;
    result = install(download->id, download->tmp, result, http);
    *updated = result == FETCH_OK;
    const char *problem = problem_text(result);
    snprintf(message, len, "%s", problem != NULL ? problem : "");
    return true;
}

void feed_download_cancel(struct FeedDownload *download)
{
    if (!download->active)
    {
        return;
    }
    kill((pid_t)download->pid, SIGTERM);
    while (waitpid((pid_t)download->pid, NULL, 0) < 0 && errno == EINTR)
    {
    }
    close(download->status_fd);
    unlink(download->tmp);
    download->active = false;
}

#else // _WIN32

void feed_refresh(const double max_age_hours[NUM_FEEDS], char *message, size_t len)
{
    (void)max_age_hours;
    snprintf(message, len, "Satellite downloads are not supported on Windows");
}

enum FeedStart feed_download_start(enum FeedId id, struct FeedDownload *download, double *minutes_to_wait)
{
    (void)id;
    (void)download;
    *minutes_to_wait = 0.0;
    return FEED_START_UNSUPPORTED;
}

bool feed_download_poll(struct FeedDownload *download, bool *updated, char *message, size_t len)
{
    (void)download;
    (void)message;
    (void)len;
    *updated = false;
    return false;
}

void feed_download_cancel(struct FeedDownload *download)
{
    (void)download;
}

#endif // _WIN32
