#include "Game/Screen/NebulaTracker.hpp"

#ifdef TARGET_PC
#include "Game/LiveActor/Nerve.hpp"
#include "Game/Screen/GalaxyMapBackground.hpp"
#include "Game/Screen/LayoutCoreUtil.hpp"
#include "Game/Screen/LayoutManager.hpp"
#include "Game/Screen/NebulaMenuFocus.hpp"
#include "Game/Screen/NebulaPaneKit.hpp"
#include "Game/Screen/NebulaUi.hpp"
#include "Game/System/LayoutHolder.hpp"
#include "Game/System/NebulaArchipelago.hpp"
#include "Game/System/NebulaItemIcons.hpp"
#include "Game/System/NebulaText.hpp"
#include "Game/Util/GamePadUtil.hpp"
#include "Game/Util/LayoutUtil.hpp"
#include "Game/Util/MessageUtil.hpp"
#include "Game/Util/ObjUtil.hpp"
#include "Game/Util/SceneUtil.hpp"
#include "Game/Util/ScreenUtil.hpp"
#include "Game/Util/SoundUtil.hpp"
#include "Game/Util/StarPointerUtil.hpp"
#include "Game/Util/StringUtil.hpp"
#include <cstdio>
#include <cstring>
#include <nebula_game_api.h>
#include <nw4r/lyt/layout.h>
#include <nw4r/lyt/material.h>
#include <nw4r/lyt/pane.h>
#include <nw4r/lyt/picture.h>
#include <nw4r/lyt/textBox.h>
#include <nw4r/lyt/window.h>
#include <revolution/os.h>
#include <revolution/wpad.h>

namespace NrvNebulaTracker {
    NEW_NERVE(NebulaTrackerNrvAppear, NebulaTracker, Appear);
    NEW_NERVE(NebulaTrackerNrvWait, NebulaTracker, Wait);
    NEW_NERVE(NebulaTrackerNrvDisappear, NebulaTracker, Disappear);
};  // namespace NrvNebulaTracker

using namespace NebulaUi;

namespace {
    typedef NebulaArchipelago::TrackerGalaxy Galaxy;
    typedef NebulaArchipelago::TrackerStar Star;

    // The tabs: the six domes in the order they open, then everything outside the domes.
    const s32 cTabNum = 7;
    const s32 cOtherTab = 6;
    const s32 cRowMax = 14;
    const s32 cStarMax = NebulaArchipelago::cTrackerStarMax;
    const s32 cItemMax = 6;
    const s32 cCountNum = 3;
    // The first form's item number in the engine's numbering (14 Bee ... 19 Flying), and the counters' pictures.
    const s32 cFirstForm = 14;
    const s32 cIconPowerStar = 3;
    const s32 cIconGreenStar = 4;
    const s32 cIconGrandStar = 5;

    // Board units (see NebulaAchievementBoard): the layout's units made square; the screen is 456 high at 16:9 and wider.
    const f32 cMargin = 16.0f;
    const f32 cFooter = 50.0f;
    const f32 cPad = 18.0f;
    const f32 cHeaderHeight = 38.0f;
    const f32 cTabHeight = 28.0f;
    const f32 cTabGap = 8.0f;
    const f32 cBackHeight = 32.0f;
    const f32 cRowGap = 5.0f;
    const f32 cColumnGap = 12.0f;
    const f32 cItemSize = 30.0f;
    const f32 cCountHeight = 24.0f;
    const f32 cDetailHeight = 18.0f;

    // Text heights.
    const f32 cTitleSize = 25.0f;
    const f32 cTabSize = 15.0f;
    const f32 cSummarySize = 16.0f;
    const f32 cFooterSize = 14.0f;
    const f32 cFooterGlyph = 21.0f;
    const f32 cBackGlyph = 18.0f;
    const f32 cTitleGlyph = 26.0f;
    const f32 cCountSize = 16.0f;
    const f32 cDetailSize = 14.0f;
    const f32 cCrownHeight = 0.71f;
    const f32 cCrownLift = 0.02f;

    // Colours, 0xRRGGBBAA, those of the achievement board.
    const u32 cPanelFill = 0x0A1442C8;
    const u32 cPanelRing = 0x6C8CE0FF;
    const u32 cRowFill = 0x2A2EB4FF;
    const u32 cRowRing = 0x6C74E8FF;
    const u32 cWhite = 0xFFFFFFFF;
    const u32 cSubText = 0xD2DCFFFF;
    const u32 cGold = 0xFFD860FF;
    const u32 cGoldTop = 0xFFF4B0FF;
    const u32 cGoldBottom = 0xFFC038FF;
    const u32 cBlack = 0x0A0A12FF;  // a star that cannot be done yet
    const u32 cButtonText = 0x505050FF;
    const u32 cTrack = 0x0000004C;
    const u32 cFocusFill = 0xFFFFFF30;
    const u32 cFocusRing = 0xFFFFFFE0;

    const s32 cFadeFrames = 20;
    const s32 cSwitchFrames = 12;
    const s32 cRefreshFrames = 30;

    // The picture font's buttons.
    const s32 cGlyphB = '1';
    const s32 cGlyphPlus = 'H';
    const s32 cGlyphMinus = 'G';
    const s32 cGlyphPad = 'C';
    const s32 cGlyphCrown = 'P';

    // The picture font's star of each kind of location (StarData's kinds in the generated table's order).
    s32 kindGlyph(s32 kind) {
        switch (kind) {
        case 1:
            return 'e';  // a Prankster Comet star
        case 2:
            return 'Z';  // a hidden star
        case 3:
            return 'N';  // a Purple Coin star
        case 5:
            return 'O';  // a Green Star
        default:
            return '7';  // a Power Star, a Grand Star, Luigi's star, the Star Festival
        }
    }

    enum { HintTabs, HintMove, HintNum };
    enum { ZoneTabs, ZoneStars };

    bool sRequested = false;
    bool sShown = false;

    void formatInt(wchar_t* pDst, s32 capacity, s32 value, s32 digits) {
        char text[16];
        snprintf(text, sizeof(text), "%0*d", digits, value);
        s32 i = 0;
        for (; text[i] != 0 && i + 1 < capacity; i++) {
            pDst[i] = static_cast< wchar_t >(text[i]);
        }
        pDst[i] = 0;
    }

    // NebulaText's `pId` with its {name} tokens replaced.
    void fill(wchar_t* pDst, s32 capacity, const char* pId, const char* const* pNames, const wchar_t* const* pValues, s32 count) {
        wchar_t pattern[160];
        NebulaText::get(pattern, 160, pId);
        wchar_t* p = pDst;
        wchar_t* pEnd = pDst + capacity;
        *p = 0;
        for (const wchar_t* s = pattern; *s != 0;) {
            bool replaced = false;
            if (*s == L'{') {
                for (s32 i = 0; i < count && !replaced; i++) {
                    const char* pName = pNames[i];
                    s32 n = 0;
                    while (pName[n] != 0 && s[1 + n] == static_cast< wchar_t >(pName[n])) {
                        n++;
                    }
                    if (pName[n] == 0 && s[1 + n] == L'}') {
                        p = append(p, pEnd, pValues[i]);
                        s += n + 2;
                        replaced = true;
                    }
                }
            }
            if (!replaced) {
                if (p + 1 < pEnd) {
                    *p++ = *s;
                    *p = 0;
                }
                s++;
            }
        }
    }

    // A pattern with one number in it ({n}).
    void fillNumber(wchar_t* pDst, s32 capacity, const char* pId, s32 value) {
        wchar_t number[16];
        formatInt(number, 16, value, 1);
        const char* const names[] = {"n"};
        const wchar_t* const values[] = {number};
        fill(pDst, capacity, pId, names, values, 1);
    }

    struct Row {
        Pane* mRoot;
        Window* mBody;
        TextBox* mName;
        TextBox* mCount;
        TextBox* mStars[cStarMax];
    };
}  // namespace

