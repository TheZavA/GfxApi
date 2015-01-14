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

class ChunkManager;

struct zeroCrossings_t
{
   	float3 positions[6];
    float3 normals[6];
	int edgeCount;
};

struct corner_cl_t
{
   	float mins[3];
	int corners;
};

struct zeroCrossings_cl_t
{
   	float positions[6][3];
    float normals[6][3];
	int edgeCount;
};

struct cl_float3_t
{
   	float x;
    float y;
    float z;
};

class Chunk
{
private:
    float m_scale;

    ChunkManager* m_pChunkManager;

public:
    AABB m_bounds;

    bool m_bHasNodes;

    int m_leafCount;

    OctreeNode* m_treeRoot;

    boost::shared_ptr<IndexBuffer> m_pIndices;
    boost::shared_ptr<VertexBuffer> m_pVertices;

    Chunk(AABB& bounds, ChunkManager* pChunkManager);
    ~Chunk();

    void buildTree(const int size, const float threshold);
    void createVertices();
    void createMesh();

    void generateCorners();
    void generateZeroCrossings();
    void generateDensities();

    boost::shared_ptr<GfxApi::Mesh> m_pMesh;
    boost::shared_ptr<GfxApi::Mesh> getMeshPtr();

    boost::shared_ptr<TVolume3d<float>> m_densities3d;

    boost::shared_ptr<TVolume3d<corner_cl_t>> m_corners3d;
    boost::shared_ptr<std::vector<corner_cl_t>> m_cornersCompact;
    boost::shared_ptr<TVolume3d<zeroCrossings_cl_t>> m_zeroCrossingsCl3d;

    boost::shared_ptr<TOctree<boost::shared_ptr<Chunk>>> m_pTree;

    boost::shared_ptr<bool> m_workInProgress;
    boost::shared_ptr<bool> m_bLoadingDone;

    OctreeNode* createOctree(const float3& min, const int size, const float threshold);
    OctreeNode* constructLeaf(OctreeNode* leaf);
    OctreeNode* constructOctreeNodes(OctreeNode* node);
    OctreeNode* simplifyOctree(OctreeNode* node, float threshold, OctreeNode* root);




};

#endif