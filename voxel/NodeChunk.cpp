#include "NodeChunk.h"
#include <assert.h>
#include "ChunkManager.h"
#include "../ocl.h"
#include "../MainClass.h"

#include "../cubelib/cube.hpp"

NodeChunk::NodeChunk( float scale, const AABB& bounds, ChunkManager* pChunkManager, bool hasNodes )
   : m_scale( scale )
   , m_chunk_bounds( bounds )
   , m_pChunkManager( pChunkManager )
   , m_workInProgress( nullptr )
   , m_bHasNodes( false )
   , m_bGenerated( false )
  // , m_bGenerationStarted( false )
{
   m_scale = ( ( m_chunk_bounds.MaxX() - m_chunk_bounds.MinX() ) / ( ChunkManager::CHUNK_SIZE ) );
}


NodeChunk::~NodeChunk()
{
   //std::cout << "destructor____";
}


void NodeChunk::generateDensities()
{

   int tree_level = m_pTree->getLevel();

   // size of the border to be generated
   int borderSize = 2;
   m_pChunkManager->m_ocl->density3d( ChunkManager::CHUNK_SIZE + borderSize,
                                      m_chunk_bounds.minPoint.x,
                                      m_chunk_bounds.minPoint.y,
                                      m_chunk_bounds.minPoint.z,
                                      m_scale,
                                      tree_level );
}

void NodeChunk::classifyBlocks()
{
   boost::shared_ptr< std::vector<cl_block_info_t> > compactedBlocks = boost::make_shared <std::vector<cl_block_info_t>>();
   m_pChunkManager->m_ocl->classifyBlocks( ChunkManager::CHUNK_SIZE + 2, *compactedBlocks, 2 );

   if( compactedBlocks && compactedBlocks->size() > 0 )
   {
      m_bHasNodes = true;
      m_compactedBlocks = compactedBlocks;
   }
}

boost::shared_ptr<GfxApi::Mesh> NodeChunk::getMeshPtr()
{
   return m_pMesh;
}


