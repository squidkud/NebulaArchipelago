#include "Game/System/NebulaArchipelago.hpp"
#include "Game/AudioLib/AudSoundNameConverter.hpp"
#include "Game/NPC/TicoGalaxy.hpp"
#include "Game/Player/Mario.hpp"
#include "Game/Player/MarioAccess.hpp"
#include "Game/Player/MarioActor.hpp"
#include "Game/System/GalaxyStatusAccessor.hpp"
#include "Game/System/GameDataFunction.hpp"
#include "Game/System/GameSequenceFunction.hpp"
#include "Game/Util/FileUtil.hpp"
#include "Game/Util/EventUtil.hpp"
#include "Game/Util/JMapInfo.hpp"
#include "Game/Util/ObjUtil.hpp"
#include "Game/Util/PlayerUtil.hpp"
#include "Game/Util/SceneUtil.hpp"
#include "Game/Util/ScreenUtil.hpp"
#include <cstdio>
#include <cstring>
#include <nebula_game_api.h>
#include <revolution/os.h>

namespace {
    // StarData.py's kinds, in the generated table.
    enum {
        ApKind_star,
        ApKind_comet,
        ApKind_secret,
        ApKind_purple,
        ApKind_luigi,
        ApKind_green,
        ApKind_grand,
        ApKind_ending,
        ApKind_festival,
    };

    struct ApGalaxy {
        const char* mName;
        s32 mDome;
        s32 mCap;
    };

    struct ApStar {
        const char* mStage;
        s32 mScenario;
        s32 mKind;
    };

// cApGalaxies and cApStars, written by tools/gen_archipelago_stars.py from the APWorld's StarData.py.
#include "NebulaArchipelagoStars.inc"

    const s32 cGalaxyNum = sizeof(cApGalaxies) / sizeof(cApGalaxies[0]);
    const s32 cStarNum = sizeof(cApStars) / sizeof(cApStars[0]);
    const u32 cNoCap = 0xFFFFFFFFu;

    const s32 cScanFrames = 60;     // frames between two scans of the save's stars
    const s32 cStatusFrames = 30;   // frames between two reads of the host's counts
    const s32 cNoticeFrames = 6;    // frames between two asks for a notice
    const s32 cPollFrames = 30;     // frames between two asks for an item, while none waits
    const s32 cItemFrames = 120;    // frames after an item before the next one
    const s32 cStatusCalls = 2000;  // calls to the game-logic overrides between two reads, where no frame runs (menus)

    const s32 cNoticeMax = 8;
    const s32 cNoticeLength = 80;

    // What the host says about the slot, read now and then (the game's logic asks hundreds of times a second).
    struct Status {
        bool mStarItems;
        bool mStartObservatory;
        bool mExactRequirements;
        bool mLumaCounts;
        bool mStarColors;
        bool mMusicShuffle;
        bool mSkyShuffle;
        bool mTransformItems;  // a transformation is usable only once its item is received
        u32 mForms[7];         // by the power-up's type (MorphItemObjNeo): how many of its item came
        s32 mForcedCharacter;  // -1: both (the file select's button chooses), 0: Mario only, 1: Luigi only
        bool mHideComets;      // the slot leaves the comet stars out: the star select does not show them
        bool mHidePurple;      // the same for the Purple Coin stars
        bool mMovementItems;   // a move is usable only once its item is received
        u32 mSkills[6];        // by NebulaArchipelago::Skill: how many of its item came
        s32 mPower;
        s32 mGreen;
        s32 mGrand;       // the Grand Star flags the game sees: the one given at the start (it opens the first dome), and those received
        s32 mGrandItems;  // the Grand Star items received: these count in the star counter, the one given at the start does not
        bool mDomeShuffle;
        u32 mCaps[64];
        u32 mShuffle[64];  // by galaxy slot: the galaxy that stands in it (cNoCap: itself)
        u32 mDomeOrder[6];  // by dome (room) - 1: the Grand Stars - 1 it opens with (cNoCap: its own)
    };

    Status sStatus;
    s32 sStatusAge = 0x40000000;  // frames or calls since the status was read: a large number reads it at the first ask
    s32 sActive = -1;             // the host's client runs: -1 not asked yet

    s32 sFile;  // the save file the counters below belong to (0: none yet)
    bool sReported[256];
    bool sValid[256];  // the star exists in this game's data
    bool sValidChecked;
    s32 sOwned;  // the stars of this file the last scan found
    s32 sTotalAll;   // how many stars the table has in this game (all kinds)
    s32 sOwnedMain;  // of those, the ones that exist before the ending (not the Purple Coin stars)
    s32 sTotalMain;  // how many such stars the table has in this game
    s32 sScanWait;
    s32 sStatusWait;
    s32 sNoticeWait;
    s32 sItemWait;
    bool sGoalReported;
    s32 sLoggedOwned = -1;
    s32 sLoggedTotal = -1;

    wchar_t sNotices[cNoticeMax][cNoticeLength];
    s32 sNoticeIcons[cNoticeMax];  // the item number the notice is about (the pop-up's icon), 0: none
    s32 sNoticeNum;

    bool isHostActive() {
        if (sActive < 0) {
            sActive = __NebulaArchipelago(NEBULA_AP_ACTIVE, 0) != 0 ? 1 : 0;
        }

        return sActive != 0;
    }

    void writeLog(const char* pFormat, s32 a = 0, s32 b = 0, s32 c = 0) {
        char line[160];
        snprintf(line, sizeof(line), pFormat, a, b, c);
        __NebulaArchipelagoText(NEBULA_AP_TEXT_LOG, line, sizeof(line));
    }

    bool optionIs(u32 option) {
        return __NebulaArchipelago(NEBULA_AP_OPTION_SET, option) != 0 && __NebulaArchipelago(NEBULA_AP_OPTION, option) != 0;
    }

