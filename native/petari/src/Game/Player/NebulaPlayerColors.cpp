#ifdef TARGET_PC
#include "Game/Player/NebulaPlayerColors.hpp"
#include "Game/System/NebulaArchipelago.hpp"
#include <JSystem/J3DGraphAnimator/J3DModelData.hpp>
#include <JSystem/J3DGraphBase/J3DTexture.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>
#include <JSystem/JUtility/JUTTexture.hpp>
#include <math.h>
#include <stdio.h>

namespace {
    // ResTIMG::mFormat
    enum {
        Fmt_RGB565 = 4,
        Fmt_RGB5A3 = 5,
        Fmt_RGBA8 = 6,
        Fmt_C4 = 8,
        Fmt_C8 = 9,
        Fmt_C14X2 = 10,
        Fmt_CMPR = 14,
    };

    // ResTIMG::mPaletteFormat
    enum {
        Pal_IA8 = 0,
        Pal_RGB565 = 1,
        Pal_RGB5A3 = 2,
    };

    const u8 cDoneMark = 0xC1;  // written in the picture's header (its unused byte) once recoloured

    // What a class of colours is recoloured from, and to.
    struct Recolor {
        bool mOn;
        bool mWhole;     // every colour of the picture (gloves, shoes), not only the colours near mCenterHue
        f32 mCenterHue;  // degrees, the hue of what is replaced (red cap, green cap, blue overalls)
        f32 mHueFull;    // within this many degrees of it, everything is replaced
        f32 mHueNone;    // beyond this many, nothing is
        f32 mRefVal;     // the brightness the original usually has (its shade is kept against it)
        f32 mRefSat;     // the saturation the original usually has
        f32 mHue;        // the new colour, HSV
        f32 mSat;
        f32 mVal;
    };

    f32 smooth(f32 edge0, f32 edge1, f32 x) {
        if (x <= edge0) {
            return 0.0f;
        }

        if (x >= edge1) {
            return 1.0f;
        }

        const f32 t = (x - edge0) / (edge1 - edge0);
        return t * t * (3.0f - 2.0f * t);
    }

    void toHsv(f32 r, f32 g, f32 b, f32* pH, f32* pS, f32* pV) {
        const f32 mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
        const f32 mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
        const f32 d = mx - mn;
        f32 h = 0.0f;

        if (d > 0.0001f) {
            if (mx == r) {
                h = 60.0f * fmodf((g - b) / d, 6.0f);
            } else if (mx == g) {
                h = 60.0f * ((b - r) / d + 2.0f);
            } else {
                h = 60.0f * ((r - g) / d + 4.0f);
            }

            if (h < 0.0f) {
                h += 360.0f;
            }
        }

        *pH = h;
        *pS = mx > 0.0001f ? d / mx : 0.0f;
        *pV = mx;
    }

    void toRgb(f32 h, f32 s, f32 v, f32* pR, f32* pG, f32* pB) {
        const f32 c = v * s;
        const f32 x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
        const f32 m = v - c;
        f32 r = 0.0f;
        f32 g = 0.0f;
        f32 b = 0.0f;

        if (h < 60.0f) {
            r = c, g = x;
        } else if (h < 120.0f) {
            r = x, g = c;
        } else if (h < 180.0f) {
            g = c, b = x;
        } else if (h < 240.0f) {
            g = x, b = c;
        } else if (h < 300.0f) {
            r = x, b = c;
        } else {
            r = c, b = x;
        }

        *pR = r + m;
        *pG = g + m;
        *pB = b + m;
    }

