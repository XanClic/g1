#include <stdexcept>
#include <string>

#include "localize.hpp"


Localization olo = DE_LATIN;


static const std::string translations[][LOCALIZATIONS] = {
    {
        "",
        "",
        "",
        "",
    },

    {
        " ",
        " ",
        "\x0f",
        " ",
    },

    {
        "Orbitalgeschwindigkeit",
        "Orbitalgeschwindigkeit",
    //   ō   r       bi  ta  l       ge      sch     wi  n       di  ch      ka      -i      t
        "\x24\xc3\xaa\x69\x30\xc3\xb4\xc2\xbb\xc3\xb3\x79\xc3\xab\x39\xc3\xb7\xc2\xb0\xc3\xba\xc3\xa8",
        "Orbital velocity",
    },

    {
        "Höhe über Grund",
        "Höhe über Grund",
    //   hö  he  _   ü   be  r       _   g       ru  n       t
        "\x5e\x5b\x0f\x27\x6b\xc3\xaa\x0f\xc3\xb6\x42\xc3\xab\xc3\xa8",
        "Height over ground",
    },

    {
        "Testumgebung",
        "Testumgebung",
    //  te   s       t       u   m       ge      bu  ng
        "\x33\xc3\xb2\xc3\xa8\x2a\xc3\xb1\xc2\xbb\x6a\xc3\xb0",
        "Test environment",
    },

    {
        "Testszenario",
        "Testszenario",
    //  te   s       t       se      na  ri  ō
        "\x33\xc3\xb2\xc3\xa8\xc2\x8b\x48\x41\x24",
        "Test scenario",
    },

    {
        "Lädt...",
        "Lädt...",
    //  SOH  lä       t      EOF
        "\x02\xc2\xa5\xc3\xa8\x03",
        "Loading...",
    },

    {
        "Beschleunigung",
        "Beschleunigung",
    //   be  sch     lo      -i      ni  gu      ng
        "\x6b\xc3\xb3\xc2\xa4\xc3\xba\x49\xc2\xba\xc3\xb0",
        "Speed-up",
    },


    {
        "km",
        "km",
    //   ka      e   m"
        "\xc2\xb0\x2b\xc3\xb1",
        "km",
    },

    {
        "m/s",
        "m/s",
    //   e   m       /   e   s
        "\x2b\xc3\xb1\x06\x2b\xc3\xb2",
        "m/s",
    },

    {
        "x",
        "-fach",
    //   -   fa  ch
        "\x01\x70\xc3\xb7",
        "x",
    },
};


const std::string localize(int i)
{
    if (olo != DE_TENGWAR) {
        return std::to_string(i);
    } else {
        char bump[16];

        int o = 0;
        if (i < 0) {
            bump[o++] = 1;
            i = -i;

            if (i < 0) {
                throw std::range_error("Oh god please no");
            }
        }

        do {
            bump[o++] = i % 10 + 16;
            i /= 10;
        } while (i > 0);
        bump[o] = 0;

        return std::string(bump);
    }
}


static const std::string localize_float(float f, int prec)
{
    if (olo != DE_TENGWAR) {
        char bump[32];
        snprintf(bump, 32, "%.*f", prec, f);
        return std::string(bump);
    } else {
        // FIXME
        return localize(static_cast<int>(f));
    }
}


const std::string localize(float f, int prec, LocalizedStrings unit)
{
    const std::string unit_str = localize(unit);
    if (unit_str.empty()) {
        return localize_float(f, prec);
    } else {
        if (unit == LS_UNIT_TIMES) {
            return localize_float(f, prec) + unit_str;
        } else {
            return localize_float(f, prec) + localize(LS_SEPARATOR) + unit_str;
        }
    }
}


const std::string localize(LocalizedStrings str)
{
    return translations[str][olo];
}