    void readStatus() {
        sStatusAge = 0;

        if (!isHostActive()) {
            memset(&sStatus, 0, sizeof(sStatus));
            return;
        }

        sStatus.mStarItems = optionIs(NEBULA_AP_OPT_STAR_ITEMS);
        sStatus.mStartObservatory = optionIs(NEBULA_AP_OPT_START_OBSERVATORY);
        sStatus.mExactRequirements = sStatus.mStarItems && optionIs(NEBULA_AP_OPT_EXACT_REQUIREMENTS);
        sStatus.mLumaCounts = optionIs(NEBULA_AP_OPT_LUMA_COUNTS);
        sStatus.mStarColors = optionIs(NEBULA_AP_OPT_STAR_COLORS);
        sStatus.mMusicShuffle = optionIs(NEBULA_AP_OPT_MUSIC_SHUFFLE);
        sStatus.mSkyShuffle = optionIs(NEBULA_AP_OPT_SKYBOX_SHUFFLE);
        sStatus.mTransformItems = optionIs(NEBULA_AP_OPT_TRANSFORMATION_ITEMS);

        {
            // 1 Spring, 2 Bee, 3 Boo, 4 Ice, 5 Fire, 6 Flying: the item each power-up asks.
            static const u32 cFormItems[7] = {0,
                                              NEBULA_AP_ITEM_SPRING_MUSHROOM,
                                              NEBULA_AP_ITEM_BEE_MUSHROOM,
                                              NEBULA_AP_ITEM_BOO_MUSHROOM,
                                              NEBULA_AP_ITEM_ICE_FLOWER,
                                              NEBULA_AP_ITEM_FIRE_FLOWER,
                                              NEBULA_AP_ITEM_FLYING_STAR};

            for (s32 i = 0; i < 7; i++) {
                sStatus.mForms[i] = (i > 0 && sStatus.mTransformItems) ? __NebulaArchipelago(NEBULA_AP_ITEM_COUNT, cFormItems[i]) : 0;
            }
        }
        sStatus.mMovementItems = optionIs(NEBULA_AP_OPT_MOVEMENT_ITEMS);
        sStatus.mForcedCharacter = -1;

        if (__NebulaArchipelago(NEBULA_AP_OPTION_SET, NEBULA_AP_OPT_CHARACTER) != 0) {
            const u32 character = __NebulaArchipelago(NEBULA_AP_OPTION, NEBULA_AP_OPT_CHARACTER);

            if (character <= 1) {
                sStatus.mForcedCharacter = static_cast< s32 >(character);
            }
        }

        // Only when the server said so (an older slot does not give these two): then the stars stay.
        sStatus.mHideComets = __NebulaArchipelago(NEBULA_AP_OPTION_SET, NEBULA_AP_OPT_INCLUDE_COMETS) != 0 &&
                              __NebulaArchipelago(NEBULA_AP_OPTION, NEBULA_AP_OPT_INCLUDE_COMETS) == 0;
        sStatus.mHidePurple = __NebulaArchipelago(NEBULA_AP_OPTION_SET, NEBULA_AP_OPT_INCLUDE_PURPLE) != 0 &&
                              __NebulaArchipelago(NEBULA_AP_OPTION, NEBULA_AP_OPT_INCLUDE_PURPLE) == 0;

        for (s32 i = 0; i < 6; i++) {
            sStatus.mSkills[i] = sStatus.mMovementItems ? __NebulaArchipelago(NEBULA_AP_ITEM_COUNT, NEBULA_AP_ITEM_SPIN + i) : 0;
        }

        sStatus.mPower = static_cast< s32 >(__NebulaArchipelago(NEBULA_AP_ITEM_COUNT, NEBULA_AP_ITEM_POWER_STAR));
        sStatus.mGreen = static_cast< s32 >(__NebulaArchipelago(NEBULA_AP_ITEM_COUNT, NEBULA_AP_ITEM_GREEN_STAR));
        sStatus.mGrandItems = static_cast< s32 >(__NebulaArchipelago(NEBULA_AP_ITEM_COUNT, NEBULA_AP_ITEM_GRAND_STAR));
        sStatus.mGrand = sStatus.mGrandItems + (sStatus.mStartObservatory ? 1 : 0);

        sStatus.mDomeShuffle = optionIs(NEBULA_AP_OPT_DOME_SHUFFLE);

        for (s32 i = 0; i < cGalaxyNum && i < 64; i++) {
            sStatus.mCaps[i] = sStatus.mStarItems ? __NebulaArchipelago(NEBULA_AP_GALAXY_CAP, i) : cNoCap;
            sStatus.mShuffle[i] = __NebulaArchipelago(NEBULA_AP_GALAXY_SHUFFLE, i);
        }

        for (s32 i = 0; i < 6; i++) {
            sStatus.mDomeOrder[i] = __NebulaArchipelago(NEBULA_AP_DOME_ORDER, i);
        }
    }

    // The status, read again after `cStatusCalls` asks (the frames read it too).
    const Status& status() {
        if (sStatusAge >= cStatusCalls) {
            readStatus();
        }

        sStatusAge++;
        return sStatus;
    }

    // A notice from the host, as the pop-up shows it (ASCII).
    void drainNotices() {
        char text[cNoticeLength];

        while (sNoticeNum < cNoticeMax && __NebulaArchipelagoText(NEBULA_AP_TEXT_NOTICE, text, sizeof(text)) > 0) {
            const s32 slot = sNoticeNum++;
            wchar_t* pDst = sNotices[slot];
            const char* pText = text;
            sNoticeIcons[slot] = 0;

            // A received item's notice starts with the byte 1 and the item number as '0' + number.
            if (text[0] == 1 && text[1] != 0) {
                sNoticeIcons[slot] = text[1] - '0';
                pText = text + 2;
            }

            s32 i = 0;

            for (; pText[i] != 0 && i < cNoticeLength - 1; i++) {
                pDst[i] = static_cast< wchar_t >(static_cast< u8 >(pText[i]));
            }

            pDst[i] = 0;
        }
    }

    void checkStars() {
        sValidChecked = true;

        for (s32 i = 0; i < cStarNum && i < 256; i++) {
            const ApStar& rStar = cApStars[i];
            sValid[i] = rStar.mKind != ApKind_ending && MR::makeGalaxyStatusAccessor(rStar.mStage).getPowerStarNum() >= rStar.mScenario;

            if (!sValid[i] && rStar.mKind != ApKind_ending) {
                writeLog("a star of the table is not in this game's data: number %d", i + 1);
            }
        }
    }

    // Luigi's save is a save of its own (his stars are other locations, kLuigiOffset further on).
    bool isLuigiSave() {
        return !GameDataFunction::isDataMario();
    }

    const u32 cLuigiOffset = 300;

    // Reports every star the save holds (the host ignores one it already has).
    void scanStars() {
        if (!sValidChecked) {
            checkStars();
        }

        s32 owned = 0;
        s32 ownedMain = 0;
        s32 totalMain = 0;
        s32 totalAll = 0;
        const u32 locationOffset = isLuigiSave() ? cLuigiOffset : 0;

        for (s32 i = 0; i < cStarNum && i < 256; i++) {
            const bool mainStar = sValid[i] && cApStars[i].mKind != ApKind_purple && cApStars[i].mKind != ApKind_festival;

            if (mainStar) {
                totalMain++;
            }

            if (sValid[i]) {
                totalAll++;
            }

            if (!sValid[i] || !GameDataFunction::hasPowerStar(cApStars[i].mStage, cApStars[i].mScenario)) {
                continue;
            }

            owned++;

            if (mainStar) {
                ownedMain++;
            }

            if (!sReported[i]) {
                sReported[i] = true;
                __NebulaArchipelago(NEBULA_AP_LOCATION, i + 1 + locationOffset);
            }
        }

        sOwned = owned;
        sOwnedMain = ownedMain;
        sTotalMain = totalMain;
        sTotalAll = totalAll;

        if (owned != sLoggedOwned) {
            sLoggedOwned = owned;
            writeLog("save file %d: %d stars reported as locations", sFile, owned);
        }
    }

    // The story gives the spin (and the Star Pointer) with the Gateway's Luma, which a save started in the Observatory
    // never meets: it is there from the start.
    void grantSpin() {
        if (!GameDataFunction::isPassedStoryEvent("\203\130\203\163\203\223\214\240\227\230" /* スピン権利 */)) {
            MR::onGameEventFlagEnableToSpinAndStarPointer();
            writeLog("spin granted");
        }
    }

    void grantStory() {
        if (MR::isStageFileSelect()) {
            return;
        }

        grantSpin();
        NebulaArchipelago::skipButlerTalks();
    }

