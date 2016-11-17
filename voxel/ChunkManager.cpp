#include "ChunkManager.h"

#include "NodeChunk.h"
#include "utilities.h"
#include "octreemdc.h"

#include <boost/make_shared.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/chrono.hpp>

//#include "../cubelib/cube.hpp"

#include <minmax.h>
#include <deque>

#include "../GfxApi.h"

#include <set>

#include "../ocl.h"

ChunkManager::ChunkManager( Frustum& camera )
   : m_chunks_pending( 0 )
{

   m_pCellBuffer = boost::make_shared<TVolume3d<cell_t*>>( ChunkManager::CHUNK_SIZE, ChunkManager::CHUNK_SIZE, ChunkManager::CHUNK_SIZE );

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

   //OctreeNodeMdc tree;
   //std::vector<Vertex> vertices;

   //tree.ConstructBase( 64, 0.1f, vertices );
   //tree.ClusterCellBase( 0.1f );

   //
   //tree.GenerateVertexBuffer( m_mdcVertices );
   //tree.position = float3( 0, 0, 0 );

}

boost::shared_ptr<ChunkManager::LODScene> ChunkManager::createScene( const AABB& world_bounds )
{
   boost::shared_ptr<NodeChunk> pChunk = boost::make_shared<NodeChunk>( 1.0f, world_bounds, this, false );

   m_pOctTree.reset( new ChunkTree( nullptr, nullptr, pChunk, 1, cube::corner_t::get( 0, 0, 0 ) ) );

   pChunk->m_pTree = m_pOctTree;

   m_pOctTree->split();

   auto scene = boost::make_shared <LODScene>( ChunkManager::MAX_LOD_LEVEL + 6 );

   for( auto& corner : cube::corner_t::all() )
   {
      uint8_t level = m_pOctTree->getChild( corner )->getLevel();
      m_chunks_pending++;
      initTree( m_pOctTree->getChild( corner ) );
      ( *scene )[level].insert( m_pOctTree->getChild( corner )->getValue() );
      m_visibles.insert( m_pOctTree->getChild( corner )->getValue() );
   }

   return scene;
}

void ChunkManager::initTree( ChunkTree&  pChild )
{
   boost::shared_ptr<NodeChunk>& pChunk = pChild.getValue();


   AABB bounds = pChild.getParent()->getValue()->m_chunk_bounds;

   float3 center = bounds.CenterPoint();
   float3 c0 = bounds.CornerPoint( pChild.getCorner().index() );

   AABB b0( float3( std::min( c0.x, center.x ),
                    std::min( c0.y, center.y ),
                    std::min( c0.z, center.z ) ),
            float3( std::max( c0.x, center.x ),
                    std::max( c0.y, center.y ),
                    std::max( c0.z, center.z ) ) );

   pChunk = boost::make_shared<NodeChunk>( 1.0f / pChild.getLevel(), b0, this );

   //pChunk->m_pTree = &pChild;

   pChunk->m_workInProgress = boost::make_shared<bool>( true );

   m_nodeChunkGeneratorQueue.push( pChild.getValueCopy() );

}

void ChunkManager::initTree( boost::shared_ptr<ChunkTree> pChild )
{
   boost::shared_ptr<NodeChunk>& pChunk = pChild->getValue();


   AABB bounds = pChild->getParent()->getValue()->m_chunk_bounds;

   float3 center = bounds.CenterPoint();

   float3 c0 = bounds.CornerPoint( pChild->getCorner().index() );

   // @TODO this is still not working right on the Y axis...
   AABB b0( float3( std::min( c0.x, center.x ) + pChild->getParent()->getValue()->m_scale / 4,
                    std::min( c0.y, center.y ) + pChild->getParent()->getValue()->m_scale / 4,
                    std::min( c0.z, center.z ) + pChild->getParent()->getValue()->m_scale / 4 ),
            float3( std::max( c0.x, center.x ) + pChild->getParent()->getValue()->m_scale / 4,
                    std::max( c0.y, center.y ) + pChild->getParent()->getValue()->m_scale / 4,
                    std::max( c0.z, center.z ) + pChild->getParent()->getValue()->m_scale / 4 ) );

   pChunk = boost::make_shared<NodeChunk>( 1.0f / pChild->getLevel(), b0, this, false );

   pChunk->m_pTree = pChild;

   pChunk->m_workInProgress = boost::make_shared<bool>( true );

   m_nodeChunkGeneratorQueue.push( pChild->getValueCopy() );

}

