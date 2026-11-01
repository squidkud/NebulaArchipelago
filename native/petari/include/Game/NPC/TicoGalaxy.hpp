#pragma once

#include "Game/NPC/TicoFat.hpp"

class TicoStarRing;

class TicoGalaxy : public TicoFat {
public:
    TicoGalaxy(const char*);
#ifdef TARGET_PC
    virtual ~TicoGalaxy();
    // Archipelago: a Luma whose dome was shut when the scene loaded appears as soon as the dome opens (a Grand Star arrives
    // while the player stands in the Observatory). Called every frame.
    static void reviveWaiting();
    // The scene's Lumas are gone (or none made yet): forget them. The game frees a scene's actors without running their
    // destructors, so the list would otherwise hold dead pointers in the next scene.
    static void forgetAll();
#endif

    virtual void init(const JMapInfoIter&);
    virtual void initAfterPlacement();
    virtual void shootStarPiece();
    virtual void startEat();
    virtual bool enableAppear();
    virtual void initStarPieceSaveData(const JMapInfoIter&);
    virtual void addStarPieceSaveData(s32);
    virtual void disappear(bool);
    virtual void appearInformation() const;

    /* 0x208 */ const char* mGalaxyName;
    /* 0x208 */ TicoStarRing* mStarRing;
};
