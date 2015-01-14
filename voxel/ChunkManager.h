#ifndef _CHUNKMANAGER_H
#define _CHUNKMANAGER_H

#include <vector>
#include <list>
#include <set>

#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>


#include "mgl/MathGeoLib.h"
#include "../TOctree.h"
#include "../TQueueLocked.h"


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
    static const int MAX_LOD_LEVEL = 15;

    boost::shared_ptr<ocl_t> m_ocl;

    void render(void);

    void initTree(boost::shared_ptr<ChunkTree> pChild);
    void initTree1(boost::shared_ptr<ChunkTree> pChild);
    void initTreeOnly(boost::shared_ptr<ChunkTree> pChild);

    bool allChildsGenerated(boost::shared_ptr<ChunkTree> pChild);

    void updateVisibles(boost::shared_ptr<ChunkTree> pTree);

    void updateLoDTree(Frustum& camera);

    bool isAcceptablePixelError(float3& cameraPos, ChunkTree& tree);

    void chunkLoaderThread();

    void renderBounds(const Frustum& cameraPos);

    typedef std::set< boost::shared_ptr<Chunk> > VisibleList;

    bool isBatchDone(VisibleList& batches);

    TQueueLocked<boost::shared_ptr<Chunk>> m_chunkGeneratorQueue;

    TQueueLocked<std::vector<boost::shared_ptr<Chunk>>> m_chunkGeneratorBatchQueue;

    TQueueLocked<std::vector<boost::shared_ptr<Chunk>>> m_chunkGeneratorBatchReturnQueue;

    std::vector< boost::shared_ptr<Chunk> > m_chunkList;

    std::vector< boost::shared_ptr<GfxApi::ShaderProgram>> m_shaders;

    boost::shared_ptr<GfxApi::ShaderProgram> m_pLastShader;

    boost::shared_ptr< ChunkTree > m_pOctTree;

    VisibleList m_visibles;
 



};


#endif

