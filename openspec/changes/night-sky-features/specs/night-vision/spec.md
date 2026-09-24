## ADDED Requirements

### Requirement: Red-on-black rendering
The application SHALL draw every glyph in red on black while night vision is on: stars, planets, the Moon,
labels, constellation lines, grid, cardinal letters, space stations, Starlink dots, vectors, the metadata panel,
corner lines, the help modal and toasts, including the area outside the sky window. The Moon SHALL use a text
glyph, not an emoji. No other hue SHALL be emitted.

#### Scenario: Starlink is red, not blue
- **WHEN** night vision and the Starlink overlay are both on
- **THEN** Starlink dots are drawn in red

#### Scenario: Moon
- **WHEN** night vision is on and the Moon is above the horizon
- **THEN** the Moon is a red text glyph

#### Scenario: Light terminal theme
- **WHEN** the terminal's default background is white and night vision is turned on
- **THEN** the whole screen background becomes black

### Requirement: Brightness hierarchy in red
Night vision SHALL use three red intensities by render role: bright (Sun, Moon, planets, sunlit stations), medium
(stars, labels, sunlit Starlinks, UI text), dim (eclipsed satellites, constellation lines, vectors, grid). With
at least 256 colors: xterm 196/160/88 on index 16. Otherwise `COLOR_RED` with bold/normal/dim on `COLOR_BLACK`.

#### Scenario: Sunlit and eclipsed stay distinct
- **WHEN** night vision is on and `X` shows eclipsed Starlinks
- **THEN** sunlit Starlinks are medium red and eclipsed ones dim red

### Requirement: Independent of the color setting
Night vision SHALL work whether or not day colors are on, and turning it off SHALL restore the previous rendering.

#### Scenario: Round trip
- **WHEN** colors are off and the user presses `r` twice
- **THEN** the display returns to uncolored rendering

### Requirement: Flag and key
Night vision SHALL be enabled at startup by `--night` and toggled with `r`.

#### Scenario: Start in the dark
- **WHEN** astroterm starts with `--night`
- **THEN** the first frame is red-on-black

### Requirement: No color support
Without color support, `r` SHALL show "Night vision needs a color terminal" and change nothing.

#### Scenario: Monochrome terminal
- **WHEN** `has_colors()` is false and the user presses `r`
- **THEN** the toast explains why
