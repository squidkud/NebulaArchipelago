#pragma once

#include "Game/LiveActor/LiveActor.hpp"
#include <JSystem/JGeometry/TMatrix.hpp>

class ProjmapEffectMtxSetter;

class FileSelectSky : public LiveActor {
public:
    /// @brief Creates a new `FileSelectSky`.
    /// @param pName A pointer to the null-terminated name of the object.
    FileSelectSky(const char* pName);

    /// @brief Destroys the `FileSelectSky`.
    virtual ~FileSelectSky();

    virtual void init(const JMapInfoIter& rIter);
    virtual void control();
    virtual void calcAnim();
    virtual void calcAndSetBaseMtx();
    virtual bool receiveOtherMsg(u32 msg, HitSensor* pSender, HitSensor* pReceiver);

    void exeWait();
#ifdef TARGET_PC
    // skybox_shuffle: the model becomes the shuffled sky as soon as the connection tells which.
    void swapSky();
#endif

private:
    /* 0x8C */ f32 mAngleX;
    /* 0x90 */ f32 mAngleY;
    /* 0x94 */ TPos3f mBaseMtx;
    /* 0xC4 */ ProjmapEffectMtxSetter* mProjmapEffectMtxSetter;
#ifdef TARGET_PC
    s32 mSkyWait;
#endif
};