    // One colour (0 to 255 each): the first class that claims it replaces it, with the shading of the original kept.
    void recolor(const Recolor* pClasses, int classNum, u8* pR, u8* pG, u8* pB) {
        f32 h;
        f32 s;
        f32 v;
        toHsv(*pR / 255.0f, *pG / 255.0f, *pB / 255.0f, &h, &s, &v);

        // For the classes that look at the hue: greys and whites (gloves, the cap's emblem, the eyes' white) and the darkest
        // shades are never touched.
        const f32 satWeight = smooth(0.45f, 0.65f, s);

        for (int i = 0; i < classNum; i++) {
            const Recolor& rClass = pClasses[i];

            if (!rClass.mOn) {
                continue;
            }

            f32 weight = 1.0f;
            f32 newS = rClass.mSat;

            if (!rClass.mWhole) {
                if (satWeight <= 0.0f || v < 0.15f) {
                    continue;
                }

                f32 dist = fabsf(h - rClass.mCenterHue);

                if (dist > 180.0f) {
                    dist = 360.0f - dist;
                }

                weight = (1.0f - smooth(rClass.mHueFull, rClass.mHueNone, dist)) * satWeight;

                if (weight <= 0.0f) {
                    continue;
                }

                // The original's saturation against what it usually is (highlights are paler).
                newS = rClass.mSat * (s / rClass.mRefSat);
            }

            f32 newV = rClass.mVal * (v / rClass.mRefVal);
            newS = newS > 1.0f ? 1.0f : newS;
            newV = newV > 1.0f ? 1.0f : newV;
            f32 nr;
            f32 ng;
            f32 nb;
            toRgb(rClass.mHue, newS, newV, &nr, &ng, &nb);
            const f32 r = *pR / 255.0f;
            const f32 g = *pG / 255.0f;
            const f32 b = *pB / 255.0f;
            *pR = static_cast< u8 >((r + (nr - r) * weight) * 255.0f + 0.5f);
            *pG = static_cast< u8 >((g + (ng - g) * weight) * 255.0f + 0.5f);
            *pB = static_cast< u8 >((b + (nb - b) * weight) * 255.0f + 0.5f);
            return;
        }
    }

    u16 readBE16(const u8* p) {
        return static_cast< u16 >((p[0] << 8) | p[1]);
    }

    void writeBE16(u8* p, u16 v) {
        p[0] = static_cast< u8 >(v >> 8);
        p[1] = static_cast< u8 >(v);
    }

    u16 recolor565(const Recolor* pClasses, int classNum, u16 c) {
        u8 r = static_cast< u8 >(((c >> 11) & 0x1F) * 255 / 31);
        u8 g = static_cast< u8 >(((c >> 5) & 0x3F) * 255 / 63);
        u8 b = static_cast< u8 >((c & 0x1F) * 255 / 31);
        recolor(pClasses, classNum, &r, &g, &b);
        return static_cast< u16 >(((r * 31 + 127) / 255) << 11 | ((g * 63 + 127) / 255) << 5 | ((b * 31 + 127) / 255));
    }

    u16 recolor5A3(const Recolor* pClasses, int classNum, u16 c) {
        if (c & 0x8000) {
            u8 r = static_cast< u8 >(((c >> 10) & 0x1F) * 255 / 31);
            u8 g = static_cast< u8 >(((c >> 5) & 0x1F) * 255 / 31);
            u8 b = static_cast< u8 >((c & 0x1F) * 255 / 31);
            recolor(pClasses, classNum, &r, &g, &b);
            return static_cast< u16 >(0x8000 | ((r * 31 + 127) / 255) << 10 | ((g * 31 + 127) / 255) << 5 | ((b * 31 + 127) / 255));
        }

        u8 r = static_cast< u8 >(((c >> 8) & 0xF) * 255 / 15);
        u8 g = static_cast< u8 >(((c >> 4) & 0xF) * 255 / 15);
        u8 b = static_cast< u8 >((c & 0xF) * 255 / 15);
        recolor(pClasses, classNum, &r, &g, &b);
        return static_cast< u16 >((c & 0x7000) | ((r * 15 + 127) / 255) << 8 | ((g * 15 + 127) / 255) << 4 | ((b * 15 + 127) / 255));
    }

    u32 blocks(u32 dim, u32 block) {
        return (dim + block - 1) / block;
    }

    // The bytes of one mip level.
    u32 levelBytes(u8 format, u32 w, u32 h) {
        switch (format) {
        case Fmt_RGBA8:
            return blocks(w, 4) * blocks(h, 4) * 64;
        case Fmt_RGB565:
        case Fmt_RGB5A3:
        case Fmt_C14X2:
            return blocks(w, 4) * blocks(h, 4) * 32;
        case Fmt_C8:
            return blocks(w, 8) * blocks(h, 4) * 32;
        default:  // C4, CMPR
            return blocks(w, 8) * blocks(h, 8) * 32;
        }
    }

