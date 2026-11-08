#pragma once

#ifdef TARGET_PC
#include "Game/Screen/LayoutActor.hpp"

class ButtonPaneController;

// The pause menu's Settings button: the pause menu's own lower button (its art, animations and pointer behaviour) at a
// smaller size in the middle of the menu's lower band, between the counters. PauseMenu shows and draws it with its own
// buttons and opens Nebula's settings menu (NebulaSettingsMenu) when it is chosen. The Archipelago tracker's button is
// the same class (side 1: above the middle, the Settings button then sits below it, side -1).
class NebulaPauseSettingsButton : public LayoutActor {
public:
    // `pName`: the layout's own name; `pTextId`: the label's NebulaText id; `side`: 0 in the middle, 1 above it, -1 below it.
    NebulaPauseSettingsButton(const char* pName = "NebulaPauseSettings", const char* pTextId = "SettingsTitle", s32 side = 0);

    virtual void init(const JMapInfoIter& rIter);
    virtual void control();

    void show();
    void hide();
    ButtonPaneController* getButton() const { return mButton; }

private:
    ButtonPaneController* mButton;
    const char* mName;
    const char* mTextId;
    s32 mSide;
};
#endif
