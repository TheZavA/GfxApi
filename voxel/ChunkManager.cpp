#include "ChunkManager.h"

#include "NodeChunk.h"

#include <boost/make_shared.hpp>

#include <boost/shared_ptr.hpp>
#include <boost/chrono.hpp>

#include <minmax.h>
#include <deque>

#include "../GfxApi.h"

#include <set>

#include "../ocl.h"

ChunkManager::ChunkManager( Frustum& camera )
   : m_chunks_pending( 0 )
{
   m_ocl = boost::make_shared<ocl_t>( 0, 0 );
   std::vector<std::string> sources;


   sources.push_back( "scan_kernel.cl" );
   sources.push_back( "noisetest.cl" );
   sources.push_back( "chunk_kernel.cl" );

   m_ocl->load_cl_source( sources );


   boost::shared_ptr<GfxApi::Shader> vs = boost::make_shared<GfxApi::Shader>( GfxApi::ShaderType::VertexShader );
   vs->LoadFromFile( GfxApi::ShaderType::VertexShader, "shader/chunk.vs" );

   boost::shared_ptr<GfxApi::Shader> ps = boost::make_shared<GfxApi::Shader>( GfxApi::ShaderType::PixelShader );
   ps->LoadFromFile( GfxApi::ShaderType::PixelShader, "shader/chunk.ps" );

   auto sp = boost::make_shared<GfxApi::ShaderProgram>();
   sp->Attach( *vs );
   sp->Attach( *ps );

   boost::shared_ptr<GfxApi::Shader> vs1 = boost::make_shared<GfxApi::Shader>( GfxApi::ShaderType::VertexShader );
   vs1->LoadFromFile( GfxApi::ShaderType::VertexShader, "shader/line.vs" );

   boost::shared_ptr<GfxApi::Shader> ps1 = boost::make_shared<GfxApi::Shader>( GfxApi::ShaderType::PixelShader );
   ps1->LoadFromFile( GfxApi::ShaderType::PixelShader, "shader/line.ps" );

   auto sp1 = boost::make_shared<GfxApi::ShaderProgram>();
   sp1->Attach( *vs1 );
   sp1->Attach( *ps1 );

   m_shaders.push_back( sp );
   m_shaders.push_back( sp1 );

   std::thread chunkLoadThread( &ChunkManager::chunkLoaderThread, this );
   chunkLoadThread.detach();

   std::thread chunkContourThread( &ChunkManager::chunkContourThread, this );
   chunkContourThread.detach();

   //std::thread chunkLoadThread1(&ChunkManager::chunkLoaderThread, this);
   //chunkLoadThread1.detach();

   AABB rootBounds( float3( ( float ) camera.pos.x - WORLD_BOUNDS_MAX_XZ, ( float ) camera.pos.y - WORLD_BOUNDS_MAX_XZ, ( float ) camera.pos.z - WORLD_BOUNDS_MAX_XZ ),
                    float3( ( float ) WORLD_BOUNDS_MAX_XZ + ( float ) camera.pos.x, ( float ) WORLD_BOUNDS_MAX_Y + ( float ) camera.pos.y, ( float ) WORLD_BOUNDS_MAX_XZ + ( float ) camera.pos.z ) );

   m_scene_next = createScene( rootBounds );

}

boost::shared_ptr<ChunkManager::LODScene> ChunkManager::createScene( const AABB& world_bounds )
{
   boost::shared_ptr<NodeChunk> pChunk = boost::make_shared<NodeChunk>( 1.0f, world_bounds, this, false );

   m_pOctTree.reset( new ChunkTree( nullptr, nullptr, pChunk, 1, cube::corner_t::get( 0, 0, 0 ) ) );

   pChunk->m_pTree = m_pOctTree;

   m_pOctTree->split();

   for( auto& corner : cube::corner_t::all() )
   {
      uint8_t level = m_pOctTree->getChild( corner )->getLevel();
      m_chunks_pending++;
      initTree( m_pOctTree->getChild( corner ) );
      m_visibles.insert( m_pOctTree->getChild( corner )->getValue() );
   }

   return nullptr;
}

