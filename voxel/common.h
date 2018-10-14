#pragma once

#ifndef ENGINE_COMMON_H
#define ENGINE_COMMON_H

#include <stdint.h>
#include "mgl/MathGeoLib.h"

const int faceList[6][4][3] = 
{
   { { 0, 1, 1 }, { 1, 1, 1 }, { 1, 1, 0 }, { 0, 1, 0 } },
   { { 0, 0, 1 }, { 1, 0, 1 }, { 1, 0, 0 }, { 0, 0, 0 } },
   { { 0, 0, 1 }, { 0, 1, 1 }, { 1, 1, 1 }, { 1, 0, 1 } },
   { { 1, 1, 0 }, { 0, 1, 0 }, { 0, 0, 0 }, { 1, 0, 0 } },
   { { 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 1 }, { 0, 1, 0 } },
   { { 1, 0, 0 }, { 1, 0, 1 }, { 1, 1, 1 }, { 1, 1, 0 } }
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



typedef struct cl_block_info
{
    uint8_t grid_pos[3];
    uint8_t block_info;
} cl_block_info_t;

typedef struct cl_vertex
{
   cl_vertex( float x, float y, float z, int8_t face = 0 )
   {
      px = ( uint8_t ) x;
      py = ( uint8_t ) y;
      pz = ( uint8_t ) z;
      globalAO = 255;
      localAO = 255;
   }

   uint8_t px;
   uint8_t py;
   uint8_t pz;
   uint8_t localAO;
   uint8_t globalAO;

} cl_vertex_t;

typedef struct textureTreeNode
{
	AABB node_bounds;
	float4 data;
} TextureTreeNode;

#endif