struct NebulaTrackerParts {
    NebulaPaneKit mText;    // InformationWindow: the game's message font
    NebulaPaneKit mWindow;  // SysInfoWindowMini: the window with a ring
    NebulaPaneKit mButton;  // BackButton: the game's button
    Pane* mBoard;
    Window* mPanel;
    TextBox* mTitleGlyph;
    TextBox* mTitleShadow;
    TextBox* mTitle;
    TextBox* mSummary;
    Window* mSummaryTrack;
    Window* mSummaryBar;
    TextBox* mPercent;
    Chip mTabs[cTabNum];
    Chip mBack;
    TextBox* mHintGlyphs[HintNum];
    TextBox* mHintLabels[HintNum];
    Row mRows[cRowMax];
    Window* mFocusRing;
    TextBox* mDetail;
    TextBox* mEmpty;
    Picture* mCountIcons[cCountNum];
    TextBox* mCountTexts[cCountNum];
    TextBox* mItemsLabel;
    TextBox* mItemsNone;
    Picture* mItemPics[cItemMax];

    // What the tracker shows, read again every half second.
    Galaxy mGalaxies[cRowMax];
    s32 mRowNum;
    bool mTabOpen[cTabNum];
    s32 mChecked;
    s32 mTotal;
    s32 mStarCounter;
    s32 mStarsToFinish;
    s32 mGrandStars;
    s32 mGreenStars;
    bool mFormsOn;     // the forms are items: the row of items shows
    s32 mFormMask;     // the forms received (bit i: item cFirstForm + i)

    // State.
    s32 mTab;
    s32 mZone;
    s32 mTabFocus;
    s32 mStarFocus;  // among every star of the rows shown, row by row
    s32 mRefresh;
    bool mPointerMode;
    bool mPointerIn;
    bool mOverBack;
    f32 mPointerX;
    f32 mPointerY;
    NebulaMenuFocus::Repeat mRepeat;
    s32 mFrame;
    s32 mSwitchFrame;
    f32 mOpenLevel;
    bool mShown;
    bool mArranged;

    // Layout, again whenever the screen's shape or the rows change.
    f32 mScaleX;
    f32 mWidth;
    f32 mHeight;
    f32 mLeft;
    f32 mRight;
    f32 mTabY;
    f32 mFooterY;
    f32 mHeaderY;
    f32 mCountsY;
    f32 mItemsY;
    f32 mDetailY;
    f32 mListTop;
    f32 mListBottom;
    f32 mRowWidth;
    f32 mRowHeight;
    f32 mGlyphSize;
    s32 mColumns;
    s32 mPerColumn;
    f32 mRowX[cRowMax];
    f32 mRowY[cRowMax];
    f32 mStarX[cRowMax][cStarMax];
    f32 mItemX[cItemMax];
    f32 mCountX[cCountNum];
};

namespace {
    Kits trackerKits(NebulaTrackerParts* pParts) {
        const Kits kits = {&pParts->mText, &pParts->mWindow, &pParts->mButton};
        return kits;
    }

    Window* makeWindow(NebulaTrackerParts* pParts, Pane* pParent, const char* pName) {
        return NebulaUi::makeWindow(trackerKits(pParts), pParent, pName);
    }

    TextBox* makeText(NebulaTrackerParts* pParts, Pane* pParent, const char* pName, u32 length) {
        return NebulaUi::makeText(trackerKits(pParts), pParent, pName, length);
    }

    bool makeChip(NebulaTrackerParts* pParts, Pane* pParent, Chip* pChip, const char* pName, bool arrow) {
        return NebulaUi::makeChip(trackerKits(pParts), pParent, pChip, pName, arrow, cTabSize, cButtonText);
    }

    // An item's picture on a picture pane (a square), else nothing.
    void setPicture(Picture* pPicture, const nw4r::lyt::TexMap* pTexture, f32 size, u32 tint) {
        if (pPicture == nullptr) {
            return;
        }
        if (pTexture == nullptr) {
            pPicture->SetVisible(false);
            return;
        }
        pPicture->GetMaterial()->SetTexture(0, *pTexture);
        nw4r::math::VEC2(*pCoords)[4] = pPicture->mTexCoordAry.mData;
        if (pCoords != nullptr && pPicture->mTexCoordAry.mNum > 0) {
            pCoords[0][0] = nw4r::math::VEC2(0.0f, 0.0f);
            pCoords[0][1] = nw4r::math::VEC2(1.0f, 0.0f);
            pCoords[0][2] = nw4r::math::VEC2(0.0f, 1.0f);
            pCoords[0][3] = nw4r::math::VEC2(1.0f, 1.0f);
        }
        pPicture->mSize = nw4r::lyt::Size(size, size);
        for (u32 i = 0; i < 4; i++) {
            pPicture->SetVtxColor(i, color(tint));
        }
        pPicture->SetVisible(true);
    }

    void placeChip(Chip& rChip, f32 x, f32 y, bool active, bool focused, f32 pulse) {
        rChip.mFocusLevel += ((focused ? 1.0f : 0.0f) - rChip.mFocusLevel) * 0.35f;
        const f32 grow = 1.0f + 0.06f * rChip.mFocusLevel;
        rChip.mRoot->mTranslate = nw4r::math::VEC3(x, y, 0.0f);
        rChip.mRoot->mScale = nw4r::math::VEC2(grow, grow);
        shapeChip(rChip, rChip.mWidth, rChip.mHeight);
        rChip.mArt->mAlpha = toAlpha(active ? 1.0f : 0.5f);
        rChip.mLayers[LayerShine]->mAlpha = toAlpha(rChip.mFocusLevel * pulse);
    }

    bool isInside(f32 x, f32 y, f32 cx, f32 cy, f32 w, f32 h) {
        return x >= cx - w * 0.5f && x <= cx + w * 0.5f && y >= cy - h * 0.5f && y <= cy + h * 0.5f;
    }

    // The room a tab shows: the room behind its door, or 0 for what is outside the domes.
    s32 tabRoom(s32 tab) {
        return tab == cOtherTab ? 0 : NebulaArchipelago::getTrackerDoorRoom(tab + 1);
    }

    // The star `flat` of the rows shown: its row and its place in it.
    bool starAt(const NebulaTrackerParts* p, s32 flat, s32* pRow, s32* pStar) {
        for (s32 r = 0; r < p->mRowNum; r++) {
            if (flat < p->mGalaxies[r].mStarNum) {
                *pRow = r;
                *pStar = flat;
                return true;
            }
            flat -= p->mGalaxies[r].mStarNum;
        }
        return false;
    }

    s32 starFlat(const NebulaTrackerParts* p, s32 row, s32 star) {
        s32 flat = star;
        for (s32 r = 0; r < row; r++) {
            flat += p->mGalaxies[r].mStarNum;
        }
        return flat;
    }

    s32 starTotal(const NebulaTrackerParts* p) {
        s32 total = 0;
        for (s32 r = 0; r < p->mRowNum; r++) {
            total += p->mGalaxies[r].mStarNum;
        }
        return total;
    }

    // The forms of a mask, "Bee or Boo".
    wchar_t* appendForms(wchar_t* pDst, wchar_t* pEnd, s32 mask) {
        static const char* const cIds[6] = {"TrackerFormBee", "TrackerFormBoo", "TrackerFormSpring", "TrackerFormFire", "TrackerFormIce", "TrackerFormFlying"};
        bool first = true;
        for (s32 i = 0; i < 6; i++) {
            if ((mask & (1 << i)) == 0) {
                continue;
            }
            wchar_t name[24];
            NebulaText::get(name, 24, cIds[i]);
            if (!first) {
                pDst = append(pDst, pEnd, L" / ");
            }
            pDst = append(pDst, pEnd, name);
            first = false;
        }
        return pDst;
    }

