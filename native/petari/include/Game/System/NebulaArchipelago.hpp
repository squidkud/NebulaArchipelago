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

    // The enemy_shuffle option: the name of the enemy that stands in place of the stage object `pName` (placed as `rIter`
    // says), or `pName` itself. Only plain enemies are swapped - no path, no group, no switch, no argument - and the
    // choice follows the multiworld's seed and the object's place, so it is the same every time the stage loads.
    const char* getShuffledEnemy(const char* pName, const JMapInfoIter& rIter);

    // The automatic save's small "Saving..." (NebulaAchievementToast, bottom-left): the save starts the three seconds; the
    // indicator shows while they run and ticks them (false once they ran out); they go on across a scene change.
    void beginSaveIndicator();
    bool isSaveIndicatorRunning();
    bool tickSaveIndicator();

    // The transformations option: a power-up (MorphItemObjNeo's type: 1 Spring, 2 Bee, 3 Boo, 4 Ice, 5 Fire, 6 Flying) whose
    // item was not received does nothing when taken. Rainbow Mario is never locked.
    bool isTransformationLocked(s32 morphType);

    // What the host wants shown: the oldest notice (ASCII), NebulaAchievementToast's pop-up shows it.
    const wchar_t* getWaitingNotice();
    //   the number of the item the waiting notice is about when it is one received (1 to 19: the pop-up shows its icon), else 0
    s32 getWaitingNoticeIcon();
    void popNotice();
    bool isMoreNotices();
};  // namespace NebulaArchipelago