void ChunkManager::initTree( boost::shared_ptr<ChunkTree> pChild )
{
   boost::shared_ptr<NodeChunk>& pChunk = pChild->getValue();


   AABB bounds = pChild->getParent()->getValue()->m_chunk_bounds;

   float3 center = bounds.CenterPoint();

   float3 c0 = bounds.CornerPoint( pChild->getCorner().index() );

   // @TODO this is still not working right on the Y axis...
   AABB b0( float3( std::min( c0.x, center.x ) ,
                    std::min( c0.y, center.y ) ,
                    std::min( c0.z, center.z ) ),
            float3( std::max( c0.x, center.x ) ,
                    std::max( c0.y, center.y ) ,
                    std::max( c0.z, center.z ) ) );

   pChunk = boost::make_shared<NodeChunk>( 1.0f / pChild->getLevel(), b0, this, false );

   pChunk->m_pTree = pChild;

   pChunk->m_workInProgress = boost::make_shared< bool >( true );

   m_nodeChunkGeneratorQueue.push( pChild->getValueCopy() );

}

void ChunkManager::initTreeNoQueue( boost::shared_ptr<ChunkTree> pChild )
{
   boost::shared_ptr<NodeChunk>& pChunk = pChild->getValue();


   AABB bounds = pChild->getParent()->getValue()->m_chunk_bounds;

   float3 center = bounds.CenterPoint();

   float3 c0 = bounds.CornerPoint( pChild->getCorner().index() );

   // @TODO this is still not working right on the Y axis...
   AABB b0( float3( std::min( c0.x, center.x ),
                    std::min( c0.y, center.y )  ,
                    std::min( c0.z, center.z )   ),
            float3( std::max( c0.x, center.x )  ,
                    std::max( c0.y, center.y ) ,
                    std::max( c0.z, center.z )   ) );

   pChunk = boost::make_shared<NodeChunk>( 1.0f / pChild->getLevel(), b0, this, false );

   pChunk->m_pTree = pChild;

}

bool ChunkManager::isAcceptablePixelError( float3& cameraPos, ChunkTree& tree )
{
   const AABB& box = tree.getValue()->m_chunk_bounds;

   const AABB& rootBounds = tree.getRoot()->getValue()->m_chunk_bounds;

   //World length of highest LOD node
   float x_0 = rootBounds.Size().Length() / math::Pow( 2, ( float ) MAX_LOD_LEVEL );

   //All nodes within this distance will surely be rendered
   //float f_0 = x_0 * 1.9f;
   float f_0 = x_0 * 2.0f;

   float3 delta = cameraPos - box.CenterPoint();
   //Node distance from camera
   //float d = std::max(f_0, box.CenterPoint().Distance(cameraPos));
   float d = std::max( f_0, std::max( abs( delta.x ), std::max( abs( delta.y ), abs( delta.z ) ) ) );

   //Minimum optimal node level
   float n_opt = ( float ) MAX_LOD_LEVEL - math::Log2( d / f_0 );

   float size_opt = rootBounds.Size().Length() / math::Pow( 2, n_opt );

   return size_opt > box.Size().Length();
}

