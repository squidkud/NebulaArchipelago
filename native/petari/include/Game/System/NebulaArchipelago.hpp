#pragma once

#include <revolution/types.h>

class JMapInfoIter;

// The game's side of the Archipelago multiworld connection (https://archipelago.gg). The host runs the client (it reads
// archipelago.ini, talks to the server and writes archipelago.log; galaxy/archipelago.h); the game does these things:
//   - tells the host which stars the save has (every star is a location: StarData.STARS in the APWorld, generated into
//     NebulaArchipelagoStars.inc) and the goal;
//   - takes the items the server sent, one at a time while the player plays, and applies them (a 1-Up Mushroom, 50 Star
//     Bits);
//   - with the slot's "star items" option, counts the Power Stars, Grand Stars and Green Stars the server sent instead
//     of the ones the save holds (the star counter, the Grand Star flags and the galaxies' star requirements read the
//     items), and with the slot's other options starts a save in the Observatory and lets Luigi be chosen at any time;
//   - shows what the host tells (connected, refused, items sent and received) with the achievement pop-up.
// Without archipelago.ini the host's client is off and every function here returns at once, doing nothing.
namespace NebulaArchipelago {
    // Each frame of a game scene. `inPlay`: the player is playing (no demo, wipe, menu or death), so an item may be
    // handed over now.
    void update(bool inPlay);

    // The client runs.
    bool isActive();

    // The title's A: the connection screen first (the host's settings menu on its Archipelago page, alone), then, once
    // connected, the save files: only one is kept for the multiworld, the others are cleared (after a copy of them in the
    // settings folder). Called each frame the title waits; true when the game may go on (connected and the files ready, or
    // the screen skipped, or no host menu).
    bool passConnectionGate();
    // The file select is the multiworld's (the connection screen was passed connected): it offers one save file, or two when
    // the slot plays both characters. Copy, Icon and Erase are not there.
    bool isSingleSaveFile();
    // How many save files the file select offers: 0 when it is the game's own (six files), 1, or 2 (file 1 is Mario's, file 2
    // is Luigi's, each named "<player> (Mario)" and "<player> (Luigi)").
    s32 getSaveFileCount();
    // Who plays the file (numbered from 1): 0 Mario, 1 Luigi, -1 when the player chooses with the file select's button.
    s32 getFileCharacter(s32 id);
    // The slot's name the client is connected with, as the 16-bit text the file select's name box takes (at most
    // capacity - 1 characters, terminated); false when there is none.
    bool getSlotName(u16* pName, s32 capacity);

    // The star items option: the game's own logic asks these. Each returns false (and leaves its result) when the
    // game's own value stands.
    //   flag: SpecialStarGrand1 to 6 and SpecialStarGreen1 to 3 are on once that many such items came.
    bool overrideEventFlag(const char* pFlagName, bool* pResult);
    //   the star counter: the Power, Green and Grand Star items received.
    bool overridePowerStarNum(s32* pNum);
    //   one of the flags a galaxy asks to open (besides the star count) does not count: the Reactor (the star count alone
    //   opens it) and the galaxies of the domes (the door of their room is what the Grand Stars open).
    bool ignoreGalaxyOpenCondition(const char* pGalaxyName, const char* pFlagName);
    //   the stars a galaxy asks to open: at most what the slot says.
    u32 limitPowerStarNumToOpenGalaxy(const char* pGalaxyName, u32 powerStarNum);

    //   the star bits a Hungry Luma asks (the flag "StarPiece<Galaxy>"): the slot's number, in Star Bits.
    bool overrideStarPieceNum(const char* pFlagName, s32* pNum);

    // The galaxy shuffle: the Observatory's miniature in the slot `pSlotName` (a galaxy's game name) shows, names and
    // launches the galaxy this returns (the slot's own name when nothing is shuffled). The slot keeps the rules that open it.
    const char* getShuffledGalaxy(const char* pSlotName);
    // A galaxy of a dome is always shown in its room (no "???" while a story event or a particular star of the game is
    // missing): its place opens with the star count, as the logic says.
    bool isDomeGalaxyAppearing(const char* pGalaxyName);

