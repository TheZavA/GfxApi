#ifndef _CHUNK_H
#define _CHUNK_H

#include "../MainClass.h"
#include "octree.h"

#include "qef.h"

#include "../noisepp/core/Noise.h"
#include "../noisepp/utils/NoiseUtils.h"
#include "../xmlnoise/xml_noise3d.hpp"
#include "../xmlnoise/xml_noise3d_handlers.hpp"
#include "../xmlnoise/xml_noise2d.hpp"
#include "../xmlnoise/xml_noise2d_handlers.hpp"

#include "TVolume2d.h"
#include "TVolume3d.h"
#include "TBorderVolume3d.h"
#include "../TOctree.h"
#include <unordered_map>


class ChunkManager;
class Ring;

struct zeroCrossings_t
{
   	float3 positions[6];
    float3 normals[6];
	int edgeCount;
};

struct corner_cl_t
{
   	int mins[3];
	int corners;
    int count;
};

struct zeroCrossings_cl_t
{
   	float positions[12][3];
    float normals[12][3];
    uint16_t xPos;
    uint16_t yPos;
    uint16_t zPos;
	uint8_t edgeCount;
    int corners;
};

struct cube_t
{
    int edge_indices[12];
    int count;
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
    int grid_pos[4];
    int edge;
    cl_float3_t normal;
    cl_float3_t zero_crossing;
};

typedef struct cl_edge_info
{
    uint8_t grid_pos[3];
    uint8_t edge_info;
} cl_edge_info_t;

class Chunk
{
private:
   

    ChunkManager* m_pChunkManager;

public:
    float m_scale;

    bool m_bHasNodes;

    uint32_t m_octree_root_index;

    boost::shared_ptr<Ring> m_p_parent_ring;

    // own position in the ring grid
    std::size_t m_x;
    std::size_t m_y;
    std::size_t m_z;

    // base position of the ring
    float3 m_basePos;

    AABB m_ringBounds;

    boost::shared_ptr<IndexBuffer> m_pIndices;
    boost::shared_ptr<VertexBuffer> m_pVertices;

    boost::shared_ptr<TVolume3d<zeroCrossings_cl_t>> m_zeroCrossingsCl3d;

    boost::shared_ptr<std::vector<edge_t>> m_edgesCompact;
    boost::shared_ptr<std::vector<zeroCrossings_cl_t>> m_zeroCrossCompact;
    boost::shared_ptr<std::vector<cl_int4_t>> m_cornersCompact;

    boost::shared_ptr<bool> m_workInProgress;
  
    boost::shared_ptr<GfxApi::Mesh> m_pMesh;

    std::vector<OctreeNode> m_seam_nodes;

    int32_t m_nodeIdxCurr;
    int32_t m_nodeDrInf;
    boost::shared_ptr<std::vector<OctreeNode>> m_pOctreeNodes;
    boost::shared_ptr<std::vector<uint32_t>> m_pOctreeNodeIndices;
    //boost::shared_ptr<std::vector<OctreeDrawInfo>> m_pOctreeDrawInfo;

    Chunk(AABB& bounds, ChunkManager* pChunkManager);

    Chunk(float3& basePos, std::size_t gridX, std::size_t gridY, std::size_t gridZ, float scale, ChunkManager* pChunkManager, const AABB& ringBounds, boost::shared_ptr<Ring> p_ring);

    ~Chunk();

    void draw();

    std::vector<uint32_t> createLeafNodes();

    void buildTree(const int size, const float threshold);

    void createVertices();
    void createMesh();

    void classifyEdges();
    void generateZeroCross();

    void generateZeroCrossings();
    void generateDensities();

    void generateFullEdges();

    boost::shared_ptr<GfxApi::Mesh> getMeshPtr();


    uint32_t createOctree(std::vector<uint32_t>& leaf_indices, const float threshold, bool simplify);


    uint32_t simplifyOctree(uint32_t node_index, float threshold, OctreeNode* root);



};

#endif