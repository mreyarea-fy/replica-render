#version 430 core

layout(lines_adjacency) in;
layout(triangle_strip, max_vertices = 4) out;

smooth in vec4 pos_next[];

smooth out vec4 vpos;
smooth out vec4 vposNext;

void main()
{
    gl_PrimitiveID = gl_PrimitiveIDIn;
    
    gl_ClipDistance[0] = gl_in[1].gl_ClipDistance[0];    
    gl_Position = gl_in[1].gl_Position;
    vpos = gl_in[1].gl_Position;
    vposNext = pos_next[1];
    EmitVertex();

    gl_ClipDistance[0] = gl_in[0].gl_ClipDistance[0];
    gl_Position = gl_in[0].gl_Position;
    vpos = gl_in[0].gl_Position;
    vposNext = pos_next[0];
    EmitVertex();

    gl_ClipDistance[0] = gl_in[2].gl_ClipDistance[0];
    gl_Position = gl_in[2].gl_Position;
    vpos = gl_in[2].gl_Position;
    vposNext = pos_next[2];
    EmitVertex();

    gl_ClipDistance[0] = gl_in[3].gl_ClipDistance[0];
    gl_Position = gl_in[3].gl_Position;
    vpos = gl_in[3].gl_Position;
    vposNext = pos_next[3];
    EmitVertex();

    EndPrimitive();
}
