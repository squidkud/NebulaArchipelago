#ifdef TARGET_PC
#include "Game/Screen/NebulaTitleScreen.hpp"
#include "Game/Screen/LayoutActor.hpp"
#include "Game/Screen/LayoutManager.hpp"
#include "Game/System/NebulaArchipelago.hpp"
#include <JSystem/JUtility/JUTTexture.hpp>
#include <cstdio>
#include <cstring>
#include <nw4r/lyt/layout.h>
#include <nw4r/lyt/material.h>
#include <nw4r/lyt/pane.h>
#include <nw4r/lyt/picture.h>
#include <nw4r/lyt/texMap.h>
#include <nw4r/lyt/textBox.h>
#include <new>

namespace {
    // The logo (a BTI file, RGBA8).
#include "NebulaTitleLogo.inc"
    const f32 cLogoWidth = 1024.0f;
    const f32 cLogoHeight = 396.0f;

    // In static storage, as NebulaItemIcons does (a map made on a scene's heap is freed with the scene).
    alignas(16) u8 sLogoStorage[sizeof(nw4r::lyt::TexMap)];
    nw4r::lyt::TexMap* sLogo = nullptr;

    nw4r::lyt::TexMap* getLogoMap() {
        if (sLogo == nullptr) {
            JUTTexture texture(reinterpret_cast< ResTIMG* >(cTitleLogoBti), 0);
            sLogo = new (sLogoStorage) nw4r::lyt::TexMap(texture.getTexObj());
        }

        return sLogo;
    }

    const int cMaxPanes = 64;

    nw4r::lyt::Pane* getRoot(LayoutActor* pLayout) {
        if (pLayout == nullptr || pLayout->getLayoutManager() == nullptr || pLayout->getLayoutManager()->mLayout == nullptr) {
            return nullptr;
        }

        return pLayout->getLayoutManager()->mLayout->mpRootPane;
    }

    bool contains(const char* pText, const char* pPart) {
        return strstr(pText, pPart) != nullptr;
    }

    // Every pane of the branch, parents first.
    void collect(nw4r::lyt::Pane* pPane, int depth, nw4r::lyt::Pane** pList, int* pNum) {
        for (nw4r::lyt::PaneList::Iterator it = pPane->GetChildList().GetBeginIter(); it != pPane->GetChildList().GetEndIter(); ++it) {
            if (*pNum >= cMaxPanes) {
                return;
            }

            pList[(*pNum)++] = &*it;
            collect(&*it, depth + 1, pList, pNum);
        }
    }

    void logPane(const char* pStep, nw4r::lyt::Pane* pPane) {
        const char* pKind = "pane";

        if (nw4r::ut::DynamicCast< nw4r::lyt::Picture* >(pPane) != nullptr) {
            pKind = "picture";
        } else if (nw4r::ut::DynamicCast< nw4r::lyt::TextBox* >(pPane) != nullptr) {
            pKind = "text";
        }

        char note[128];
        snprintf(note, sizeof(note), "%s %dx%d at %d,%d%s", pKind, static_cast< int >(pPane->mSize.width), static_cast< int >(pPane->mSize.height),
                 static_cast< int >(pPane->mTranslate.x), static_cast< int >(pPane->mTranslate.y), pPane->IsVisible() ? "" : " (hidden)");
        NebulaArchipelago::logNote(pStep, pPane->mName, note);
    }

    nw4r::lyt::Pane* findNamed(nw4r::lyt::Pane* pPane, const char* pName) {
        for (nw4r::lyt::PaneList::Iterator it = pPane->GetChildList().GetBeginIter(); it != pPane->GetChildList().GetEndIter(); ++it) {
            if (strcmp(it->mName, pName) == 0) {
                return &*it;
            }

            nw4r::lyt::Pane* pFound = findNamed(&*it, pName);

            if (pFound != nullptr) {
                return pFound;
            }
        }

        return nullptr;
    }

    bool isLetter(wchar_t c) {
        return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z');
    }

    void eraseUnits(nw4r::lyt::TextBox* pText, u16 from, u16 count) {
        wchar_t* pBuf = pText->mTextBuf;

        for (u16 j = from; j + count <= pText->mTextLen; j++) {
            pBuf[j] = pBuf[j + count];
        }

        pText->mTextLen = static_cast< u16 >(pText->mTextLen - count);
        pBuf[pText->mTextLen] = 0;
    }

