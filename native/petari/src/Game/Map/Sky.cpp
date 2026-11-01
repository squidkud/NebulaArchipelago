#include "Game/Map/Sky.hpp"
#include "Game/LiveActor/MaterialCtrl.hpp"
#include "Game/LiveActor/MirrorReflectionModel.hpp"
#include "Game/LiveActor/Nerve.hpp"
#include "Game/Map/SpaceInner.hpp"
#include "Game/Util/ActorSwitchUtil.hpp"
#include "Game/Util/CameraUtil.hpp"
#include "Game/Util/DemoUtil.hpp"
#include "Game/Util/JMapUtil.hpp"
#include "Game/Util/LiveActorUtil.hpp"
#include "Game/Util/ObjUtil.hpp"
#include "Game/Util/StringUtil.hpp"
#ifdef TARGET_PC
#include "Game/System/NebulaArchipelago.hpp"
#include "Game/LiveActor/ModelManager.hpp"
#include <JSystem/J3DGraphAnimator/J3DModelData.hpp>
#include <JSystem/J3DGraphBase/J3DNativeTexture.hpp>
#include <JSystem/J3DGraphBase/J3DTexture.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>
#include <stdio.h>
#endif

namespace {
    const char* cChangeAnimName = "Change";
};  // namespace

namespace NrvSky {
    NEW_NERVE(HostTypeWait, Sky, Wait);
    NEW_NERVE(HostTypeChange, Sky, Change);
};  // namespace NrvSky

Sky::Sky(const char* pName) : LiveActor(pName), mSpaceInner(), mReflectionModel() {
}

void Sky::init(const JMapInfoIter& rIter) {
    MR::initDefaultPos(this, rIter);

    const char* objName = nullptr;
    MR::getObjectName(&objName, rIter);

    const char* modelName = objName;
#ifdef TARGET_PC
    modelName = NebulaArchipelago::getShuffledSky(objName);
    NebulaArchipelago::logNote("Sky object", objName, modelName);
#endif

    initModel(modelName);
#ifdef TARGET_PC
    if (modelName != objName && mModelManager != nullptr) {
        // A sky swapped in by skybox_shuffle: its pictures are handed to the renderer again (an image already handed over
        // unchanged is skipped) and listed in the log, to find why the sky can come out as a plain blue screen.
        J3DModelData* pData = mModelManager->getJ3DModelData();
        J3DTexture* pTexture = pData != nullptr ? pData->getTexture() : nullptr;
        JUTNameTab* pNames = pData != nullptr ? pData->getTextureName() : nullptr;

        if (pTexture != nullptr) {
            const u16 num = pTexture->getNum();

            for (u16 i = 0; i < num; i++) {
                ResTIMG* pImage = pTexture->getResTIMG(i);
                char note[96];
                snprintf(note, sizeof(note), "%dx%d format %d levels %d palette %d", static_cast< int >(pImage->mWidth),
                         static_cast< int >(pImage->mHeight), static_cast< int >(pImage->mFormat), static_cast< int >(pImage->mImageNum),
                         static_cast< int >(pImage->mPaletteNum));
                NebulaArchipelago::logNote("Sky picture", pNames != nullptr ? pNames->getName(i) : "(no name)", note);

                if (pImage->mImageDataOffset != 0) {
                    J3DNativeTexture::registerImage(reinterpret_cast< const u8* >(pImage) + static_cast< s32 >(pImage->mImageDataOffset),
                                                    pImage->mFormat, pImage->mWidth, pImage->mHeight,
                                                    pImage->mMipmap ? pImage->mImageNum : 1);
                }

                if (pImage->mPaletteNum != 0 && pImage->mPaletteDataOffset != 0) {
                    J3DNativeTexture::registerPalette(reinterpret_cast< const u8* >(pImage) + static_cast< s32 >(pImage->mPaletteDataOffset),
                                                      pImage->mPaletteNum);
                }
            }
        }
    }
#endif
    MR::connectToSceneSky(this);
    MR::useStageSwitchReadA(this, rIter);
    MR::useStageSwitchReadB(this, rIter);
    MR::useStageSwitchReadAppear(this, rIter);

    if (MR::isEqualString(objName, "SummerSky")) {
        mSpaceInner = new SpaceInner("\223\340\221\244\211\106\222\210" /* 内側宇宙 */);
        mSpaceInner->initWithoutIter();

        if (MR::isValidSwitchB(this)) {
            MR::listenStageSwitchOnOffB(this, MR::Functor(this, &Sky::appearSpaceInner), MR::Functor(this, &Sky::disappearSpaceInner));
        }
    }

    s32 arg0 = -1;
    MR::getJMapInfoArg0NoInit(rIter, &arg0);

    if (arg0 == 0) {
        mReflectionModel = new MirrorReflectionModel(this, "\213\276\223\340\203\202\203\146\203\213" /* 鏡内モデル */, modelName, getBaseMtx());
        mReflectionModel->initWithoutIter();
    }

    MR::tryStartAllAnim(this, modelName);

    if (mReflectionModel) {
        MR::tryStartAllAnim(mReflectionModel, modelName);
    }

    MR::invalidateClipping(this);
    MR::registerDemoSimpleCastAll(this);
    initNerve(GET_NERVE(Sky, HostTypeWait));

    if (MR::isValidSwitchAppear(this)) {
        MR::syncStageSwitchAppear(this);
        makeActorDead();
    } else {
        makeActorAppeared();
    }
}

void Sky::calcAnim() {
    mPosition.set(MR::getCamPos());
    LiveActor::calcAnim();
}

void Sky::initModel(const char* pModelName) {
    initModelManagerWithAnm(pModelName, nullptr, false);
}

void Sky::control() {
    if (mSpaceInner != nullptr && MR::isValidSwitchB(this)) {
        if (MR::isDead(mSpaceInner)) {
            MR::showModelIfHidden(this);
        } else if (mSpaceInner->isAppeared()) {
            MR::hideModelIfShown(this);
        } else {
            MR::showModelIfHidden(this);
        }
    }
}

void Sky::appearSpaceInner() {
    if (mSpaceInner != nullptr && MR::isValidSwitchB(this)) {
        mSpaceInner->appear();
    }
}

void Sky::disappearSpaceInner() {
    if (mSpaceInner != nullptr && MR::isValidSwitchB(this)) {
        mSpaceInner->disappear();
    }
}

void Sky::exeWait() {
    if (MR::isValidSwitchA(this) && MR::isOnSwitchA(this)) {
        setNerve(GET_NERVE(Sky, HostTypeChange));
    }
}

void Sky::exeChange() {
    if (MR::isFirstStep(this)) {
#ifdef TARGET_PC
        MR::tryStartAllAnim(this, ::cChangeAnimName);  // a sky swapped in by skybox_shuffle may have no such animation
#else
        MR::startAllAnim(this, ::cChangeAnimName);
#endif
    }
}

ProjectionMapSky::ProjectionMapSky(const char* pSkyName) : Sky(pSkyName), mProjmapEffectMtxSetter() {
}

void ProjectionMapSky::calcAndSetBaseMtx() {
    LiveActor::calcAndSetBaseMtx();

    if (mProjmapEffectMtxSetter != nullptr) {
        mProjmapEffectMtxSetter->updateMtxUseBaseMtx();
    }
}

void ProjectionMapSky::initModel(const char* pName) {
    initModelManagerWithAnm(pName, 0, true);

    mProjmapEffectMtxSetter = MR::initDLMakerProjmapEffectMtxSetter(this);
    MR::newDifferedDLBuffer(this);
    mProjmapEffectMtxSetter->updateMtxUseBaseMtx();
}
