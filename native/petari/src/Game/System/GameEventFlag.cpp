#include "Game/System/GameEventFlag.hpp"
#ifdef TARGET_PC
#include "Game/System/NebulaArchipelago.hpp"
#endif

GameEventFlagAccessor::GameEventFlagAccessor(const GameEventFlag* pFlag) : mFlag(pFlag) {
}

const char* GameEventFlagAccessor::getName() const {
    return mFlag->mName;
}

bool GameEventFlagAccessor::isTypeGalaxyOpenStar() const {
    return mFlag->mType == GameEventFlag::Type_GalaxyOpenStar;
}

bool GameEventFlagAccessor::isTypeSpecialStar() const {
    return mFlag->mType == GameEventFlag::Type_SpecialStar;
}

bool GameEventFlagAccessor::isTypeStarPiece() const {
    return mFlag->mType == GameEventFlag::Type_StarPiece;
}

bool GameEventFlagAccessor::isTypeEventValueIsZero() const {
    return mFlag->mType == GameEventFlag::Type_EventValueIsZero;
}

const char* GameEventFlagAccessor::getGalaxyName() const {
    return mFlag->mGalaxyName;
}

s32 GameEventFlagAccessor::getStarId() const {
    return mFlag->mStarID;
}

s32 GameEventFlagAccessor::getStarPieceIndex() const {
    return mFlag->mStarPieceIndex;
}

const char* GameEventFlagAccessor::getGalaxyNameWithStarPiece() const {
    return mFlag->mGalaxyName;
}

s32 GameEventFlagAccessor::getNeedStarPieceNum() const {
#ifdef TARGET_PC
    s32 archipelagoNum = 0;

    if (NebulaArchipelago::overrideStarPieceNum(mFlag->mName, &archipelagoNum)) {
        return archipelagoNum;
    }
#endif

    return mFlag->mNeedStarPieceNum * 10;
}

const char* GameEventFlagAccessor::getEventValueName() const {
    return mFlag->mEventValueName;
}

const char* GameEventFlagAccessor::getRequirement() const {
    return mFlag->mRequirement;
}
