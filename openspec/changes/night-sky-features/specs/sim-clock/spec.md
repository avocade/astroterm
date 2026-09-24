## ADDED Requirements

### Requirement: Drift-free simulation time
Simulation time SHALL be computed as `anchor_jd + (wall_now - anchor_wall) × speed`, where the anchor is reset
whenever speed, pause state or the reference time changes. Simulation time SHALL NOT be accumulated from frame
durations.

#### Scenario: Realtime stays locked to the wall clock
- **WHEN** astroterm runs at speed 1 without `--datetime` for 6 hours
- **THEN** simulation time differs from the system clock by less than 1 second

#### Scenario: Starting datetime honoured
- **WHEN** astroterm starts with `--datetime 2026-08-12T22:00:00`
- **THEN** the first frame shows the sky at that UTC instant

### Requirement: Pause
`space` SHALL freeze simulation time; pressing it again SHALL resume at the previous speed from the frozen instant.

#### Scenario: Pause and resume
- **WHEN** the user pauses, waits 10 s, and resumes
- **THEN** simulation time continues from the paused instant rather than jumping forward 10 s

### Requirement: Speed ladder
`>` (or `.`) and `<` (or `,`) SHALL step through the speed ladder
`-3600, -600, -60, -10, -1, 1, 10, 60, 600, 3600` (sim seconds per wall second). A speed given with `--speed`
that is not on the ladder SHALL snap to the nearest ladder value on the first step.

#### Scenario: Speed up
- **WHEN** speed is 1 and the user presses `>` twice
- **THEN** speed is 60 and the toast reads "Speed: 60x"

#### Scenario: Into reverse
- **WHEN** speed is 1 and the user presses `<`
- **THEN** speed is -1 (time runs backwards) and the toast reads "Speed: -1x (reverse)"

#### Scenario: Ladder ends
- **WHEN** speed is 3600 and the user presses `>`
- **THEN** speed stays 3600

### Requirement: Jump to now
`n` SHALL set simulation time to the current system time, speed to 1 and unpause.

#### Scenario: Return to the present
- **WHEN** the user has fast-forwarded two days and presses `n`
- **THEN** the sky shows the present moment at realtime speed

### Requirement: Clock state visible
The metadata panel SHALL show the current speed and a paused marker, and elapsed time SHALL be signed so reverse
travel reads correctly.

#### Scenario: Reverse elapsed time
- **WHEN** time has run backwards by 1 hour from the start
- **THEN** elapsed time is shown as "-000 years, 000 days, 01:00:00"
