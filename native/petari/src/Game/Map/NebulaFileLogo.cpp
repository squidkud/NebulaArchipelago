#ifdef TARGET_PC
#include "Game/Map/NebulaFileLogo.hpp"
#include "Game/Map/FIleSelectItem.hpp"
#include "Game/Scene/SceneFunction.hpp"
#include "Game/Util/CameraUtil.hpp"
#include "Game/Util/DrawUtil.hpp"
#include "Game/Util/LiveActorUtil.hpp"
#include "Game/Util/MathUtil.hpp"
#include "Game/Util/ObjUtil.hpp"
#include <revolution/gx/GXCull.h>
#include <revolution/gx/GXEnum.h>
#include <revolution/gx/GXGeometry.h>
#include <revolution/gx/GXPixel.h>
#include <revolution/gx/GXTev.h>
#include <revolution/gx/GXTransform.h>
#include <revolution/gx/GXVert.h>

namespace {
#include "../Screen/NebulaArchipelagoLogoMesh.inc"

    // The logo's outer radius for a file at its normal size, in the file select's units (the head is about as wide).
    const f32 cLogoRadius = 1000.0f;
    // The model's centre above the file's position (the character's models sit there too).
    const f32 cLogoHeight = 900.0f;

    inline u32 shade(const unsigned char* pColor, f32 nx, f32 ny, f32 nz) {
        // A light from the upper left and front, a soft sky light under it, and a highlight: the logo reads as glossy.
        const f32 lx = -0.395f;
        const f32 ly = 0.543f;
        const f32 lz = 0.741f;
        f32 diffuse = nx * lx + ny * ly + nz * lz;
        if (diffuse < 0.0f) {
            diffuse = 0.0f;
        }
        // The half vector between the light and the viewer (who looks along -Z from +Z), normalised.
        const f32 hx = -0.2117f;
        const f32 hy = 0.291f;
        const f32 hz = 0.933f;
        f32 specular = nx * hx + ny * hy + nz * hz;
        if (specular < 0.0f) {
            specular = 0.0f;
        }
        specular *= specular;
        specular *= specular;
        specular *= specular;
        specular *= specular;  // ^16
        const f32 light = 0.4f + 0.5f * diffuse;
        u32 channel[3];
        for (int i = 0; i < 3; i++) {
            f32 value = pColor[i] * light + 255.0f * 0.25f * specular;
            channel[i] = value > 255.0f ? 255u : static_cast< u32 >(value);
        }
        return (channel[0] << 24) | (channel[1] << 16) | (channel[2] << 8) | 0xFFu;
    }

    void emitVertex(int petal, int index, f32 cosY, f32 sinY, const TVec3f& rCentre, f32 radius) {
        const f32* pV = cLogoMesh[petal][index];
        // Turned about the vertical axis, then scaled and put where the file is.
        const f32 x = pV[0] * cosY + pV[2] * sinY;
        const f32 z = -pV[0] * sinY + pV[2] * cosY;
        const f32 nx = pV[3] * cosY + pV[5] * sinY;
        const f32 nz = -pV[3] * sinY + pV[5] * cosY;
        GXPosition3f32(rCentre.x + x * radius, rCentre.y + pV[1] * radius, rCentre.z + z * radius);
        GXColor1u32(shade(cLogoColors[petal], nx, pV[4], nz));
    }
};  // namespace

NebulaFileLogo::NebulaFileLogo(FileSelectItem* pHost) : LiveActor("NebulaFileLogo"), mHost(pHost) {
}

void NebulaFileLogo::init(const JMapInfoIter& rIter) {
    // It is drawn with the opaque things, in the list the ropes of the Trapeze use (nothing else is there in this scene).
    MR::connectToScene(this, MR::MovementType_Environment, MR::CalcAnimType_None, MR::DrawBufferType_None, MR::DrawType_Trapeze);
    MR::invalidateClipping(this);
    makeActorAppeared();
}

void NebulaFileLogo::draw() const {
    if (!MR::isValidDraw(this) || mHost == nullptr || MR::isDead(mHost) || !mHost->mLogoOn) {
        return;
    }

    const f32 radius = cLogoRadius * mHost->getLogoScale();
    TVec3f centre = mHost->mPosition;
    centre.y += cLogoHeight;
    // Like the game's models, the logo's front faces the screen when the file's rotation is 0 (it turns like a planet when dragged).
    const f32 turn = mHost->mRotation.y * 3.14159265f / 180.0f;
    const f32 cosY = MR::cos(turn);
    const f32 sinY = MR::sin(turn);

    MR::reinitGX();
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GXLoadPosMtxImm(MR::getCameraViewMtx(), 0);
    GXSetCurrentMtx(0);
    GXSetNumChans(1);
    GXSetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX, 0, GX_DF_NONE, GX_AF_NONE);
    GXSetChanCtrl(GX_COLOR1A1, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE, GX_AF_NONE);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_FALSE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_FALSE, GX_TEVPREV);
    GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_NOOP);
    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
    GXSetZMode(GX_ENABLE, GX_LEQUAL, GX_ENABLE);
    GXSetZCompLoc(GX_DISABLE);
    GXSetCullMode(GX_CULL_NONE);

    for (int petal = 0; petal < 6; petal++) {
        // The two caps.
        for (int side = 0; side < 2; side++) {
            const int ring = side == 0 ? 0 : cLogoRings - 1;
            GXBegin(GX_TRIANGLES, GX_VTXFMT0, cLogoCapTriangles * 3);
            for (int t = 0; t < cLogoCapTriangles; t++) {
                for (int corner = 0; corner < 3; corner++) {
                    emitVertex(petal, ring * (cLogoSamples + 1) + cLogoCaps[petal][t][corner], cosY, sinY, centre, radius);
                }
            }
            GXEnd();
        }

        // The rounded rim and the side, ring to ring.
        for (int ring = 0; ring < cLogoRings - 1; ring++) {
            GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT0, (cLogoSamples + 1) * 2);
            for (int k = 0; k <= cLogoSamples; k++) {
                emitVertex(petal, ring * (cLogoSamples + 1) + k, cosY, sinY, centre, radius);
                emitVertex(petal, (ring + 1) * (cLogoSamples + 1) + k, cosY, sinY, centre, radius);
            }
            GXEnd();
        }
    }

    MR::reinitGX();
}
#endif