    // Takes a whole English word (not one inside a picture tag) out of a text, with the space that separated it.
    bool removeWord(nw4r::lyt::TextBox* pText, const char* pWord) {
        wchar_t* pBuf = pText->mTextBuf;
        const u16 length = static_cast< u16 >(strlen(pWord));

        for (u16 i = 0; i < pText->mTextLen; i++) {
            if (pBuf[i] == 0x1A && i + 1 < pText->mTextLen) {
                const u16 units = static_cast< u16 >(reinterpret_cast< const u8* >(&pBuf[i + 1])[0] / 2);
                i = static_cast< u16 >(i + (units > 0 ? units - 1 : 0));
                continue;
            }

            if (i + length > pText->mTextLen || (i > 0 && isLetter(pBuf[i - 1])) || (i + length < pText->mTextLen && isLetter(pBuf[i + length]))) {
                continue;
            }

            bool same = true;

            for (u16 k = 0; k < length && same; k++) {
                wchar_t c = pBuf[i + k];
                c = (c >= L'A' && c <= L'Z') ? static_cast< wchar_t >(c + 32) : c;
                same = c == static_cast< wchar_t >(pWord[k]);
            }

            if (!same) {
                continue;
            }

            u16 from = i;
            u16 count = length;

            if (i + length < pText->mTextLen && pBuf[i + length] == L' ') {
                count++;
            } else if (i > 0 && pBuf[i - 1] == L' ') {
                from--;
                count++;
            }

            eraseUnits(pText, from, count);
            return true;
        }

        return false;
    }

    // "Press both [A] and [B]." with the B taken out -> "Press [A]": the words of the two buttons and the full stop go.
    void trimPressText(nw4r::lyt::TextBox* pText) {
        removeWord(pText, "both");
        removeWord(pText, "and");

        while (pText->mTextLen > 0 && (pText->mTextBuf[pText->mTextLen - 1] == L'.' || pText->mTextBuf[pText->mTextLen - 1] == L' ')) {
            eraseUnits(pText, static_cast< u16 >(pText->mTextLen - 1), 1);
        }
    }

    // Takes the B button's picture (picture font group 3, picture 1) out of a text, with one of the spaces around it.
    bool removeButtonB(nw4r::lyt::TextBox* pText) {
        wchar_t* pBuf = pText->mTextBuf;

        if (pBuf == nullptr) {
            return false;
        }

        for (u16 i = 0; i + 2 < pText->mTextLen; i++) {
            if (pBuf[i] != 0x1A) {
                continue;
            }

            const u8* pHead = reinterpret_cast< const u8* >(&pBuf[i + 1]);
            const u16 units = static_cast< u16 >(pHead[0] / 2);

            if (units < 2 || i + units > pText->mTextLen) {
                continue;
            }

            if (pHead[1] == 3 && pBuf[i + 2] == 1) {
                u16 from = i;
                u16 count = units;

                if (from > 0 && pBuf[from - 1] == L' ') {
                    from--;
                    count++;
                } else if (from + count < pText->mTextLen && pBuf[from + count] == L' ') {
                    count++;
                }

                for (u16 j = from; j + count <= pText->mTextLen; j++) {
                    pBuf[j] = pBuf[j + count];
                }

                pText->mTextLen = static_cast< u16 >(pText->mTextLen - count);
                pBuf[pText->mTextLen] = 0;
                return true;
            }

            i = static_cast< u16 >(i + units - 1);
        }

        return false;
    }
};  // namespace

