## ADDED Requirements

### Requirement: TLE parsing
The parser SHALL accept NORAD two-line element sets with an optional name line, validate each line's length
(≥ 69 columns), line numbers, matching catalog numbers and the modulo-10 checksum, and SHALL skip (not abort on)
invalid sets. Satellite names SHALL be reduced to printable ASCII and trimmed, so no terminal control sequence
from the data can reach the screen.

#### Scenario: Valid 3LE
- **WHEN** the ISS 3-line set is parsed
- **THEN** one satellite with catalog number 25544 and name "ISS (ZARYA)" is produced

#### Scenario: Bad checksum
- **WHEN** a set's line 1 checksum digit is wrong
- **THEN** that set is skipped and the rest of the file still loads

#### Scenario: Hostile name
- **WHEN** a name line contains `ESC[2J`
- **THEN** the stored name contains no ESC byte

### Requirement: SGP4 propagation
Satellite positions SHALL be computed with the SGP4 near-Earth model (WGS-72 constants, Vallado 2006 revision).
Results SHALL match the reference implementation (python-sgp4) to within 10 m in position for tested sets over
±3 days from epoch. Sets with an orbital period ≥ 225 minutes (deep space) SHALL be marked unsupported and not
drawn. A propagation error (decay, eccentricity out of range) SHALL hide that satellite, not crash.

#### Scenario: Matches reference
- **WHEN** the ISS set is propagated to +1440 minutes
- **THEN** the TEME position is within 10 m of the reference value

#### Scenario: Deep-space set
- **WHEN** a geostationary set is loaded
- **THEN** it is marked unsupported and never rendered

### Requirement: Topocentric position
Each satellite's azimuth and altitude SHALL be computed from its TEME position, Greenwich sidereal time and the
observer's geodetic position on the WGS-84 ellipsoid (including parallax). Results SHALL agree with Skyfield to
within 0.1° for the tested cases.

#### Scenario: Parallax matters
- **WHEN** the ISS is overhead one observer
- **THEN** an observer 1,000 km away sees it well below the zenith

### Requirement: Sunlit state
Each satellite SHALL be classified as sunlit or in Earth's shadow using a cylindrical shadow model and the Sun
position the application already computes.

#### Scenario: Midnight shadow
- **WHEN** a satellite lies directly between the Earth's center and the anti-solar point at 550 km altitude
- **THEN** it is classified as eclipsed

### Requirement: Space station markers
The ISS (25544) and Tiangong (48274) SHALL be drawn when above the horizon, like planets: labeled ("ISS",
"Tiangong"), in a distinct color, with a distinct glyph (`⌖` in Unicode, `#` in ASCII), drawn above stars.
When in Earth's shadow they SHALL be drawn dimmed. They SHALL be on by default and toggled with `i`.

#### Scenario: ISS overhead
- **WHEN** the ISS is at altitude 40° for the observer
- **THEN** the ISS marker and label are drawn at that position

#### Scenario: No data
- **WHEN** no station TLEs are available (offline, empty cache)
- **THEN** nothing is drawn and pressing `i` shows the toast "Stations: no data (offline?)"

### Requirement: Next ISS pass
The metadata panel SHALL show one ISS line: "ISS: up now, <alt>° <compass>" while the ISS is above 10°, or
"ISS: next visible <local HH:MM>, max <alt>°" for the next pass within 72 hours in which the ISS is sunlit, above
10° and the Sun is below -6°, or "ISS: no visible pass in 72 h". A prediction SHALL be cached and reused while simulation time stays between the
instant it was computed and the end of the predicted pass. It SHALL be recomputed when simulation time leaves
that window (including time jumps and reverse travel) or the station data changes, and at most twice per
wall-clock second.

#### Scenario: Fast-forward stays cheap
- **WHEN** time runs at 3600x for one minute
- **THEN** no more than 120 pass predictions are computed

#### Scenario: Upcoming pass
- **WHEN** the next qualifying pass starts at 21:43 local time with culmination 67°
- **THEN** the panel shows "ISS: next visible 21:43, max 67°"
