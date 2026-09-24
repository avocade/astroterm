#include "sgp4.h"
#include "macros.h"

#include <math.h>
#include <string.h>

// WGS-72 constants, as used by NORAD when fitting TLEs
#define SGP4_MU 398600.8                 // Earth gravitational parameter (km^3/s^2)
#define SGP4_RADIUS_EARTH_KM 6378.135    // Equatorial radius (km)
#define SGP4_J2 0.001082616
#define SGP4_J3 -0.00000253881
#define SGP4_J4 -0.00000165597
#define SGP4_J3OJ2 (SGP4_J3 / SGP4_J2)

#define TWO_PI (2.0 * M_PI)
#define X2O3 (2.0 / 3.0)

static double sgp4_xke(void)
{
    // sqrt(mu) in earth radii^1.5 per minute
    return 60.0 / sqrt(SGP4_RADIUS_EARTH_KM * SGP4_RADIUS_EARTH_KM * SGP4_RADIUS_EARTH_KM / SGP4_MU);
}

double sgp4_gmst(double jd_ut1)
{
    double tut1 = (jd_ut1 - 2451545.0) / 36525.0;
    double seconds = -6.2e-6 * tut1 * tut1 * tut1 + 0.093104 * tut1 * tut1 + (876600.0 * 3600 + 8640184.812866) * tut1 +
                     67310.54841;
    double gmst = fmod(seconds * (M_PI / 180.0) / 240.0, TWO_PI);
    if (gmst < 0.0)
    {
        gmst += TWO_PI;
    }
    return gmst;
}

