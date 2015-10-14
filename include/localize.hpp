#ifndef LOCALIZE_HPP
#define LOCALIZE_HPP

#include <string>


enum Localization {
    DE_LATIN,
    DE_MADO,
    DE_TENGWAR,

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

    LS_UNIT_KM,
    LS_UNIT_M_S,
    LS_UNIT_TIMES,
};


const std::string localize(int i);
const std::string localize(float f, int prec = 2,
    LocalizedStrings unit = LS_REALLY_NOTHING_AND_NOTHING_BUT);
const std::string localize(LocalizedStrings str);


extern Localization olo;

#endif