    // What the detail line says of the star: its name, then done, doable, or what it needs.
    void buildDetail(const NebulaTrackerParts* p, s32 row, s32 index, wchar_t* pDst, s32 capacity) {
        const Galaxy& rGalaxy = p->mGalaxies[row];
        const Star& rStar = rGalaxy.mStars[index];
        wchar_t* q = pDst;
        wchar_t* pEnd = pDst + capacity;
        *q = 0;
        q = append(q, pEnd, MR::getScenarioNameOnCurrentLanguage(rGalaxy.mStage, rStar.mScenario));
        q = append(q, pEnd, L"  -  ");
        if (rStar.mState == NebulaArchipelago::TrackerState_Done) {
            wchar_t text[48];
            NebulaText::get(text, 48, "TrackerStatusDone");
            append(q, pEnd, text);
            return;
        }
        if (rStar.mState == NebulaArchipelago::TrackerState_Doable) {
            wchar_t text[48];
            NebulaText::get(text, 48, "TrackerStatusDoable");
            append(q, pEnd, text);
            return;
        }
        // What it needs, one by one.
        wchar_t list[200];
        wchar_t* l = list;
        wchar_t* pListEnd = list + 200;
        *l = 0;
        bool any = false;
        const s32 need = rStar.mNeed;
        wchar_t part[64];
        if ((need & NebulaArchipelago::TrackerNeed_Dome) != 0) {
            fillNumber(part, 64, "TrackerNeedDome", rGalaxy.mDoor);
            l = append(l, pListEnd, part);
            any = true;
        }
        if ((need & NebulaArchipelago::TrackerNeed_Stars) != 0) {
            fillNumber(part, 64, "TrackerNeedStars", rGalaxy.mNeedStars);
            l = append(append(l, pListEnd, any ? L", " : L""), pListEnd, part);
            any = true;
        }
        if ((need & NebulaArchipelago::TrackerNeed_Grand) != 0) {
            fillNumber(part, 64, "TrackerNeedGrand", rGalaxy.mNeedGrand);
            l = append(append(l, pListEnd, any ? L", " : L""), pListEnd, part);
            any = true;
        }
        if ((need & NebulaArchipelago::TrackerNeed_Green) != 0) {
            fillNumber(part, 64, "TrackerNeedGreen", 3);
            l = append(append(l, pListEnd, any ? L", " : L""), pListEnd, part);
            any = true;
        }
        if ((need & NebulaArchipelago::TrackerNeed_Form) != 0) {
            const s32 groups[2] = {rStar.mForms & 0xFF, (rStar.mForms >> 8) & 0xFF};
            for (s32 g = 0; g < 2; g++) {
                if (groups[g] != 0) {
                    l = append(l, pListEnd, any ? L", " : L"");
                    l = appendForms(l, pListEnd, groups[g]);
                    any = true;
                }
            }
        }
        if ((need & NebulaArchipelago::TrackerNeed_Others) != 0) {
            NebulaText::get(part, 64, "TrackerNeedOthers");
            l = append(append(l, pListEnd, any ? L", " : L""), pListEnd, part);
            any = true;
        }
        const char* const names[] = {"list"};
        const wchar_t* const values[] = {list};
        wchar_t needs[240];
        fill(needs, 240, "TrackerNeeds", names, values, 1);
        append(q, pEnd, needs);
    }

    // The slot's numbers, read again: the tabs open, the rows of the tab, the counters, the items.
    void refresh(NebulaTrackerParts* p) {
        const s32 grand = NebulaArchipelago::getTrackerGrandFlags();
        for (s32 t = 0; t < cTabNum; t++) {
            p->mTabOpen[t] = t == cOtherTab || grand >= t + 1;
        }
        // Every location of the slot, for the header's count.
        p->mChecked = 0;
        p->mTotal = 0;
        const s32 places = NebulaArchipelago::getTrackerPlaceNum();
        Galaxy galaxy;
        for (s32 g = 0; g < places; g++) {
            if (!NebulaArchipelago::getTrackerGalaxy(g, &galaxy)) {
                continue;
            }
            p->mTotal += galaxy.mStarNum;
            for (s32 k = 0; k < galaxy.mStarNum; k++) {
                p->mChecked += galaxy.mStars[k].mState == NebulaArchipelago::TrackerState_Done ? 1 : 0;
            }
        }
        // The rows of the tab.
        p->mRowNum = 0;
        const s32 room = tabRoom(p->mTab);
        for (s32 g = 0; g < places && p->mRowNum < cRowMax; g++) {
            if (NebulaArchipelago::getTrackerPlaceRoom(g) != room) {
                continue;
            }
            if (NebulaArchipelago::getTrackerGalaxy(g, &p->mGalaxies[p->mRowNum])) {
                p->mRowNum++;
            }
        }
        const s32 stars = starTotal(p);
        if (p->mStarFocus >= stars) {
            p->mStarFocus = stars > 0 ? stars - 1 : 0;
        }
        p->mStarCounter = MR::getPowerStarNum();
        const s32 finish = NebulaArchipelago::getSlotOption(NEBULA_AP_OPT_STARS_TO_FINISH);
        p->mStarsToFinish = finish > 0 ? finish : 120;
        p->mGrandStars = NebulaArchipelago::getReceivedCount(NEBULA_AP_ITEM_GRAND_STAR);
        p->mGreenStars = NebulaArchipelago::getReceivedCount(NEBULA_AP_ITEM_GREEN_STAR);
        p->mFormsOn = NebulaArchipelago::getSlotOption(NEBULA_AP_OPT_TRANSFORMATION_ITEMS) != 0;
        p->mFormMask = 0;
        for (s32 i = 0; i < cItemMax; i++) {
            if (NebulaArchipelago::getReceivedCount(cFirstForm + i) > 0) {
                p->mFormMask |= 1 << i;
            }
        }
        p->mArranged = false;
    }

    void setHeader(NebulaTrackerParts* p) {
        wchar_t n[16], total[16];
        formatInt(n, 16, p->mChecked, 1);
        formatInt(total, 16, p->mTotal, 1);
        const char* const names[] = {"n", "total"};
        const wchar_t* const values[] = {n, total};
        wchar_t text[48];
        fill(text, 48, "BoardCount", names, values, 2);
        setText(p->mSummary, text);
        fillNumber(text, 48, "BoardPercent", p->mTotal > 0 ? p->mChecked * 100 / p->mTotal : 0);
        setText(p->mPercent, text);
    }

