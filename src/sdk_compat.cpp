#include "d/d_meter2_draw.h"
#include "d/d_select_cursor.h"
#include "JSystem/J3DGraphAnimator/J3DAnimation.h"
#include "JSystem/J3DGraphBase/J3DTexture.h"

#include "dolphin/gx.h"

J3DFrameCtrl::~J3DFrameCtrl() {}

void dSelect_cursor_c::setPos(f32 x, f32 y, const cXyz* world) {
    mPositionX = x;
    mPositionY = y;
    mWorldCursor = (world != nullptr);
    if (world != nullptr) {
        mpPane = nullptr;
        mWorldPosition = *world;
    }
    mInterpolatePosition = false;
}

void J3DTexture::loadGXTexObj(u16 idx) {
    if (idx >= mNum) return;

    ResTIMG* timg = getResTIMG(idx);

    GXTlutObj& tlutObj = mpTlutObj[idx];
    TGXTexObj& texObj = mpTexObj[idx];

    if (!timg->indexTexture) {
        GXInitTexObj(&texObj, mpImgDataPtr[idx], timg->width, timg->height,
                     (GXTexFmt)timg->format, (GXTexWrapMode)timg->wrapS, (GXTexWrapMode)timg->wrapT,
                     timg->mipmapEnabled);
    } else {
        GXInitTexObjCI(&texObj, mpImgDataPtr[idx], timg->width, timg->height,
                       (GXCITexFmt)timg->format, (GXTexWrapMode)timg->wrapS,
                       (GXTexWrapMode)timg->wrapT, timg->mipmapEnabled, GX_TLUT0);
        GXInitTlutObj(&tlutObj, mpTlutDataPtr[idx], (GXTlutFmt)timg->colorFormat,
                      timg->numColors);
    }

    const f32 kLODClampScale = 1.0f / 8.0f;
    const f32 kLODBiasScale = 1.0f / 100.0f;
    GXInitTexObjLOD(&texObj, (GXTexFilter)timg->minFilter, (GXTexFilter)timg->magFilter,
                    timg->minLOD * kLODClampScale, timg->maxLOD * kLODClampScale,
                    timg->LODBias * kLODBiasScale, timg->biasClamp, timg->doEdgeLOD,
                    (GXAnisotropy)timg->maxAnisotropy);
}

f32 dMeter2Draw_c::getMeterGaugeAlphaRate(u8 i_no) {
    return mMeterAlphaRate[i_no];
}
