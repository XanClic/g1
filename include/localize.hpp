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
    LS_ORBITAL_VELOCITY,
    LS_HEIGHT_OVER_GROUND,
    LS_TEST_ENVIRONMENT,
    LS_TEST_SCENARIO,
    LS_LOADING,
};


std::string localize(int i);
std::string localize(float f, int prec = 2);
std::string localize(LocalizedStrings str);


extern Localization olo;

#endif