    // A compressed picture (S3TC, 4x4 blocks of two 565 colours and 16 two-bit choices): the two colours of every block. The
    // order of the two colours tells the block's mode (c0 > c1: four colours; else three and a clear one), so when the new
    // colours would turn it round they are swapped and the choices follow.
    void recolorBlocks(const Recolor* pClasses, int classNum, u8* pData, u32 bytes) {
        for (u32 off = 0; off + 8 <= bytes; off += 8) {
            u8* p = pData + off;
            const u16 c0 = readBE16(p);
            const u16 c1 = readBE16(p + 2);
            u16 n0 = recolor565(pClasses, classNum, c0);
            u16 n1 = recolor565(pClasses, classNum, c1);

            if (n0 == c0 && n1 == c1) {
                continue;
            }

            const bool fourColors = c0 > c1;

            if (fourColors && n0 == n1) {
                // Equal colours would read as the three-colour mode: keep the order by one step.
                if (n0 < 0xFFFF) {
                    n0++;
                } else {
                    n1--;
                }
            }

            const bool swap = fourColors ? (n0 < n1) : (n0 > n1);

            if (swap) {
                const u16 t = n0;
                n0 = n1;
                n1 = t;

                for (int row = 0; row < 4; row++) {
                    u8 bits = p[4 + row];
                    u8 out = 0;

                    for (int px = 0; px < 4; px++) {
                        u8 idx = (bits >> (px * 2)) & 3;

                        if (fourColors) {
                            idx ^= 1;  // 0<->1 and 2<->3
                        } else if (idx < 2) {
                            idx ^= 1;  // 0<->1; the middle colour and the clear one stay
                        }

                        out |= static_cast< u8 >(idx << (px * 2));
                    }

                    p[4 + row] = out;
                }
            }

            writeBE16(p, n0);
            writeBE16(p + 2, n1);
        }
    }

    void recolorImage(const Recolor* pClasses, int classNum, ResTIMG* pImage) {
        if (pImage->_19 == cDoneMark) {
            return;
        }

        pImage->_19 = cDoneMark;

        u8* pData = reinterpret_cast< u8* >(pImage) + static_cast< s32 >(pImage->mImageDataOffset);
        const u32 width = pImage->mWidth;
        const u32 height = pImage->mHeight;
        const u32 levels = pImage->mImageNum > 0 ? pImage->mImageNum : 1;

        if (static_cast< s32 >(pImage->mImageDataOffset) == 0 || width == 0 || height == 0) {
            return;
        }

        const u8 format = pImage->mFormat;

        // Palette pictures: the palette is the colours.
        if (format == Fmt_C4 || format == Fmt_C8 || format == Fmt_C14X2) {
            const u32 num = pImage->mPaletteNum;
            const s32 paletteOffset = static_cast< s32 >(pImage->mPaletteDataOffset);

            if (paletteOffset == 0 || num == 0 || num > 16384 || pImage->mPaletteFormat == Pal_IA8) {
                return;
            }

            u8* pPalette = reinterpret_cast< u8* >(pImage) + paletteOffset;

            for (u32 i = 0; i < num; i++) {
                const u16 c = readBE16(pPalette + i * 2);
                writeBE16(pPalette + i * 2, pImage->mPaletteFormat == Pal_RGB565 ? recolor565(pClasses, classNum, c) : recolor5A3(pClasses, classNum, c));
            }

            return;
        }

        u32 w = width;
        u32 h = height;

        for (u32 level = 0; level < levels; level++) {
            const u32 bytes = levelBytes(format, w, h);

            switch (format) {
            case Fmt_RGB565:
                for (u32 off = 0; off + 2 <= bytes; off += 2) {
                    writeBE16(pData + off, recolor565(pClasses, classNum, readBE16(pData + off)));
                }

                break;
            case Fmt_RGB5A3:
                for (u32 off = 0; off + 2 <= bytes; off += 2) {
                    writeBE16(pData + off, recolor5A3(pClasses, classNum, readBE16(pData + off)));
                }

                break;
            case Fmt_RGBA8:
                // 64-byte blocks of 16 pixels: first the 16 (alpha, red) pairs, then the 16 (green, blue) pairs.
                for (u32 off = 0; off + 64 <= bytes; off += 64) {
                    for (u32 i = 0; i < 16; i++) {
                        u8* pAR = pData + off + i * 2;
                        u8* pGB = pData + off + 32 + i * 2;
                        recolor(pClasses, classNum, &pAR[1], &pGB[0], &pGB[1]);
                    }
                }

                break;
            case Fmt_CMPR:
                recolorBlocks(pClasses, classNum, pData, bytes);
                break;
            default:
                break;
            }

            pData += bytes;
            w = w > 1 ? w >> 1 : 1;
            h = h > 1 ? h >> 1 : 1;
        }
    }

