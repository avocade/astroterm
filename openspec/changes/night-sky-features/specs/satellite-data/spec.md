## ADDED Requirements

### Requirement: Local cache
Satellite feeds SHALL be stored in `$XDG_CACHE_HOME/astroterm` (absolute paths only), else `~/.cache/astroterm`,
created with mode 0700, as `stations.csv` and `starlink.csv`. Cached data of any age SHALL be used when nothing
fresher is available.

#### Scenario: Offline with an old cache
- **WHEN** there is no network and `starlink.csv` is 5 days old
- **THEN** Starlink is drawn from it and the launch toast notes "data 5 d old"

### Requirement: Refresh before the UI starts
At launch, if a satellite layer is on and `--offline` is not given, each feed in use (stations when on, Starlink
when started with `--starlink`) older than 12 hours SHALL be refreshed, and the Starlink feed when not in use only
when missing or older than 14 days (the element-age limit). Refreshes SHALL happen from `https://celestrak.org/NORAD/elements/gp.php?GROUP=<stations|starlink>&FORMAT=csv` before curses
starts, by running `curl` without a shell into a unique temporary file, unless an attempt for that feed was made
in the last 2 hours. The download SHALL replace the feed only on HTTP 200 with at least one valid row and at least
half the previous row count. HTTP 403 SHALL be reported as "CelesTrak: not updated yet" and leave the cache as is.

#### Scenario: First run online
- **WHEN** stations are on, there is no cache and the network is up
- **THEN** both feeds are downloaded before the first frame

#### Scenario: Starlink kept warm, not fresh
- **WHEN** Starlink is off, stations are on and the Starlink cache is 13 days old
- **THEN** only the stations feed is refreshed, and the Starlink catalog is not loaded until `x` is pressed

#### Scenario: Truncated download
- **WHEN** the download yields 3,000 rows and the cache holds 10,000
- **THEN** the cache is kept and the toast reports the failed refresh

#### Scenario: Rate limited
- **WHEN** CelesTrak answers 403
- **THEN** the cache is kept and no new attempt is made for 2 hours

#### Scenario: curl missing
- **WHEN** `curl` is not on `PATH`
- **THEN** the application starts normally and the toast reports "curl not found"

### Requirement: Offline mode
`--offline` SHALL prevent every network access.

#### Scenario: Offline flag
- **WHEN** astroterm runs with `--offline` and no cache
- **THEN** no process is spawned and satellite layers report "no data"

### Requirement: Network only for satellite layers
The application SHALL NOT contact the network when all satellite layers are off at launch.

#### Scenario: Stations off
- **WHEN** astroterm starts with stations off (`i` pressed later does not trigger a download)
- **THEN** no network request is made
