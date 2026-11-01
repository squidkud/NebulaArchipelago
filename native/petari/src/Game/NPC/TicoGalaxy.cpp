#include "Game/NPC/TicoGalaxy.hpp"
#include "Game/MapObj/StarPieceDirector.hpp"
#include "Game/NPC/TicoStarRing.hpp"
#include "Game/System/GameEventFlagTable.hpp"
#include "Game/Util/EventUtil.hpp"
#include "Game/Util/JMapUtil.hpp"
#include "Game/Util/LiveActorUtil.hpp"
#include "Game/Util/MessageUtil.hpp"
#include "Game/Util/ObjUtil.hpp"
#include "Game/Util/ScreenUtil.hpp"

namespace {
    static const char* sInfoMessageID = "InformationGalaxy";
};  // namespace

TicoGalaxy::TicoGalaxy(const char* pName) : TicoFat(pName), mGalaxyName() {
}

#ifdef TARGET_PC
namespace {
    const s32 cMaxTicos = 16;
    TicoGalaxy* sTicos[cMaxTicos];
    bool sWaiting[cMaxTicos];
};  // namespace

TicoGalaxy::~TicoGalaxy() {
    for (s32 i = 0; i < cMaxTicos; i++) {
        if (sTicos[i] == this) {
            sTicos[i] = nullptr;
            sWaiting[i] = false;
        }
    }
}

void TicoGalaxy::forgetAll() {
    for (s32 i = 0; i < cMaxTicos; i++) {
        sTicos[i] = nullptr;
        sWaiting[i] = false;
    }
}

void TicoGalaxy::reviveWaiting() {
    for (s32 i = 0; i < cMaxTicos; i++) {
        TicoGalaxy* pTico = sTicos[i];

        if (pTico != nullptr && sWaiting[i] && pTico->_1E0 != 0 && pTico->enableAppear()) {
            sWaiting[i] = false;
            pTico->appear();
        }
    }
}
#endif

void TicoGalaxy::init(const JMapInfoIter& rIter) {
    TicoFat::init(rIter);

    _1F0 = 2;

    if (MR::isOnTicoGalaxyAlreadyTalk(_1EC)) {
        MR::forwardNode(mMsgCtrl);
        MR::forwardNode(mMsgCtrl);
    }

    mStarRing = new TicoStarRing("\203\130\203\136\201\133\203\212\203\223\203\117\203\140\203\122" /* スターリングチコ */);
    mStarRing->initWithoutIter();

#ifdef TARGET_PC
    // The Luma that the shut dome keeps away (TicoFat::init made it dead) waits for the dome to open.
    for (s32 i = 0; i < cMaxTicos; i++) {
        if (sTicos[i] == nullptr) {
            sTicos[i] = this;
            sWaiting[i] = MR::isDead(this);
            break;
        }
    }
#endif
}

void TicoGalaxy::initAfterPlacement() {
    mStarRing->initialize(this, MR::getPairedGroupMember(this), mGalaxyName);
    TicoFat::initAfterPlacement();
}

void TicoGalaxy::shootStarPiece() {
    s32 v5 = 5;

    if (_1E0 <= 5) {
        v5 = _1E0;
    }

    if (MR::giftStarPieceToTarget(getSensor("Mouth"), v5)) {
        mCurrentFed += v5;
        MR::tryRumblePadVeryWeak(this, WPAD_CHAN0);
    }
}

void TicoGalaxy::startEat() {
    TicoFat::startEat();
    MR::setTicoGalaxyAlreadyTalk(_1EC, true);
}

bool TicoGalaxy::enableAppear() {
    return MR::isAppearGalaxy(mGalaxyName);
}

void TicoGalaxy::initStarPieceSaveData(const JMapInfoIter& rIter) {
    MR::getJMapInfoArg7WithInit(rIter, &_1EC);
    mGalaxyName = GameEventFlagTable::getExclamationGalaxyNameFromIndex(_1EC);
    _1E4 = GameEventFlagTable::getStarPieceNumToOpenExclamationGalaxy(mGalaxyName);
    s32 starPieceNum = MR::getStarPieceNumGivingToTicoGalaxy(_1EC);
    _1E0 = (_1E4 - starPieceNum <= 0) ? 0 : _1E4 - starPieceNum;
}

void TicoGalaxy::addStarPieceSaveData(s32 num) {
    MR::addStarPieceGivingToTicoGalaxy(_1EC, num);
}

void TicoGalaxy::disappear(bool a1) {
    if (a1) {
        MR::onGameEventFlagGalaxyOpen(mGalaxyName);
        MR::callAppearAllGroupMember(this);
    } else {
        mStarRing->appear();
    }
}

void TicoGalaxy::appearInformation() const {
    MR::appearInformationMessage(MR::getGameMessageDirect(::sInfoMessageID), true);
    MR::setInformationMessageReplaceString(MR::getGalaxyNameShortOnCurrentLanguage(mGalaxyName), 0);
}
