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

void NodeChunk::generateVertices()
{
   if( !m_compactedBlocks || m_compactedBlocks->size() < 1 )
      return;

   for( auto vertex : *m_compactedBlocks )
   {
      float x = vertex.grid_pos[0] - 1;
      float y = vertex.grid_pos[1] - 1;
      float z = vertex.grid_pos[2] - 1;

      for( int i = 0; i < 6; i++ )
      {
         uint8_t mask = 1 << i;
         if( vertex.block_info & mask )
         {
            int highestIndex = m_vertices.size();
            for( int c = 0; c < 4; c++ )
            {
               uint8_t x1 = faceList[i][c][0];
               uint8_t y1 = faceList[i][c][1];
               uint8_t z1 = faceList[i][c][2];

               uint8_t nx = faceList[i][5][0];
               uint8_t ny = faceList[i][5][1];
               uint8_t nz = faceList[i][5][2];

               m_vertices.push_back( cl_vertex_t( x + x1, y + y1, z + z1, nx, ny, nz ) );
            }

            m_indices.push_back( highestIndex );
            m_indices.push_back( highestIndex + 1 );
            m_indices.push_back( highestIndex + 2 );

            m_indices.push_back( highestIndex + 2 );
            m_indices.push_back( highestIndex + 3 );
            m_indices.push_back( highestIndex );

         }
      }
   }
   m_pChunkManager->m_ocl->calulateBlockAO( m_vertices.size(), m_vertices );
}

void NodeChunk::createMesh()
{


   if( m_vertices.size() < 1 )
      return;
   
   GfxApi::VertexDeclaration decl;
   decl.Add( GfxApi::VertexElement( GfxApi::VertexDataSemantic::VCOORD, GfxApi::VertexDataType::UNSIGNED_BYTE, 3, "vertex_position" ) );
   decl.Add( GfxApi::VertexElement( GfxApi::VertexDataSemantic::NORMAL, GfxApi::VertexDataType::BYTE, 3, "vertex_normal" ) );
   decl.Add( GfxApi::VertexElement( GfxApi::VertexDataSemantic::TCOORD, GfxApi::VertexDataType::UNSIGNED_BYTE, 1, "vertex_ao" ) );
   //decl.Add( GfxApi::VertexElement( GfxApi::VertexDataSemantic::TCOORD, GfxApi::VertexDataType::UNSIGNED_BYTE, 2, "vertex_material" ) );

   auto mesh = boost::make_shared<GfxApi::Mesh>( GfxApi::PrimitiveType::TriangleList );
   mesh->Bind();

   boost::shared_ptr<GfxApi::VertexBuffer> pVertexBuffer =
      boost::make_shared<GfxApi::VertexBuffer>( m_vertices.size(), decl, GfxApi::Usage::STATIC_DRAW );

   boost::shared_ptr<GfxApi::IndexBuffer> pIndexBuffer =
      boost::make_shared<GfxApi::IndexBuffer>( mesh, m_indices.size(), GfxApi::PrimitiveIndexType::Indices16Bit, GfxApi::Usage::STATIC_DRAW );

   uint8_t * vbPtr = reinterpret_cast< uint8_t* >( pVertexBuffer->CpuPtr() );
   uint16_t * ibPtr = reinterpret_cast< uint16_t* >( pIndexBuffer->CpuPtr() );

   uint32_t offset = 0;

   for( auto& vertexInf : m_vertices )
   {
      vbPtr[offset + 0] = vertexInf.px;
      vbPtr[offset + 1] = vertexInf.py;
      vbPtr[offset + 2] = vertexInf.pz;
      vbPtr[offset + 3] = vertexInf.nx;
      vbPtr[offset + 4] = vertexInf.ny;
      vbPtr[offset + 5] = vertexInf.nz;
      vbPtr[offset + 6] = vertexInf.localAO;
   //   *( int* ) ( &vbPtr[offset + 7] ) = 0;
      offset += 3;
      offset += 3;
      offset += 1;
      //offset += 2;

     /* if( vertexInf.m_material == 0 )
      {
         continue;
      }*/
   }

   pVertexBuffer->Bind();
   pVertexBuffer->UpdateToGpu();
   offset = 0;
   
   for( auto& idx : m_indices )
   {
      ibPtr[offset++] = idx;
   }

   pIndexBuffer->Bind();
   pIndexBuffer->UpdateToGpu();

   mesh->vbs.push_back( pVertexBuffer );
   mesh->ib.swap( pIndexBuffer );

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