int sgp4_init(struct Sgp4State *s, const struct Sgp4Elements *el)
{
    memset(s, 0, sizeof(*s));
    s->el = *el;

    const double xke = sgp4_xke();
    const double ss = 78.0 / SGP4_RADIUS_EARTH_KM + 1.0;
    const double qzms2t = pow((120.0 - 78.0) / SGP4_RADIUS_EARTH_KM, 4);

    const double ecco = el->ecco;
    const double inclo = el->inclo;
    const double bstar = el->bstar;

    // Un-Kozai the mean motion
    double eccsq = ecco * ecco;
    double omeosq = 1.0 - eccsq;
    double rteosq = sqrt(omeosq);
    double cosio = cos(inclo);
    double cosio2 = cosio * cosio;

    double ak = pow(xke / el->no_kozai, X2O3);
    double d1 = 0.75 * SGP4_J2 * (3.0 * cosio2 - 1.0) / (rteosq * omeosq);
    double del = d1 / (ak * ak);
    double adel = ak * (1.0 - del * del - del * (1.0 / 3.0 + 134.0 * del * del / 81.0));
    del = d1 / (adel * adel);
    double no_unkozai = el->no_kozai / (1.0 + del);

    double ao = pow(xke / no_unkozai, X2O3);
    double sinio = sin(inclo);
    double po = ao * omeosq;
    double con42 = 1.0 - 5.0 * cosio2;
    double con41 = -con42 - cosio2 - cosio2;
    double posq = po * po;
    double rp = ao * (1.0 - ecco);

    s->no_unkozai = no_unkozai;
    s->ao = ao;
    s->con41 = con41;
    s->cosio = cosio;
    s->sinio = sinio;

    if (no_unkozai <= 0.0)
    {
        s->error = SGP4_ERR_MEAN_MOTION;
        return s->error;
    }

    if (TWO_PI / no_unkozai >= 225.0)
    {
        s->error = SGP4_ERR_DEEP_SPACE;
        return s->error;
    }

    if (ecco < 0.0 || ecco >= 1.0)
    {
        s->error = SGP4_ERR_ECCENTRICITY;
        return s->error;
    }

    // Perigee below 220 km: simplified drag model
    s->isimp = rp < (220.0 / SGP4_RADIUS_EARTH_KM + 1.0);

    double sfour = ss;
    double qzms24 = qzms2t;
    double perige = (rp - 1.0) * SGP4_RADIUS_EARTH_KM;
    if (perige < 156.0)
    {
        sfour = perige - 78.0;
        if (perige < 98.0)
        {
            sfour = 20.0;
        }
        qzms24 = pow((120.0 - sfour) / SGP4_RADIUS_EARTH_KM, 4);
        sfour = sfour / SGP4_RADIUS_EARTH_KM + 1.0;
    }

    double pinvsq = 1.0 / posq;
    double tsi = 1.0 / (ao - sfour);
    double eta = ao * ecco * tsi;
    double etasq = eta * eta;
    double eeta = ecco * eta;
    double psisq = fabs(1.0 - etasq);
    double coef = qzms24 * pow(tsi, 4);
    double coef1 = coef / pow(psisq, 3.5);
    double cc2 = coef1 * no_unkozai *
                 (ao * (1.0 + 1.5 * etasq + eeta * (4.0 + etasq)) +
                  0.375 * SGP4_J2 * tsi / psisq * con41 * (8.0 + 3.0 * etasq * (8.0 + etasq)));
    double cc1 = bstar * cc2;
    double cc3 = 0.0;
    if (ecco > 1.0e-4)
    {
        cc3 = -2.0 * coef * tsi * SGP4_J3OJ2 * no_unkozai * sinio / ecco;
    }
    double x1mth2 = 1.0 - cosio2;
    double cc4 = 2.0 * no_unkozai * coef1 * ao * omeosq *
                 (eta * (2.0 + 0.5 * etasq) + ecco * (0.5 + 2.0 * etasq) -
                  SGP4_J2 * tsi / (ao * psisq) *
                      (-3.0 * con41 * (1.0 - 2.0 * eeta + etasq * (1.5 - 0.5 * eeta)) +
                       0.75 * x1mth2 * (2.0 * etasq - eeta * (1.0 + etasq)) * cos(2.0 * el->argpo)));
    double cc5 = 2.0 * coef1 * ao * omeosq * (1.0 + 2.75 * (etasq + eeta) + eeta * etasq);

    double cosio4 = cosio2 * cosio2;
    double temp1 = 1.5 * SGP4_J2 * pinvsq * no_unkozai;
    double temp2 = 0.5 * temp1 * SGP4_J2 * pinvsq;
    double temp3 = -0.46875 * SGP4_J4 * pinvsq * pinvsq * no_unkozai;

    s->mdot = no_unkozai + 0.5 * temp1 * rteosq * con41 + 0.0625 * temp2 * rteosq * (13.0 - 78.0 * cosio2 + 137.0 * cosio4);
    s->argpdot = -0.5 * temp1 * con42 + 0.0625 * temp2 * (7.0 - 114.0 * cosio2 + 395.0 * cosio4) +
                 temp3 * (3.0 - 36.0 * cosio2 + 49.0 * cosio4);
    double xhdot1 = -temp1 * cosio;
    s->nodedot = xhdot1 + (0.5 * temp2 * (4.0 - 19.0 * cosio2) + 2.0 * temp3 * (3.0 - 7.0 * cosio2)) * cosio;

    s->omgcof = bstar * cc3 * cos(el->argpo);
    s->xmcof = 0.0;
    if (ecco > 1.0e-4)
    {
        s->xmcof = -X2O3 * coef * bstar / eeta;
    }
    s->nodecf = 3.5 * omeosq * xhdot1 * cc1;
    s->t2cof = 1.5 * cc1;

    // Avoid a division by zero for inclinations of 180 degrees
    if (fabs(cosio + 1.0) > 1.5e-12)
    {
        s->xlcof = -0.25 * SGP4_J3OJ2 * sinio * (3.0 + 5.0 * cosio) / (1.0 + cosio);
    }
    else
    {
        s->xlcof = -0.25 * SGP4_J3OJ2 * sinio * (3.0 + 5.0 * cosio) / 1.5e-12;
    }
    s->aycof = -0.5 * SGP4_J3OJ2 * sinio;

    double delmotemp = 1.0 + eta * cos(el->mo);
    s->delmo = delmotemp * delmotemp * delmotemp;
    s->sinmao = sin(el->mo);
    s->x7thm1 = 7.0 * cosio2 - 1.0;
    s->x1mth2 = x1mth2;
    s->eta = eta;
    s->cc1 = cc1;
    s->cc4 = cc4;
    s->cc5 = cc5;

    if (!s->isimp)
    {
        double cc1sq = cc1 * cc1;
        s->d2 = 4.0 * ao * tsi * cc1sq;
        double temp = s->d2 * tsi * cc1 / 3.0;
        s->d3 = (17.0 * ao + sfour) * temp;
        s->d4 = 0.5 * temp * ao * tsi * (221.0 * ao + 31.0 * sfour) * cc1;
        s->t3cof = s->d2 + 2.0 * cc1sq;
        s->t4cof = 0.25 * (3.0 * s->d3 + cc1 * (12.0 * s->d2 + 10.0 * cc1sq));
        s->t5cof = 0.2 * (3.0 * s->d4 + 12.0 * cc1 * s->d3 + 6.0 * s->d2 * s->d2 + 15.0 * cc1sq * (2.0 * s->d2 + cc1sq));
    }

    // Propagate once to epoch to surface errors early, as the reference does
    double r[3], v[3];
    s->error = SGP4_OK;
    s->error = sgp4_propagate(s, 0.0, r, v);
    return s->error;
}

