#pragma once

#include <mods/service.hpp>

#if __has_include(<mods/svc/interp.h>)
#include <mods/svc/interp.h>
#else
#define INTERP_SERVICE_ID DUSKLIGHT_SERVICE_ID_PREFIX "interp"
#define INTERP_SERVICE_MAJOR 1u
#define INTERP_SERVICE_MINOR 0u

typedef float InterpMtx[3][4];

typedef struct InterpService {
    ServiceHeader header;
    ModResult (*record_mtx_keyed)(ModContext* mod, InterpMtx matrix, void const* key);
    ModResult (*record_mtx)(ModContext* mod, InterpMtx matrix);
    ModResult (*forget_mtx)(ModContext* mod, void const* key);
    bool (*lookup_replacement_mtx)(void const* key, InterpMtx out);
} InterpService;

MOD_DECLARE_SERVICE(InterpService, svc_interp, INTERP_SERVICE_ID, INTERP_SERVICE_MAJOR,
    INTERP_SERVICE_MINOR);
#endif

#include "SSystem/SComponent/c_xyz.h"

struct InterpPos {
    InterpMtx mtx = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
    };

    void record(const cXyz& pos) {
        mtx[0][3] = pos.x;
        mtx[1][3] = pos.y;
        mtx[2][3] = pos.z;
        if (svc_interp == nullptr || mod_ctx == nullptr) return;
        svc_interp->record_mtx(mod_ctx, mtx);
    }

    bool lookup(cXyz& out) const {
        if (svc_interp == nullptr) return false;
        InterpMtx m;
        if (!svc_interp->lookup_replacement_mtx(&mtx, m)) return false;
        out.set(m[0][3], m[1][3], m[2][3]);
        return true;
    }

    void forget() const {
        if (svc_interp == nullptr || mod_ctx == nullptr) return;
        svc_interp->forget_mtx(mod_ctx, &mtx);
    }
};