    void checkGoal() {
        // The goal counts only once the server is connected, which is when the host knows what the slot asks for.
        if (sGoalReported || __NebulaArchipelago(NEBULA_AP_STATE, 0) != 2) {
            return;
        }

        bool reached;

        const u32 goalOption = __NebulaArchipelago(NEBULA_AP_GOAL_OPTION, 0);

        if (goalOption == NEBULA_AP_GOAL_POWER_STARS) {
            const u32 target = __NebulaArchipelago(NEBULA_AP_OPTION, NEBULA_AP_OPT_STARS_TO_FINISH);
            reached = sOwned >= static_cast< s32 >(target != 0 ? target : 120);
        } else {
            // The star of Bowser's Galaxy Reactor (SpecialStarGrand7): the normal ending. Whatever Grand Stars the
            // player holds, only this star ends the game.
            // The save only records that star after the epilogue and the credits; the goal is sent as soon as the
            // player takes it (the result sequence knows the star from then on).
            reached = GameDataFunction::hasPowerStar("KoopaBattleVs3Galaxy", 1) ||
                      GameSequenceFunction::isPowerStarAtResultSequence("KoopaBattleVs3Galaxy", 1);

            if (goalOption == NEBULA_AP_GOAL_ALL_STARS && (sTotalAll == 0 || sOwned < sTotalAll)) {
                // Every star there is, the Purple Coin stars too.
                reached = false;
            }

            if (goalOption == NEBULA_AP_GOAL_ENDING_AND_STARS && (sTotalMain == 0 || sOwnedMain < sTotalMain)) {
                // The ending only counts together with every star that exists before it (the Purple Coin stars come after the
                // ending); the last scan counted them in the save.
                reached = false;
            }
        }

        if (reached) {
            sGoalReported = true;
            __NebulaArchipelago(NEBULA_AP_GOAL, isLuigiSave() ? 1 : 0);
            writeLog("goal reached");
        }
    }

    const s32 cTrapRetryFrames = 20;  // frames before a trap that could not start (Mario hurt, in a demo...) tries again
    const s32 cDeathSuppressFrames = 1800;

    // The music shuffle: the themes ("MBGM_..." in the game's sound names) are shuffled among those of the same kind (a
    // single track, or a layered one; their IDs differ in these bits), so a theme keeps the way the game plays it. Jingles
    // ("BGM_...": the star fanfare, the game over) and streamed songs (STM_...) stay as they are: the game waits for them.
    const s32 cMusicMax = 256;
    const u32 cMusicKindMask = 0x03010000u;

    u32 sMusicSeed;
    s32 sMusicNum;
    u32 sMusicFrom[cMusicMax];
    u32 sMusicTo[cMusicMax];

    void buildMusicMap(u32 seed) {
        sMusicNum = 0;
        const AudSoundNameConverter* pConverter = AudSingletonHolder< AudSoundNameConverter >::get();

        if (pConverter == nullptr) {
            return;  // the sound table is not loaded yet: tried again at the next theme
        }

        sMusicSeed = seed;

        for (s32 i = 0; i < pConverter->mNumItems && sMusicNum < cMusicMax; i++) {
            const AudSoundNameData& rData = pConverter->mSoundNameData[i];

            if (rData.mName == nullptr || strncmp(rData.mName, "MBGM_", 5) != 0) {
                continue;
            }

            sMusicFrom[sMusicNum] = static_cast< u32 >(rData.mID);
            sMusicTo[sMusicNum] = static_cast< u32 >(rData.mID);
            sMusicNum++;
        }

        // A shuffle inside each kind: a seeded Fisher-Yates over the themes of that kind.
        u32 state = seed * 2654435761u + 12345u;

        for (s32 i = 0; i < sMusicNum; i++) {
            const u32 kind = sMusicFrom[i] & cMusicKindMask;
            s32 same[cMusicMax];
            s32 count = 0;

            for (s32 j = 0; j < sMusicNum; j++) {
                if ((sMusicFrom[j] & cMusicKindMask) == kind) {
                    same[count++] = j;
                }
            }

            if (same[0] != i) {
                continue;  // handled with the first of its kind
            }

            for (s32 j = count - 1; j > 0; j--) {
                state ^= state << 13;
                state ^= state >> 17;
                state ^= state << 5;
                const s32 k = static_cast< s32 >(state % static_cast< u32 >(j + 1));
                const u32 swap = sMusicTo[same[j]];
                sMusicTo[same[j]] = sMusicTo[same[k]];
                sMusicTo[same[k]] = swap;
            }
        }

        writeLog("music shuffle: %d themes shuffled", sMusicNum);
    }

    const s32 cQuakeFrames = 600;      // the camera trap lasts ten seconds
    const s32 cConfusionFrames = 1200; // the control trap twenty

    s32 sQuakeFrames;
    s32 sConfusionFrames;
    NameObj* sQuakeObj;
    s32 sDeathSuppress;  // frames left in which the next life lost is the DeathLink's own (it is not sent back)

    // A trap: false when Mario cannot take it now (the item stays and is tried again).
    bool applyTrap(u32 item) {
        MarioActor* pActor = MarioAccess::getPlayerActor();

        if (pActor == nullptr || pActor->mHealth == 0 || MR::isPlayerDead()) {
            return false;
        }

        Mario* pMario = pActor->getMario();

        switch (item) {
        case NEBULA_AP_ITEM_FIRE_TRAP:
            return pMario->doFireDanceWithInitialDamage(1);
        case NEBULA_AP_ITEM_SHOCK_TRAP:
            return pMario->doParalyze();
        case NEBULA_AP_ITEM_FREEZE_TRAP:
            return pMario->doFreeze();
        case NEBULA_AP_ITEM_SPIKE_TRAP:
            return pMario->doNeedleWithInitialDamage(1);
        case NEBULA_AP_ITEM_STAR_BIT_THIEF: {
            const s32 lost = MR::getStarPieceNum() < 50 ? MR::getStarPieceNum() : 50;

            if (lost > 0) {
                MR::addStarPiece(-lost);
            }

            return true;
        }
        case NEBULA_AP_ITEM_QUAKE_TRAP:
            if (sQuakeFrames <= 0) {
                sQuakeObj = pActor;
                MR::shakeCameraInfinity(pActor, 0.6f, 4.0f);
            }

            sQuakeFrames = cQuakeFrames;
            return true;
        case NEBULA_AP_ITEM_CONFUSION_TRAP:
            sConfusionFrames = cConfusionFrames;
            return true;
        default:
            return true;
        }
    }