void NodeChunk::createMesh()
{
   //if( m_vertices.size() < 1 || m_indices.size() < 1 )
   //   return;

   if( m_compactedBlocks->size() < 1 )
      return;

   float size = 1;
   int i = 0;
   for( auto vertex : *m_compactedBlocks )
   {
      float x = vertex.grid_pos[0];
      float y = vertex.grid_pos[1];
      float z = vertex.grid_pos[2];

      //float size = m_scale / ChunkManager::CHUNK_SIZE / 100;

      if( vertex.block_info & 4 )
      {

         m_vertices.push_back( VertexPosition( x + size,   y - size,  z + size ) );
         m_vertices.push_back( VertexPosition( x,          y - size,  z + size ) );
         m_vertices.push_back( VertexPosition( x + size,   y,         z + size ) );
         m_vertices.push_back( VertexPosition( x,          y,         z + size ) );
      }
      if( vertex.block_info & 8 )
      {
         m_vertices.push_back( VertexPosition( x + size,        y - size, z ) );
         m_vertices.push_back( VertexPosition( x , y - size, z ) );
         m_vertices.push_back( VertexPosition( x + size,        y,        z ) );
         m_vertices.push_back( VertexPosition( x , y,        z ) );
      }
      if( vertex.block_info & 1 )
      {
         m_vertices.push_back( VertexPosition( x,        y,        z ) );
         m_vertices.push_back( VertexPosition( x + size, y,        z ) );
         m_vertices.push_back( VertexPosition( x,        y,        z + size ) );
         m_vertices.push_back( VertexPosition( x + size, y,        z + size ) );
      }
      if( vertex.block_info & 2 )
      {
         m_vertices.push_back( VertexPosition( x,        y - size, z + size ) );
         m_vertices.push_back( VertexPosition( x + size, y - size, z + size ) );
         m_vertices.push_back( VertexPosition( x,        y - size, z ) );
         m_vertices.push_back( VertexPosition( x + size, y - size, z ) );
      }
      //if( vertex.block_info & 16 )
      //{
      //   m_vertices.push_back( VertexPosition( x,        y - size, z ) );
      //   m_vertices.push_back( VertexPosition( x + size, y - size, z ) );
      //   m_vertices.push_back( VertexPosition( x,        y,        z ) );
      //   m_vertices.push_back( VertexPosition( x + size, y,        z ) );
      //}
      //if( vertex.block_info & 32 )
      //{
      //   m_vertices.push_back( VertexPosition( x + size,   y - size,  z + size ) );
      //   m_vertices.push_back( VertexPosition( x,          y - size,  z + size ) );
      //   m_vertices.push_back( VertexPosition( x + size,   y,         z + size ) );
      //   m_vertices.push_back( VertexPosition( x,          y,         z + size ) );
      //}
      

     // if( i > 900 )
     //    break;

      i++;
   }

   if( m_vertices.size() < 1 )
      return;
   
   GfxApi::VertexDeclaration decl;
   decl.Add( GfxApi::VertexElement( GfxApi::VertexDataSemantic::VCOORD, GfxApi::VertexDataType::FLOAT, 3, "vertex_position" ) );
   decl.Add( GfxApi::VertexElement( GfxApi::VertexDataSemantic::NORMAL, GfxApi::VertexDataType::FLOAT, 3, "vertex_normal" ) );
   decl.Add( GfxApi::VertexElement( GfxApi::VertexDataSemantic::TCOORD, GfxApi::VertexDataType::INT, 2, "vertex_material" ) );

   auto mesh = boost::make_shared<GfxApi::Mesh>( GfxApi::PrimitiveType::TriangleStrip );
   mesh->Bind();

   boost::shared_ptr<GfxApi::VertexBuffer> pVertexBuffer =
      boost::make_shared<GfxApi::VertexBuffer>( m_vertices.size(), decl, GfxApi::Usage::STATIC_DRAW );

  /* boost::shared_ptr<GfxApi::IndexBuffer> pIndexBuffer =
      boost::make_shared<GfxApi::IndexBuffer>( mesh, m_indices.size(), GfxApi::PrimitiveIndexType::Indices32Bit, GfxApi::Usage::STATIC_DRAW );
*/
   float * vbPtr = reinterpret_cast< float* >( pVertexBuffer->CpuPtr() );
  // unsigned int * ibPtr = reinterpret_cast< unsigned int* >( pIndexBuffer->CpuPtr() );

   uint32_t offset = 0;

   for( auto& vertexInf : m_vertices )
   {
      vbPtr[offset + 0] = vertexInf.px;
      vbPtr[offset + 1] = vertexInf.py;
      vbPtr[offset + 2] = vertexInf.pz;
      vbPtr[offset + 3] = 1;
      vbPtr[offset + 4] = 1;
      vbPtr[offset + 5] = 1;
   //   *( int* ) ( &vbPtr[offset + 6] ) = 1;
   //   *( int* ) ( &vbPtr[offset + 7] ) = 0;
      offset += 8;

     /* if( vertexInf.m_material == 0 )
      {
         continue;
      }*/
   }

   pVertexBuffer->Bind();
   pVertexBuffer->UpdateToGpu();
   offset = 0;
   
 /*  for( auto& idx : m_indices )
   {
      ibPtr[offset++] = idx;
   }

   pIndexBuffer->Bind();
   pIndexBuffer->UpdateToGpu();*/

   mesh->vbs.push_back( pVertexBuffer );
   //mesh->ib.swap( pIndexBuffer );

   mesh->sp = m_pChunkManager->m_shaders[0];

   mesh->GenerateVAO();
   mesh->LinkShaders();

   m_pMesh = mesh;

   m_vertices.clear();
   m_indices.clear();

   m_compactedBlocks.reset();

}



void NodeChunk::generate( float threshold )
{

 /*  std::vector< OctreeNodeMdc > octreeNodes( 40000 );
   m_nodeIdxCurr = 1;
   ConstructBase( &octreeNodes[0], ChunkManager::CHUNK_SIZE, octreeNodes );

   ( &octreeNodes[0] )->ClusterCellBase( threshold, octreeNodes );

   ( &octreeNodes[0] )->GenerateVertexBuffer( m_mdcVertices, octreeNodes );

   ( &octreeNodes[0] )->ProcessCell( m_indices, m_tri_count, threshold, octreeNodes );

   m_tri_count.clear();
   m_pCellBuffer.reset();
   m_compactCorners.reset();
   m_edgesCompact.reset();
   m_zeroCrossCompact.reset();*/
}