    // Whether the galaxy has a miniature of its own ("Mini<Galaxy>": the galaxies of the domes). The galaxies of the Observatory's
    // planets (Sweet Sweet...) have none: standing in a dome's place, one keeps the model of that place and only its name,
    // stars and launch change.
    bool hasMiniatureModel(const char* pGalaxyName);
    // The Hungry Luma of an Observatory galaxy (Sweet Sweet...) is there as soon as its dome is open (the game also asks
    // for a particular star of another galaxy: Space Junk's third star for one). Returns false when the galaxy has no such Luma
    // (or nothing is overridden).
    bool overrideLumaAppear(const char* pGalaxyName, bool* pResult);
    // Whether the galaxy is one of the Bowser / Bowser Jr. / boss galaxies (the Bowser look of a miniature goes with them).
    bool isBossGalaxy(const char* pGalaxyName);
    // One line to archipelago.log (flushed at once): a mark of where the game stands, to find where it stops if it crashes.
    void logNote(const char* pStep, const char* pA, const char* pB);
    // After a galaxy the game returns to the room (1 to 6, 0: the Observatory itself) of the galaxy, `vanillaDome` being
    // its own room. With the galaxy shuffle it is the room of the slot the galaxy stood in (where the player chose it).
    s32 getReturnDome(const char* pContentName, s32 vanillaDome);
    // The dome shuffle: a dome's room (the galaxies in it, 1 to 6) stands in the place of another: the door that opens with
    // the Grand Star number N leads to another room. getDomeOpenNumber: the Grand Stars the room `domeId` needs (the number of
    // its door); getDomeOfDoor: the room behind the door N. Both are the number itself when nothing is shuffled.
    s32 getDomeOpenNumber(s32 domeId);
    s32 getDomeOfDoor(s32 door);

    // The Butler's talks (lectures, a dome discovered) are marked as seen: no black Luma's text box in the domes.
    void skipButlerTalks();

    // The star_colors option: the colour (PowerStar.btp's frame 0 to 3) of the star `scenario` of the current stage tells
    // the class of its item. false (frame untouched) when the option is off or the class is not known yet.
    bool getStarColorFrame(s32 scenario, s32* pFrame);
    bool areStarColorsOn();
    bool isStarReleased(s32 scenario);

    // The skybox_shuffle option also recolours the nebula of the star select screen: a hue shift (degrees, 60 to 300) chosen
    // from the multiworld's seed (the same multiworld, the same colour). false when the option is off or there is no seed.
    bool getStarSelectSkyHueShift(f32* pDegrees);

    // The music shuffle option: the sound ID of the music the game asks for (a stage's or boss's theme) -> the one that plays
    // instead (the same ID when the option is off or the ID is not a theme). The same multiworld shuffles the same way.
    u32 getShuffledMusic(u32 soundID);

    // The Confusion Trap: the control stick Mario reads is inverted while it lasts.
    void distortStick(f32* pStickX, f32* pStickY);

    // DeathLink: a life was lost (MR::decPlayerLeft); the host sends it to the others when the option is on.
    void onLifeLost();

    // A save loaded after the file select goes to the Observatory (the first Grand Star is given).
    bool isStartAtObservatory();
    // The file select offers Luigi on any save file.
    bool isLuigiUnlocked();

    // The Star Bit shop of the Observatory (the Luma at the start of the Observatory opens it in place of the achievements):
    // each shelf is a location of the multiworld, bought with the player's Star Bits.
    //   the slot has a shop (its prices came with the slot's data)
    bool hasShop();
    //   the number of shelves, a shelf's price, whether it is bought
    s32 getShopNum();
    s32 getShopPrice(s32 index);
    bool isShopBought(s32 index);
    //   what a shelf holds, as the board writes it ("Power Star for Bob"); false while the server has not told
    bool getShopLabel(s32 index, wchar_t* pDst, s32 capacity);
    //   the item number of the picture of a shelf (1 1-Up ... 19 Flying: this game's own items, traps included), or 0 for the
    //   Archipelago logo (another player's item, or one with no picture)
    s32 getShopIcon(s32 index);
    //   the shop has opened: its shelves are declared as hints to the players concerned; and the flag that tells it is
    //   open (no pointer cursor over it)
    void requestShopHints();
    void setShopOpen(bool open);
    bool isShopOpen();
    //   the Star Bits the shop can be paid with, and whether the connection is up (nothing is bought without it)
    bool isShopOnline();
    //   buys the shelf: takes the Star Bits and sends the location. false when it cannot be (bought, too dear, offline).
    bool buyShopItem(s32 index);