namespace NebulaTitleScreen {
    void replaceLogo(LayoutActor* pLayout) {
        nw4r::lyt::Pane* pRoot = getRoot(pLayout);

        if (pRoot == nullptr) {
            NebulaArchipelago::logNote("Title logo", "no layout", "");
            return;
        }

        nw4r::lyt::Pane* panes[cMaxPanes];
        int paneNum = 0;
        collect(pRoot, 0, panes, &paneNum);

        for (int i = 0; i < paneNum; i++) {
            logPane("Title pane", panes[i]);
        }

        // The logo's pictures: those of the "SMGTitleLogo" branch (the whole layout when there is none), without the flashes and
        // the lights.
        nw4r::lyt::Pane* pBranch = findNamed(pRoot, "SMGTitleLogo");
        nw4r::lyt::Pane* branch[cMaxPanes];
        int branchNum = 0;

        if (pBranch != nullptr) {
            collect(pBranch, 0, branch, &branchNum);
        } else {
            for (int i = 0; i < paneNum; i++) {
                branch[branchNum++] = panes[i];
            }
        }

        nw4r::lyt::Picture* pits[cMaxPanes];
        int pitNum = 0;
        nw4r::lyt::Picture* pBiggest = nullptr;
        f32 biggest = 0.0f;

        for (int i = 0; i < branchNum; i++) {
            nw4r::lyt::Picture* pPicture = nw4r::ut::DynamicCast< nw4r::lyt::Picture* >(branch[i]);
            const char* pName = branch[i]->mName;

            if (pPicture == nullptr || contains(pName, "Flash") || contains(pName, "Light") || contains(pName, "Shine") ||
                contains(pName, "Glow") || contains(pName, "BG") || contains(pName, "Back")) {
                continue;
            }

            pits[pitNum++] = pPicture;
            const f32 area = pPicture->mSize.width * pPicture->mSize.height;

            if (area > biggest) {
                biggest = area;
                pBiggest = pPicture;
            }
        }

        if (pBiggest == nullptr || pBiggest->GetMaterial() == nullptr || pBiggest->GetMaterial()->GetTextureNum() < 1 ||
            pBiggest->mTexCoordAry.mNum < 1 || pBiggest->mTexCoordAry.mData == nullptr) {
            NebulaArchipelago::logNote("Title logo", "no picture to replace", "");
            return;
        }

        // The other big pictures are parts of the same logo: hidden.
        for (int i = 0; i < pitNum; i++) {
            if (pits[i] != pBiggest && pits[i]->mSize.width * pits[i]->mSize.height >= biggest * 0.25f) {
                pits[i]->SetVisible(false);
                NebulaArchipelago::logNote("Title logo", pits[i]->mName, "hidden (part of the old logo)");
            }
        }

        pBiggest->GetMaterial()->SetTexture(0, *getLogoMap());
        nw4r::math::VEC2(*pCoords)[4] = pBiggest->mTexCoordAry.mData;
        pCoords[0][0] = nw4r::math::VEC2(0.0f, 0.0f);
        pCoords[0][1] = nw4r::math::VEC2(1.0f, 0.0f);
        pCoords[0][2] = nw4r::math::VEC2(0.0f, 1.0f);
        pCoords[0][3] = nw4r::math::VEC2(1.0f, 1.0f);

        // As wide as the old picture, as high as the new proportions make it (not higher than the old one).
        f32 width = pBiggest->mSize.width;
        f32 height = width * cLogoHeight / cLogoWidth;

        if (height > pBiggest->mSize.height) {
            height = pBiggest->mSize.height;
            width = height * cLogoWidth / cLogoHeight;
        }

        pBiggest->mSize = nw4r::lyt::Size(width, height);
        char note[64];
        snprintf(note, sizeof(note), "now %dx%d", static_cast< int >(width), static_cast< int >(height));
        NebulaArchipelago::logNote("Title logo", pBiggest->mName, note);
    }

    void keepAOnly(LayoutActor* pLayout) {
        nw4r::lyt::Pane* pRoot = getRoot(pLayout);

        if (pRoot == nullptr) {
            return;
        }

        nw4r::lyt::Pane* panes[cMaxPanes];
        int paneNum = 0;
        collect(pRoot, 0, panes, &paneNum);

        for (int i = 0; i < paneNum; i++) {
            logPane("Press pane", panes[i]);
            nw4r::lyt::TextBox* pText = nw4r::ut::DynamicCast< nw4r::lyt::TextBox* >(panes[i]);

            if (pText != nullptr && pText->mTextBuf != nullptr) {
                // The text, units in hex (an escape 0x1A starts a picture), to find the B button in it.
                char units[120];
                int length = 0;

                for (u16 u = 0; u < pText->mTextLen && length < 110; u++) {
                    length += snprintf(units + length, sizeof(units) - length, "%X ", static_cast< unsigned >(pText->mTextBuf[u]));
                }

                units[length] = '\0';
                NebulaArchipelago::logNote("Press text", panes[i]->mName, units);

                if (removeButtonB(pText)) {
                    trimPressText(pText);
                    NebulaArchipelago::logNote("Press text", panes[i]->mName, "B button taken out, \"both\" and \"and\" too");
                }
            }

            // A button drawn as a picture of its own (named for B): hidden.
            if (nw4r::ut::DynamicCast< nw4r::lyt::Picture* >(panes[i]) != nullptr &&
                (strcmp(panes[i]->mName, "ButtonB") == 0 || strcmp(panes[i]->mName, "BtnB") == 0 || strcmp(panes[i]->mName, "PicB") == 0 ||
                 strcmp(panes[i]->mName, "B") == 0 || strcmp(panes[i]->mName, "PicButtonB") == 0)) {
                panes[i]->SetVisible(false);
                NebulaArchipelago::logNote("Press pane", panes[i]->mName, "hidden (the B button)");
            }
        }
    }
};  // namespace NebulaTitleScreen
#endif
