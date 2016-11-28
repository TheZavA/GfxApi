#pragma once

#ifndef ENGINE_COMMON_H
#define ENGINE_COMMON_H

#include <stdint.h>
#include "mgl/MathGeoLib.h"

const int faceList[6][5][3] = 
{
   { { 0, 1, 1 },{ 1, 1, 1 },{ 1, 1, 0 },{ 0, 1, 0 }, /*normal*/{ 0, -1, 0 } },

   { { 0, 0, 1 },{ 1, 0, 1 },{ 1, 0, 0 },{ 0, 0, 0 }, /*normal*/{ 0, 1, 0 } },

   { { 0, 0, 1 },{ 0, 1, 1 },{ 1, 1, 1 },{ 1, 0, 1 }, /*normal*/{ -1, 0, 0 } },

   { { 1, 1, 0 },{ 0, 1, 0 },{ 0, 0, 0 },{ 1, 0, 0 }, /*normal*/{ 1, 0, 0 } },

   { { 0, 0, 0 },{ 0, 0, 1 },{ 0, 1, 1 },{ 0, 1, 0 }, /*normal*/{ 0, 0, -1 } },

   { { 1, 0, 0 },{ 1, 0, 1 },{ 1, 1, 1 },{ 1, 1, 0 }, /*normal*/{ 0, 0, 1 } }
};

struct cell_t
{
   cell_t()
   {
      xPos = 0;
      yPos = 0;
      zPos = 0;
      corners = 0;
   }
   float positions[12][3];
   float normals[12][3];
	uint8_t materials[12];
	uint8_t xPos;
	uint8_t yPos;
	uint8_t zPos;
	uint8_t corners;
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

typedef struct cl_block_info
{
    uint8_t grid_pos[3];
    uint8_t block_info;
} cl_block_info_t;

typedef struct cl_vertex
{
   cl_vertex( float x, float y, float z, int8_t nx1 = 0, int8_t ny1 = 0, int8_t nz1 = 0 )
   {
      px = ( uint8_t ) x;
      py = ( uint8_t ) y;
      pz = ( uint8_t ) z;
      nx = nx1;
      ny = ny1;
      nz = nz1;
   }

   uint8_t px;
   uint8_t py;
   uint8_t pz;
   int8_t nx;
   int8_t ny;
   int8_t nz;
   uint8_t localAO;

} cl_vertex_t;

typedef struct textureTreeNode
{
	AABB node_bounds;
	float4 data;
} TextureTreeNode;

#endif
