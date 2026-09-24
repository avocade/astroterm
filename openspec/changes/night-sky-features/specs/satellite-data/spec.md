## ADDED Requirements

### Requirement: Local TLE cache
Satellite data SHALL be read from a per-user cache directory: `$ASTROTERM_CACHE_DIR` if set, else
`$XDG_CACHE_HOME/astroterm`, else `~/.cache/astroterm` (Windows: `%LOCALAPPDATA%\astroterm`). Feeds SHALL be stored
as `stations.tle` and `starlink.tle`. Cached data of any age SHALL be used when nothing fresher is available.

#### Scenario: Offline with old cache
- **WHEN** the device has no network and `starlink.tle` is 5 days old
- **THEN** Starlink is drawn from the 5-day-old data and the toast notes the data age

### Requirement: Background refresh
When a satellite layer is enabled and its feed's cache is missing or older than 12 hours, the application SHALL
refresh it from CelesTrak (`https://celestrak.org/NORAD/elements/gp.php?GROUP=<stations|starlink>&FORMAT=tle`) by
spawning `curl` as a child process, without a shell, writing to a temporary file and atomically renaming it into
place only after the download succeeds and parses to at least one valid set. The render loop SHALL NOT block
on the download; the new data SHALL be loaded when the child exits. At most one download per feed SHALL be in
flight, and a failed feed SHALL NOT be retried for 15 minutes.

#### Scenario: First run
- **WHEN** Starlink is enabled with no cache and the network is up
- **THEN** the sky keeps animating, a toast shows "Starlink: downloading…", and the dots appear when the download finishes

#### Scenario: Truncated download
- **WHEN** the download is cut off and the file parses to zero valid sets
- **THEN** the previous cache file is kept unchanged

#### Scenario: curl missing
- **WHEN** `curl` is not on `PATH`
- **THEN** the toast reports "Starlink: curl not found" and the application keeps running

### Requirement: Offline mode
`--offline` (`-O`) SHALL prevent every network access; satellite layers then use the cache only.

#### Scenario: Offline flag
- **WHEN** astroterm runs with `--offline` and no cache
- **THEN** no child process is spawned and satellite layers report "no data"

### Requirement: Network only for enabled layers
The application SHALL NOT contact the network for a feed whose layer is off. Starlink is off by default; stations
are on by default.

#### Scenario: Default launch
- **WHEN** astroterm starts without `--starlink` and with a fresh stations cache
- **THEN** no network request is made
