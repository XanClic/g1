#ifndef LOCALIZE_HPP
#define LOCALIZE_HPP

#include <string>


enum Localization {
    DE_LATIN,
    DE_MADO,
    DE_TENGWAR,
    EN_US_LATIN,

    LOCALIZATIONS
};

enum LocalizedStrings {
    LS_REALLY_NOTHING_AND_NOTHING_BUT,

    LS_SEPARATOR,

    LS_ORBITAL_VELOCITY,
    LS_HEIGHT_OVER_GROUND,
    LS_TEST_ENVIRONMENT,
    LS_TEST_SCENARIO,
    LS_LOADING,
    LS_SPEED_UP,

    LS_UNIT_M,
    LS_UNIT_M_S,
    LS_UNIT_TIMES,

    LS_UNIT_PREFIX_MILLI,
    LS_UNIT_PREFIX_KILO,
    LS_UNIT_PREFIX_MEGA,
};


const std::string localize(int i);
const std::string localize(float f, int prec = 2,
    LocalizedStrings unit = LS_REALLY_NOTHING_AND_NOTHING_BUT);
const std::string localize(LocalizedStrings str);


extern Localization olo;

#endif
