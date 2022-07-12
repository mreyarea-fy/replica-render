// Copyright (c) Facebook, Inc. and its affiliates. All Rights Reserved
#version 430 core

layout(location = 0) in vec4 position;

uniform mat4 MVP_current;
uniform mat4 MVP_next;
uniform vec4 clipPlane;

out vec4 pos_next;

void main()
{
    gl_ClipDistance[0] = dot(position, clipPlane);
    gl_Position = MVP_current * position;
    pos_next = MVP_next * position;
}