#include <stdexcept>
#include <string>

#include "localize.hpp"


Localization olo = DE_LATIN;


static const char *translations[][3] = {
    {
        "Orbitalgeschwindigkeit",
        "Orbitalgeschwindigkeit",
    //   ō   r   bi      ta  l       ge      sch     wi  n       di  ch      ka      -i      t
        "\x24\xc3\xaa\x69\x30\xc3\xb4\xc2\xbb\xc3\xb3\x79\xc3\xab\x39\xc3\xb7\xc2\xb0\xc3\xba\xc3\xa8",
    },

    {
        "Höhe über Grund",
        "Höhe über Grund",
    //   hö  he  _   ü   be  r       _   g       ru  n       t
        "\x5e\x5b\x0f\x27\x6b\xc3\xaa\x0f\xc3\xb6\x42\xc3\xab\xc3\xa8",
    },

    {
        "Testumgebung",
        "Testumgebung",
    //  te   s       t       u   m       ge      bu  ng
        "\x33\xc3\xb2\xc3\xa8\x2a\xc3\xb1\xc2\xbb\x6a\xc3\xb0",
    },

    {
        "Lädt...",
        "Lädt...",
    //  SOH  lä       t      EOF
        "\x02\xc2\xa5\xc3\xa8\x03",
    },
};


std::string localize(int i)
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


std::string localize(float f, int prec)
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


std::string localize(LocalizedStrings str)
{
    return translations[str][olo];
}
