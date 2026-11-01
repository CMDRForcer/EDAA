// Frame generation (fg_present.cpp): inputs captured at the resolve point.

#pragma once

#include <cstdint>
#include <d3d11.h>

namespace edaa::fg
{
    struct frame_inputs
    {
        uint32_t render_w = 0, render_h = 0;
        float jitter_x = 0, jitter_y = 0; // pixels
        bool reset = false; // camera cut
        float view_proj[16] = {}; // current unjittered c270-c273 (row-major, row vectors)
        float inv_view_proj[16] = {};
        float prev_view_proj[16] = {};
        float camera_delta[3] = {}; // current camera position - previous
        float camera_pos[3] = {};
        float near_plane = 0, vfov_deg = 0, aspect = 0, sx = 0, sy = 0;
    };

    // True while DLSS Frame Generation runs and needs depth and motion vectors.
    bool wants_inputs();

    // Called from resolve() on the game's immediate context: depth is R32_FLOAT,
    // motion the EDAA motion vectors (previous UV - current UV, without jitter).
    void capture(ID3D11DeviceContext* ctx, ID3D11Texture2D* depth, ID3D11Texture2D* motion, const frame_inputs& in);
}
