#pragma once

#ifdef TARGET_PC
#include <revolution/types.h>

class J3DModelData;

// Mario's and Luigi's colours (archipelago.ini: mario_main, mario_overalls, luigi_main, luigi_overalls). The textures of the
// character's models are recoloured in memory once, before the first picture is drawn: what is red on Mario (his cap and
// shirt) or green on Luigi becomes the "main" colour, what is blue (the overalls) the other. A colour the player did not set
// leaves its parts alone.
namespace NebulaPlayerColors {
    // The textures of one model's data. Safe to call again for data already done (each picture is marked).
    void apply(J3DModelData* pData, bool isLuigi);

    // The star's textures when star_colors is on: the gold star becomes the progression colour (#897EC9), the green one the
    // normal colour (#5CBEFF), the blue one the useful colour (#6D8BE8); the red one (traps) stays. Which picture is which is
    // read from the picture's own colour.
    void applyStar(J3DModelData* pData);
};  // namespace NebulaPlayerColors
#endif