    // The green sphere: a dome place whose galaxy has no miniature of its own (a Hungry Luma's or a secret one, after the galaxy
    // shuffle) shows the Observatory's green sphere instead of a model that does not exist. false for galaxies with a miniature,
    // for Bowser's Reactors (they keep the place's own model), and when the spheres are off (archipelago.ini `green_spheres=0`,
    // or the last run stopped while they were on screen). The guard marks the time a sphere is on screen: begin when a dome is
    // made, end when the game leaves it.
    bool useGreenSphere(const char* pContentName);
    void beginSphereGuard();
    void endSphereGuard();

    // Mario's and Luigi's colours (the slot's YAML: mario_cap_color, mario_shirt_color, mario_overalls_color, mario_gloves_color,
    // mario_shoes_color, then the same for luigi_): part 0 to 4 Mario's, 5 to 9 Luigi's. false when the part has no colour (the
    // model keeps its own).
    static const s32 cPlayerColorParts = 10;
    bool getPlayerColor(s32 part, u8* pRed, u8* pGreen, u8* pBlue);

    // The enemy_shuffle option: the name of the enemy that stands in place of the stage object `pName` (placed as `rIter`
    // says), or `pName` itself. Only plain enemies are swapped - no path, no group, no switch, no argument - and the
    // choice follows the multiworld's seed and the object's place, so it is the same every time the stage loads.
    const char* getShuffledEnemy(const char* pName, const JMapInfoIter& rIter);

    // The skybox_shuffle option: the sky model that stands in place of the sky object `pName` (a name such as "GalaxySky"), or
    // `pName` itself. The same multiworld gives the same skies.
    const char* getShuffledSky(const char* pName);

    // The same for the sky inside a dome (AstroDomeSkyA, B or C).
    const char* getShuffledDomeSky(const char* pName);
    // The same, for the files to load (they differ from the shown sky only in the test mode 2 of skybox_shuffle).
    const char* getShuffledSkyArchive(const char* pName);
    const char* getShuffledDomeSkyArchive(const char* pName);

    // skybox_shuffle: the model files of the skies (a file name such as "VROrbit.bdl") are loaded without being handed to the
    // renderer (shapes, pictures, curved surfaces): the renderer takes them when a sky is drawn, and the many extra skies a
    // slot loads (each stage loads the one that replaces its own) leave the renderer's tables alone.
    bool isSkyModelFile(const char* pFileName);
    void setHandOverSuppressed(bool suppressed);
    bool isHandOverSuppressed();

    // The automatic save's small "Saving..." (NebulaAchievementToast, bottom-left): the save starts the three seconds; the
    // indicator shows while they run and ticks them (false once they ran out); they go on across a scene change.
    void beginSaveIndicator();
    bool isSaveIndicatorRunning();
    bool tickSaveIndicator();

    // The transformations option: a power-up (MorphItemObjNeo's type: 1 Spring, 2 Bee, 3 Boo, 4 Ice, 5 Fire, 6 Flying) whose
    // item was not received does nothing when taken. Rainbow Mario is never locked.
    bool isTransformationLocked(s32 morphType);

    // The movement option: a move whose item was not received is not there. The skills, by number:
    enum Skill {
        Skill_Spin = 0,        // every spin (MarioActor::isRequestSpin)
        Skill_WallJump = 1,
        Skill_LongJump = 2,    // the jump out of a run while crouching
        Skill_Backflip = 3,
        Skill_SideFlip = 4,    // the jump when turning round
        Skill_TripleJump = 5,  // the third jump of the chain
    };
    bool isSkillLocked(s32 skill);