void ChunkManager::updateLoDTree2( Frustum& camera )
{

   /// get the next chunk off the queue that needs its mesh created / uploaded
   for( std::size_t i = 0; i < 10; ++i )
   {
      auto chunk = m_nodeChunkMeshGeneratorQueue.pop();
      if( !chunk )
      {
         continue;
      }

      chunk->m_bGenerated = true;

      
      if( chunk->m_bHasNodes )
      {
         chunk->createMesh();
      }
   }

   NodeChunks::iterator w = m_visibles.begin();

   // loop through the current visible nodes, act accordingly
   while( w != m_visibles.end() )
   {

      if( !( *w ) )
      {
         std::set< boost::shared_ptr<NodeChunk> >::iterator e = w;
         ++w;
         m_visibles.erase( e );
         continue;
      }

      if( !( *w )->m_pTree )
      {
         ++w;
         continue;
      }

      ChunkTree* leaf = ( *w )->m_pTree.get();
      ChunkTree* parent = leaf->getParent();

      // If this is the root node, just move on.
      if( !parent )
      {
         ++w;
         continue;
      }


      bool parent_acceptable_error = !parent->isRoot() && isAcceptablePixelError( camera.pos, *parent );
      bool acceptable_error = isAcceptablePixelError( camera.pos, *leaf );

      if( parent_acceptable_error )
      {
         uint8_t parent_level = parent->getLevel();
         /// Add the parent to visibles
         m_visibles.insert( parent->getValue() );

         if( leaf->hasChildren() )
         {
            for( auto& corner : cube::corner_t::all() )
            {
               if( leaf->getChild( corner )->hasChildren() )
               {
                  for( auto& c_corner : cube::corner_t::all() )
                  {
                     auto c_child = leaf->getChild( corner )->getChild( c_corner );
                     if( c_child->getValue() )
                        c_child->getValue()->m_pTree.reset();
                  }
                  leaf->getChild( corner )->join();
               }
            }
         }

         std::set< boost::shared_ptr<NodeChunk> >::iterator e = w;
         ++w;
         m_visibles.erase( e );
         continue;

      }
      else if( acceptable_error )
      {
         /// Let things stay the same
      }
      else
      {
         if( leaf->getValue()->m_bHasNodes )
         {

            /// If visible doesn't have children
            if( !leaf->hasChildren() )
            {
               /// Create children for visible
               leaf->split();
               /// Foreach child of visible
            }

            for( auto& corner : cube::corner_t::all() )
            {

               auto& chunkRef = leaf->getChild( corner );

               initTree( chunkRef );
               m_visibles.insert( chunkRef->getValue() );
               m_visiblesTemp.insert( leaf->getValue() );

            }
               
            std::set< boost::shared_ptr<NodeChunk> >::iterator e = w;
            ++w;
            m_visibles.erase( e );
            continue;


         }
      }
      ++w;
   }

   NodeChunks::iterator w1 = m_visiblesTemp.begin();

   // loop through the current visible nodes, act accordingly
   while( w1 != m_visiblesTemp.end() )
   {
      ChunkTree* leaf = ( *w1 )->m_pTree.get();
      ChunkTree* parent = leaf->getParent();

      // If this is the root node, just move on.
      if( !parent )
      {
         ++w1;
         continue;
      }

      bool allNodesGenerated = true;

      for( auto& corner : cube::corner_t::all() )
      {
         if( !leaf->m_children )
         {
            continue;
         }

         auto& chunkRef = leaf->getChild( corner );

         if( chunkRef && 
             chunkRef->getValue() && 
             chunkRef->getValue()->m_bGenerated == false )
         {
            allNodesGenerated = false;
         }
      }

      if( allNodesGenerated )
      {
         /// Remove visible from visibles

         std::set< boost::shared_ptr<NodeChunk> >::iterator e = w1;
         ++w1;
         m_visiblesTemp.erase( e );
         continue;
      }
      ++w1;
   }

}

uint64_t voxels = 0;
void ChunkManager::chunkLoaderThread()
{
   //this has to be in ms or something, not framerate, but time!
   //int frameRate = 60;
   typedef boost::chrono::high_resolution_clock Clock;
   typedef boost::chrono::milliseconds ms;
   //60 fps is ~15 ms / frame
   std::chrono::milliseconds frametime( 25 );
   bool did_some_work = false;
   while( 1 )
   {

      did_some_work = false;

      std::size_t max_chunks_per_frame = 30;

      for( std::size_t i = 0; i < max_chunks_per_frame; ++i )
      {
         auto& chunk = m_nodeChunkGeneratorQueue.pop();

         if( !chunk )
            continue;

         auto parent = chunk->m_pTree->getParent();

         if( !chunk->m_pTree )
            continue;

         //chunk->classifyChunk();

         //if( !chunk->m_bHasNodes )
         {
           // m_nodeChunkMeshGeneratorQueue.push( chunk );
            //continue;
         }
            
         chunk->generateDensities();
         chunk->classifyBlocks();
         if( chunk->m_compactedBlocks )
            voxels += chunk->m_compactedBlocks->size();
         chunk->generateVertices();
         chunk->generateLocalAO();
         m_nodeChunkMeshGeneratorQueue.push( chunk );

         did_some_work = true;

      }

      if( !did_some_work )
      {
         //we can afford to poll 10 X a frame or even more, thread will remain 100% idle, because of sleep.
         std::this_thread::sleep_for( frametime / 10 );
      }

   }

}

void ChunkManager::chunkContourThread()
{
   typedef boost::chrono::high_resolution_clock Clock;

   //this has to be in ms or something, not framerate, but time!
   //60 fps is ~15 ms / frame
   std::chrono::milliseconds frametime( 25 );
   bool did_some_work = false;
   while( 1 )
   {

      did_some_work = false;

      std::size_t max_chunks_per_frame = 30;

      for( std::size_t i = 0; i < max_chunks_per_frame; ++i )
      {

         auto& chunk = m_nodeChunkContourGeneratorQueue.pop();

         if( !chunk )
            continue;

         //chunk->generate( -1.0f );
         chunk->generateVertices();
         m_nodeChunkMeshGeneratorQueue.push( chunk );

         did_some_work = true;
      }

      if( !did_some_work )
      {
         //we can afford to poll 10 X a frame or even more, thread will remain 100% idle, because of sleep.
         std::this_thread::sleep_for( frametime / 5 );
      }


   }

}


ChunkManager::~ChunkManager( void )
{
}


void ChunkManager::render( void )
{

}