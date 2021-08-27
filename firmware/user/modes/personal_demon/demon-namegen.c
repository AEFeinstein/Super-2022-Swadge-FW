#include "user_main.h"
#include "demon-namegen.h"
#include "esp_niceness.h"

// const char *nm1[] = {"", "b", "br", "d", "dr", "g", "j", "k", "m", "r", "s", "t", "th", "tr", "v", "x", "z"};
// const char *nm2[] = {"a", "e", "i", "o", "u"};
// const char *nm3[] = {"g", "g'dr", "g'th", "gdr", "gg", "gl", "gm", "gr", "gth", "k", "l'g", "lg", "lgr", "llm", "lm", "lr", "lv", "n", "ngr", "nn", "r", "r'", "r'g", "rg", "rgr", "rk", "rn", "rr", "rthr", "rz", "str", "th't", "z", "z'g", "zg", "zr", "zz"};
// const char *nm4[] = {"a", "e", "i", "o", "u", "iu", "uu", "au", "aa"};
// const char *nm5[] = {"d", "k", "l", "ll", "m", "n", "nn", "r", "th", "x", "z"};
// const char *nm6[] = {"ch", "d", "g", "k", "l", "n", "r", "s", "th", "z"};
const char* nm1[] = {"", "", "", "", "b", "br", "d", "dr", "g", "j", "k", "m", "r", "s", "t", "th", "tr", "v", "x", "z"};
const char* nm2[] = {"a", "e", "i", "o", "u", "a", "a", "o", "o"};
const char* nm3[] = {"g", "g'dr", "g'th", "gdr", "gg", "gl", "gm", "gr", "gth", "k", "l'g", "lg", "lgr", "llm", "lm", "lr", "lv", "n", "ngr", "nn", "r", "r'", "r'g", "rg", "rgr", "rk", "rn", "rr", "rthr", "rz", "str", "th't", "z", "z'g", "zg", "zr", "zz"};
const char* nm4[] = {"a", "e", "i", "o", "u", "a", "a", "o", "o", "a", "e", "i", "o", "u", "a", "a", "o", "o", "a", "e", "i", "o", "u", "a", "a", "o", "o", "a", "e", "i", "o", "u", "a", "a", "o", "o", "a", "e", "i", "o", "u", "a", "a", "o", "o", "iu", "uu", "au", "aa"};
const char* nm5[] = {"d", "k", "l", "ll", "m", "m", "m", "n", "n", "n", "nn", "r", "r", "r", "th", "x", "z"};
const char* nm6[] = {"ch", "d", "g", "k", "l", "n", "n", "n", "n", "n", "r", "s", "th", "th", "th", "th", "th", "z"};

/**
 * @brief Randomly generate a demon name
 *
 * @param name    A pointer to store the name in
 * @param namelen The length of the name
 */
void ICACHE_FLASH_ATTR namegen(char* name)
{
    int32_t nTp = os_random() % 3;
    int32_t rnd = os_random() % lengthof(nm1);
    int32_t rnd2 = os_random() % lengthof(nm2);
    int32_t rnd3 = os_random() % lengthof(nm6);
    int32_t rnd4 = os_random() % lengthof(nm3);
    int32_t rnd5 = os_random() % lengthof(nm4);
    while (nm3[rnd4] == nm1[rnd] || nm3[rnd4] == nm6[rnd3])
    {
        rnd4 = os_random() % lengthof(nm3);
    }
    if (nTp == 0)
    {
        ets_strcat(name, nm1[rnd]);
        ets_strcat(name, nm2[rnd2]);
        ets_strcat(name, nm3[rnd4]);
        ets_strcat(name, nm4[rnd5]);
        ets_strcat(name, nm6[rnd3]);
    }
    else
    {
        int32_t rnd6 = os_random() % lengthof(nm2);
        int32_t rnd7 = os_random() % lengthof(nm5);
        while (nm5[rnd7] == nm3[rnd4] || nm5[rnd7] == nm6[rnd3])
        {
            rnd7 = os_random() % lengthof(nm5);
        }
        ets_strcat(name, nm1[rnd]);
        ets_strcat(name, nm2[rnd2]);
        ets_strcat(name, nm3[rnd4]);
        ets_strcat(name, nm2[rnd6]);
        ets_strcat(name, nm5[rnd7]);
        ets_strcat(name, nm4[rnd5]);
        ets_strcat(name, nm6[rnd3]);
    }
    // testSwear(nMs);
}