    void applyItem(s32 file) {
        const u32 item = __NebulaArchipelago(NEBULA_AP_NEXT_ITEM, file);

        if (item == 0) {
            sItemWait = cPollFrames;
            return;
        }

        if (item == NEBULA_AP_ITEM_ONE_UP) {
            MR::incPlayerLeft();
            MR::requestOneUp();
            writeLog("received a 1-Up Mushroom");
        } else if (item == NEBULA_AP_ITEM_ONE_UP_BURST) {
            for (s32 i = 0; i < 3; i++) {
                MR::incPlayerLeft();
            }

            MR::requestOneUp();
            writeLog("received three 1-Up Mushrooms");
        } else if (item == NEBULA_AP_ITEM_STAR_BITS_200) {
            MR::addStarPiece(200);
            writeLog("received 200 Star Bits");
        } else if (item == NEBULA_AP_ITEM_FULL_HEAL) {
            MarioActor* pActor = MarioAccess::getPlayerActor();

            if (pActor == nullptr || pActor->mHealth == 0 || MR::isPlayerDead()) {
                sItemWait = cTrapRetryFrames;
                return;
            }

            // A Full Heal at full health is spent for nothing: waiting for a hit would hold every item behind it.
            if (pActor->mHealth < pActor->mMaxHealth) {
                if (!pActor->isEnableNerveChange()) {
                    sItemWait = cTrapRetryFrames;
                    return;
                }

                pActor->incLife(pActor->mMaxHealth);
            }

            writeLog("received a Full Heal");
        } else if (item == NEBULA_AP_ITEM_STAR_BITS_50) {
            MR::addStarPiece(50);
            writeLog("received 50 Star Bits");
        } else if (item == NEBULA_AP_ITEM_RAINBOW_STAR) {
            MarioActor* pActor = MarioAccess::getPlayerActor();

            if (pActor == nullptr || pActor->mHealth == 0 || MR::isPlayerDead()) {
                sItemWait = cTrapRetryFrames;
                return;
            }

            if (pActor->_9C8 == nullptr) {
                // Rainbow Mario's model is not there in this stage: asking for it would crash the game; try again later.
                static s32 sTold = 0;

                if (sTold < 3) {
                    sTold++;
                    NebulaArchipelago::logNote("rainbow star waits", MR::isLoadedObjectOrLayoutArchive("InvincibleMario") ? "InvincibleMario loaded" : "InvincibleMario not loaded",
                            MR::isLoadedObjectOrLayoutArchive("InvincibleLuigi") ? "InvincibleLuigi loaded" : "InvincibleLuigi not loaded");
                }

                sItemWait = cTrapRetryFrames;
                return;
            }

            MarioAccess::changeItemStatus(7);
            writeLog("received a Rainbow Star");
        } else if ((item >= NEBULA_AP_ITEM_FIRE_TRAP && item <= NEBULA_AP_ITEM_SPIKE_TRAP) || (item >= NEBULA_AP_ITEM_STAR_BIT_THIEF && item <= NEBULA_AP_ITEM_CONFUSION_TRAP)) {
            if (!applyTrap(item)) {
                sItemWait = cTrapRetryFrames;
                return;
            }

            writeLog("received a trap (%d)", static_cast< s32 >(item));
        } else {
            writeLog("received an item this version does not know (%d)", static_cast< s32 >(item));
        }

        __NebulaArchipelago(NEBULA_AP_ITEM_APPLIED, file);
        sItemWait = cItemFrames;
    }

    bool isDigit(char c, char last) {
        return c >= '1' && c <= last;
    }
};  // namespace

namespace NebulaArchipelago {
    bool isActive() {
        return isHostActive();
    }

    void update(bool inPlay) {
        if (!isHostActive()) {
            return;
        }

        // A Hungry Luma whose dome has just opened appears (it was left out when the scene loaded with the dome shut).
        TicoGalaxy::reviveWaiting();

        if (sConfusionFrames > 0) {
            sConfusionFrames--;
        }

        if (sQuakeFrames > 0) {
            sQuakeFrames--;

            if (sQuakeFrames == 0) {
                MR::stopShakingCamera(sQuakeObj);
                sQuakeObj = nullptr;
            }
        }

        // Mario's save of a slot keeps the slot's own number; Luigi's has another (each save has its own items to receive).
        const s32 file = GameDataFunction::getUserFileIndex() + (isLuigiSave() ? 10 : 0);

        if (file != sFile) {
            // Another save file: its stars are reported again (the host ignores a location it already has).
            sFile = file;
            sGoalReported = false;
            memset(sReported, 0, sizeof(sReported));
            sOwned = 0;
            sOwnedMain = 0;
            sLoggedOwned = -1;
            sScanWait = 0;
            sItemWait = cItemFrames;
        }

        if (sStatusWait > 0) {
            sStatusWait--;
        } else {
            sStatusWait = cStatusFrames;
            readStatus();

            const s32 total = sStatus.mPower + sStatus.mGreen + sStatus.mGrandItems;

            if (total != sLoggedTotal) {
                sLoggedTotal = total;
                writeLog("items: %d Power Stars, %d Green Stars, %d Grand Stars: the star counter shows the sum", sStatus.mPower, sStatus.mGreen, sStatus.mGrandItems);
            }
        }

        if (sNoticeWait > 0) {
            sNoticeWait--;
        } else {
            sNoticeWait = cNoticeFrames;
            drainNotices();
        }

        if (sScanWait > 0) {
            sScanWait--;
        } else {
            sScanWait = cScanFrames;
            scanStars();
            checkGoal();
            grantStory();
        }

        if (!inPlay) {
            return;
        }

        if (sDeathSuppress > 0) {
            sDeathSuppress--;
        }

        // DeathLink: another player died (the host says so only when the option is on).
        if (__NebulaArchipelago(NEBULA_AP_DEATH_TAKE, 0) != 0) {
            MarioActor* pActor = MarioAccess::getPlayerActor();

            if (pActor != nullptr && pActor->mHealth != 0 && !MR::isPlayerDead()) {
                sDeathSuppress = cDeathSuppressFrames;
                writeLog("DeathLink: another player died");
                MarioAccess::forceKill(3, 0);
            }
        }

        if (sItemWait > 0) {
            sItemWait--;
        } else {
            applyItem(file);
        }
    }

