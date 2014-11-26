#ifndef _CHUNKMANAGER_H
#define _CHUNKMANAGER_H

#include <vector>
#include <list>
#include <set>

#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include "TOctree.h"
#include "TQueueLocked.h"

#include "mgl/MathGeoLib.h"



class Chunk;

namespace GfxApi {
class ShaderProgram;
}

struct ocl_t;

class ChunkManager
{
public:
    ChunkManager(void);
    ~ChunkManager(void);

    typedef TOctree<boost::shared_ptr<Chunk>> ChunkTree;

    static const int CHUNK_SIZE = 32;
    static const int MAX_LOD_LEVEL = 16;

    boost::shared_ptr<ocl_t> m_ocl;

    static const int WORLD_BOUNDS_MIN_XZ = -16000;
    static const int WORLD_BOUNDS_MIN_Y = -16000;
    static const int WORLD_BOUNDS_MAX_XZ = 16000;
    static const int WORLD_BOUNDS_MAX_Y = 16000;

    void render(void);

    void initTree(ChunkTree& pChild);
    void initTree1(ChunkTree& pChild);

    bool allChildsGenerated(ChunkTree& pChild);

    void updateVisibles(ChunkTree& pTree);

    void updateLoDTree(Frustum& camera);

    bool isAcceptablePixelError(float3& cameraPos, ChunkTree& tree);

    void chunkLoaderThread();

    void renderBounds(const Frustum& cameraPos);

    TQueueLocked<boost::shared_ptr<Chunk>> m_chunkNoiseGeneratorQueue;
    TQueueLocked<boost::shared_ptr<Chunk>> m_chunkVertexGeneratorQueue;

//private:
    std::vector< boost::shared_ptr<Chunk> > m_chunkList;

    std::vector< boost::shared_ptr<GfxApi::ShaderProgram>> m_shaders;
    boost::shared_ptr<GfxApi::ShaderProgram> m_pLastShader;

    boost::scoped_ptr< ChunkTree > m_pOctTree;
    typedef std::set< boost::shared_ptr<Chunk> > VisibleList;
    VisibleList m_visibles;

};


#endif

