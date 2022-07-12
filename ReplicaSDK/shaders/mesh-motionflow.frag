// Copyright (c) Facebook, Inc. and its affiliates. All Rights Reserved
#version 430 core

layout(location = 0) out vec4 optical_flow;

smooth in vec4 vpos;
smooth in vec4 vposNext;

uniform vec2 window_size;

void main()
{
    float vpos_image_x = (vpos.x / vpos.w + 1.0f ) * 0.5 * window_size.x;
    float vposNext_image_x = (vposNext.x / vposNext.w + 1.0f ) * 0.5 * window_size.x;
    float diff_x_forward =  vposNext_image_x - vpos_image_x;
    float vpos_image_y = (vpos.y / vpos.w + 1.0f ) * 0.5 * window_size.y;
    float vposNext_image_y = (vposNext.y / vposNext.w + 1.0f ) * 0.5 * window_size.y;
    float diff_y_forward = vposNext_image_y - vpos_image_y;

    // output the target points z to find the point wrap-around.
    optical_flow = vec4(diff_x_forward, diff_y_forward, vposNext.z, 1.0f);
}