void ChunkManager::initTreeNoQueue( boost::shared_ptr<ChunkTree> pChild )
{
   boost::shared_ptr<NodeChunk>& pChunk = pChild->getValue();


   AABB bounds = pChild->getParent()->getValue()->m_chunk_bounds;

   float3 center = bounds.CenterPoint();

   float3 c0 = bounds.CornerPoint( pChild->getCorner().index() );

   // @TODO this is still not working right on the Y axis...
   AABB b0( float3( std::min( c0.x, center.x ) + pChild->getParent()->getValue()->m_scale / 4,
                    std::min( c0.y, center.y ) + pChild->getParent()->getValue()->m_scale / 4,
                    std::min( c0.z, center.z ) + pChild->getParent()->getValue()->m_scale / 4 ),
            float3( std::max( c0.x, center.x ) + pChild->getParent()->getValue()->m_scale / 4,
                    std::max( c0.y, center.y ) + pChild->getParent()->getValue()->m_scale / 4,
                    std::max( c0.z, center.z ) + pChild->getParent()->getValue()->m_scale / 4 ) );

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
   float f_0 = x_0 * 1.9f;

   float3 delta = cameraPos - box.CenterPoint();
   //Node distance from camera
   //float d = std::max(f_0, box.CenterPoint().Distance(cameraPos));
   float d = std::max( f_0, std::max( abs( delta.x ), std::max( abs( delta.y ), abs( delta.z ) ) ) );

   //Minimum optimal node level
   float n_opt = ( float ) MAX_LOD_LEVEL - math::Log2( d / f_0 );

   float size_opt = rootBounds.Size().Length() / math::Pow( 2, n_opt );

   return size_opt > box.Size().Length();
}


