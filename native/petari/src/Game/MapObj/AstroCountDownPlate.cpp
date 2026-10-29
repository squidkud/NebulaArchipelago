#include "Game/MapObj/AstroCountDownPlate.hpp"
#include "Game/LiveActor/MaterialCtrl.hpp"
#include "Game/LiveActor/Nerve.hpp"
#include "Game/MapObj/AstroMapObjFunction.hpp"
#include "Game/Util/DemoUtil.hpp"
#include "Game/Util/EffectUtil.hpp"
#include "Game/Util/EventUtil.hpp"
#include "Game/Util/LiveActorUtil.hpp"
#include "Game/Util/MathUtil.hpp"
#include "Game/Util/ModelUtil.hpp"
#include "Game/Util/ObjUtil.hpp"
#include "Game/Util/SoundUtil.hpp"
#include <JSystem/J3DGraphBase/J3DTevs.hpp>
#ifdef TARGET_PC
#include "Game/Util/MtxUtil.hpp"
#include <JSystem/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3DGraphAnimator/J3DModelData.hpp>
#include <JSystem/J3DGraphBase/J3DMaterial.hpp>
#include <JSystem/J3DGraphBase/J3DShape.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>
#endif

namespace NrvAstroCountDownPlate {
    NEW_NERVE(AstroCountDownPlateNrvDead, AstroCountDownPlate, Wait);
    NEW_NERVE(AstroCountDownPlateNrvAlive, AstroCountDownPlate, Wait);
    NEW_NERVE(AstroCountDownPlateNrvRevival, AstroCountDownPlate, Revival);
    NEW_NERVE(AstroCountDownPlateNrvCountToZero, AstroCountDownPlate, CountToZero);
};  // namespace NrvAstroCountDownPlate

namespace {
    const char* cMaterialName01 = "StarNumber01_v_x";
    const char* cMaterialName10 = "StarNumber01_v_x(2)";
    const char* cLastBattleDemoName = "\203\215\203\133\203\142\203\136\215\305\217\111\214\210\220\355\203\146\203\202" /* ロゼッタ最終決戦デモ */;
    const char* cStartCountDownDemoName = "\203\215\203\133\203\142\203\136\203\112\203\105\203\223\203\147\203\137\203\105\203\223\212\112\216\156\203\146\203\202" /* ロゼッタカウントダウン開始デモ */;
    static const s32 sStepForCountToZero = 50;
};  // namespace

#ifdef TARGET_PC
// The plate's third digit: its model with everything hidden but the "ones" digit.
class AstroCountDownPlateDigit : public LiveActor {
public:
    AstroCountDownPlateDigit() : LiveActor("AstroCountDownPlateDigit"), mTexMtx() {
    }

    virtual void init(const JMapInfoIter& rIter) {
        initModelManagerWithAnm("AstroCountDownPlate", nullptr, true);
        J3DModelData* pData = MR::getJ3DModelData(this);
        const s32 num = pData->getMaterialNum();
        const u16 ones = MR::getMaterialNo(pData, ::cMaterialName01);

        for (s32 i = 0; i < num; i++) {
            if (i != ones) {
                MR::hideMaterial(this, MR::getMaterialName(pData, i));
            }
        }

        MR::initDLMakerTexMtx(this, ::cMaterialName01)->setTexMtx(0, &mTexMtx);
        MR::newDifferedDLBuffer(this);
        MR::connectToSceneMapObj(this);
        MR::invalidateClipping(this);
        makeActorAppeared();
        MR::hideModel(this);
    }

    void setDigit(s32 digit) {
        mTexMtx.getTexMtxInfo().mSRT.mTranslationY = 0.1f * digit;
    }

    J3DTexMtx mTexMtx;
};
#endif

AstroCountDownPlate::AstroCountDownPlate(const char* pName) : LiveActor(pName), _1B4() {
#ifdef TARGET_PC
    mDigits[0] = mDigits[1] = mDigits[2] = nullptr;
#endif
}

void AstroCountDownPlate::init(const JMapInfoIter& rIter) {
    MR::initDefaultPos(this, rIter);
    initModelManagerWithAnm("AstroCountDownPlate", nullptr, true);
    initTextureAtNumLeftStar();
    MR::newDifferedDLBuffer(this);
#ifdef TARGET_PC
    initExtraDigits();  // shown or hidden by setupStateWait, once the nerve is set
#endif
    MR::connectToSceneMapObj(this);
    initEffectKeeper(0, nullptr, false);
    initSound(2, false);
    MR::invalidateClipping(this);

    if (MR::tryRegisterDemoCast(this, ::cStartCountDownDemoName, rIter)) {
        MR::tryRegisterDemoActionFunctorDirect(this, MR::Functor(this, &AstroCountDownPlate::startDemoStartCountDown), ::cStartCountDownDemoName,
                                               nullptr);
    }

    if (MR::tryRegisterDemoCast(this, ::cLastBattleDemoName, rIter)) {
        MR::tryRegisterDemoActionFunctorDirect(this, MR::Functor(this, &AstroCountDownPlate::startDemoLastBattle), ::cLastBattleDemoName, nullptr);
    }

    MR::registerDemoSimpleCastAll(this);
    initNerve(GET_NERVE(AstroCountDownPlate, AstroCountDownPlateNrvDead));
    selectNrvWait();
    makeActorAppeared();
}

