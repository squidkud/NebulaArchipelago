#pragma once

#ifdef TARGET_PC
#include <revolution/types.h>

namespace nw4r {
    namespace lyt {
        class TexMap;
    };
};  // namespace nw4r

// The pictures of the Archipelago items (tools/gen_item_icons.py) and the Archipelago logo, as layout textures shared by
// the pop-ups and the shop. Made on first use.
namespace NebulaItemIcons {
    // The picture of the item (the engine's item number: 1 1-Up ... 19 Flying), or nullptr when it has none.
    const nw4r::lyt::TexMap* getItem(s32 item);
    // The Archipelago logo.
    const nw4r::lyt::TexMap* getLogo();
};  // namespace NebulaItemIcons
#endif
