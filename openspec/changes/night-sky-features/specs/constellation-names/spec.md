## ADDED Requirements

### Requirement: Constellation name labels
When constellation names are on (`N` key, `--constellation-names` flag), each constellation in the figure data SHALL be
labeled with its full IAU name (for example "Ursa Major" for `UMa`), centered on the mean direction of its
figure's stars, if that point is above the horizon. Names SHALL be drawn in the dim tier, beneath stars and
planets. Names are independent of whether figures (`C`) are drawn.

#### Scenario: Ursa Major
- **WHEN** names are on and the centroid of Ursa Major's figure is above the horizon
- **THEN** "Ursa Major" is drawn centered on that point

#### Scenario: Below the horizon
- **WHEN** the centroid of a constellation is below the horizon
- **THEN** its name is not drawn

#### Scenario: Name table complete
- **WHEN** every abbreviation in `bsc5_constellations.txt` is looked up
- **THEN** each resolves to a full name