    // The skies: each kind (a plain sky, a sky with a projection map, the three skies inside the domes) is shuffled among its own,
    // as a single cycle (every sky is replaced by another), by the multiworld's seed. The summer sky is in too: its inner space
    // (Sky::init) follows the sky object, not the model. `isDome`: the sky inside a dome (AstroDomeSky), which has its own three.
    const char* getShuffledSkyOf(const char* pName, bool isDome) {
        if (pName == nullptr || !isHostActive() || !status().mSkyShuffle) {
            return pName;
        }

        const u32 seed = __NebulaArchipelago(NEBULA_AP_SEED, 0);

        if (seed == 0) {
            return pName;
        }

        static const char* const cPlain[] = {"GalaxySky",     "MilkyWaySky",   "PhantomSky",   "DesertSky",    "AuroraSky",
                                             "StarrySky",     "FamicomMarioSky", "KoopaVS2Sky", "ChildRoomSky", "AstroDomeSkyA", "VRDarkSpace", "SummerSky"};
        static const char* const cProjection[] = {"HalfGalaxySky", "GreenPlanetOrbitSky", "CloudSky", "RockPlanetOrbitSky", "KoopaVS1Sky", "CometNearOrbitSky", "VROrbit", "VRSandwichSun"};
        static const char* const cDome[] = {"AstroDomeSkyA", "AstroDomeSkyB", "AstroDomeSkyC"};
        const char* const* pPool = nullptr;
        s32 num = 0;
        u32 salt = 0;

        if (isDome) {
            for (s32 i = 0; i < 3 && pPool == nullptr; i++) {
                if (strcmp(cDome[i], pName) == 0) {
                    pPool = cDome;
                    num = 3;
                    salt = 0xD3u;
                }
            }
        } else {
            for (s32 i = 0; i < 12 && pPool == nullptr; i++) {
                if (strcmp(cPlain[i], pName) == 0) {
                    pPool = cPlain;
                    num = 12;
                    salt = 0x51u;
                }
            }

            for (s32 i = 0; i < 8 && pPool == nullptr; i++) {
                if (strcmp(cProjection[i], pName) == 0) {
                    pPool = cProjection;
                    num = 8;
                    salt = 0xA7u;
                }
            }
        }

        if (pPool == nullptr) {
            return pName;
        }

        // Sattolo's shuffle: one cycle through the pool, so that no sky stays.
        s32 order[16];

        for (s32 i = 0; i < num; i++) {
            order[i] = i;
        }

        u32 state = (seed ^ (salt * 2246822519u)) * 2654435761u + 12345u;

        for (s32 i = num - 1; i > 0; i--) {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            const s32 j = static_cast< s32 >(state % static_cast< u32 >(i));
            const s32 swap = order[i];
            order[i] = order[j];
            order[j] = swap;
        }

        for (s32 i = 0; i < num; i++) {
            if (strcmp(pPool[i], pName) == 0) {
                const char* pResult = pPool[order[i]];
                static u32 sLogged[3];
                const s32 poolIndex = salt == 0x51u ? 0 : (salt == 0xA7u ? 1 : 2);

                if ((sLogged[poolIndex] & (1u << i)) == 0) {
                    sLogged[poolIndex] |= 1u << i;
                    logNote("sky shuffle", pName, pResult);
                }

                return pResult;
            }
        }

        return pName;
    }

    const char* getShuffledSky(const char* pName) {
        return getShuffledSkyOf(pName, false);
    }

    const char* getShuffledDomeSky(const char* pName) {
        return getShuffledSkyOf(pName, true);
    }

    u32 getShuffledMusic(u32 soundID) {
        if (!isHostActive() || !status().mMusicShuffle) {
            return soundID;
        }

        const u32 seed = __NebulaArchipelago(NEBULA_AP_SEED, 0);

        if (seed == 0) {
            return soundID;  // the server has not spoken yet: nothing to shuffle by
        }

        if (sMusicSeed != seed) {
            buildMusicMap(seed);
        }

        for (s32 i = 0; i < sMusicNum; i++) {
            if (sMusicFrom[i] == soundID) {
                return sMusicTo[i];
            }
        }

        return soundID;
    }

    void distortStick(f32* pStickX, f32* pStickY) {
        if (sConfusionFrames <= 0) {
            return;
        }

        if (pStickX != nullptr) {
            *pStickX = -*pStickX;
        }

        if (pStickY != nullptr) {
            *pStickY = -*pStickY;
        }
    }

    void onLifeLost() {
        if (!isHostActive()) {
            return;
        }

        if (sDeathSuppress > 0) {
            sDeathSuppress = 0;  // this death came from a DeathLink: not sent back
            return;
        }

        __NebulaArchipelago(NEBULA_AP_DEATH_REPORT, 0);
    }

    bool overrideEventFlag(const char* pFlagName, bool* pResult) {
        if (!isHostActive() || pFlagName == nullptr || strncmp(pFlagName, "SpecialStar", 11) != 0) {
            return false;
        }

        // SpecialStarGrand1 to 6 and SpecialStarGreen1 to 3: nothing longer ("SpecialStarGreen1/2", "...All").
        const bool isGrand = strncmp(pFlagName + 11, "Grand", 5) == 0 && isDigit(pFlagName[16], '6') && pFlagName[17] == 0;
        const bool isGreen = strncmp(pFlagName + 11, "Green", 5) == 0 && isDigit(pFlagName[16], '3') && pFlagName[17] == 0;

        if (!isGrand && !isGreen) {
            return false;
        }

        const Status& rStatus = status();
        const s32 number = pFlagName[16] - '0';

        if (isGrand) {
            if (rStatus.mStarItems) {
                *pResult = number <= rStatus.mGrand;
                return true;
            }

            if (rStatus.mStartObservatory && number == 1) {
                *pResult = true;
                return true;
            }

            return false;
        }

        if (rStatus.mStarItems) {
            *pResult = number <= rStatus.mGreen;
            return true;
        }

        return false;
    }

    bool overridePowerStarNum(s32* pNum) {
        if (!isHostActive()) {
            return false;
        }

        const Status& rStatus = status();

        if (!rStatus.mStarItems) {
            return false;
        }

        *pNum = rStatus.mPower + rStatus.mGreen + rStatus.mGrandItems;
        return true;
    }

    bool ignoreGalaxyOpenCondition(const char* pGalaxyName, const char* pFlagName) {
        if (!isHostActive() || pGalaxyName == nullptr) {
            return false;
        }

        // Bowser's Galaxy Reactor opens with the star count alone (the game also asks for the 6th Grand Star).
        if (strcmp(pGalaxyName, "KoopaBattleVs3Galaxy") == 0) {
            return true;
        }

        // A galaxy of a dome opens with its star count once the dome's door is open (the rest of what the game asks, the
        // Grand Star of its own dome for one, is the door's: the room can be behind another door).
        for (s32 i = 0; i < cGalaxyNum; i++) {
            if (strcmp(cApGalaxies[i].mName, pGalaxyName) != 0) {
                continue;
            }

            if (cApGalaxies[i].mDome < 1) {
                return false;
            }

            return true;
        }

        return false;
    }

    u32 limitPowerStarNumToOpenGalaxy(const char* pGalaxyName, u32 powerStarNum) {
        if (!isHostActive() || pGalaxyName == nullptr) {
            return powerStarNum;
        }

        const Status& rStatus = status();

        if (!rStatus.mStarItems) {
            return powerStarNum;
        }

        for (s32 i = 0; i < cGalaxyNum && i < 64; i++) {
            if (strcmp(cApGalaxies[i].mName, pGalaxyName) == 0) {
                const u32 cap = rStatus.mCaps[i];

                if (cap == cNoCap) {
                    return powerStarNum;
                }

                // With the slot's own requirements the number is the slot's, higher or lower than the game's.
                return rStatus.mExactRequirements || cap < powerStarNum ? cap : powerStarNum;
            }
        }

        return powerStarNum;
    }

    bool overrideStarPieceNum(const char* pFlagName, s32* pNum) {
        if (!isHostActive() || pFlagName == nullptr || strncmp(pFlagName, "StarPiece", 9) != 0) {
            return false;
        }

        const Status& rStatus = status();

        if (!rStatus.mLumaCounts) {
            return false;
        }

        // The order of the APWorld's "luma_counts": the dome Lumas, then the Gateway Galaxy's.
        static const char* const sLumaGalaxies[] = {
            "BeltConveyerExGalaxy", "CocoonExGalaxy", "TearDropGalaxy", "FishTunnelGalaxy",
            "TransformationExGalaxy", "SnowCapsuleGalaxy", "TeresaMario2DGalaxy",
        };

        for (u32 i = 0; i < sizeof(sLumaGalaxies) / sizeof(sLumaGalaxies[0]); i++) {
            if (strcmp(pFlagName + 9, sLumaGalaxies[i]) == 0) {
                const u32 count = __NebulaArchipelago(NEBULA_AP_LUMA_COUNT, i);

                if (count == cNoCap) {
                    return false;
                }

                *pNum = static_cast< s32 >(count);
                return true;
            }
        }

        return false;
    }