void ChunkManager::updateLoDTree( Frustum& camera )
{

   /// get the next chunk off the queue that needs its mesh created / uploaded
   for( std::size_t i = 0; i < 2; ++i )
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
      //std::cout << m_chunks_pending << "\n";
      m_chunks_pending--;
   }

   /// see if there is a scene pending and ready
   if( m_scene_next && ( m_chunks_pending == 0 ) )
   {
      m_scene_current = m_scene_next;
      m_scene_next.reset();
      return;
   }
   else if( m_scene_next && ( m_chunks_pending > 0 ) )
   {
      /// a scene is pending generation, step out here
      return;
   }

   NodeChunks::iterator l = m_visibles.begin();
   /// check if a new scene needs to be generated
   bool needs_update = false;
   while( l != m_visibles.end() )
   {
      ChunkTree* visible = ( *l )->m_pTree.get();
      ChunkTree* parent = visible->getParent();

      bool parent_acceptable_error = !parent->isRoot() && isAcceptablePixelError( camera.pos, *parent );
      bool acceptable_error = isAcceptablePixelError( camera.pos, *visible );

      if( parent_acceptable_error || !acceptable_error )
      {
         needs_update = true;
         break;
      }
      ++l;
   }

   /// check if we need to generate a new scene
   if( !needs_update )
   {
      /// nothing to be done
      return;
   }

   /// create a new empty scene
   auto scene_next = boost::make_shared <LODScene>( ChunkManager::MAX_LOD_LEVEL + 6 );

   std::deque< boost::shared_ptr< NodeChunk > > chunkQueue;

   NodeChunks::iterator w = m_visibles.begin();

   while( w != m_visibles.end() )
   {
      chunkQueue.push_back( *w );
      ++w;
   }

   w = m_visibles.begin();

   m_visibles.clear();

   while( chunkQueue.size() > 0 )
   {
      auto chunk = chunkQueue.front();
      chunkQueue.pop_front();

      ChunkTree* leaf = chunk->m_pTree.get();
      ChunkTree* parent = leaf->getParent();

      bool parent_acceptable_error = !parent->isRoot() && isAcceptablePixelError( camera.pos, *parent );
      bool acceptable_error = isAcceptablePixelError( camera.pos, *leaf );

      if( parent_acceptable_error )
      {
         uint8_t parent_level = parent->getLevel();
         /// Add the parent to visibles
         m_visibles.insert( parent->getValue() );
         ( *scene_next )[parent_level].insert( parent->getValue() );

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
      }
      else if( acceptable_error )
      {
         /// Let things stay the same
         m_visibles.insert( leaf->getValue() );
         ( *scene_next )[leaf->getLevel()].insert( leaf->getValue() );
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
               for( auto& corner : cube::corner_t::all() )
               {
                  
                  uint8_t level = leaf->getChild( corner )->getLevel();
                  //m_visibles.insert( leaf->getChild( corner )->getValue() );
                  //initTreeNoQueue( leaf->getChild( corner ) );
                  ////m_chunks_pending++;
                  //chunkQueue.push_back( leaf->getChild( corner )->getValue() );
                  initTree( leaf->getChild( corner ) );
                  m_chunks_pending++;
                  ( *scene_next )[level].insert( leaf->getChild( corner )->getValue() );
                  m_visibles.insert( leaf->getChild( corner )->getValue() );
               }
            }

            /// Foreach child of visible
            //for( auto& corner : cube::corner_t::all() )
            //{
            //   uint8_t level = leaf->getChild( corner )->getLevel();
            //   m_visibles.insert( leaf->getChild( corner )->getValue() );
            //   if( isAcceptablePixelError( camera.pos, *leaf->getChild( corner ) ) )
            //   {

            //      //if( leaf->getValue()->m_occupation[corner.x_i()][corner.y_i()][corner.z_i()] )
            //      {
            //         initTree( leaf->getChild( corner ) );
            //         m_chunks_pending++;
            //         ( *scene_next )[level].insert( leaf->getChild( corner )->getValue() );
            //      }

            //   }
            //   else
            //   {
            //      int test = 0;
            //      //
            //   }

            //}
         }
      }


         
   }

   //// loop through the current visible nodes, act accordingly
   //while( w != m_visibles.end() )
   //{
   //   if( !( *w )->m_pTree )
   //   {
   //      ++w;
   //      continue;
   //   }

   //   ChunkTree* leaf = ( *w )->m_pTree.get();
   //   ChunkTree* parent = leaf->getParent();
   //   // If this is the root node, just move on.
   //   if( !parent )
   //   {
   //      ++w;
   //      continue;
   //   }


   //   bool parent_acceptable_error = !parent->isRoot() && isAcceptablePixelError( camera.pos, *parent );
   //   bool acceptable_error = isAcceptablePixelError( camera.pos, *leaf );

   //   if( parent_acceptable_error )
   //   {
   //      uint8_t parent_level = parent->getLevel();
   //      /// Add the parent to visibles
   //      m_visibles.insert( parent->getValue() );
   //      ( *scene_next )[parent_level].insert( parent->getValue() );

   //      if( leaf->hasChildren() )
   //      {
   //         for( auto& corner : cube::corner_t::all() )
   //         {
   //            if( leaf->getChild( corner )->hasChildren() )
   //            {
   //               for( auto& c_corner : cube::corner_t::all() )
   //               {
   //                  auto c_child = leaf->getChild( corner )->getChild( c_corner );
   //                  if( c_child->getValue() )
   //                     c_child->getValue()->m_pTree.reset();
   //               }
   //               leaf->getChild( corner )->join();
   //            }
   //         }
   //      }

   //      std::set< boost::shared_ptr<NodeChunk> >::iterator e = w;
   //      ++w;
   //      m_visibles.erase( e );
   //      continue;

   //   }
   //   else if( acceptable_error )
   //   {
   //      /// Let things stay the same
   //      ( *scene_next )[leaf->getLevel()].insert( leaf->getValue() );
   //   }
   //   else
   //   {
   //      //if( leaf->getValue()->m_bHasNodes )
   //      {

   //         /// If visible doesn't have children
   //         if( !leaf->hasChildren() )
   //         {
   //            /// Create children for visible
   //            leaf->split();
   //            for( auto& corner : cube::corner_t::all() )
   //            {
   //               initTreeNoQueue( leaf->getChild( corner ) );
   //               //m_chunks_pending++;
   //            }
   //         }

   //         /// Foreach child of visible
   //         for( auto& corner : cube::corner_t::all() )
   //         {
   //            uint8_t level = leaf->getChild( corner )->getLevel();
   //            m_visibles.insert( leaf->getChild( corner )->getValue() );
   //            if( isAcceptablePixelError( camera.pos, *leaf->getChild( corner ) ) )
   //            {

   //               initTree( leaf->getChild( corner ) );
   //               m_chunks_pending++;
   //               ( *scene_next )[level].insert( leaf->getChild( corner )->getValue() );
   //               
   //            }

   //         }

   //         /// Remove visible from visibles
   //         std::set< boost::shared_ptr<NodeChunk> >::iterator e = w;
   //         ++w;
   //         m_visibles.erase( e );
   //         continue;

   //      }
   //   }
   //   ++w;
   //}

   /// finally set the next scene to the new scene
   m_scene_next = scene_next;

}