void AstroCountDownPlate::exeWait() {
    if (MR::isFirstStep(this) && isNerve(GET_NERVE(AstroCountDownPlate, AstroCountDownPlateNrvAlive)) && !MR::isEffectValid(this, "Light")) {
        MR::emitEffect(this, "Light");

        _1B4 = true;
    }

    if (_1B4) {
        MR::startLevelSound(this, "SE_OJ_LV_CDN_PLATE_LIGHT");
    }
}

void AstroCountDownPlate::exeRevival() {
    if (MR::isFirstStep(this)) {
        MR::showMaterial(this, ::cMaterialName01);
        MR::showMaterial(this, ::cMaterialName10);
#ifdef TARGET_PC
        updateExtraDigits(MR::getPowerStarLeftToDisplayCountDownPlate());
#endif
        MR::tryStartAllAnim(this, "Revival");
        MR::emitEffect(this, "Light");

        _1B4 = true;
    }

    if (_1B4) {
        MR::startLevelSound(this, "SE_OJ_LV_CDN_PLATE_LIGHT");
    }

    if (MR::isBrkStopped(this)) {
        setNerve(GET_NERVE(AstroCountDownPlate, AstroCountDownPlateNrvAlive));
    }
}

void AstroCountDownPlate::exeCountToZero() {
    if (MR::isFirstStep(this)) {
        MR::showMaterial(this, ::cMaterialName01);
        MR::showMaterial(this, ::cMaterialName10);
#ifdef TARGET_PC
        updateExtraDigits(MR::getPowerStarLeftToDisplayCountDownPlate());
#endif
        MR::startSound(this, "SE_OJ_CDN_PLATE_ONE_TO_ZERO");
    }

    f32 v2 = MR::calcNerveRate(this, ::sStepForCountToZero);
    f32 v3 = MR::getConvergeVibrationValue(v2, 0.1f, 0.0f, 0.3f, 4.0f);
    _8C.getTexMtxInfo().mSRT.mTranslationY = v3;

    if (_1B4) {
        MR::startLevelSound(this, "SE_OJ_LV_CDN_PLATE_LIGHT");
    }

    if (MR::isStep(this, ::sStepForCountToZero)) {
        setNerve(GET_NERVE(AstroCountDownPlate, AstroCountDownPlateNrvAlive));
    }
}

void AstroCountDownPlate::initTextureAtNumLeftStar() {
    MR::initDLMakerTexMtx(this, ::cMaterialName01)->setTexMtx(0, &_8C);
    MR::initDLMakerTexMtx(this, ::cMaterialName10)->setTexMtx(0, &_120);
    setNumLeftStar();
}

void AstroCountDownPlate::setNumLeftStar() {
    s32 starLeft = MR::getPowerStarLeftToDisplayCountDownPlate();
    f32 mod = starLeft % 10;
#ifdef TARGET_PC
    // The tens digit shows the tens of the last two digits only; the hundreds have their own.
    _120.getTexMtxInfo().mSRT.mTranslationY = 0.1f * ((starLeft / 10) % 10);
#else
    _120.getTexMtxInfo().mSRT.mTranslationY = 0.1f * (starLeft / 10);
#endif
    _8C.getTexMtxInfo().mSRT.mTranslationY = 0.1f * mod;
#ifdef TARGET_PC
    updateExtraDigits(starLeft);
#endif
}