    s32 getReturnDome(const char* pContentName, s32 vanillaDome) {
        if (!isHostActive() || pContentName == nullptr) {
            return vanillaDome;
        }

        const Status& rStatus = status();
        s32 content = -1;

        for (s32 i = 0; i < cGalaxyNum && i < 64; i++) {
            if (strcmp(cApGalaxies[i].mName, pContentName) == 0) {
                content = i;
                break;
            }
        }

        if (content < 0) {
            return vanillaDome;
        }

        // The slot where that galaxy stands is the miniature the player chose: its dome is where the player returns.
        for (s32 i = 0; i < cGalaxyNum && i < 64; i++) {
            const u32 shown = rStatus.mShuffle[i];
            const s32 here = shown == cNoCap || shown >= static_cast< u32 >(cGalaxyNum) ? i : static_cast< s32 >(shown);

            if (here == content) {
                return cApGalaxies[i].mDome;
            }
        }

        return vanillaDome;
    }

    bool isDomeGalaxyAppearing(const char* pGalaxyName) {
        if (!isHostActive() || pGalaxyName == nullptr) {
            return false;
        }

        for (s32 i = 0; i < cGalaxyNum; i++) {
            if (strcmp(cApGalaxies[i].mName, pGalaxyName) == 0) {
                return cApGalaxies[i].mDome >= 1;
            }
        }

        return false;
    }

    void logNote(const char* pStep, const char* pA, const char* pB) {
        if (!isHostActive()) {
            return;
        }

        char line[160];
        snprintf(line, sizeof(line), "mark: %s %s %s", pStep != nullptr ? pStep : "", pA != nullptr ? pA : "", pB != nullptr ? pB : "");
        __NebulaArchipelagoText(NEBULA_AP_TEXT_LOG, line, sizeof(line));
    }

    bool overrideLumaAppear(const char* pGalaxyName, bool* pResult) {
        if (!isHostActive() || pGalaxyName == nullptr || pResult == nullptr) {
            return false;
        }

        const Status& rStatus = status();

        // The Lumas of the domes 1 to 6, in the order of the APWorld's "luma_counts"; the Gateway's Luma comes with dome 6.
        static const char* const sLumaGalaxies[] = {
            "BeltConveyerExGalaxy", "CocoonExGalaxy",       "TearDropGalaxy",        "FishTunnelGalaxy",
            "TransformationExGalaxy", "SnowCapsuleGalaxy",  "TeresaMario2DGalaxy",
        };

        for (s32 i = 0; i < 7; i++) {
            if (strcmp(pGalaxyName, sLumaGalaxies[i]) == 0) {
                const s32 dome = i < 6 ? i + 1 : 6;
                const s32 door = getDomeOpenNumber(dome);

                if (rStatus.mStarItems) {
                    *pResult = door <= rStatus.mGrand;
                } else {
                    // The game's own Grand Stars (the first one is given at the start when the Observatory starts open).
                    char flag[24];
                    snprintf(flag, sizeof(flag), "SpecialStarGrand%d", door);
                    *pResult = (door == 1 && rStatus.mStartObservatory) || GameDataFunction::isOnGameEventFlag(flag);
                }

                return true;
            }
        }

        return false;
    }

    bool isBossGalaxy(const char* pGalaxyName) {
        static const char* const cBosses[] = {"TriLegLv1Galaxy",    "KoopaBattleVs1Galaxy", "KoopaBattleVs2Galaxy",
                                              "KoopaBattleVs3Galaxy", "KoopaJrShipLv1Galaxy", "FloaterOtaKingGalaxy"};

        if (pGalaxyName == nullptr) {
            return false;
        }

        for (const char* pBoss : cBosses) {
            if (strcmp(pGalaxyName, pBoss) == 0) {
                return true;
            }
        }

        return false;
    }

    bool hasMiniatureModel(const char* pGalaxyName) {
        if (pGalaxyName == nullptr) {
            return true;
        }

        // The galaxies of the domes have a "Mini<Galaxy>" model. Bowser's three Reactors have none: the game never loads
        // an archive of that name for them (a Bowser place uses "MiniKoopaGalaxy"), and asking for one stops the game
        // ("File isn't exist: MiniKoopaBattleVs1Galaxy.arc").
        if (strncmp(pGalaxyName, "KoopaBattleVs", strlen("KoopaBattleVs")) == 0) {
            return false;
        }

        for (s32 i = 0; i < cGalaxyNum; i++) {
            if (strcmp(cApGalaxies[i].mName, pGalaxyName) == 0) {
                return cApGalaxies[i].mDome >= 1;
            }
        }

        return true;
    }

    const char* getShuffledGalaxy(const char* pSlotName) {
        if (!isHostActive() || pSlotName == nullptr) {
            return pSlotName;
        }

        const Status& rStatus = status();

        for (s32 i = 0; i < cGalaxyNum && i < 64; i++) {
            if (strcmp(cApGalaxies[i].mName, pSlotName) != 0) {
                continue;
            }

            const u32 content = rStatus.mShuffle[i];

            if (content == cNoCap || content >= static_cast< u32 >(cGalaxyNum) || content == static_cast< u32 >(i)) {
                return pSlotName;
            }

            static bool sLogged[64];

            if (!sLogged[i]) {
                sLogged[i] = true;
                writeLog("galaxy shuffle: slot %d shows galaxy %d (StarData order)", i, static_cast< s32 >(content));
            }

            return cApGalaxies[content].mName;
        }

        return pSlotName;
    }

    s32 getDomeOfDoor(s32 door) {
        for (s32 room = 1; room <= 6; room++) {
            if (getDomeOpenNumber(room) == door) {
                return room;
            }
        }

        return door;
    }

    s32 getDomeOpenNumber(s32 domeId) {
        if (!isHostActive() || domeId < 1 || domeId > 6) {
            return domeId;
        }

        const Status& rStatus = status();

        if (!rStatus.mDomeShuffle) {
            return domeId;
        }

        const u32 order = rStatus.mDomeOrder[domeId - 1];

        if (order == cNoCap || order > 5) {
            return domeId;
        }

        static bool sLogged[7];

        if (!sLogged[domeId]) {
            sLogged[domeId] = true;
            writeLog("dome shuffle: dome %d opens with Grand Star %d", domeId, static_cast< s32 >(order) + 1);
        }

        return static_cast< s32 >(order) + 1;
    }

    void skipButlerTalks() {
        if (!isHostActive()) {
            return;
        }

        // The Butler's lectures (dome, moving between galaxies, Star Bits) and his report of the Observatory's first
        // dome: the talks of the black Luma that the player gets every time a new place opens.
        static const char* const cTalks[] = {
            "\223\126\213\205\213\126\203\214\203\116\203\140\203\203\201\133" /* 天球儀レクチャー */,
            "\203\115\203\203\203\211\203\116\203\126\201\133\210\332\223\256\203\214\203\116\203\140\203\203\201\133" /* ギャラクシー移動レクチャー */,
            "\203\130\203\136\201\133\203\163\201\133\203\130\203\214\203\116\203\140\203\203\201\133" /* スターピースレクチャー */,
            "\203\157\203\147\203\211\201\133\217\356\225\361\202\140" /* バトラー情報Ａ */,
        };

        for (s32 i = 0; i < 4; i++) {
            if (!GameDataFunction::isPassedStoryEvent(cTalks[i])) {
                GameDataFunction::followStoryEventByName(cTalks[i]);
            }
        }
    }