int sgp4_propagate(const struct Sgp4State *s, double t, double r[3], double v[3])
{
    if (s->error == SGP4_ERR_DEEP_SPACE || s->error == SGP4_ERR_MEAN_MOTION)
    {
        return s->error;
    }

    const double xke = sgp4_xke();
    const double vkmpersec = SGP4_RADIUS_EARTH_KM * xke / 60.0;
    const struct Sgp4Elements *el = &s->el;

    // Secular gravity and atmospheric drag
    double xmdf = el->mo + s->mdot * t;
    double argpdf = el->argpo + s->argpdot * t;
    double nodedf = el->nodeo + s->nodedot * t;
    double argpm = argpdf;
    double mm = xmdf;
    double t2 = t * t;
    double nodem = nodedf + s->nodecf * t2;
    double tempa = 1.0 - s->cc1 * t;
    double tempe = el->bstar * s->cc4 * t;
    double templ = s->t2cof * t2;

    if (!s->isimp)
    {
        double delomg = s->omgcof * t;
        double delmtemp = 1.0 + s->eta * cos(xmdf);
        double delm = s->xmcof * (delmtemp * delmtemp * delmtemp - s->delmo);
        double temp = delomg + delm;
        mm = xmdf + temp;
        argpm = argpdf - temp;
        double t3 = t2 * t;
        double t4 = t3 * t;
        tempa = tempa - s->d2 * t2 - s->d3 * t3 - s->d4 * t4;
        tempe = tempe + el->bstar * s->cc5 * (sin(mm) - s->sinmao);
        templ = templ + s->t3cof * t3 + t4 * (s->t4cof + t * s->t5cof);
    }

    double nm = s->no_unkozai;
    double em = el->ecco;
    double inclm = el->inclo;

    if (nm <= 0.0)
    {
        return SGP4_ERR_MEAN_MOTION;
    }

    double am = pow(xke / nm, X2O3) * tempa * tempa;
    nm = xke / pow(am, 1.5);
    em = em - tempe;

    if (em >= 1.0 || em < -0.001)
    {
        return SGP4_ERR_ECCENTRICITY;
    }
    if (em < 1.0e-6)
    {
        em = 1.0e-6;
    }

    mm = mm + s->no_unkozai * templ;
    double xlm = mm + argpm + nodem;

    nodem = fmod(nodem, TWO_PI);
    argpm = fmod(argpm, TWO_PI);
    xlm = fmod(xlm, TWO_PI);
    mm = fmod(xlm - argpm - nodem, TWO_PI);

    double sinip = sin(inclm);
    double cosip = cos(inclm);

    // Long period periodics
    double axnl = em * cos(argpm);
    double temp = 1.0 / (am * (1.0 - em * em));
    double aynl = em * sin(argpm) + temp * s->aycof;
    double xl = mm + argpm + nodem + temp * s->xlcof * axnl;

    // Solve Kepler's equation
    double u = fmod(xl - nodem, TWO_PI);
    double eo1 = u;
    double tem5 = 9999.9;
    double sineo1 = 0.0;
    double coseo1 = 0.0;
    for (int ktr = 1; fabs(tem5) >= 1.0e-12 && ktr <= 10; ++ktr)
    {
        sineo1 = sin(eo1);
        coseo1 = cos(eo1);
        tem5 = 1.0 - coseo1 * axnl - sineo1 * aynl;
        tem5 = (u - aynl * coseo1 + axnl * sineo1 - eo1) / tem5;
        if (fabs(tem5) >= 0.95)
        {
            tem5 = tem5 > 0.0 ? 0.95 : -0.95;
        }
        eo1 = eo1 + tem5;
    }

    // Short period preliminary quantities
    double ecose = axnl * coseo1 + aynl * sineo1;
    double esine = axnl * sineo1 - aynl * coseo1;
    double el2 = axnl * axnl + aynl * aynl;
    double pl = am * (1.0 - el2);
    if (pl < 0.0)
    {
        return SGP4_ERR_SEMI_LATUS;
    }

    double rl = am * (1.0 - ecose);
    double rdotl = sqrt(am) * esine / rl;
    double rvdotl = sqrt(pl) / rl;
    double betal = sqrt(1.0 - el2);
    temp = esine / (1.0 + betal);
    double sinu = am / rl * (sineo1 - aynl - axnl * temp);
    double cosu = am / rl * (coseo1 - axnl + aynl * temp);
    double su = atan2(sinu, cosu);
    double sin2u = (cosu + cosu) * sinu;
    double cos2u = 1.0 - 2.0 * sinu * sinu;
    temp = 1.0 / pl;
    double temp1 = 0.5 * SGP4_J2 * temp;
    double temp2 = temp1 * temp;

    // Update for short period periodics
    double mrt = rl * (1.0 - 1.5 * temp2 * betal * s->con41) + 0.5 * temp1 * s->x1mth2 * cos2u;
    su = su - 0.25 * temp2 * s->x7thm1 * sin2u;
    double xnode = nodem + 1.5 * temp2 * cosip * sin2u;
    double xinc = inclm + 1.5 * temp2 * cosip * sinip * cos2u;
    double mvt = rdotl - nm * temp1 * s->x1mth2 * sin2u / xke;
    double rvdot = rvdotl + nm * temp1 * (s->x1mth2 * cos2u + 1.5 * s->con41) / xke;

    // Orientation vectors
    double sinsu = sin(su);
    double cossu = cos(su);
    double snod = sin(xnode);
    double cnod = cos(xnode);
    double sini = sin(xinc);
    double cosi = cos(xinc);
    double xmx = -snod * cosi;
    double xmy = cnod * cosi;
    double ux = xmx * sinsu + cnod * cossu;
    double uy = xmy * sinsu + snod * cossu;
    double uz = sini * sinsu;
    double vx = xmx * cossu - cnod * sinsu;
    double vy = xmy * cossu - snod * sinsu;
    double vz = sini * cossu;

    double mr = mrt * SGP4_RADIUS_EARTH_KM;
    r[0] = mr * ux;
    r[1] = mr * uy;
    r[2] = mr * uz;
    v[0] = (mvt * ux + rvdot * vx) * vkmpersec;
    v[1] = (mvt * uy + rvdot * vy) * vkmpersec;
    v[2] = (mvt * uz + rvdot * vz) * vkmpersec;

    if (mrt < 1.0)
    {
        return SGP4_ERR_DECAYED;
    }

    return SGP4_OK;
}
