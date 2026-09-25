## ADDED Requirements

### Requirement: Find the observer when no location is given
The application SHALL, when started without `-a`, `-o` or `-i`, ask the operating system for its location
(macOS Location Services via `CoreLocationCLI`, when on `PATH`), waiting at most 3 seconds. A fix SHALL be
remembered in the cache directory, and when no fix is available the last remembered location SHALL be used.
Only with neither SHALL it fall back to 0°, 0° with the existing warning. Explicit coordinates always win and
skip the lookup.

#### Scenario: Location Services available
- **WHEN** astroterm starts with no location and Location Services answers 56.51, 16.60
- **THEN** the sky is drawn for 56.51° N, 16.60° E and the launch toast reads "Location: near Kalmar (56.5° N, 16.6° E)"

#### Scenario: Offline at a dark site
- **WHEN** Location Services cannot answer but a location was remembered two days ago
- **THEN** that location is used and the toast says it is the last known one, 2 days old

#### Scenario: Nothing known
- **WHEN** there is no locator and nothing remembered
- **THEN** the location is 0°, 0° and the toast asks for `-i <city>` or `-a`/`-o`

#### Scenario: Explicit coordinates
- **WHEN** astroterm starts with `-i Stockholm`
- **THEN** no locator runs

### Requirement: Name the place offline
The toast SHALL name the nearest city from astroterm's built-in city list, so no network lookup is needed.

#### Scenario: Nearest city
- **WHEN** the fix is 59.33, 18.07
- **THEN** the toast says "near Stockholm"
