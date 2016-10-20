#ifndef _CHUNKMANAGER_H
#define _CHUNKMANAGER_H

#include <vector>
#include <list>
#include <set>

#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "common.h"
#include "mgl/MathGeoLib.h"
#include "TOctree.h"
#include "TQueueLocked.h"
#include "TVolume3d.h"
#include "octreemdc.h"

class NodeChunk;

namespace GfxApi {
struct ShaderProgram;
}

struct ocl_t;

class ChunkManager
{
public:
   ChunkManager( Frustum& camera );
   ~ChunkManager( void );

   static const int CHUNK_SIZE = 32;
   static const int MAX_LOD_LEVEL = 10;

   static const int WORLD_BOUNDS_MIN_XZ = -512000;
   static const int WORLD_BOUNDS_MIN_Y = -512000;
   static const int WORLD_BOUNDS_MAX_XZ = 512000;
   static const int WORLD_BOUNDS_MAX_Y = 512000;

   typedef TOctree< boost::shared_ptr< NodeChunk > > ChunkTree;

   typedef std::set< boost::shared_ptr< NodeChunk > > NodeChunks;

   boost::shared_ptr < TVolume3d<cell_t*> > m_pCellBuffer;

   boost::shared_ptr< ChunkTree > m_pOctTree;

   NodeChunks m_visibles;

   NodeChunks m_visiblesTemp;

   typedef std::vector< std::set< boost::shared_ptr< NodeChunk > > > LODScene;

   /// scene currently being drawn
   boost::shared_ptr< LODScene > m_scene_current;

   /// next scene in generation
   boost::shared_ptr< LODScene > m_scene_next;

   /// create a mew sceme for the world boundaries given
   boost::shared_ptr< LODScene > createScene( const AABB& world_bounds );

   /// chunks pending generation or in generation process
   uint32_t m_chunks_pending;

   void initTree( ChunkTree& pChild );
   void initTree( boost::shared_ptr<ChunkTree> pChild );
   void initTreeNoQueue( boost::shared_ptr<ChunkTree> pChild );

   boost::shared_ptr< ocl_t > m_ocl;

   void render( void );

   void chunkLoaderThread();

   void chunkContourThread();

   bool isAcceptablePixelError( float3& cameraPos, ChunkTree& tree );

   void updateLoDTree( Frustum& camera );

   void updateLoDTree2( Frustum& camera );


   TQueueLocked<boost::shared_ptr< NodeChunk > > m_nodeChunkClassifyQueue;

   TQueueLocked<boost::shared_ptr< NodeChunk > > m_nodeChunkGeneratorQueue;

   TQueueLocked<boost::shared_ptr< NodeChunk > > m_nodeChunkMeshGeneratorQueue;

   TQueueLocked<boost::shared_ptr< NodeChunk > > m_nodeChunkContourGeneratorQueue;

   std::vector< boost::shared_ptr< GfxApi::ShaderProgram > > m_shaders;

   boost::shared_ptr< GfxApi::ShaderProgram > m_pLastShader;

   std::vector< VertexPositionNormal> m_mdcVertices;

};


#endif

