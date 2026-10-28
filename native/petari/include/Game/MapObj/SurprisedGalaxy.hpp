#pragma once

#include "Game/LiveActor/LiveActor.hpp"

class SurprisedGalaxy : public LiveActor {
public:
    SurprisedGalaxy(const char*);

    virtual void init(const JMapInfoIter&);
    virtual void appear();
    virtual void kill();
    virtual void attackSensor(HitSensor* pSender, HitSensor* pReceiver);
    virtual bool receiveOtherMsg(u32 msg, HitSensor* pSender, HitSensor* pReceiver);

    void exeExit();
    void exeWait();

    const char* mGalaxyName;
    // Archipelago's galaxy shuffle: mGalaxyName is the place (it keeps what opens it), this is the galaxy that opens.
    const char* mContentGalaxyName;
};
