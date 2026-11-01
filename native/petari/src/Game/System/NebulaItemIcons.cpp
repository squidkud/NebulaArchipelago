#ifdef TARGET_PC
#include "Game/System/NebulaItemIcons.hpp"
#include "Game/Util/LayoutUtil.hpp"
#include <JSystem/JUtility/JUTTexture.hpp>
#include <nw4r/lyt/texMap.h>
#include <new>

namespace {
    // The Archipelago logo and the icons of the items (BTI files, RGBA8).
#include "NebulaArchipelagoLogo.inc"
#include "NebulaItemIcons.inc"

    // The texture maps live in static storage, not on the current heap: a map made with `new` while a scene's heap is
    // current is freed with that scene (the Observatory, when a dome is entered), and the next shop or pop-up would read
    // a freed map ("OSCachedToPhysical: 0xd0fed0fe... is outside the game's arena").
    alignas(16) u8 sLogoStorage[sizeof(nw4r::lyt::TexMap)];
    alignas(16) u8 sItemStorage[cItemIconNum][sizeof(nw4r::lyt::TexMap)];
    nw4r::lyt::TexMap* sLogo = nullptr;
    nw4r::lyt::TexMap* sItems[cItemIconNum];
    bool sMade = false;

    nw4r::lyt::TexMap* makeMap(void* pStorage, const void* pBti) {
        JUTTexture texture(const_cast< ResTIMG* >(reinterpret_cast< const ResTIMG* >(pBti)), 0);

        return new (pStorage) nw4r::lyt::TexMap(texture.getTexObj());
    }

    void make() {
        if (sMade) {
            return;
        }

        sMade = true;
        sLogo = makeMap(sLogoStorage, cArchipelagoLogoBti);

        for (int i = 0; i < cItemIconNum; i++) {
            sItems[i] = makeMap(sItemStorage[i], cItemIconBtis[i]);
        }
    }
};  // namespace

namespace NebulaItemIcons {
    const nw4r::lyt::TexMap* getItem(s32 item) {
        make();

        for (int i = 0; i < cItemIconNum; i++) {
            if (cItemIconNumbers[i] == item) {
                return sItems[i];
            }
        }

        return nullptr;
    }

    const nw4r::lyt::TexMap* getLogo() {
        make();
        return sLogo;
    }
};  // namespace NebulaItemIcons
#endif