    // The in-game tracker (the pause menu's Tracker button). A "place" is a miniature of StarData.GALAXIES (the galaxy that
    // stands in it is the shuffle's choice); each place lists the slot's locations of the galaxy standing in it, as the
    // current save sees them, and what the logic asks of each (the APWorld's rules: the star counter, the Grand Stars of
    // the dome, the Observatory galaxies' extra Grand Stars, the Green Stars of the trial galaxies, the forms when they are
    // items, the Star Festival's other stars).
    static const s32 cTrackerStarMax = 8;
    enum TrackerState {
        TrackerState_Done,    // checked
        TrackerState_Doable,  // not checked, and the logic lets the player do it
        TrackerState_Locked,  // not checked, and something is missing (mNeed)
    };
    enum TrackerNeed {
        TrackerNeed_Dome = 1,    // the dome is not open (TrackerGalaxy::mDoor)
        TrackerNeed_Stars = 2,   // the star counter is short of TrackerGalaxy::mNeedStars
        TrackerNeed_Grand = 4,   // the Observatory galaxy asks Grand Stars (TrackerGalaxy::mNeedGrand)
        TrackerNeed_Green = 8,   // the trial galaxy asks the three Green Stars
        TrackerNeed_Form = 16,   // forms are missing (TrackerStar::mForms: the first group not met in the low byte, the second in the next; each the forms of which one is needed: 1 Bee 2 Boo 4 Spring 8 Fire 16 Ice 32 Flying)
        TrackerNeed_Others = 32, // the Star Festival asks every other star first
    };
    struct TrackerStar {
        s32 mScenario;  // the star's number in its galaxy (1-based)
        s32 mKind;      // StarData's kinds in the generated table's order: 0 star, 1 comet, 2 secret, 3 purple, 4 luigi, 5 green, 6 grand, 8 festival
        s32 mState;     // TrackerState
        s32 mNeed;      // TrackerNeed bits (locked only)
        s32 mForms;
    };
    struct TrackerGalaxy {
        const char* mStage;  // the galaxy standing in the place (its game name)
        s32 mPlace;
        s32 mStarNum;
        TrackerStar mStars[cTrackerStarMax];
        s32 mNeedStars;      // what the place asks of the star counter
        s32 mNeedGrand;      // the Observatory galaxy's Grand Stars (0 none)
        s32 mDoor;           // the dome's door number (0 outside the domes)
        bool mGiven;         // the Gateway's Grand Star: given at the start, not a location; the tracker shows it as a done star in front of the others (and does not count it)
        bool mOpen;          // the galaxy can be entered (nothing asked of the galaxy itself is missing) or one of its stars is done: the tracker names it; else it shows ???
    };
    s32 getTrackerPlaceNum();
    //   the place's own room (1 to 6, 0 outside the domes), and the room behind door `door` (1 to 6)
    s32 getTrackerPlaceRoom(s32 place);
    s32 getTrackerDoorRoom(s32 door);
    //   the Grand Stars the player has (the door `door` of a dome is open with that many)
    s32 getTrackerGrandFlags();
    //   false when the galaxy standing in the place has no location in this slot
    bool getTrackerGalaxy(s32 place, TrackerGalaxy* pOut);
    //   how many of an item (NEBULA_AP_ITEM_*) the server has sent in all, and a slot option (NEBULA_AP_OPT_*)
    s32 getReceivedCount(s32 item);
    s32 getSlotOption(s32 option);

    // The star select does not show this star: the slot leaves its kind (the Prankster Comet stars, the Purple Coin stars)
    // out of the multiworld.
    bool isScenarioHidden(const char* pStageName, s32 scenarioNo);

    // Who plays when the slot settles it: 0 Mario only, 1 Luigi only; -1 when both (or no Archipelago game): the file
    // select's button is there then.
    s32 getForcedCharacter();

    // What the host wants shown: the oldest notice (ASCII), NebulaAchievementToast's pop-up shows it.
    const wchar_t* getWaitingNotice();
    //   the number of the item the waiting notice is about when it is one received (1 to 19: the pop-up shows its icon), else 0
    s32 getWaitingNoticeIcon();
    void popNotice();
    bool isMoreNotices();
};  // namespace NebulaArchipelago
