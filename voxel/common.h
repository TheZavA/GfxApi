#pragma once

#ifndef ENGINE_COMMON_H
#define ENGINE_COMMON_H

#include <stdint.h>
#include "mgl/MathGeoLib.h"


struct cell_t
{
   float positions[12][3];
   float normals[12][3];
	uint8_t materials[12];
	uint8_t xPos;
	uint8_t yPos;
	uint8_t zPos;
   uint8_t edgeCount;
	uint8_t corners;
	uint16_t padding;
};

struct cl_float3_t
{
    float x;
    float y;
    float z;
};

struct cl_int4_t
{
    uint8_t x;
    uint8_t y;
    uint8_t z;
    uint8_t w;
};

struct cl_int3_t
{
    bool operator>(const cl_int3_t& b) const
    {
        return ((b.x + b.y + b.z) < (x + y + z));
    }

    bool operator<(const cl_int3_t& b) const
    {
        return ((b.x + b.y + b.z) > (x + y + z));
    }

    int x;
    int y;
    int z;
};


struct edge_t
{
	uint8_t grid_pos[3];
	uint8_t edge;
    cl_float3_t normal;
    cl_float3_t zero_crossing;
	uint8_t material;
};

typedef struct cl_edge_info
{
    uint8_t grid_pos[3];
    uint8_t edge_info;
} cl_edge_info_t;

typedef struct textureTreeNode
{
	AABB node_bounds;
	float4 data;
} TextureTreeNode;

#endif