    // The parts, as the YAML names them (NebulaArchipelago::getPlayerColor: Mario 0 to 4, Luigi 5 to 9).
    enum {
        Part_Cap,
        Part_Shirt,
        Part_Overalls,
        Part_Gloves,
        Part_Shoes,
        Part_Num,
    };

    struct Colors {
        bool mOn[Part_Num];
        u8 mRgb[Part_Num][3];
    };

    bool contains(const char* pName, const char* pPart) {
        // Case-insensitive search of a word in a picture's name.
        for (const char* p = pName; *p != 0; p++) {
            const char* a = p;
            const char* b = pPart;

            while (*a != 0 && *b != 0 && (*a | 0x20) == *b) {
                a++;
                b++;
            }

            if (*b == 0) {
                return true;
            }
        }

        return false;
    }

    // What a picture is, from its name in the model (the log lists the names: "player picture").
    s32 kindOfPicture(const char* pName) {
        if (pName == nullptr) {
            return -1;
        }

        if (contains(pName, "glove") || contains(pName, "hand")) {
            return Part_Gloves;
        }

        if (contains(pName, "shoe") || contains(pName, "boot") || contains(pName, "foot")) {
            return Part_Shoes;
        }

        if (contains(pName, "cap") || contains(pName, "hat")) {
            return Part_Cap;
        }

        if (contains(pName, "shirt")) {
            return Part_Shirt;
        }

        if (contains(pName, "overall") || contains(pName, "pants")) {
            return Part_Overalls;
        }

        return -1;
    }

    void setWhole(Recolor* pOut, const Colors& rColors, s32 part, f32 refVal) {
        pOut->mOn = rColors.mOn[part];
        pOut->mWhole = true;
        pOut->mCenterHue = 0.0f;
        pOut->mHueFull = 0.0f;
        pOut->mHueNone = 0.0f;
        pOut->mRefVal = refVal;
        pOut->mRefSat = 0.9f;
        toHsv(rColors.mRgb[part][0] / 255.0f, rColors.mRgb[part][1] / 255.0f, rColors.mRgb[part][2] / 255.0f, &pOut->mHue, &pOut->mSat, &pOut->mVal);
    }

    void setByHue(Recolor* pOut, const Colors& rColors, s32 part, f32 centerHue, f32 hueFull, f32 hueNone) {
        pOut->mOn = rColors.mOn[part];
        pOut->mWhole = false;
        pOut->mCenterHue = centerHue;
        pOut->mHueFull = hueFull;
        pOut->mHueNone = hueNone;
        pOut->mRefVal = 0.85f;
        pOut->mRefSat = 0.9f;
        toHsv(rColors.mRgb[part][0] / 255.0f, rColors.mRgb[part][1] / 255.0f, rColors.mRgb[part][2] / 255.0f, &pOut->mHue, &pOut->mSat, &pOut->mVal);
    }

    // ---- The stars ----

    // What the colours of the stars become (star_colors): progression, a normal item, a useful one. Red (a trap) stays.
    const u8 cStarProgression[3] = {0x89, 0x7E, 0xC9};
    const u8 cStarNormal[3] = {0x5C, 0xBE, 0xFF};
    const u8 cStarUseful[3] = {0x6D, 0x8B, 0xE8};

    // The saturated colours of a picture: the sum of their hue (as a vector, so that red wraps round) and brightness.
    struct HueSum {
        f32 mX;
        f32 mY;
        f32 mWeight;
        f32 mVal;
        f32 mSat;
        s32 mCount;
    };

    void addSample(HueSum* pSum, u8 r, u8 g, u8 b) {
        f32 h;
        f32 s;
        f32 v;
        toHsv(r / 255.0f, g / 255.0f, b / 255.0f, &h, &s, &v);

        if (s < 0.5f || v < 0.3f) {
            return;
        }

        const f32 w = s * v;
        const f32 rad = h * 3.14159265f / 180.0f;
        pSum->mX += w * cosf(rad);
        pSum->mY += w * sinf(rad);
        pSum->mWeight += w;
        pSum->mVal += w * v;
        pSum->mSat += w * s;
        pSum->mCount++;
    }

    void addSample565(HueSum* pSum, u16 c) {
        addSample(pSum, static_cast< u8 >(((c >> 11) & 0x1F) * 255 / 31), static_cast< u8 >(((c >> 5) & 0x3F) * 255 / 63),
                  static_cast< u8 >((c & 0x1F) * 255 / 31));
    }

