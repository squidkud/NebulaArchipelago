#pragma once

#include "Game/LiveActor/LiveActor.hpp"
#include <JSystem/J3DGraphBase/J3DTexture.hpp>

#ifdef TARGET_PC
class AstroCountDownPlateDigit;
#endif

class AstroCountDownPlate : public LiveActor {
public:
    AstroCountDownPlate(const char*);

    virtual void init(const JMapInfoIter&);

    void exeWait();
    void exeRevival();
    void exeCountToZero();
    void initTextureAtNumLeftStar();
    void setNumLeftStar();
    void selectNrvWait();
    void setupStateWait();
    void startDemoStartCountDown();
    void startDemoLastBattle();

    /* 0x08C */ J3DTexMtx _8C;
    /* 0x120 */ J3DTexMtx _120;
    /* 0x1B4 */ bool _1B4;
#ifdef TARGET_PC
    // From 100 stars left on, the plate's own two digits are hidden and three smaller copies of the "ones" digit (second
    // copies of the plate's model) take their place, so that all of it fits in the circle.
    AstroCountDownPlateDigit* mDigits[3];
    void initExtraDigits();
    void updateExtraDigits(s32 starLeft);
#endif
};