    // Where everything goes for the screen's shape, and what the rows say.
    void arrange(NebulaTrackerParts* p) {
        readScreen(&p->mScaleX, &p->mWidth, &p->mHeight);
        const f32 left = -p->mWidth * 0.5f + cMargin + cPad;
        const f32 right = p->mWidth * 0.5f - cMargin - cPad;
        const f32 top = p->mHeight * 0.5f - cMargin - cPad;
        const f32 panelBottom = -p->mHeight * 0.5f + cFooter;
        p->mLeft = left;
        p->mRight = right;
        p->mHeaderY = top - cHeaderHeight * 0.5f;
        p->mTabY = top - cHeaderHeight - 12.0f - cTabHeight * 0.5f;
        p->mFooterY = -p->mHeight * 0.5f + cFooter * 0.5f;
        p->mItemsY = panelBottom + cPad + cItemSize * 0.5f;
        p->mCountsY = p->mItemsY + cItemSize * 0.5f + 6.0f + cCountHeight * 0.5f;
        p->mDetailY = p->mCountsY + cCountHeight * 0.5f + 8.0f + cDetailHeight * 0.5f;
        p->mListTop = p->mTabY - cTabHeight * 0.5f - 12.0f;
        p->mListBottom = p->mDetailY + cDetailHeight * 0.5f + 6.0f;

        // The tabs across the panel's width, each the same; a label too long for its tab narrows.
        const f32 tabWidth = (right - left - cTabGap * (cTabNum - 1)) / cTabNum;
        for (s32 i = 0; i < cTabNum; i++) {
            Chip& rChip = p->mTabs[i];
            wchar_t label[48];
            if (i == cOtherTab) {
                NebulaText::get(label, 48, "TrackerOther");
            } else {
                fillNumber(label, 48, "TrackerDome", i + 1);
            }
            setText(rChip.mText, label);
            rChip.mText->mScale = nw4r::math::VEC2(1.0f, 1.0f);
            f32 width, height;
            measure(rChip.mText, 4000.0f, &width, &height);
            const f32 room = tabWidth - 24.0f;
            rChip.mText->mScale = nw4r::math::VEC2(width > room && width > 0.0f ? room / width : 1.0f, 1.0f);
            rChip.mWidth = tabWidth;
            rChip.mHeight = cTabHeight;
            rChip.mX = left + tabWidth * 0.5f + i * (tabWidth + cTabGap);
        }

        // The rows: one column up to six galaxies, two beyond.
        const s32 n = p->mRowNum;
        p->mColumns = n > 6 ? 2 : 1;
        p->mPerColumn = n > 0 ? (n + p->mColumns - 1) / p->mColumns : 1;
        const f32 listHeight = p->mListTop - p->mListBottom;
        f32 rowHeight = (listHeight - (p->mPerColumn - 1) * cRowGap) / p->mPerColumn;
        rowHeight = clamp(rowHeight, 20.0f, 42.0f);
        p->mRowHeight = rowHeight;
        p->mRowWidth = (right - left - (p->mColumns - 1) * cColumnGap) / p->mColumns;
        p->mGlyphSize = clamp(rowHeight * 0.74f, 15.0f, 30.0f);
        const f32 nameSize = clamp(rowHeight * 0.46f, 11.0f, 16.0f);
        const f32 nameRoom = p->mColumns == 1 ? clamp(p->mRowWidth * 0.30f, 120.0f, 250.0f) : p->mRowWidth * 0.34f;
        for (s32 r = 0; r < cRowMax; r++) {
            Row& rRow = p->mRows[r];
            if (r >= n) {
                continue;
            }
            const Galaxy& rGalaxy = p->mGalaxies[r];
            const s32 column = r / p->mPerColumn;
            const s32 line = r % p->mPerColumn;
            p->mRowX[r] = left + column * (p->mRowWidth + cColumnGap) + p->mRowWidth * 0.5f;
            p->mRowY[r] = p->mListTop - line * (rowHeight + cRowGap) - rowHeight * 0.5f;
            // The name, narrowed into its room; the stars' count at the right end.
            setTextStyle(rRow.mName, nameSize, cWhite, cWhite, 3);
            setText(rRow.mName, MR::getGalaxyNameOnCurrentLanguage(rGalaxy.mStage));
            f32 width, height;
            measure(rRow.mName, 4000.0f, &width, &height);
            rRow.mName->mScale = nw4r::math::VEC2(width > nameRoom && width > 0.0f ? nameRoom / width : 1.0f, 1.0f);
            s32 done = 0;
            for (s32 k = 0; k < rGalaxy.mStarNum; k++) {
                done += rGalaxy.mStars[k].mState == NebulaArchipelago::TrackerState_Done ? 1 : 0;
            }
            wchar_t a[16], b[16];
            formatInt(a, 16, done, 1);
            formatInt(b, 16, rGalaxy.mStarNum, 1);
            const char* const names[] = {"n", "total"};
            const wchar_t* const values[] = {a, b};
            wchar_t count[32];
            fill(count, 32, "BoardCount", names, values, 2);
            setTextStyle(rRow.mCount, clamp(nameSize - 2.0f, 10.0f, 14.0f), done >= rGalaxy.mStarNum ? cGoldTop : cSubText,
                         done >= rGalaxy.mStarNum ? cGoldBottom : cSubText, 5);
            setText(rRow.mCount, count);
            // The stars, from the end of the name's room, each by its kind and state.
            const f32 rowLeft = -p->mRowWidth * 0.5f;
            const f32 starsLeft = rowLeft + 12.0f + nameRoom + 12.0f;
            const f32 avail = p->mRowWidth - 12.0f - nameRoom - 12.0f - 48.0f;
            f32 spacing = p->mGlyphSize * 1.4f;
            if (rGalaxy.mStarNum > 0 && spacing * rGalaxy.mStarNum > avail) {
                spacing = avail / rGalaxy.mStarNum;
            }
            for (s32 k = 0; k < cStarMax; k++) {
                TextBox* pStar = rRow.mStars[k];
                if (k >= rGalaxy.mStarNum) {
                    pStar->SetVisible(false);
                    continue;
                }
                const Star& rStar = rGalaxy.mStars[k];
                p->mStarX[r][k] = p->mRowX[r] + starsLeft + spacing * (k + 0.5f);
                setGlyph(pStar, kindGlyph(rStar.mKind), p->mGlyphSize);
                if (rStar.mState == NebulaArchipelago::TrackerState_Done) {
                    setTextColor(pStar, cGoldTop, cGoldBottom);
                } else if (rStar.mState == NebulaArchipelago::TrackerState_Doable) {
                    setTextColor(pStar, cWhite, cWhite);
                } else {
                    setTextColor(pStar, cBlack, cBlack);
                }
                pStar->mAlpha = toAlpha(rStar.mState == NebulaArchipelago::TrackerState_Doable ? 0.42f : 1.0f);
            }
        }

        // The empty tab's line.
        wchar_t empty[96] = {0};
        if (n == 0) {
            NebulaText::get(empty, 96, "TrackerEmpty");
        }
        setText(p->mEmpty, empty);

        // The counters: the star counter, the Grand Stars, the Green Stars, each after its picture.
        wchar_t text[64];
        {
            wchar_t a[16], b[16];
            formatInt(a, 16, p->mStarCounter, 1);
            formatInt(b, 16, p->mStarsToFinish, 1);
            const char* const names[] = {"n", "total"};
            const wchar_t* const values[] = {a, b};
            fill(text, 64, "TrackerStarsCount", names, values, 2);
            setText(p->mCountTexts[0], text);
            fillNumber(text, 64, "TrackerGrandCount", p->mGrandStars);
            setText(p->mCountTexts[1], text);
            fillNumber(text, 64, "TrackerGreenCount", p->mGreenStars);
            setText(p->mCountTexts[2], text);
        }
        f32 x = left;
        const s32 icons[cCountNum] = {cIconPowerStar, cIconGrandStar, cIconGreenStar};
        for (s32 i = 0; i < cCountNum; i++) {
            setPicture(p->mCountIcons[i], NebulaItemIcons::getItem(icons[i]), cCountHeight, cWhite);
            p->mCountIcons[i]->mTranslate = nw4r::math::VEC3(x + cCountHeight * 0.5f, p->mCountsY, 0.0f);
            f32 width, height;
            measure(p->mCountTexts[i], 4000.0f, &width, &height);
            p->mCountTexts[i]->mTranslate = nw4r::math::VEC3(x + cCountHeight + 8.0f, textCentre(p->mCountsY, cCountSize), 0.0f);
            p->mCountX[i] = x;
            x += cCountHeight + 8.0f + width + 34.0f;
        }

        // The items received so far (the forms, when they are items): their pictures after the label, one by one.
        NebulaText::get(text, 64, "TrackerItemsLabel");
        {
            // "Items:" with a colon after the word.
            wchar_t label[72];
            wchar_t* q = append(label, label + 72, text);
            append(q, label + 72, L":");
            label[71] = 0;
            setText(p->mItemsLabel, label);
        }
        f32 labelWidth, labelHeight;
        measure(p->mItemsLabel, 4000.0f, &labelWidth, &labelHeight);
        p->mItemsLabel->mTranslate = nw4r::math::VEC3(left, textCentre(p->mItemsY, cCountSize), 0.0f);
        p->mItemsLabel->SetVisible(p->mFormsOn);
        f32 itemX = left + labelWidth + 14.0f;
        bool anyItem = false;
        for (s32 i = 0; i < cItemMax; i++) {
            const bool have = p->mFormsOn && (p->mFormMask & (1 << i)) != 0;
            Picture* pPicture = p->mItemPics[i];
            if (have) {
                setPicture(pPicture, NebulaItemIcons::getItem(cFirstForm + i), cItemSize, cWhite);
                pPicture->mTranslate = nw4r::math::VEC3(itemX + cItemSize * 0.5f, p->mItemsY, 0.0f);
                itemX += cItemSize + 8.0f;
                anyItem = true;
            } else if (pPicture != nullptr) {
                pPicture->SetVisible(false);
            }
        }
        NebulaText::get(text, 64, "TrackerItemsNone");
        setText(p->mItemsNone, text);
        p->mItemsNone->mTranslate = nw4r::math::VEC3(left + labelWidth + 14.0f, textCentre(p->mItemsY, cDetailSize), 0.0f);
        p->mItemsNone->SetVisible(p->mFormsOn && !anyItem);

        // The detail line: the star the highlight is on.
        wchar_t detail[300];
        s32 row, index;
        if (starAt(p, p->mStarFocus, &row, &index) && p->mZone == ZoneStars && (!p->mPointerMode || p->mPointerIn)) {
            buildDetail(p, row, index, detail, 300);
        } else {
            NebulaText::get(detail, 300, "TrackerDetailHint");
        }
        setText(p->mDetail, detail);
        f32 detailWidth, detailHeight;
        measure(p->mDetail, 4000.0f, &detailWidth, &detailHeight);
        const f32 detailRoom = right - left;
        p->mDetail->mScale = nw4r::math::VEC2(detailWidth > detailRoom && detailWidth > 0.0f ? detailRoom / detailWidth : 1.0f, 1.0f);
        p->mDetail->mTranslate = nw4r::math::VEC3(left, textCentre(p->mDetailY, cDetailSize), 0.0f);

        setHeader(p);
        // The footer's labels' sizes, for the pad's guide at the right.
        for (s32 i = 0; i < HintNum; i++) {
            f32 width, height;
            measure(p->mHintLabels[i], 4000.0f, &width, &height);
        }
        measure(p->mBack.mText, 4000.0f, &p->mBack.mWidth, &p->mBack.mHeight);
        p->mBack.mHeight = cBackHeight;
        {
            f32 width, height;
            measure(p->mBack.mText, 4000.0f, &width, &height);
            p->mBack.mWidth = cBackHeight * 0.56f + backArrowSize(cBackHeight) * 0.5f + 6.0f + cBackGlyph + 5.0f + width + cBackHeight * 0.5f;
        }
        p->mBack.mX = -p->mWidth * 0.5f + cMargin + p->mBack.mWidth * 0.5f;
        p->mArranged = true;
    }

