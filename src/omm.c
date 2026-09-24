#include "omm.h"
#include "macros.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FIELDS 64

// The columns we need, by header name
enum OmmColumn
{
    COL_EPOCH = 0,
    COL_MEAN_MOTION,
    COL_ECCENTRICITY,
    COL_INCLINATION,
    COL_RA_OF_ASC_NODE,
    COL_ARG_OF_PERICENTER,
    COL_MEAN_ANOMALY,
    COL_NORAD_CAT_ID,
    COL_BSTAR,
    NUM_COLUMNS
};

static const char *column_names[NUM_COLUMNS] = {
    "EPOCH",          "MEAN_MOTION",       "ECCENTRICITY", "INCLINATION", "RA_OF_ASC_NODE",
    "ARG_OF_PERICENTER", "MEAN_ANOMALY", "NORAD_CAT_ID", "BSTAR",
};

bool omm_parse_number(const char *str, size_t len, double *out)
{
    size_t i = 0;

    // Trim surrounding blanks
    while (i < len && str[i] == ' ')
    {
        i++;
    }
    while (len > i && str[len - 1] == ' ')
    {
        len--;
    }

    double sign = 1.0;
    if (i < len && (str[i] == '-' || str[i] == '+'))
    {
        sign = str[i] == '-' ? -1.0 : 1.0;
        i++;
    }

    double mantissa = 0.0;
    int digits = 0;
    int scale = 0; // Power of ten applied to the mantissa

    while (i < len && str[i] >= '0' && str[i] <= '9')
    {
        if (digits < 17)
        {
            mantissa = mantissa * 10.0 + (str[i] - '0');
        }
        else
        {
            scale++;
        }
        digits++;
        i++;
    }
    if (i < len && str[i] == '.')
    {
        i++;
        while (i < len && str[i] >= '0' && str[i] <= '9')
        {
            if (digits < 17)
            {
                mantissa = mantissa * 10.0 + (str[i] - '0');
                scale--;
            }
            digits++;
            i++;
        }
    }
    if (digits == 0)
    {
        return false;
    }

    if (i < len && (str[i] == 'e' || str[i] == 'E'))
    {
        i++;
        int exp_sign = 1;
        if (i < len && (str[i] == '-' || str[i] == '+'))
        {
            exp_sign = str[i] == '-' ? -1 : 1;
            i++;
        }
        int exponent = 0;
        int exp_digits = 0;
        while (i < len && str[i] >= '0' && str[i] <= '9' && exp_digits < 4)
        {
            exponent = exponent * 10 + (str[i] - '0');
            exp_digits++;
            i++;
        }
        if (exp_digits == 0)
        {
            return false;
        }
        scale += exp_sign * exponent;
    }

    if (i != len)
    {
        return false;
    }

    double value = scale >= 0 ? mantissa * pow(10.0, scale) : mantissa / pow(10.0, -scale);
    if (!isfinite(value))
    {
        return false;
    }
    *out = sign * value;
    return true;
}

static bool parse_int(const char *str, size_t len, int *out)
{
    int value = 0;
    if (len == 0 || len > 9)
    {
        return false;
    }
    for (size_t i = 0; i < len; ++i)
    {
        if (str[i] < '0' || str[i] > '9')
        {
            return false;
        }
        value = value * 10 + (str[i] - '0');
    }
    *out = value;
    return true;
}

/* Days from 1970-01-01 to a proleptic Gregorian date (Howard Hinnant's
 * days_from_civil)
 */
static long days_from_civil(long y, int m, int d)
{
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    long yoe = y - era * 400;
    long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

bool omm_parse_epoch(const char *s, size_t len, double *jd_out)
{
    // YYYY-MM-DDTHH:MM:SS[.ffffff]
    int year, month, day, hour, minute, second;
    if (len < 19 || s[4] != '-' || s[7] != '-' || s[10] != 'T' || s[13] != ':' || s[16] != ':' ||
        !parse_int(s, 4, &year) || !parse_int(s + 5, 2, &month) || !parse_int(s + 8, 2, &day) ||
        !parse_int(s + 11, 2, &hour) || !parse_int(s + 14, 2, &minute) || !parse_int(s + 17, 2, &second))
    {
        return false;
    }
    if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 60)
    {
        return false;
    }

    double fraction = 0.0;
    if (len > 19)
    {
        if (s[19] != '.' || !omm_parse_number(s + 19, len - 19, &fraction))
        {
            return false;
        }
    }

    double seconds = hour * 3600.0 + minute * 60.0 + second + fraction;
    *jd_out = 2440587.5 + (double)days_from_civil(year, month, day) + seconds / 86400.0;
    return true;
}

/* Split one CSV line into fields, honouring double quotes. Returns the number
 * of fields
 */
