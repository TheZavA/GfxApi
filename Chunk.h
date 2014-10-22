#ifndef _CHUNK_H
#define _CHUNK_H

#include <boost\shared_ptr.hpp>
#include <boost/noncopyable.hpp>
#include "voxel/TVolume3d.h"
#include "voxel/TVolume2d.h"

#include <mgl/MathGeoLib.h>

#include <tuple>
#include <map>

#include "TOctree.h"


namespace GfxApi 
{
    class VertexBuffer;
    class IndexBuffer;
    class Mesh;
}

class ChunkManager;


typedef std::tuple<int, int, int> Vector3Int;
typedef std::pair< Vector3Int, Vector3Int > EdgeIndex;

typedef struct 
{

    float3 vertex;
    float3 normal;

} Vertex;

float3 LinearInterp(Vector3Int p1, float p1Val, Vector3Int p2, float p2Val,  float value);

class Chunk : boost::noncopyable
{
public:
    Chunk(AABB bounds, double scale,ChunkManager* pChunkManager);
    ~Chunk(void);

    void render(void);

    void generateTerrain(void);

    void generateVertices(void);

    void generateMesh(void);

    void generateBaseMap(void);

    boost::shared_ptr<GfxApi::Mesh> m_pMesh;

    AABB m_bounds;

    double m_scale;

    TOctree<boost::shared_ptr<Chunk>>* m_pTree;

    boost::shared_ptr<bool> m_workInProgress;

    boost::shared_ptr<TVolume3d<float>> m_blockVolumeFloat;

    boost::shared_ptr<TVolume2d<float>> m_baseMap;
    boost::shared_ptr<TVolume2d<float>> m_slopeMap;

    boost::shared_ptr<std::vector<Vertex>> m_vertexData;
    boost::shared_ptr<std::vector<int>> m_indexData;

private:



    ChunkManager* m_pChunkManager;

    uint32_t getOrCreateVertex(float3& vertex, std::vector<Vertex>& tmpVectorList, EdgeIndex& idx, std::map<EdgeIndex, uint32_t>& vertexMap);

    EdgeIndex makeEdgeIndex(Vector3Int& vertA, Vector3Int& vertB);

};


#endif

