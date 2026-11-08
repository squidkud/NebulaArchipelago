#pragma once

#ifdef TARGET_PC
#include "Game/Screen/LayoutActor.hpp"

class GalaxyMapBackground;
struct NebulaTrackerParts;

// Nebula's Archipelago tracker, opened by the pause menu's Tracker button: the galaxy map's starry backdrop with a panel
// over it. Along the top, a tab for each dome in the order the domes open (shuffled or not; a closed dome cannot be
// opened) and one for everything outside the domes. Under the tab chosen, a line for each galaxy of the dome with a star
// for each of its locations: a gold one when it is done, a faint one when the logic lets the player do it, a black one
// when something is missing. Below: the star counter, the Grand and Green Stars, and the items received so far.
// Built from the game's own windows, font and icons (NebulaPaneKit) and laid out for the screen's shape every frame.
// The pointer and the D-pad both work: -/+ change tabs, B or Back closes it. GameScene runs and draws it while the pause
// menu waits under it.
class NebulaTracker : public LayoutActor {
public:
    NebulaTracker();

    virtual void init(const JMapInfoIter& rIter);
    virtual void control();

    void open();
    bool isOpen() const;
    // GameScene: one frame, and the drawing at the end of its 2D.
    void update();
    void drawOnTop() const;

    void exeAppear();
    void exeWait();
    void exeDisappear();

    // The pause menu's Tracker button asks; GameScene (paused) takes the request and opens the tracker. While it is shown
    // the pause menu below takes no input.
    static void request();
    static bool takeRequest();
    static bool isShown();

private:
    /* 0x20 */ NebulaTrackerParts* mParts;
    /* 0x24 */ GalaxyMapBackground* mBackground;
    /* 0x28 */ bool mIsOpen;
};
#endif
