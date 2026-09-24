## ADDED Requirements

### Requirement: Drift-free simulation time
Simulation time SHALL be `anchor_jd + (wall_now − anchor_wall) × speed / 86400` (days), with the anchor reset at
every change of speed, pause state or reference instant. It SHALL NOT be accumulated from frame periods. The
wall clock SHALL be the system realtime clock, so suspending the process or the machine causes no lag.

#### Scenario: Suspended process
- **WHEN** astroterm is stopped with SIGSTOP for 60 seconds and resumed
- **THEN** the displayed time matches the system clock within 1 second (upstream v1.2.0 lags by 60 seconds)

#### Scenario: Slow frames
- **WHEN** 100 frames each take 200 ms at a nominal 24 fps
- **THEN** simulation time advances by 20 seconds, not 4.2 seconds

#### Scenario: Starting datetime honoured
- **WHEN** astroterm starts with `--datetime 2026-08-12T22:00:00`
- **THEN** the first frame shows the sky at that UTC instant

### Requirement: Pause
`space` SHALL freeze simulation time; pressing it again SHALL resume from the frozen instant at the same speed.

#### Scenario: Pause and resume
- **WHEN** the user pauses, waits 10 s, and resumes
- **THEN** simulation time continues from the paused instant

### Requirement: Speed ladder
`>`/`.` and `<`/`,` SHALL step through `1, 10, 60, 600, 3600` (sim seconds per wall second). A `--speed` value
off the ladder SHALL be kept until the first step, which moves to the next rung in that direction.

#### Scenario: Speed up
- **WHEN** speed is 1 and the user presses `>` twice
- **THEN** speed is 60 and the toast reads "Speed: 60x"

#### Scenario: Ladder ends
- **WHEN** speed is 3600 and the user presses `>`
- **THEN** speed stays 3600

### Requirement: Jump to now
`n` SHALL set simulation time to the system time, speed to 1 and unpause.

#### Scenario: Return to the present
- **WHEN** the user has fast-forwarded two days and presses `n`
- **THEN** the sky shows the present moment at realtime speed

### Requirement: Clock state visible
The metadata panel SHALL show the speed and a paused marker.

#### Scenario: Paused marker
- **WHEN** the clock is paused at 60x
- **THEN** the panel shows "Speed: paused (60x)"