void ChunkManager::updateLoDTree2( Frustum& camera )
{

   /// get the next chunk off the queue that needs its mesh created / uploaded
   for( std::size_t i = 0; i < 4; ++i )
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

         if( chunkRef && chunkRef->getValue() && chunkRef->getValue()->m_bGenerated == false )
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


int toint2( uint8_t x, uint8_t y, uint8_t z )
{
   int index = 0;
   ( ( char* ) ( &index ) )[0] = x;
   ( ( char* ) ( &index ) )[1] = y;
   ( ( char* ) ( &index ) )[2] = z;
   return index;
}

uint32_t ChunkManager::createOctree( std::vector< uint32_t >& leaf_indices,
                                     std::vector< OctreeNodeMdc >& node_list,
                                     const float threshold,
                                     int32_t& node_counter )
{

   int index = 0;

   while( leaf_indices.size() > 1 )
   {

      std::vector< uint32_t > tmp_idx_vec;
      std::unordered_map< int, uint32_t > tmp_map;
      for( auto& nodeIdx : leaf_indices )
      {
         auto& node = node_list[nodeIdx];
         OctreeNodeMdc* new_node = nullptr;

         boost::array<int, 3> pos;

         int level = node.m_size * 2;

         uint16_t node_min_x = node.m_gridX;
         uint16_t node_min_y = node.m_gridY;
         uint16_t node_min_z = node.m_gridZ;

         pos[0] = ( node_min_x / level ) * level;
         pos[1] = ( node_min_y / level ) * level;
         pos[2] = ( node_min_z / level ) * level;

         auto& it = tmp_map.find( toint2( pos[0], pos[1], pos[2] ) );
         if( it == tmp_map.end() )
         {
            uint32_t index = node_counter;
            if( node_counter >= node_list.size() )
               node_list.push_back( OctreeNodeMdc() );

            new_node = &node_list[node_counter++];
            new_node->m_type = NodeType::Internal;
            new_node->m_size = level;
            new_node->m_gridX = pos[0];
            new_node->m_gridY = pos[1];
            new_node->m_gridZ = pos[2];
            tmp_map[toint2( pos[0], pos[1], pos[2] )] = index;

            tmp_idx_vec.push_back( index );
         }
         else
         {
            new_node = &node_list[it->second];
         }

         boost::array<int, 3> diff;
         diff[0] = pos[0] - node_min_x;
         diff[1] = pos[1] - node_min_y;
         diff[2] = pos[2] - node_min_z;

         float3 corner_pos = float3( diff[0] ? 1 : 0, diff[1] ? 1 : 0, diff[2] ? 1 : 0 );

         uint8_t idx = ( ( uint8_t ) corner_pos.x << 2 ) | ( ( uint8_t ) corner_pos.y << 1 ) | ( ( uint8_t ) corner_pos.z );

         new_node->m_childIndex[idx] = nodeIdx;

      }
      leaf_indices.swap( tmp_idx_vec );
   }


   if( leaf_indices.size() == 0 )
      return -1;

   return leaf_indices[0];

}

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

      std::size_t max_chunks_per_frame = 5;

      for( std::size_t i = 0; i < max_chunks_per_frame; ++i )
      {

         auto& chunk = m_nodeChunkClassifyQueue.pop();

         if( !chunk )
         {
            continue;
         }

         auto parent = chunk->m_pTree->getParent();

         if( !parent->isRoot() && !parent->getValue()->m_bClassified )
         {
            m_nodeChunkClassifyQueue.push( parent->getValue() );
            m_nodeChunkClassifyQueue.push( chunk );
            continue;
         }

         if( !parent->isRoot() && parent->getValue()->m_bClassified && !parent->getValue()->m_bHasNodes )
         {
            chunk->m_bHasNodes = false;
            continue;
         }

         chunk->generateDensities();
         chunk->classifyEdges();

         chunk->m_bClassified = true;

      }


      for( std::size_t i = 0; i < max_chunks_per_frame; ++i )
      {
         auto& chunk = m_nodeChunkGeneratorQueue.pop();

         if( !chunk )
            continue;

         auto parent = chunk->m_pTree->getParent();

         chunk->generateDensities();
         chunk->classifyEdges();

         chunk->m_bClassified = true;

         if( chunk->m_bHasNodes )
         {
            chunk->generateZeroCross();
            chunk->generateFullEdges();
         }

         if( !chunk->m_bHasNodes )
         {
            chunk->m_edgesCompact.reset();
            chunk->m_workInProgress.reset();
            chunk->m_pCellBuffer.reset();
            did_some_work = true;
            m_nodeChunkMeshGeneratorQueue.push( chunk );
            continue;
         }

         m_nodeChunkContourGeneratorQueue.push( chunk );

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

      std::size_t max_chunks_per_frame = 5;

      for( std::size_t i = 0; i < max_chunks_per_frame; ++i )
      {

         auto& chunk = m_nodeChunkContourGeneratorQueue.pop();

         if( !chunk )
            continue;

         assert( chunk->m_zeroCrossCompact != 0 );
         
         chunk->generate( -1.0f );

         chunk->m_edgesCompact.reset();

         chunk->m_zeroCrossCompact.reset();
         chunk->m_workInProgress.reset();

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


ChunkManager::~ChunkManager( void )
{
}


void ChunkManager::render( void )
{

}