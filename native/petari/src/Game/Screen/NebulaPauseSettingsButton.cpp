#include "Game/Screen/NebulaPauseSettingsButton.hpp"

#ifdef TARGET_PC
#include "Game/Screen/ButtonPaneController.hpp"
#include "Game/Screen/LayoutManager.hpp"
#include "Game/System/NebulaText.hpp"
#include "Game/Util/LayoutUtil.hpp"
#include "Game/Util/MessageUtil.hpp"
#include "Game/Util/ObjUtil.hpp"
#include <cstring>
#include <nw4r/lyt/layout.h>
#include <nw4r/lyt/pane.h>

namespace {
    // The middle of the pause menu's lower band, between its counters' two rows (4 and -32 about their pane at -147).
    const f32 cBandMiddle = -161.0f;
    // The button at the size of the counters beside it; two buttons share the band at a smaller size.
    const f32 cScale = 0.62f;
    const f32 cPairScale = 0.46f;
    // The button is about 72 layout units high at scale 1 (rim and shadow included); the gap between the two buttons.
    const f32 cButtonHeight = 72.0f;
    const f32 cPairGap = 3.0f;
};  // namespace

NebulaPauseSettingsButton::NebulaPauseSettingsButton(const char* pName, const char* pTextId, s32 side)
    : LayoutActor(pName, true), mButton(nullptr), mName(pName), mTextId(pTextId), mSide(side) {
}

void NebulaPauseSettingsButton::init(const JMapInfoIter& rIter) {
    initLayoutManager("PauseMenu", 1);

    // Its own name for the canvas rules (LayoutAspect): it stays in the middle at every width.
    LayoutManager* pManager = getLayoutManager();
    delete[] pManager->mLayoutName;
    pManager->mLayoutName = new char[strlen(mName) + 1];
    memcpy(pManager->mLayoutName, mName, strlen(mName) + 1);

    // The menu's lower button alone, moved down into the band.
    nw4r::lyt::Pane* pRoot = pManager->mLayout->mpRootPane;
    for (nw4r::lyt::PaneList::Iterator it = pRoot->GetChildList().GetBeginIter(); it != pRoot->GetChildList().GetEndIter(); ++it) {
        it->SetVisible(strcmp(it->mName, "BackButton") == 0);
    }
    MR::hidePane(this, "NBackUp");
    nw4r::lyt::Pane* pGroup = MR::getPane(this, "BackButton");
    nw4r::lyt::Pane* pLower = MR::getPane(this, "BackDown");
    if (pGroup != nullptr && pLower != nullptr) {
        const f32 scale = mSide == 0 ? cScale : cPairScale;
        pGroup->mScale = nw4r::math::VEC2(scale, scale);
        // Two buttons stack in the band, about its middle: the Tracker (side 1) above, Settings (side -1) below.
        const f32 offset = mSide == 0 ? 0.0f : mSide * (cButtonHeight * scale * 0.5f + cPairGap * 0.5f);
        pGroup->mTranslate.y = cBandMiddle + offset - pLower->mTranslate.y * scale;
        if (mSide != 0) {
            pGroup->mTranslate.x = -pLower->mTranslate.x * scale;
        }
    }
    wchar_t label[64];
    NebulaText::get(label, 64, mTextId);
    MR::setTextBoxMessageRecursive(this, "NBackDown", label);

    mButton = new ButtonPaneController(this, "NBackDown", "BoxButton2", 0, true);
    mButton->_22 = false;
    // PauseMenu::draw draws it with the menu's own layout: under the windows that open over the menu (its questions,
    // the save's) and under Luigi's letter, as the menu's own buttons are.
    MR::connectToSceneLayoutOnPauseMovementCalcAnim(this);
    kill();
}

void NebulaPauseSettingsButton::control() {
    mButton->update();
}

void NebulaPauseSettingsButton::show() {
    LayoutActor::appear();
    mButton->appear();
}

void NebulaPauseSettingsButton::hide() {
    if (!mButton->isHidden()) {
        mButton->disappear();
    }
}
#endif
