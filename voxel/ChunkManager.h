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
class Ring;
class Clipmap;

static const float LOD_MIN_RES = 1.0f;

namespace GfxApi {
class ShaderProgram;
}

struct ocl_t;

class ChunkManager
{
public:
    ChunkManager(Frustum& camera);
    ~ChunkManager(void);

    static const int CHUNK_SIZE = 32;
    static const int MAX_LOD_LEVEL = 7;
    

    boost::shared_ptr<ocl_t> m_ocl;

    void render(void);

    void chunkLoaderThread();

    void resetClipmap(Frustum& camera);

    TQueueLocked<boost::shared_ptr<Chunk>> m_chunkGeneratorQueue;

    TQueueLocked<boost::shared_ptr<Ring>> m_ring_generator_queue;

    std::vector< boost::shared_ptr<Chunk> > m_chunkList;

    std::vector< boost::shared_ptr<GfxApi::ShaderProgram>> m_shaders;

    boost::shared_ptr<GfxApi::ShaderProgram> m_pLastShader;

    boost::shared_ptr<Clipmap> m_pClipmap;

};


#endif