    bool isStartAtObservatory() {
        return isHostActive() && optionIs(NEBULA_AP_OPT_START_OBSERVATORY);
    }

    bool isLuigiUnlocked() {
        // Luigi can be chosen on any save file of an Archipelago game (there is no option for it).
        return isHostActive();
    }

    const wchar_t* getWaitingNotice() {
        if (!isHostActive()) {
            return nullptr;
        }

        if (sNoticeNum == 0) {
            drainNotices();
        }

        return sNoticeNum > 0 ? sNotices[0] : nullptr;
    }

    void popNotice() {
        if (sNoticeNum == 0) {
            return;
        }

        for (s32 i = 1; i < sNoticeNum; i++) {
            memcpy(sNotices[i - 1], sNotices[i], sizeof(sNotices[i]));
            sNoticeIcons[i - 1] = sNoticeIcons[i];
        }

        sNoticeNum--;
    }

    s32 getWaitingNoticeIcon() {
        return sNoticeNum > 0 ? sNoticeIcons[0] : 0;
    }

    bool isMoreNotices() {
        return sNoticeNum > 1;
    }
};  // namespace NebulaArchipelago

// The colour of a star tells the class of the item it holds: green for a normal item, blue for useful, gold for progression and
// red for a trap (the hues the star already has: PowerStar.btp's frames 0 to 3).
bool NebulaArchipelago::areStarColorsOn() {
    return isHostActive() && status().mStarColors;
}

bool NebulaArchipelago::getStarColorFrame(s32 scenario, s32* pFrame) {
    if (!isHostActive() || !status().mStarColors) {
        return false;
    }

    const char* pStage = MR::getCurrentStageName();

    if (pStage == nullptr) {
        return false;
    }

    for (s32 i = 0; i < cStarNum; i++) {
        const ApStar& rStar = cApStars[i];

        if (rStar.mScenario != scenario || rStar.mKind == ApKind_grand || rStar.mKind == ApKind_ending || strcmp(rStar.mStage, pStage) != 0) {
            continue;
        }

        switch (__NebulaArchipelago(NEBULA_AP_STAR_CLASS, i + 1 + (isLuigiSave() ? cLuigiOffset : 0))) {
        case NEBULA_AP_CLASS_FILLER:
            *pFrame = 2;
            return true;
        case NEBULA_AP_CLASS_USEFUL:
            *pFrame = 1;
            return true;
        case NEBULA_AP_CLASS_PROGRESSION:
            *pFrame = 0;
            return true;
        case NEBULA_AP_CLASS_TRAP:
            *pFrame = 3;
            return true;
        default:
            return false;  // not known yet: the star's own colour
        }
    }

    return false;
}

// ---- The green sphere in the domes ----

namespace NebulaArchipelago {
    bool useGreenSphere(const char* pContentName) {
        if (pContentName == nullptr || !isHostActive() || hasMiniatureModel(pContentName)) {
            return false;
        }

        if (strncmp(pContentName, "KoopaBattleVs", strlen("KoopaBattleVs")) == 0) {
            return false;
        }

        return __NebulaArchipelago(NEBULA_AP_SPHERE_GUARD, 0) != 0;
    }

    static bool sSphereGuard = false;

    void beginSphereGuard() {
        if (!sSphereGuard) {
            sSphereGuard = true;
            __NebulaArchipelago(NEBULA_AP_SPHERE_GUARD, 1);
            writeLog("green spheres: a dome with a sphere is being made");
        }
    }

    void endSphereGuard() {
        if (sSphereGuard) {
            sSphereGuard = false;
            __NebulaArchipelago(NEBULA_AP_SPHERE_GUARD, 2);
        }
    }
};  // namespace NebulaArchipelago

// ---- Mario's and Luigi's colours ----

namespace NebulaArchipelago {
    bool getPlayerColor(s32 part, u8* pRed, u8* pGreen, u8* pBlue) {
        if (part < 0 || part >= cPlayerColorParts || !isHostActive()) {
            return false;
        }

        // The slot's options color_0 to color_9 (the APWorld's mario_cap_color ... luigi_shoes_color): 0x01000000 | 0xRRGGBB,
        // or 0 when the player left the part alone.
        const u32 option = NEBULA_AP_OPT_COLOR_FIRST + static_cast< u32 >(part);

        if (__NebulaArchipelago(NEBULA_AP_OPTION_SET, option) == 0) {
            return false;
        }

        const u32 value = __NebulaArchipelago(NEBULA_AP_OPTION, option);

        if ((value & 0x01000000) == 0) {
            return false;
        }

        *pRed = static_cast< u8 >(value >> 16);
        *pGreen = static_cast< u8 >(value >> 8);
        *pBlue = static_cast< u8 >(value);
        return true;
    }
};  // namespace NebulaArchipelago

// ---- The shop ----

namespace NebulaArchipelago {
    bool hasShop() {
        return isHostActive() && __NebulaArchipelago(NEBULA_AP_SHOP_PRICE, 0) != cNoCap;
    }

    s32 getShopNum() {
        if (!hasShop()) {
            return 0;
        }

        s32 num = 0;

        while (num < 20 && __NebulaArchipelago(NEBULA_AP_SHOP_PRICE, num) != cNoCap) {
            num++;
        }

        return num;
    }

    s32 getShopPrice(s32 index) {
        const u32 price = isHostActive() ? __NebulaArchipelago(NEBULA_AP_SHOP_PRICE, index) : cNoCap;
        return price == cNoCap ? 0 : static_cast< s32 >(price);
    }

    bool isShopBought(s32 index) {
        return isHostActive() && __NebulaArchipelago(NEBULA_AP_SHOP_BOUGHT, index) != 0;
    }

    bool getShopLabel(s32 index, wchar_t* pDst, s32 capacity) {
        if (pDst == nullptr || capacity < 2) {
            return false;
        }

        pDst[0] = 0;

        if (!isHostActive()) {
            return false;
        }

        char text[160];
        snprintf(text, sizeof(text), "%d", index);

        if (__NebulaArchipelagoText(NEBULA_AP_TEXT_SHOP_LABEL, text, sizeof(text)) == 0) {
            return false;
        }

        // UTF-8 to the game's wide characters (what does not fit in 16 bits is a question mark).
        s32 length = 0;

        for (const unsigned char* q = reinterpret_cast< const unsigned char* >(text); *q != 0 && length < capacity - 1;) {
            u32 code = *q;
            s32 extra = 0;

            if (code >= 0xF0) {
                code &= 0x07, extra = 3;
            } else if (code >= 0xE0) {
                code &= 0x0F, extra = 2;
            } else if (code >= 0xC0) {
                code &= 0x1F, extra = 1;
            }

            q++;

            for (s32 k = 0; k < extra && *q != 0; k++, q++) {
                code = (code << 6) | (*q & 0x3F);
            }

            pDst[length++] = code > 0xFFFF ? L'?' : static_cast< wchar_t >(code);
        }

        pDst[length] = 0;
        return length > 0;
    }