    void addSample5A3(HueSum* pSum, u16 c) {
        if (c & 0x8000) {
            addSample(pSum, static_cast< u8 >(((c >> 10) & 0x1F) * 255 / 31), static_cast< u8 >(((c >> 5) & 0x1F) * 255 / 31),
                      static_cast< u8 >((c & 0x1F) * 255 / 31));
        } else {
            addSample(pSum, static_cast< u8 >(((c >> 8) & 0xF) * 255 / 15), static_cast< u8 >(((c >> 4) & 0xF) * 255 / 15),
                      static_cast< u8 >((c & 0xF) * 255 / 15));
        }
    }

    // The main colour of a picture (the first mip level): its hue, brightness and saturation. False when it has none.
    bool sampleColor(const ResTIMG* pImage, f32* pHue, f32* pVal, f32* pSat) {
        if (static_cast< s32 >(pImage->mImageDataOffset) == 0 || pImage->mWidth == 0 || pImage->mHeight == 0) {
            return false;
        }

        HueSum sum = {};
        const u8 format = pImage->mFormat;
        const u8* pData = reinterpret_cast< const u8* >(pImage) + static_cast< s32 >(pImage->mImageDataOffset);

        if (format == Fmt_C4 || format == Fmt_C8 || format == Fmt_C14X2) {
            const s32 paletteOffset = static_cast< s32 >(pImage->mPaletteDataOffset);

            if (paletteOffset == 0 || pImage->mPaletteNum == 0 || pImage->mPaletteNum > 16384 || pImage->mPaletteFormat == Pal_IA8) {
                return false;
            }

            const u8* pPalette = reinterpret_cast< const u8* >(pImage) + paletteOffset;

            for (u32 i = 0; i < pImage->mPaletteNum; i++) {
                const u16 c = readBE16(pPalette + i * 2);

                if (pImage->mPaletteFormat == Pal_RGB565) {
                    addSample565(&sum, c);
                } else {
                    addSample5A3(&sum, c);
                }
            }
        } else {
            const u32 bytes = levelBytes(format, pImage->mWidth, pImage->mHeight);

            switch (format) {
            case Fmt_RGB565:
                for (u32 off = 0; off + 2 <= bytes; off += 2) {
                    addSample565(&sum, readBE16(pData + off));
                }

                break;
            case Fmt_RGB5A3:
                for (u32 off = 0; off + 2 <= bytes; off += 2) {
                    addSample5A3(&sum, readBE16(pData + off));
                }

                break;
            case Fmt_RGBA8:
                for (u32 off = 0; off + 64 <= bytes; off += 64) {
                    for (u32 i = 0; i < 16; i++) {
                        if (pData[off + i * 2] > 16) {
                            addSample(&sum, pData[off + i * 2 + 1], pData[off + 32 + i * 2], pData[off + 32 + i * 2 + 1]);
                        }
                    }
                }

                break;
            case Fmt_CMPR:
                for (u32 off = 0; off + 8 <= bytes; off += 8) {
                    addSample565(&sum, readBE16(pData + off));
                    addSample565(&sum, readBE16(pData + off + 2));
                }

                break;
            default:
                return false;
            }
        }

        if (sum.mCount < 4 || sum.mWeight <= 0.0f) {
            return false;
        }

        f32 hue = atan2f(sum.mY, sum.mX) * 180.0f / 3.14159265f;

        if (hue < 0.0f) {
            hue += 360.0f;
        }

        *pHue = hue;
        *pVal = sum.mVal / sum.mWeight;
        *pSat = sum.mSat / sum.mWeight;
        return true;
    }
};  // namespace