#ifdef TARGET_PC
void AstroCountDownPlate::initExtraDigits() {
    // Where the digits stand: read from the shapes of the plate's two digits, in the plate's own frame (O: ones, T: tens).
    J3DModelData* pData = MR::getJ3DModelData(this);
    const u16 ones = MR::getMaterialNo(pData, ::cMaterialName01);
    const u16 tens = MR::getMaterialNo(pData, ::cMaterialName10);
    J3DShape* pOnesShape = pData->getMaterialNodePointer(ones)->getShape();
    J3DShape* pTensShape = pData->getMaterialNodePointer(tens)->getShape();

    if (pOnesShape == nullptr || pTensShape == nullptr) {
        return;
    }

    Vec o, pitch;
    o.x = (pOnesShape->getMin()->x + pOnesShape->getMax()->x) * 0.5f;
    o.y = (pOnesShape->getMin()->y + pOnesShape->getMax()->y) * 0.5f;
    o.z = (pOnesShape->getMin()->z + pOnesShape->getMax()->z) * 0.5f;
    pitch.x = (pTensShape->getMin()->x + pTensShape->getMax()->x) * 0.5f - o.x;
    pitch.y = (pTensShape->getMin()->y + pTensShape->getMax()->y) * 0.5f - o.y;
    pitch.z = (pTensShape->getMin()->z + pTensShape->getMax()->z) * 0.5f - o.z;

    if (pitch.x * pitch.x + pitch.y * pitch.y + pitch.z * pitch.z < 1.0f) {
        return;  // the two digits are not apart: no place for a third
    }

    // Three digits take the room of two: each is 2/3 as big, the middle one on the middle of the two original digits.
    const f32 s = 2.0f / 3.0f;
    Mtx mtx;
    MR::makeMtxTR(mtx, this);

    for (s32 i = 0; i < 3; i++) {
        const f32 k = 0.5f + (i - 1) * s;  // ones, tens, hundreds: where along the pitch, from the ones' centre
        Vec local;
        // target centre = o + pitch * k ; the copy is scaled round the plate's origin, so move it by target - s * o
        local.x = (o.x + pitch.x * k - s * o.x) * mScale.x;
        local.y = (o.y + pitch.y * k - s * o.y) * mScale.y;
        local.z = (o.z + pitch.z * k - s * o.z) * mScale.z;
        Vec world;
        PSMTXMultVec(mtx, &local, &world);

        AstroCountDownPlateDigit* pDigit = new AstroCountDownPlateDigit();
        pDigit->initWithoutIter();
        pDigit->mPosition.set(world.x, world.y, world.z);
        pDigit->mRotation = mRotation;
        pDigit->mScale.set(mScale.x * s, mScale.y * s, mScale.z * s);
        mDigits[i] = pDigit;
    }
}

void AstroCountDownPlate::updateExtraDigits(s32 starLeft) {
    if (mDigits[0] == nullptr) {
        return;
    }

    const bool alive = !isNerve(GET_NERVE(AstroCountDownPlate, AstroCountDownPlateNrvDead));
    const bool three = alive && starLeft >= 100;

    for (s32 i = 0; i < 3; i++) {
        s32 digit = starLeft;

        for (s32 j = 0; j < i; j++) {
            digit /= 10;
        }

        mDigits[i]->setDigit(digit % 10);

        if (three) {
            MR::showModel(mDigits[i]);
        } else {
            MR::hideModel(mDigits[i]);
        }
    }

    if (three) {
        MR::hideMaterial(this, ::cMaterialName01);
        MR::hideMaterial(this, ::cMaterialName10);
    } else if (alive) {
        MR::showMaterial(this, ::cMaterialName01);
        MR::showMaterial(this, ::cMaterialName10);
    }
}
#endif

void AstroCountDownPlate::selectNrvWait() {
    if (AstroMapObjFunction::isAlreadyRevival("AstroCountDownPlate", -1)) {
        setNerve(GET_NERVE(AstroCountDownPlate, AstroCountDownPlateNrvAlive));
    } else {
        setNerve(GET_NERVE(AstroCountDownPlate, AstroCountDownPlateNrvDead));
    }

    setupStateWait();
}

void AstroCountDownPlate::setupStateWait() {
    if (isNerve(GET_NERVE(AstroCountDownPlate, AstroCountDownPlateNrvDead))) {
        MR::hideMaterial(this, ::cMaterialName01);
        MR::hideMaterial(this, ::cMaterialName10);
#ifdef TARGET_PC
        updateExtraDigits(MR::getPowerStarLeftToDisplayCountDownPlate());
#endif
    } else {
        MR::showMaterial(this, ::cMaterialName01);
        MR::showMaterial(this, ::cMaterialName10);

        if (!MR::isOnGameEventFlagViewNormalEnding()) {
            MR::startBrk(this, "Red");
        } else {
            MR::startBrk(this, "Green");
        }

#ifdef TARGET_PC
        updateExtraDigits(MR::getPowerStarLeftToDisplayCountDownPlate());

        for (s32 i = 0; i < 3; i++) {
            if (mDigits[i] != nullptr) {
                MR::startBrk(mDigits[i], MR::isOnGameEventFlagViewNormalEnding() ? "Green" : "Red");
            }
        }
#endif
    }
}

void AstroCountDownPlate::startDemoStartCountDown() {
    setNerve(GET_NERVE(AstroCountDownPlate, AstroCountDownPlateNrvRevival));
}

void AstroCountDownPlate::startDemoLastBattle() {
    setNerve(GET_NERVE(AstroCountDownPlate, AstroCountDownPlateNrvCountToZero));
}
