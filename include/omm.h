/* CelesTrak OMM CSV parsing.
 *
 * Translates CelesTrak's CSV rendering of Orbit Mean-Elements Messages into
 * SGP4 mean elements. This is the only module that knows the wire format.
 * CSV replaces the two-line element (TLE) format because TLE cannot represent
 * the 6-digit catalog numbers assigned since July 2026.
 *
 * Input is untrusted: columns are found by header name, quoted fields are
 * handled, numbers are parsed without the C locale (the application calls
 * setlocale(LC_ALL, "")), and rows failing range checks are skipped. Object
 * names are never read, so nothing from the feed reaches the terminal.
 *
 * Reference: https://celestrak.org/NORAD/documentation/gp-data-formats.php
 */

#ifndef OMM_H
#define OMM_H

#include "sgp4.h"

#include <stdbool.h>
#include <stddef.h>

struct OmmRecord
{
    long catnr;                   // NORAD catalog number
    struct Sgp4Elements elements; // Mean elements in SGP4 units
};

/* Parse a decimal number such as "-.27596078E-3" independently of the locale.
 * Rejects anything else, including "nan" and "inf"
 */
bool omm_parse_number(const char *str, size_t len, double *out);

/* Parse an ISO 8601 UTC epoch such as "2026-09-24T06:48:06.955776" into a
 * Julian date
 */
bool omm_parse_epoch(const char *str, size_t len, double *jd_out);

/* Parse CSV text (header row first). Allocates *records_out (caller frees).
 * Returns the number of valid rows, or -1 on allocation failure or when the
 * required columns are missing
 */
int omm_parse_csv(const char *data, size_t len, struct OmmRecord **records_out);

#endif // OMM_H