static int split_csv(const char *line, size_t len, const char **starts, size_t *lens, int max_fields)
{
    int n = 0;
    size_t i = 0;
    while (n < max_fields)
    {
        size_t start = i;
        if (i < len && line[i] == '"')
        {
            // Quoted field: runs to the closing quote ("" is an escaped quote)
            i++;
            start = i;
            while (i < len && !(line[i] == '"' && (i + 1 >= len || line[i + 1] != '"')))
            {
                i += (line[i] == '"') ? 2 : 1;
            }
            starts[n] = line + start;
            lens[n] = MIN(i, len) - start;
            n++;
            i++; // Closing quote
            while (i < len && line[i] != ',')
            {
                i++;
            }
        }
        else
        {
            while (i < len && line[i] != ',')
            {
                i++;
            }
            starts[n] = line + start;
            lens[n] = i - start;
            n++;
        }
        if (i >= len)
        {
            break;
        }
        i++; // Comma
    }
    return n;
}

static bool parse_row(const char **f, const size_t *l, const int *col, struct OmmRecord *out)
{
    double v[NUM_COLUMNS];
    for (int c = 0; c < NUM_COLUMNS; ++c)
    {
        if (c == COL_EPOCH)
        {
            if (!omm_parse_epoch(f[col[c]], l[col[c]], &v[c]))
            {
                return false;
            }
        }
        else if (!omm_parse_number(f[col[c]], l[col[c]], &v[c]))
        {
            return false;
        }
    }

    double catnr = v[COL_NORAD_CAT_ID];
    if (catnr < 1 || catnr > 999999999 || catnr != floor(catnr) || v[COL_ECCENTRICITY] < 0.0 ||
        v[COL_ECCENTRICITY] >= 1.0 || v[COL_MEAN_MOTION] <= 0.0 || v[COL_MEAN_MOTION] > 20.0 ||
        v[COL_INCLINATION] < 0.0 || v[COL_INCLINATION] > 180.0 || fabs(v[COL_RA_OF_ASC_NODE]) > 360.0 ||
        fabs(v[COL_ARG_OF_PERICENTER]) > 360.0 || fabs(v[COL_MEAN_ANOMALY]) > 360.0 || fabs(v[COL_BSTAR]) > 1.0)
    {
        return false;
    }

    out->catnr = (long)catnr;
    out->elements = (struct Sgp4Elements){
        .epoch_jd = v[COL_EPOCH],
        .bstar = v[COL_BSTAR],
        .ecco = v[COL_ECCENTRICITY],
        .inclo = v[COL_INCLINATION] * TO_RAD,
        .nodeo = v[COL_RA_OF_ASC_NODE] * TO_RAD,
        .argpo = v[COL_ARG_OF_PERICENTER] * TO_RAD,
        .mo = v[COL_MEAN_ANOMALY] * TO_RAD,
        .no_kozai = v[COL_MEAN_MOTION] * (2.0 * M_PI) / 1440.0, // rev/day to rad/min
    };
    return true;
}

int omm_parse_csv(const char *data, size_t len, struct OmmRecord **records_out)
{
    const char *fields[MAX_FIELDS];
    size_t lens[MAX_FIELDS];
    int col[NUM_COLUMNS];
    bool have_header = false;
    int max_col = 0;

    // Upper bound on rows: one per newline, plus one
    size_t capacity = 1;
    for (size_t i = 0; i < len; ++i)
    {
        capacity += data[i] == '\n';
    }
    struct OmmRecord *records = malloc(capacity * sizeof(struct OmmRecord));
    if (records == NULL)
    {
        return -1;
    }

    int count = 0;
    size_t pos = 0;
    while (pos < len)
    {
        size_t start = pos;
        while (pos < len && data[pos] != '\n')
        {
            pos++;
        }
        size_t line_len = pos - start;
        if (line_len > 0 && data[start + line_len - 1] == '\r')
        {
            line_len--;
        }
        pos++;

        if (line_len == 0)
        {
            continue;
        }

        int n = split_csv(data + start, line_len, fields, lens, MAX_FIELDS);

        if (!have_header)
        {
            for (int c = 0; c < NUM_COLUMNS; ++c)
            {
                col[c] = -1;
                for (int k = 0; k < n; ++k)
                {
                    if (lens[k] == strlen(column_names[c]) && memcmp(fields[k], column_names[c], lens[k]) == 0)
                    {
                        col[c] = k;
                        max_col = MAX(max_col, k);
                    }
                }
                if (col[c] < 0)
                {
                    free(records);
                    return -1;
                }
            }
            have_header = true;
            continue;
        }

        if (n > max_col && (size_t)count < capacity && parse_row(fields, lens, col, &records[count]))
        {
            count++;
        }
    }

    if (!have_header)
    {
        free(records);
        return -1;
    }

    *records_out = records;
    return count;
}