    // One frame of placement: the open and tab animations, the highlight.
    void place(NebulaTrackerParts* p, f32 open, s32 frame) {
        const f32 pulse = 0.7f + 0.3f * __builtin_sinf(frame * 0.12f);
        p->mBoard->mScale = nw4r::math::VEC2(p->mScaleX, 1.0f);
        p->mBoard->mAlpha = toAlpha(open);
        p->mBoard->mTranslate = nw4r::math::VEC3(0.0f, (1.0f - easeOut(open)) * -16.0f, 0.0f);

        // The panel and the header: the crown and title, the count, bar and percentage.
        const f32 panelTop = p->mHeight * 0.5f - cMargin;
        const f32 panelBottom = -p->mHeight * 0.5f + cFooter;
        shape(p->mPanel, 0.0f, (panelTop + panelBottom) * 0.5f, p->mWidth - cMargin * 2.0f, panelTop - panelBottom, 16.0f);
        const f32 left = p->mLeft;
        const f32 right = p->mRight;
        const f32 headerY = p->mHeaderY;
        f32 width, height;
        const f32 baseline = headerY - cCapHeight * 0.5f * cTitleSize;
        const f32 crownMiddle = baseline + cCrownHeight * 0.5f * cTitleGlyph;
        p->mTitleGlyph->mTranslate = nw4r::math::VEC3(left + cTitleGlyph * 0.5f + 2.0f, crownMiddle + cCrownLift * cTitleGlyph, 0.0f);
        measure(p->mTitle, 4000.0f, &width, &height);
        measure(p->mTitleShadow, 4000.0f, &width, &height);
        const f32 titleX = left + cTitleGlyph + 12.0f;
        p->mTitle->mTranslate = nw4r::math::VEC3(titleX, textCentre(headerY, cTitleSize), 0.0f);
        p->mTitleShadow->mTranslate = nw4r::math::VEC3(titleX + 1.5f, textCentre(headerY, cTitleSize) - 1.5f, 0.0f);
        f32 percentWidth;
        measure(p->mPercent, 4000.0f, &percentWidth, &height);
        measure(p->mSummary, 4000.0f, &width, &height);
        const f32 barWidth = clamp((right - left) * 0.22f, 90.0f, 220.0f);
        p->mPercent->mTranslate = nw4r::math::VEC3(right, textCentre(headerY, cSummarySize), 0.0f);
        const f32 barRight = right - percentWidth - 12.0f;
        shape(p->mSummaryTrack, barRight - barWidth * 0.5f, headerY, barWidth, 10.0f, 5.0f);
        const f32 done = p->mTotal > 0 ? static_cast< f32 >(p->mChecked) / p->mTotal : 0.0f;
        const f32 fillWidth = barWidth * done * easeOut(open);
        shape(p->mSummaryBar, barRight - barWidth + fillWidth * 0.5f, headerY, fillWidth < 10.0f ? 10.0f : fillWidth, 10.0f, 5.0f);
        p->mSummaryBar->SetVisible(p->mChecked > 0);
        p->mSummary->mTranslate = nw4r::math::VEC3(barRight - barWidth - 12.0f, textCentre(headerY, cSummarySize), 0.0f);

        // The tabs: the chosen one bright, the others dimmed, a closed dome fainter still; the highlight grows and shines.
        for (s32 i = 0; i < cTabNum; i++) {
            Chip& rChip = p->mTabs[i];
            const bool chosen = i == p->mTab;
            const bool focused = p->mZone == ZoneTabs && p->mTabFocus == i && (!p->mPointerMode || p->mPointerIn) && p->mTabOpen[i];
            rChip.mRoot->SetVisible(true);
            placeChip(rChip, rChip.mX, p->mTabY, chosen, focused, pulse);
            if (!p->mTabOpen[i]) {
                rChip.mArt->mAlpha = toAlpha(0.28f);
            }
            rChip.mText->mTranslate = nw4r::math::VEC3(0.0f, textCentre(0.0f, cTabSize), 0.0f);
            rChip.mText->mAlpha = toAlpha(!p->mTabOpen[i] ? 0.4f : chosen ? 1.0f : 0.8f);
        }

        // The rows, each a moment after the one before when a tab opens.
        const f32 rowW = p->mRowWidth;
        const f32 rowH = p->mRowHeight;
        for (s32 r = 0; r < cRowMax; r++) {
            Row& rRow = p->mRows[r];
            if (r >= p->mRowNum) {
                rRow.mRoot->SetVisible(false);
                continue;
            }
            const f32 cascade = clamp((p->mSwitchFrame - (r < 10 ? r : 10) * 1.5f) / 8.0f, 0.0f, 1.0f);
            rRow.mRoot->SetVisible(cascade > 0.0f);
            rRow.mRoot->mAlpha = toAlpha(cascade);
            rRow.mRoot->mTranslate = nw4r::math::VEC3(p->mRowX[r], p->mRowY[r] + (1.0f - cascade) * -8.0f, 0.0f);
            shape(rRow.mBody, 0.0f, 0.0f, rowW, rowH, 10.0f);
            rRow.mName->mTranslate = nw4r::math::VEC3(-rowW * 0.5f + 12.0f, textCentre(0.0f, clamp(rowH * 0.46f, 11.0f, 16.0f)), 0.0f);
            rRow.mCount->mTranslate = nw4r::math::VEC3(rowW * 0.5f - 12.0f, textCentre(0.0f, clamp(clamp(rowH * 0.46f, 11.0f, 16.0f) - 2.0f, 10.0f, 14.0f)), 0.0f);
            const Galaxy& rGalaxy = p->mGalaxies[r];
            for (s32 k = 0; k < rGalaxy.mStarNum && k < cStarMax; k++) {
                TextBox* pStar = rRow.mStars[k];
                pStar->SetVisible(true);
                pStar->mTranslate = nw4r::math::VEC3(p->mStarX[r][k] - p->mRowX[r], glyphCentre(0.0f, p->mGlyphSize), 0.0f);
            }
        }
        p->mEmpty->mTranslate = nw4r::math::VEC3(0.0f, textCentre((p->mListTop + p->mListBottom) * 0.5f, cDetailSize + 3.0f), 0.0f);
        p->mEmpty->SetVisible(p->mRowNum == 0);

        // The highlight round the star the pad or the pointer is on.
        s32 row, index;
        const bool onStar = p->mZone == ZoneStars && (!p->mPointerMode || p->mPointerIn) && starAt(p, p->mStarFocus, &row, &index);
        p->mFocusRing->SetVisible(onStar);
        if (onStar) {
            const f32 ring = p->mGlyphSize + 12.0f;
            shape(p->mFocusRing, p->mStarX[row][index], p->mRowY[row], ring, ring, 9.0f);
            p->mFocusRing->mAlpha = toAlpha(0.55f + 0.45f * pulse);
        }

        // The footer: Back on the left (its arrow, then its label), the pad's guide on the right.
        Chip& rBack = p->mBack;
        placeChip(rBack, rBack.mX, p->mFooterY, true, p->mOverBack, pulse);
        const f32 arrowX = -rBack.mWidth * 0.5f + cBackHeight * 0.56f;
        rBack.mArrow->mTranslate.x = toFaceUnits(rBack, arrowX);
        const f32 glyphX = arrowX + backArrowSize(cBackHeight) * 0.5f + 6.0f + cBackGlyph * 0.5f;
        rBack.mGlyph->mTranslate = nw4r::math::VEC3(glyphX, glyphCentre(0.0f, cBackGlyph), 0.0f);
        rBack.mText->mTranslate = nw4r::math::VEC3(glyphX + cBackGlyph * 0.5f + 5.0f, textCentre(0.0f, cFooterSize + 1.0f), 0.0f);
        f32 x = p->mWidth * 0.5f - cMargin - 6.0f;
        for (s32 i = HintNum - 1; i >= 0; i--) {
            TextBox* pLabel = p->mHintLabels[i];
            pLabel->mTranslate = nw4r::math::VEC3(x, textCentre(p->mFooterY, cFooterSize), 0.0f);
            x -= pLabel->mSize.width + 7.0f;
            TextBox* pGlyphs = p->mHintGlyphs[i];
            const f32 glyphWidth = pGlyphs->mSize.width;
            pGlyphs->mTranslate = nw4r::math::VEC3(x - glyphWidth * 0.5f, glyphCentre(p->mFooterY, cFooterGlyph), 0.0f);
            x -= glyphWidth + 24.0f;
        }
    }

