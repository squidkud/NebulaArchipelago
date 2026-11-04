#pragma once

#ifdef TARGET_PC
class LayoutActor;

// The title screen of Nebula Archipelago: the Nebula Archipelago logo in place of the game's (tools/gen_title_logo.py), and the
// "Press A" prompt: the game asks for A and B together, here A alone starts, as in the later releases of the game.
namespace NebulaTitleScreen {
    // The logo layout ("TitleLogo"): its biggest picture takes the new logo (the other big pictures of the logo are hidden). The
    // panes found are written to archipelago.log ("Title pane"), to adjust the choice if a picture is not the right one.
    void replaceLogo(LayoutActor* pLayout);

    // The "Press" layout ("PressStart"): the B button's picture is taken out of the text. The panes and the text found are
    // written to archipelago.log.
    void keepAOnly(LayoutActor* pLayout);
};  // namespace NebulaTitleScreen
#endif
