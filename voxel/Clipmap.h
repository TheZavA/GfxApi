#ifndef _CLIPMAP_H
#define _CLIPMAP_H

#include "../MainClass.h"
#include "TVolume3d.h"
#include "../cubelib/cube.hpp"
#include <boost/enable_shared_from_this.hpp>
#include "octree.h"
#include <map>

class ChunkManager;
class Chunk;

/////////////////////////////////////////////////////////////////

class Ring : public boost::enable_shared_from_this<Ring>
{
public:
    Ring(std::size_t k, float3 basePos, float scale, ChunkManager* pChunkManager, std::size_t level);
    ~Ring();

    void initializeChunks();

    boost::shared_ptr<Ring> move(cube::face_t face);

    boost::shared_ptr<Ring> copy_base();

    bool m_bGenerationInProgress;

    boost::shared_ptr<Ring> m_pQueuedRing;

    bool allChunksGenrated();

    void draw();

    uint32_t createOctree(std::vector<uint32_t>& leaf_indices, const float threshold, bool simplify);
	uint32_t simplifyOctree(uint32_t node_index, float threshold, OctreeNode* root);
    void collectSeamNodes();

    //OctreeNode* simplifyOctree(OctreeNode* node, float threshold, OctreeNode* root);

    void createSeamMesh();

    int toint(uint8_t x, uint8_t y, uint8_t z);

    //each ring will contain n X n chunks, with n = 2^k-1
    std::size_t m_k;
    std::size_t m_n;
    std::size_t m_m;

    std::size_t m_level;

    AABB m_ringBounds;

    AABB m_ring_bounds_inner;

    //basepos: the base position of the ring 
    float3 m_basePos;
    float3 m_centerPos;
    float m_scale;

    float m_len;

    boost::shared_ptr<Ring> m_pNextRing;
    boost::shared_ptr<Ring> m_pPrevRing;
    boost::shared_ptr<bool> m_seams_generated;

    boost::shared_ptr<bool> m_b_work_in_progress;

    std::vector<OctreeNode> m_seam_nodes;
    std::vector<uint32_t> m_seam_indices;

    typedef boost::shared_ptr<Chunk> ChunkPtr;

    boost::shared_ptr<TVolume3d<ChunkPtr>> m_pChunks;

    ChunkManager* m_pChunkManager;

    int32_t m_seam_tree_root_index;

    boost::shared_ptr<IndexBuffer> m_pIndices;
    boost::shared_ptr<VertexBuffer> m_pVertices;

    boost::shared_ptr<GfxApi::Mesh> m_pMesh;

    int32_t m_nodeIdxCurr;
    int32_t m_nodeDrInf;
    boost::shared_ptr<std::vector<OctreeNode>> m_pOctreeNodes;

};

class Clipmap
{
public:
    Clipmap(float3 center_pos, std::size_t max_level, float min_res, std::size_t k, ChunkManager* pChunkManager);
    ~Clipmap();

    void updateRings(Frustum& camera);

    void renderBounds(Frustum& camera);

    void draw(Frustum& camera, bool hideSeams, bool hideTerrain);

    float3          m_base_pos;
    std::size_t     m_max_level;
    float           m_min_res;
    std::size_t     m_k;

    boost::shared_ptr<Ring> m_rings;

    std::vector<boost::shared_ptr<Ring>> m_ring_list;



};

#endif