namespace NebulaPlayerColors {
    void applyStar(J3DModelData* pData) {
        if (pData == nullptr || pData->getTexture() == nullptr || !NebulaArchipelago::areStarColorsOn()) {
            return;
        }

        J3DTexture* pTexture = pData->getTexture();
        JUTNameTab* pNames = pData->getTextureName();
        const u16 num = pTexture->getNum();

        for (u16 i = 0; i < num; i++) {
            ResTIMG* pImage = pTexture->getResTIMG(i);

            if (pImage->_19 == cDoneMark) {
                continue;
            }

            f32 hue = 0.0f;
            f32 val = 0.0f;
            f32 sat = 0.0f;
            const char* pTarget = "kept";
            const u8* pRgb = nullptr;

            if (sampleColor(pImage, &hue, &val, &sat)) {
                if (hue >= 25.0f && hue < 85.0f) {
                    pRgb = cStarProgression;  // the gold star
                    pTarget = "progression";
                } else if (hue >= 85.0f && hue < 170.0f) {
                    pRgb = cStarNormal;  // the green star
                    pTarget = "normal";
                } else if (hue >= 170.0f && hue < 260.0f) {
                    pRgb = cStarUseful;  // the blue star
                    pTarget = "useful";
                }  // red: a trap, left as it is
            }

            char note[96];
            snprintf(note, sizeof(note), "hue %d value %d%% -> %s", static_cast< int >(hue), static_cast< int >(val * 100.0f), pTarget);
            NebulaArchipelago::logNote("Star picture", pNames != nullptr ? pNames->getName(i) : "(no name)", note);

            if (pRgb == nullptr) {
                continue;
            }

            Recolor recolorClass;
            recolorClass.mOn = true;
            recolorClass.mWhole = false;
            recolorClass.mCenterHue = hue;
            recolorClass.mHueFull = 30.0f;
            recolorClass.mHueNone = 55.0f;
            recolorClass.mRefVal = val;
            recolorClass.mRefSat = sat;
            toHsv(pRgb[0] / 255.0f, pRgb[1] / 255.0f, pRgb[2] / 255.0f, &recolorClass.mHue, &recolorClass.mSat, &recolorClass.mVal);
            recolorImage(&recolorClass, 1, pImage);
        }
    }

    void apply(J3DModelData* pData, bool isLuigi) {
        if (pData == nullptr || pData->getTexture() == nullptr) {
            return;
        }

        Colors colors;
        bool any = false;

        for (s32 part = 0; part < Part_Num; part++) {
            colors.mOn[part] = NebulaArchipelago::getPlayerColor((isLuigi ? Part_Num : 0) + part, &colors.mRgb[part][0], &colors.mRgb[part][1], &colors.mRgb[part][2]);
            any = any || colors.mOn[part];
        }

        if (!any) {
            static bool sTold = false;

            if (!sTold) {
                sTold = true;
                NebulaArchipelago::logNote("Player colours", NebulaArchipelago::isActive() ? "none in this slot (generate the multiworld with APWorld 0.9.5 and a colour in the YAML)" : "no Archipelago connection", "");
            }

            return;
        }

        J3DTexture* pTexture = pData->getTexture();
        JUTNameTab* pNames = pData->getTextureName();
        const u16 num = pTexture->getNum();

        for (u16 i = 0; i < num; i++) {
            ResTIMG* pImage = pTexture->getResTIMG(i);
            const char* pName = pNames != nullptr ? pNames->getName(i) : nullptr;
            const s32 kind = kindOfPicture(pName);

            if (pImage->_19 == cDoneMark) {
                continue;
            }

            char note[96];
            snprintf(note, sizeof(note), "%dx%d format %d, %s", static_cast< int >(pImage->mWidth), static_cast< int >(pImage->mHeight),
                     static_cast< int >(pImage->mFormat), kind < 0 ? "colours by hue" : "by name");
            NebulaArchipelago::logNote(isLuigi ? "Luigi picture" : "Mario picture", pName != nullptr ? pName : "(no name)", note);

            // Gloves and shoes are whole pictures (a name tells which); the cap and the shirt are what is red (green on
            // Luigi) and the overalls what is blue: the cap's colour is used for both unless the picture is named for the shirt.
            Recolor classes[3];
            int classNum = 0;

            if (kind == Part_Gloves) {
                setWhole(&classes[classNum++], colors, Part_Gloves, 0.95f);
            } else if (kind == Part_Shoes) {
                setWhole(&classes[classNum++], colors, Part_Shoes, 0.45f);
            } else {
                const s32 mainPart = (kind == Part_Shirt) ? (colors.mOn[Part_Shirt] ? Part_Shirt : Part_Cap)
                                                          : (colors.mOn[Part_Cap] ? Part_Cap : Part_Shirt);
                setByHue(&classes[classNum++], colors, mainPart, isLuigi ? 125.0f : 0.0f, isLuigi ? 20.0f : 10.0f, isLuigi ? 40.0f : 20.0f);
                setByHue(&classes[classNum++], colors, Part_Overalls, 225.0f, 25.0f, 45.0f);
            }

            recolorImage(classes, classNum, pImage);
        }
    }
};  // namespace NebulaPlayerColors
#endif
