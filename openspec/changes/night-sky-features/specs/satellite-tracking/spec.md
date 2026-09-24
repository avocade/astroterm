## ADDED Requirements

### Requirement: OMM parsing
The parser SHALL read CelesTrak OMM CSV by header name (column order free, quoted fields allowed), convert numbers
independently of the process locale, accept 6-digit and larger catalog numbers, and skip (not abort on) rows that
are malformed or out of range (eccentricity outside [0, 1), mean motion outside (0, 20] rev/day, non-finite
values, unparseable epoch). Object names SHALL NOT be displayed.

#### Scenario: Six-digit catalog number
- **WHEN** a row with `NORAD_CAT_ID` 100465 is parsed
- **THEN** one satellite with catalog number 100465 is produced

#### Scenario: Comma locale
- **WHEN** the process locale is `sv_SE.UTF-8` and the ISS row is parsed
- **THEN** the inclination is 51.6318°, not 51°

#### Scenario: Garbage row
- **WHEN** a row has `ECCENTRICITY` "nan" or a missing epoch
- **THEN** that row is skipped and the others load

### Requirement: SGP4 propagation
Positions SHALL be computed with the near-Earth SGP4 model (WGS-72, Vallado revision, opsmode "i"). Position and
velocity SHALL match python-sgp4 to 1 m and 1 mm/s for the live sets and for every near-Earth case of Vallado's
`SGP4-VER.TLE`, including its error codes. Orbits with a period of 225 minutes or more SHALL be rejected and not
drawn. Any propagation error or non-finite output SHALL hide that satellite.

#### Scenario: Matches the reference
- **WHEN** the ISS elements are propagated to +1440 minutes
- **THEN** the TEME position is within 1 m of python-sgp4

#### Scenario: Vallado error case
- **WHEN** Vallado case 28872 is propagated past its decay
- **THEN** the same error code as the reference is returned

### Requirement: Topocentric position
Altitude and azimuth SHALL be computed from TEME position, IAU-82 GMST and the observer's WGS-84 position,
agreeing with Skyfield to 0.05° for the tested instants.

#### Scenario: Parallax
- **WHEN** the ISS is at a tested instant above Stockholm
- **THEN** altitude and azimuth match Skyfield to 0.05°

### Requirement: Sunlit state and element age
Each satellite SHALL be classified sunlit or eclipsed with a cylindrical Earth shadow. Sets whose element age
exceeds 14 days SHALL be hidden, and the launch toast SHALL name the data age.

#### Scenario: Midnight shadow
- **WHEN** a satellite at 550 km lies on the anti-solar line
- **THEN** it is eclipsed

#### Scenario: Before launch
- **WHEN** `--datetime` is 30 days before the element epochs
- **THEN** no satellites are drawn and the toast says the data does not cover that date

### Requirement: Space station markers
The ISS (25544) and Tiangong (48274) SHALL be drawn above the horizon like planets: labeled "ISS"/"Tiangong", a
distinct glyph (`⌖` Unicode, `#` ASCII) and color, above stars and planets, dimmed while eclipsed. On by default;
`i` toggles.

#### Scenario: No data
- **WHEN** no station data is available
- **THEN** nothing is drawn and `i` shows "Stations: no data (run once online)"

### Requirement: Next ISS pass
While stations are on, a line in the bottom-left corner SHALL read "ISS up: 34° NW" (with "in shadow" when
eclipsed) while the ISS is above 10°, otherwise "ISS 21:43 WSW, max 67°" (local time, with the weekday when not
today) for the next visible pass within 72 hours, otherwise "ISS: no visible pass in 72 h". The geometric pass
(rise, culmination, set above 10°) SHALL match Skyfield `find_events` to 20 s and 0.5°. Predictions SHALL be cached
while simulation time stays between the computation instant and the end of the predicted pass, and recomputed at
most twice per wall second.

#### Scenario: Matches Skyfield
- **WHEN** passes for the tested ISS elements over Stockholm are computed geometrically
- **THEN** each rise, culmination and set matches Skyfield within 20 s, and peak altitude within 0.5°

#### Scenario: Fast-forward stays cheap
- **WHEN** time runs at 3600x for one minute
- **THEN** no more than 120 predictions are computed
