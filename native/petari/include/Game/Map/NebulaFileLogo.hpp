#pragma once

#ifdef TARGET_PC
#include "Game/LiveActor/LiveActor.hpp"

class FileSelectItem;

// Archipelago: the file select's one save file shows the Archipelago logo in 3D (six rounded discs, drawn with the GX
// commands directly from a mesh made by tools/gen_archipelago_logo_mesh.py) where the game puts the character's head. It
// stands where its file stands, turns with it and swells when pointed at, like the head does.
class NebulaFileLogo : public LiveActor {
public:
    NebulaFileLogo(FileSelectItem* pHost);

    virtual void init(const JMapInfoIter& rIter);
    virtual void draw() const;

private:
    /* 0x8C */ FileSelectItem* mHost;
};
#endif