    // The pointer in board units; false when it is off the screen.
    bool readPointer(NebulaTrackerParts* p, f32* pX, f32* pY) {
        if (!MR::isStarPointerInScreen(WPAD_CHAN0)) {
            return false;
        }
        TVec2f screen = MR::getStarPointerScreenPositionOrEdge(WPAD_CHAN0);
        TVec2f layout;
        MR::convertScreenPosToLayoutPos(&layout, screen);
        *pX = layout.x / p->mScaleX;
        *pY = layout.y - p->mBoard->mTranslate.y;
        return true;
    }

    // A tab chosen: a closed dome only sounds the "no".
    bool selectTab(NebulaTrackerParts* p, s32 tab, bool sound) {
        if (tab < 0 || tab >= cTabNum) {
            return false;
        }
        if (!p->mTabOpen[tab]) {
            if (sound) {
                MR::startSystemSE("SE_SY_TALK_SELECT_NO");
            }
            return false;
        }
        if (tab == p->mTab) {
            return true;
        }
        p->mTab = tab;
        p->mTabFocus = tab;
        p->mStarFocus = 0;
        p->mSwitchFrame = 0;
        refresh(p);
        MR::startSystemSE("SE_SY_GALAMAP_PAGE_CHANGE");
        return true;
    }

    // -/+: the next open dome, wrapping at the ends.
    void stepTab(NebulaTrackerParts* p, s32 delta) {
        s32 tab = p->mTab;
        for (s32 i = 0; i < cTabNum; i++) {
            tab = (tab + delta + cTabNum) % cTabNum;
            if (p->mTabOpen[tab]) {
                selectTab(p, tab, false);
                return;
            }
        }
    }

    // The D-pad or stick: left and right walk the tabs or the stars, down enters the stars, up goes back to the tabs.
    void moveFocus(NebulaTrackerParts* p, NebulaMenuFocus::Direction direction) {
        const s32 beforeTab = p->mTabFocus;
        const s32 beforeStar = p->mStarFocus;
        const s32 beforeZone = p->mZone;
        if (p->mZone == ZoneTabs) {
            if (direction == NebulaMenuFocus::Direction_Left || direction == NebulaMenuFocus::Direction_Right) {
                const s32 delta = direction == NebulaMenuFocus::Direction_Left ? -1 : 1;
                for (s32 t = p->mTabFocus + delta; t >= 0 && t < cTabNum; t += delta) {
                    if (p->mTabOpen[t]) {
                        selectTab(p, t, false);
                        break;
                    }
                }
            } else if (direction == NebulaMenuFocus::Direction_Down && starTotal(p) > 0) {
                p->mZone = ZoneStars;
                p->mStarFocus = 0;
            }
        } else {
            s32 row, index;
            if (!starAt(p, p->mStarFocus, &row, &index)) {
                p->mZone = ZoneTabs;
            } else if (direction == NebulaMenuFocus::Direction_Left) {
                if (p->mStarFocus > 0) {
                    p->mStarFocus--;
                }
            } else if (direction == NebulaMenuFocus::Direction_Right) {
                if (p->mStarFocus + 1 < starTotal(p)) {
                    p->mStarFocus++;
                }
            } else if (direction == NebulaMenuFocus::Direction_Up) {
                if (row == 0) {
                    p->mZone = ZoneTabs;
                    p->mTabFocus = p->mTab;
                } else {
                    const s32 target = index < p->mGalaxies[row - 1].mStarNum ? index : p->mGalaxies[row - 1].mStarNum - 1;
                    p->mStarFocus = starFlat(p, row - 1, target);
                }
            } else if (direction == NebulaMenuFocus::Direction_Down) {
                if (row + 1 < p->mRowNum) {
                    const s32 target = index < p->mGalaxies[row + 1].mStarNum ? index : p->mGalaxies[row + 1].mStarNum - 1;
                    p->mStarFocus = starFlat(p, row + 1, target);
                }
            }
        }
        if (p->mTabFocus != beforeTab || p->mStarFocus != beforeStar || p->mZone != beforeZone) {
            MR::startSystemSE("SE_SY_BUTTON_CURSOR_ON");
            p->mArranged = false;  // the detail line follows the star
        }
    }

    // The pointer: what it is over takes the highlight, silently; A picks a tab or Back. True when Back was picked.
    bool handlePointer(NebulaTrackerParts* p) {
        f32 x, y;
        const bool in = readPointer(p, &x, &y);
        const bool moved = in && (!p->mPointerIn || __builtin_fabsf(x - p->mPointerX) > 1.5f || __builtin_fabsf(y - p->mPointerY) > 1.5f);
        p->mPointerIn = in;
        p->mOverBack = false;
        if (!in) {
            return false;
        }
        p->mPointerX = x;
        p->mPointerY = y;
        if (moved) {
            p->mPointerMode = true;
            p->mArranged = false;
        }
        if (!p->mPointerMode) {
            return false;
        }
        s32 overTab = -1;
        for (s32 i = 0; i < cTabNum; i++) {
            if (isInside(x, y, p->mTabs[i].mX, p->mTabY, p->mTabs[i].mWidth, cTabHeight + 4.0f)) {
                overTab = i;
            }
        }
        s32 overStar = -1;
        for (s32 r = 0; r < p->mRowNum && overStar < 0; r++) {
            for (s32 k = 0; k < p->mGalaxies[r].mStarNum; k++) {
                if (isInside(x, y, p->mStarX[r][k], p->mRowY[r], p->mGlyphSize + 8.0f, p->mRowHeight)) {
                    overStar = starFlat(p, r, k);
                    break;
                }
            }
        }
        p->mOverBack = isInside(x, y, p->mBack.mX, p->mFooterY, p->mBack.mWidth, cBackHeight + 4.0f);
        if (overTab >= 0) {
            p->mZone = ZoneTabs;
            p->mTabFocus = overTab;
        } else if (overStar >= 0) {
            if (p->mZone != ZoneStars || p->mStarFocus != overStar) {
                p->mArranged = false;
            }
            p->mZone = ZoneStars;
            p->mStarFocus = overStar;
        } else if (p->mZone == ZoneStars) {
            p->mZone = ZoneTabs;
            p->mTabFocus = p->mTab;
            p->mArranged = false;
        }
        if (MR::testCorePadTriggerA(WPAD_CHAN0)) {
            if (p->mOverBack) {
                return true;
            }
            if (overTab >= 0) {
                selectTab(p, overTab, true);
            }
        }
        return false;
    }
}  // namespace