    s32 getShopIcon(s32 index) {
        return isHostActive() ? static_cast< s32 >(__NebulaArchipelago(NEBULA_AP_SHOP_ICON, index)) : 0;
    }

    void requestShopHints() {
        if (isHostActive()) {
            __NebulaArchipelago(NEBULA_AP_SHOP_HINT, 0);
        }
    }

    static bool sShopOpen = false;

    void setShopOpen(bool open) {
        sShopOpen = open;
    }

    bool isShopOpen() {
        return sShopOpen;
    }

    bool isShopOnline() {
        return isHostActive() && __NebulaArchipelago(NEBULA_AP_STATE, 0) == 2;
    }

    bool buyShopItem(s32 index) {
        if (!hasShop() || index < 0 || index >= getShopNum() || !isShopOnline() || isShopBought(index)) {
            return false;
        }

        const s32 price = getShopPrice(index);

        if (MR::getStarPieceNum() < price) {
            return false;
        }

        MR::addStarPiece(-price);
        __NebulaArchipelago(NEBULA_AP_SHOP_BUY, index);
        writeLog("shop: bought item %d for %d Star Bits", index + 1, price);
        return true;
    }
};  // namespace NebulaArchipelago

namespace NebulaArchipelago {
    const char* getShuffledEnemy(const char* pName, const JMapInfoIter& rIter) {
        // Two families, each replaced among itself so that the new enemy suits the place:
        //  - the enemies that stand on the ground where they are put (a Goomba, a Bob-omb, an Octoomba, an Octoguy, a Piranha Plant,
        //    a Monty Mole, a Dry Bones, a hopping spider); a Fire Boo, which floats where it is, may stand for one too;
        //  - the ones that hang or float in the air (the Swoopins, the Fire Boos and their ice kinds), among themselves.
        // None of them needs a path, a switch or an argument from its placement (checked below).
        static const char* const sGround[] = {"Kuribo", "BombHei", "Takobo", "TakoHei", "PackunPetit", "Mogu", "Karon", "JumpSpider", "Meramera"};
        const s32 groundSources = 8;  // the first eight (a Fire Boo is a target only: it is in the air family as a source)
        const s32 groundNum = sizeof(sGround) / sizeof(sGround[0]);
        static const char* const sAir[] = {"BasaBasa", "BasaBasaIce", "Meramera", "IceMeramera"};
        const s32 airNum = sizeof(sAir) / sizeof(sAir[0]);

        if (pName == nullptr || !isHostActive() || !rIter.isValid()) {
            return pName;
        }

        const char* const* pPool = nullptr;
        s32 poolNum = 0;
        s32 own = -1;

        for (s32 i = 0; i < groundSources; i++) {
            if (strcmp(pName, sGround[i]) == 0) {
                pPool = sGround;
                poolNum = groundNum;
                own = i;
            }
        }

        if (pPool == nullptr) {
            for (s32 i = 0; i < airNum; i++) {
                if (strcmp(pName, sAir[i]) == 0) {
                    pPool = sAir;
                    poolNum = airNum;
                    own = i;
                }
            }
        }

        if (pPool == nullptr || !optionIs(NEBULA_AP_OPT_ENEMY_SHUFFLE)) {
            return pName;
        }

        // Anything the stage's own design hangs on this enemy (a path, a group such as "defeat them all", a switch, a
        // particular argument) keeps it as it is.
        static const char* const sFields[] = {"Obj_arg0", "Obj_arg1", "Obj_arg2", "Obj_arg3", "Obj_arg4", "Obj_arg5", "Obj_arg6", "Obj_arg7",
                                              "SW_A",     "SW_B",     "SW_APPEAR", "SW_DEAD", "SW_AWAKE", "CommonPath_ID", "GroupId",
                                              "DemoGroupId"};

        for (const char* pField : sFields) {
            s32 value = -1;

            if (rIter.getValue< s32 >(pField, &value) && value != -1) {
                return pName;
            }
        }

        f32 x = 0.0f, y = 0.0f, z = 0.0f;
        rIter.getValue< f32 >("pos_x", &x);
        rIter.getValue< f32 >("pos_y", &y);
        rIter.getValue< f32 >("pos_z", &z);

        u32 hash = __NebulaArchipelago(NEBULA_AP_SEED, 0) * 2654435761u + 12345u;
        const f32 coords[3] = {x, y, z};

        for (s32 i = 0; i < 3; i++) {
            u32 bits;
            memcpy(&bits, &coords[i], sizeof(bits));
            hash = (hash ^ bits) * 16777619u;
            hash ^= hash >> 15;
        }

        // Another kind than its own, evenly.
        const s32 pick = static_cast< s32 >(hash % static_cast< u32 >(poolNum - 1));
        const char* pResult = pPool[pick >= own ? pick + 1 : pick];

        static s32 sLogged = 0;

        if (sLogged < 400) {
            sLogged++;
            char what[64];
            char where[64];
            snprintf(what, sizeof(what), "%s -> %s", pName, pResult);
            snprintf(where, sizeof(where), "at %d %d %d", static_cast< int >(x), static_cast< int >(y), static_cast< int >(z));
            logNote("enemy shuffle", what, where);
        }

        return pResult;
    }

    static s32 sSaveIndicatorFrames = 0;

    void beginSaveIndicator() {
        sSaveIndicatorFrames = 180;  // three seconds
    }

    bool isSaveIndicatorRunning() {
        return sSaveIndicatorFrames > 0;
    }

    bool tickSaveIndicator() {
        if (sSaveIndicatorFrames > 0) {
            sSaveIndicatorFrames--;
        }

        return sSaveIndicatorFrames > 0;
    }

    s32 getForcedCharacter() {
        if (!isHostActive()) {
            return -1;
        }

        return status().mForcedCharacter;
    }

    bool isScenarioHidden(const char* pStageName, s32 scenarioNo) {
        if (pStageName == nullptr || !isHostActive()) {
            return false;
        }

        const Status& rStatus = status();

        if (!rStatus.mHideComets && !rStatus.mHidePurple) {
            return false;
        }

        for (s32 i = 0; i < cStarNum && i < 256; i++) {
            const ApStar& rStar = cApStars[i];

            if (rStar.mScenario == scenarioNo && strcmp(rStar.mStage, pStageName) == 0) {
                return (rStatus.mHideComets && rStar.mKind == ApKind_comet) || (rStatus.mHidePurple && rStar.mKind == ApKind_purple);
            }
        }

        return false;
    }

    bool isSkillLocked(s32 skill) {
        if (skill < 0 || skill > 5 || !isHostActive()) {
            return false;
        }

        const Status& rStatus = status();

        return rStatus.mMovementItems && rStatus.mSkills[skill] == 0;
    }

    bool isTransformationLocked(s32 morphType) {
        if (morphType < 1 || morphType > 6 || !isHostActive()) {
            return false;
        }

        const Status& rStatus = status();

        return rStatus.mTransformItems && rStatus.mForms[morphType] == 0;
    }
};  // namespace NebulaArchipelago
