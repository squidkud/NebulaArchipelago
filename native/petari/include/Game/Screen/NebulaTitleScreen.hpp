#pragma once

#ifdef TARGET_PC
class LayoutActor;

namespace nw4r {
    namespace lyt {
        class Pane;
    };
};  // namespace nw4r

// The title screen of Nebula Archipelago: the Nebula Archipelago logo in place of the game's (tools/gen_title_logo.py), and the
// "Press A" prompt: the game asks for A and B together, here A alone starts, as in the later releases of the game.
namespace NebulaTitleScreen {
    // The logo layout ("TitleLogo"): its biggest picture takes the new logo (the other big pictures of the logo are hidden). The
    // panes found are written to archipelago.log ("Title pane"), to adjust the choice if a picture is not the right one.
    void replaceLogo(LayoutActor* pLayout);

    // The "Press" layout ("PressStart"): the B button's picture is taken out of the text. The panes and the text found are
    // written to archipelago.log.
    void keepAOnly(LayoutActor* pLayout);

    // A pressed: the new logo fades out and grows a little over a second (the game's own "Decide" animation does not show the
    // new picture). isDecideDone: the fade is over (the layout may be taken down).
    void startDecide();
    bool isDecideDone();

    // Called between a layout's animation and its matrices (LayoutManager): the fade is applied to the logo's picture.
    void afterAnimate(nw4r::lyt::Pane* pRoot);
};  // namespace NebulaTitleScreen
#endif