bool NebulaTracker::isShown() {
    return sShown;
}

void NebulaTracker::request() {
    sRequested = true;
}

bool NebulaTracker::takeRequest() {
    const bool requested = sRequested;
    sRequested = false;
    return requested;
}

NebulaTracker::NebulaTracker() : LayoutActor("NebulaTracker", true), mParts(nullptr), mBackground(nullptr), mIsOpen(false) {
}

void NebulaTracker::init(const JMapInfoIter& rIter) {
    setSharp(SharpBoard, false);
    initLayoutManager("InformationWindow", 0);
    LayoutManager* pManager = getLayoutManager();
    static const char cLayoutName[] = "NebulaTracker";
    delete[] pManager->mLayoutName;
    pManager->mLayoutName = new char[sizeof(cLayoutName)];
    memcpy(pManager->mLayoutName, cLayoutName, sizeof(cLayoutName));

    // The galaxy map's backdrop without its frame window: the tracker's own panel takes its place.
    mBackground = new GalaxyMapBackground();
    mBackground->initWithoutIter();
    MR::hidePane(mBackground, "WinFrame");
    mBackground->kill();
    initNerve(&NrvNebulaTracker::NebulaTrackerNrvAppear::sInstance);
    kill();

    NebulaTrackerParts* p = new NebulaTrackerParts();
    memset(static_cast< void* >(p), 0, sizeof(NebulaTrackerParts));
    new (&p->mText) NebulaPaneKit();
    new (&p->mWindow) NebulaPaneKit();
    new (&p->mButton) NebulaPaneKit();
    Pane* pRoot = pManager->mLayout->mpRootPane;
    if (pRoot == nullptr || !p->mText.init(pManager->mLayoutHolder) || !p->mButton.init(MR::createAndAddLayoutHolder("BackButton.arc"))) {
        OSReport("NebulaTracker: no InformationWindow or BackButton layout\n");
        return;
    }
    p->mWindow.init(MR::createAndAddLayoutHolder("SysInfoWindowMini.arc"));
    for (nw4r::lyt::PaneList::Iterator it = pRoot->GetChildList().GetBeginIter(); it != pRoot->GetChildList().GetEndIter(); ++it) {
        it->SetVisible(false);
    }

    Pane* pBoard = makeGroup(pRoot, "NtBoard");
    p->mBoard = pBoard;
    if (pBoard == nullptr) {
        return;
    }
    p->mPanel = makeWindow(p, pBoard, "NtPanel");
    paint(p->mPanel, cPanelFill, cPanelRing);

    // The header: the crown and title, how many locations are checked.
    p->mTitleGlyph = makeText(p, pBoard, "NtTitleY", 8);
    p->mTitleShadow = makeText(p, pBoard, "NtTitleS", 64);
    p->mTitle = makeText(p, pBoard, "NtTitle", 64);
    p->mSummary = makeText(p, pBoard, "NtSum", 32);
    p->mSummaryTrack = makeWindow(p, pBoard, "NtSumTr");
    p->mSummaryBar = makeWindow(p, pBoard, "NtSumBar");
    p->mPercent = makeText(p, pBoard, "NtSumPc", 16);
    if (p->mPanel == nullptr || p->mTitleGlyph == nullptr || p->mTitleShadow == nullptr || p->mTitle == nullptr ||
        p->mSummary == nullptr || p->mSummaryTrack == nullptr || p->mSummaryBar == nullptr || p->mPercent == nullptr) {
        p->mBoard = nullptr;
        return;
    }
    paint(p->mSummaryTrack, cTrack, cTrack);
    paint(p->mSummaryBar, cGold, cGoldTop);
    wchar_t text[192];
    NebulaText::get(text, 64, "TrackerTitle");
    setText(p->mTitle, text);
    setText(p->mTitleShadow, text);
    setTextStyle(p->mTitle, cTitleSize, cWhite, cWhite, 3);
    setTextStyle(p->mTitleShadow, cTitleSize, 0x00000070, 0x00000070, 3);
    setGlyph(p->mTitleGlyph, cGlyphCrown, cTitleGlyph);
    setTextColor(p->mTitleGlyph, cGoldTop, cGoldBottom);
    setTextStyle(p->mSummary, cSummarySize, cWhite, cWhite, 5);
    setTextStyle(p->mPercent, cSummarySize, cGoldTop, cGoldBottom, 5);

    // The tabs.
    for (s32 i = 0; i < cTabNum; i++) {
        char name[17];
        snprintf(name, sizeof(name), "NtTab%d", i);
        if (!makeChip(p, pBoard, &p->mTabs[i], name, false)) {
            p->mBoard = nullptr;
            return;
        }
    }

    // The rows: a window, the galaxy's name, its count and a star for each of its locations.
    for (s32 r = 0; r < cRowMax; r++) {
        Row& rRow = p->mRows[r];
        char name[17];
        snprintf(name, sizeof(name), "NtR%02d", r);
        rRow.mRoot = makeGroup(pBoard, name);
        if (rRow.mRoot == nullptr) {
            p->mBoard = nullptr;
            return;
        }
        snprintf(name, sizeof(name), "NtR%02dB", r);
        rRow.mBody = makeWindow(p, rRow.mRoot, name);
        snprintf(name, sizeof(name), "NtR%02dN", r);
        rRow.mName = makeText(p, rRow.mRoot, name, 64);
        snprintf(name, sizeof(name), "NtR%02dC", r);
        rRow.mCount = makeText(p, rRow.mRoot, name, 24);
        if (rRow.mBody == nullptr || rRow.mName == nullptr || rRow.mCount == nullptr) {
            p->mBoard = nullptr;
            return;
        }
        paint(rRow.mBody, cRowFill, cRowRing);
        for (s32 k = 0; k < cStarMax; k++) {
            snprintf(name, sizeof(name), "NtR%02dS%d", r, k);
            rRow.mStars[k] = makeText(p, rRow.mRoot, name, 8);
            if (rRow.mStars[k] == nullptr) {
                p->mBoard = nullptr;
                return;
            }
            setGlyph(rRow.mStars[k], '7', 20.0f);
        }
        rRow.mRoot->SetVisible(false);
    }
    p->mFocusRing = makeWindow(p, pBoard, "NtFocus");
    p->mDetail = makeText(p, pBoard, "NtDetail", 320);
    p->mEmpty = makeText(p, pBoard, "NtEmpty", 128);
    if (p->mFocusRing == nullptr || p->mDetail == nullptr || p->mEmpty == nullptr) {
        p->mBoard = nullptr;
        return;
    }
    paint(p->mFocusRing, cFocusFill, cFocusRing);
    setTextStyle(p->mDetail, cDetailSize, cSubText, cSubText, 3);
    setTextStyle(p->mEmpty, cDetailSize + 3.0f, cSubText, cSubText, 4);

    // Below: the counters, each after the item's picture, and the items received.
    nw4r::lyt::TexMap* pLogo = const_cast< nw4r::lyt::TexMap* >(NebulaItemIcons::getLogo());
    for (s32 i = 0; i < cCountNum; i++) {
        char name[17];
        snprintf(name, sizeof(name), "NtCnt%dP", i);
        p->mCountIcons[i] = makePicture(pBoard, name, pLogo);
        snprintf(name, sizeof(name), "NtCnt%dT", i);
        p->mCountTexts[i] = makeText(p, pBoard, name, 64);
        if (p->mCountTexts[i] == nullptr) {
            p->mBoard = nullptr;
            return;
        }
        setTextStyle(p->mCountTexts[i], cCountSize, cWhite, cWhite, 3);
    }
    p->mItemsLabel = makeText(p, pBoard, "NtItemsL", 80);
    p->mItemsNone = makeText(p, pBoard, "NtItemsN", 64);
    if (p->mItemsLabel == nullptr || p->mItemsNone == nullptr) {
        p->mBoard = nullptr;
        return;
    }
    setTextStyle(p->mItemsLabel, cCountSize, cWhite, cWhite, 3);
    setTextStyle(p->mItemsNone, cDetailSize, cSubText, cSubText, 3);
    for (s32 i = 0; i < cItemMax; i++) {
        char name[17];
        snprintf(name, sizeof(name), "NtItem%d", i);
        p->mItemPics[i] = makePicture(pBoard, name, pLogo);
    }

    // The footer: the game's Back button and its own label; the pad's guide.
    if (!makeChip(p, pBoard, &p->mBack, "NtBack", true)) {
        p->mBoard = nullptr;
        return;
    }
    p->mBack.mGlyph = makeText(p, p->mBack.mRoot, "NtBackY", 8);
    if (p->mBack.mGlyph == nullptr || p->mBack.mArrow == nullptr) {
        p->mBoard = nullptr;
        return;
    }
    setGlyph(p->mBack.mGlyph, cGlyphB, cBackGlyph);
    NebulaText::get(text, 48, "BoardBack");
    const wchar_t* pBack = text;
    while (*pBack == L' ') {
        pBack++;
    }
    setText(p->mBack.mText, pBack);
    setTextStyle(p->mBack.mText, cFooterSize + 1.0f, cButtonText, cButtonText, 3);
    static const char* const cHintIds[HintNum] = {"HintTabs", "HintMove"};
    for (s32 i = 0; i < HintNum; i++) {
        char name[17];
        snprintf(name, sizeof(name), "NtHint%dY", i);
        p->mHintGlyphs[i] = makeText(p, pBoard, name, 16);
        snprintf(name, sizeof(name), "NtHint%dT", i);
        p->mHintLabels[i] = makeText(p, pBoard, name, 48);
        if (p->mHintGlyphs[i] == nullptr || p->mHintLabels[i] == nullptr) {
            p->mBoard = nullptr;
            return;
        }
        wchar_t glyphs[16] = {0};
        wchar_t* q = glyphs;
        if (i == HintTabs) {
            q = MR::addPictureFontTag(q, cGlyphMinus);
            q = MR::addPictureFontTag(q, cGlyphPlus);
        } else {
            MR::addPictureFontTag(q, cGlyphPad);
        }
        setText(p->mHintGlyphs[i], glyphs);
        p->mHintGlyphs[i]->SetFontSize(glyphFontSize(p->mHintGlyphs[i], cFooterGlyph));
        p->mHintGlyphs[i]->mCharSpace = 3.0f;
        setAnchor(p->mHintGlyphs[i], 4);
        NebulaText::get(text, 48, cHintIds[i]);
        setText(p->mHintLabels[i], text);
        setTextStyle(p->mHintLabels[i], cFooterSize, cSubText, cSubText, 5);
    }

    // The backdrop fades in and out with the tracker.
    if (Pane* pBackground = mBackground->getLayoutManager()->mLayout->mpRootPane) {
        setInfluencedAlphaRecursive(pBackground, 0);
    }

    mParts = p;
    pManager->rebuildPaneInfo();
}

