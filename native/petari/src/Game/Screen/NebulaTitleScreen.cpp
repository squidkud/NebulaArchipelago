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
#include "NebulaTitleBloom.inc"
#include "NebulaTitleCredits.inc"
    const f32 cLogoWidth = 1024.0f;
    const f32 cLogoHeight = 396.0f;

    // In static storage, as NebulaItemIcons does (a map made on a scene's heap is freed with the scene).
    alignas(16) u8 sLogoStorage[sizeof(nw4r::lyt::TexMap)];
    alignas(16) u8 sBloomStorage[sizeof(nw4r::lyt::TexMap)];
    alignas(16) u8 sCreditsStorage[sizeof(nw4r::lyt::TexMap)];
    nw4r::lyt::TexMap* sCredits = nullptr;

    nw4r::lyt::TexMap* getCreditsMap() {
        if (sCredits == nullptr) {
            JUTTexture texture(reinterpret_cast< ResTIMG* >(cTitleCreditsBti), 0);
            sCredits = new (sCreditsStorage) nw4r::lyt::TexMap(texture.getTexObj());
        }

        return sCredits;
    }

    nw4r::lyt::TexMap* sLogo = nullptr;
    nw4r::lyt::TexMap* sBloom = nullptr;

    nw4r::lyt::TexMap* getBloomMap() {
        if (sBloom == nullptr) {
            JUTTexture texture(reinterpret_cast< ResTIMG* >(cTitleBloomBti), 0);
            sBloom = new (sBloomStorage) nw4r::lyt::TexMap(texture.getTexObj());
        }

        return sBloom;
    }

    nw4r::lyt::TexMap* getLogoMap() {
        if (sLogo == nullptr) {
            JUTTexture texture(reinterpret_cast< ResTIMG* >(cTitleLogoBti), 0);
            sLogo = new (sLogoStorage) nw4r::lyt::TexMap(texture.getTexObj());
        }

        return sLogo;
    }

    const int cMaxPanes = 64;
    const int cDecideFrames = 10;  // the least fade the logo has when A is pressed (the game's animation decides when it is slower)
    const f32 cLogoWidthOnScreen = 500.0f;
    const f32 cLogoY = 40.0f;
    const f32 cBloomWidth = 580.0f;  // the glow's pictures: the logo's rectangle with 40 on each side
    const f32 cBloomHeight = 273.0f;

    int sDecideFrame = -1;  // -1: not pressed yet
    f32 sPrevState[cMaxPanes][10];

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

        // From archipelago.log of the game's layout: "PicLogo" is the logo (Super Mario) and "PicLogoGalaxy" the word Galaxy
        // over it, the bloom pictures the glow of the old logo, "PicSpeculum" and "PicLogoShine" its glints and "PicTM" the TM
        // sign. The new logo takes PicLogo and the bloom pictures a blurred copy of it (the game's own animation makes them
        // flash); the word, the glints and the TM sign are taken out (a size of 0).
        nw4r::lyt::Picture* pLogo = nw4r::ut::DynamicCast< nw4r::lyt::Picture* >(findNamed(pRoot, "PicLogo"));

        if (pLogo == nullptr || pLogo->GetMaterial() == nullptr || pLogo->GetMaterial()->GetTextureNum() < 1 || pLogo->mTexCoordAry.mNum < 1 ||
            pLogo->mTexCoordAry.mData == nullptr) {
            NebulaArchipelago::logNote("Title logo", "no PicLogo picture to replace", "");
            return;
        }

        static const char* const cTakenOut[] = {"PicLogoGalaxy", "PicSpeculum", "PicLogoShine", "PicTM"};

        for (size_t i = 0; i < sizeof(cTakenOut) / sizeof(cTakenOut[0]); i++) {
            nw4r::lyt::Pane* pPane = findNamed(pRoot, cTakenOut[i]);

            if (pPane != nullptr) {
                pPane->mSize = nw4r::lyt::Size(0.0f, 0.0f);
                pPane->SetVisible(false);
            }
        }

        // The glow: the game's animation keeps its scale and alpha; the picture and its size are the new logo's.
        for (int b = 0; b < 2; b++) {
            nw4r::lyt::Picture* pBloom = nw4r::ut::DynamicCast< nw4r::lyt::Picture* >(findNamed(pRoot, b == 0 ? "PicBloomA" : "PicBloomB"));

            if (pBloom != nullptr && pBloom->GetMaterial() != nullptr && pBloom->GetMaterial()->GetTextureNum() >= 1 && pBloom->mTexCoordAry.mNum >= 1 &&
                pBloom->mTexCoordAry.mData != nullptr) {
                pBloom->GetMaterial()->SetTexture(0, *getBloomMap());
                nw4r::math::VEC2(*pBloomCoords)[4] = pBloom->mTexCoordAry.mData;
                pBloomCoords[0][0] = nw4r::math::VEC2(0.0f, 0.0f);
                pBloomCoords[0][1] = nw4r::math::VEC2(1.0f, 0.0f);
                pBloomCoords[0][2] = nw4r::math::VEC2(0.0f, 1.0f);
                pBloomCoords[0][3] = nw4r::math::VEC2(1.0f, 1.0f);
                pBloom->mSize = nw4r::lyt::Size(cBloomWidth, cBloomHeight);
                pBloom->mTranslate.x = 0.0f;
                pBloom->mTranslate.y = cLogoY;
                NebulaArchipelago::logNote("Title logo", pBloom->mName, "glow of the new logo");
            }
        }

        // The credits (four lines) in place of "(c) 2007 Nintendo" (the picture PicNintendo, 168 x 24): 236 x 68, above the bottom.
        nw4r::lyt::Picture* pCredits = nw4r::ut::DynamicCast< nw4r::lyt::Picture* >(findNamed(pRoot, "PicNintendo"));

        if (pCredits != nullptr && pCredits->GetMaterial() != nullptr && pCredits->GetMaterial()->GetTextureNum() >= 1 && pCredits->mTexCoordAry.mNum >= 1 &&
            pCredits->mTexCoordAry.mData != nullptr) {
            pCredits->GetMaterial()->SetTexture(0, *getCreditsMap());
            nw4r::math::VEC2(*pCreditCoords)[4] = pCredits->mTexCoordAry.mData;
            pCreditCoords[0][0] = nw4r::math::VEC2(0.0f, 0.0f);
            pCreditCoords[0][1] = nw4r::math::VEC2(1.0f, 0.0f);
            pCreditCoords[0][2] = nw4r::math::VEC2(0.0f, 1.0f);
            pCreditCoords[0][3] = nw4r::math::VEC2(1.0f, 1.0f);
            pCredits->mSize = nw4r::lyt::Size(236.0f, 68.0f);
            pCredits->mTranslate.x = 0.0f;
            pCredits->mTranslate.y = -188.0f;
            NebulaArchipelago::logNote("Title logo", pCredits->mName, "credits");
        }

        pLogo->GetMaterial()->SetTexture(0, *getLogoMap());
        nw4r::math::VEC2(*pCoords)[4] = pLogo->mTexCoordAry.mData;
        pCoords[0][0] = nw4r::math::VEC2(0.0f, 0.0f);
        pCoords[0][1] = nw4r::math::VEC2(1.0f, 0.0f);
        pCoords[0][2] = nw4r::math::VEC2(0.0f, 1.0f);
        pCoords[0][3] = nw4r::math::VEC2(1.0f, 1.0f);

        // 500 wide (the old picture was 440), as high as the new proportions make it, a little above the middle.
        const f32 width = cLogoWidthOnScreen;
        const f32 height = width * cLogoHeight / cLogoWidth;
        pLogo->mSize = nw4r::lyt::Size(width, height);
        pLogo->mTranslate.y = cLogoY;
        sDecideFrame = -1;
        char note[64];
        snprintf(note, sizeof(note), "now %dx%d", static_cast< int >(width), static_cast< int >(height));
        NebulaArchipelago::logNote("Title logo", pLogo->mName, note);
    }

    void startDecide() {
        sDecideFrame = 0;
    }

    bool isDecideDone() {
        return sDecideFrame >= cDecideFrames;
    }

    void afterAnimate(nw4r::lyt::Pane* pRoot) {
        if (pRoot == nullptr) {
            return;
        }

        // Only the title's layout (the pane "SMGTitleLogo" at the root).
        bool isTitle = false;

        for (nw4r::lyt::PaneList::Iterator it = pRoot->GetChildList().GetBeginIter(); it != pRoot->GetChildList().GetEndIter(); ++it) {
            if (strcmp(it->mName, "SMGTitleLogo") == 0) {
                isTitle = true;
                break;
            }
        }

        if (!isTitle) {
            return;
        }

        // The glow is the new logo's size wherever the game's animation puts it (its scale and alpha stay the animation's).
        for (int b = 0; b < 2; b++) {
            nw4r::lyt::Pane* pBloom = findNamed(pRoot, b == 0 ? "PicBloomA" : "PicBloomB");

            if (pBloom != nullptr) {
                pBloom->mSize = nw4r::lyt::Size(cBloomWidth, cBloomHeight);
                pBloom->mTranslate.x = 0.0f;
                pBloom->mTranslate.y = cLogoY;
            }
        }

        nw4r::lyt::Pane* pCredits = findNamed(pRoot, "PicNintendo");

        if (pCredits != nullptr) {
            pCredits->mSize = nw4r::lyt::Size(236.0f, 68.0f);
            pCredits->mTranslate.x = 0.0f;
            pCredits->mTranslate.y = -188.0f;
        }

        if (sDecideFrame < 0) {
            return;
        }

        // Diagnostic: what the game's animation does to every pane after A is pressed, written when something changes (frame 0
        // gives the starting point). The game's animation is left alone.
        if (sDecideFrame <= 120) {
            nw4r::lyt::Pane* panes[cMaxPanes];
            int paneNum = 0;
            collect(pRoot, 0, panes, &paneNum);

            for (int i = 0; i < paneNum; i++) {
                const nw4r::lyt::Pane* pPane = panes[i];
                f32 state[10] = {static_cast< f32 >(pPane->mAlpha), pPane->IsVisible() ? 1.0f : 0.0f, pPane->mScale.x, pPane->mScale.y, pPane->mSize.width,
                                 pPane->mSize.height, pPane->mTranslate.x, pPane->mTranslate.y, pPane->mRotate.z, static_cast< f32 >(pPane->mFlag)};
                bool changed = sDecideFrame == 0;

                for (int k = 0; k < 10 && !changed; k++) {
                    changed = state[k] != sPrevState[i][k];
                }

                if (changed) {
                    char note[160];
                    snprintf(note, sizeof(note), "f%d alpha %d %s scale %.2f,%.2f size %.0fx%.0f at %.1f,%.1f rot %.1f flag %d", sDecideFrame, static_cast< int >(state[0]),
                             state[1] > 0.5f ? "shown" : "hidden", state[2], state[3], state[4], state[5], state[6], state[7], state[8], static_cast< int >(state[9]));
                    NebulaArchipelago::logNote("Decide", pPane->mName, note);
                }

                for (int k = 0; k < 10; k++) {
                    sPrevState[i][k] = state[k];
                }
            }
        }

        sDecideFrame++;
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