void NebulaTracker::open() {
    NebulaTrackerParts* p = mParts;
    if (p == nullptr || p->mBoard == nullptr) {
        return;
    }
    mIsOpen = true;
    sShown = true;
    setSharp(SharpBoard, true);
    p->mTab = 0;
    p->mZone = ZoneTabs;
    p->mTabFocus = 0;
    p->mStarFocus = 0;
    p->mRefresh = 0;
    p->mPointerMode = false;
    p->mPointerIn = false;
    p->mOverBack = false;
    NebulaMenuFocus::holdCurrent(&p->mRepeat);
    p->mFrame = 0;
    p->mSwitchFrame = 0;
    p->mOpenLevel = 1.0f;
    p->mShown = false;
    p->mBack.mFocusLevel = 0.0f;
    for (s32 i = 0; i < cTabNum; i++) {
        p->mTabs[i].mFocusLevel = 0.0f;
    }
    refresh(p);
    // The first tab is the first dome; open at the latest dome that is open instead when the first is closed (never).
    LayoutActor::appear();
    setNerve(&NrvNebulaTracker::NebulaTrackerNrvAppear::sInstance);
}

bool NebulaTracker::isOpen() const {
    return mIsOpen;
}

void NebulaTracker::update() {
    movement();
    mBackground->movement();
    calcAnim();
    mBackground->calcAnim();
}

void NebulaTracker::drawOnTop() const {
    NebulaTrackerParts* p = mParts;
    if (!mIsOpen || p == nullptr || p->mBoard == nullptr || !p->mShown) {
        return;
    }
    mBackground->draw();
    draw();
}

void NebulaTracker::control() {
    NebulaTrackerParts* p = mParts;
    if (p == nullptr || p->mBoard == nullptr) {
        return;
    }
    // A new screen shape (a resized window) lays the tracker out again.
    f32 scaleX, width, height;
    readScreen(&scaleX, &width, &height);
    if (!p->mArranged || __builtin_fabsf(width - p->mWidth) > 0.5f || __builtin_fabsf(height - p->mHeight) > 0.5f || scaleX != p->mScaleX) {
        arrange(p);
    }
    p->mFrame++;
    if (p->mSwitchFrame < cSwitchFrames + 40) {
        p->mSwitchFrame++;
    }
    place(p, p->mOpenLevel, p->mFrame);
}

// The world fades to black (the game's system wipe, drawn over every layout); the tracker is there when it opens.
void NebulaTracker::exeAppear() {
    NebulaTrackerParts* p = mParts;
    if (MR::isFirstStep(this)) {
        MR::startSystemSE("SE_SY_GALAMAP_WINDOW_OPEN");
        MR::closeSystemWipeFade(cFadeFrames);
        return;
    }
    if (MR::isSystemWipeActive()) {
        return;
    }
    p->mShown = true;
    p->mSwitchFrame = 0;
    mBackground->appear();
    mBackground->getLayoutManager()->mLayout->mpRootPane->mAlpha = 255;
    MR::openSystemWipeFade(cFadeFrames);
    setNerve(&NrvNebulaTracker::NebulaTrackerNrvWait::sInstance);
}

void NebulaTracker::exeWait() {
    NebulaTrackerParts* p = mParts;
    if (MR::isSystemWipeActive()) {
        return;  // it takes no input while it comes out of the black
    }
    if (MR::testSystemTriggerB()) {
        MR::startSystemSE("SE_SY_GALAMAP_WINDOW_CLOSE");
        setNerve(&NrvNebulaTracker::NebulaTrackerNrvDisappear::sInstance);
        return;
    }
    // What a release or a purchase changed, and the server's answers, are read again twice a second.
    if (++p->mRefresh >= cRefreshFrames) {
        p->mRefresh = 0;
        refresh(p);
    }
    if (MR::testCorePadTriggerMinus(WPAD_CHAN0)) {
        stepTab(p, -1);
    } else if (MR::testCorePadTriggerPlus(WPAD_CHAN0)) {
        stepTab(p, 1);
    }
    const NebulaMenuFocus::Direction press = NebulaMenuFocus::readPress(&p->mRepeat);
    if (press != NebulaMenuFocus::Direction_None) {
        if (p->mPointerMode) {
            // The D-pad takes over from where the pointer was.
            p->mPointerMode = false;
            p->mOverBack = false;
            p->mArranged = false;
        } else {
            moveFocus(p, press);
        }
        if (MR::testCorePadTriggerA(WPAD_CHAN0) && p->mZone == ZoneTabs) {
            selectTab(p, p->mTabFocus, true);
        }
    } else {
        if (!p->mPointerMode && p->mZone == ZoneTabs && MR::testCorePadTriggerA(WPAD_CHAN0)) {
            selectTab(p, p->mTabFocus, true);
        }
        if (handlePointer(p)) {
            MR::startSystemSE("SE_SY_GALAMAP_WINDOW_CLOSE");
            setNerve(&NrvNebulaTracker::NebulaTrackerNrvDisappear::sInstance);
            return;
        }
    }
}

// The tracker fades to black; the pause menu comes back out of it.
void NebulaTracker::exeDisappear() {
    NebulaTrackerParts* p = mParts;
    if (MR::isFirstStep(this)) {
        MR::closeSystemWipeFade(cFadeFrames);
        return;
    }
    if (MR::isSystemWipeActive()) {
        return;
    }
    p->mShown = false;
    sShown = false;
    mBackground->kill();
    kill();
    mIsOpen = false;
    setSharp(SharpBoard, false);
    MR::openSystemWipeFade(cFadeFrames);
}
#endif
