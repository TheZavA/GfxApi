#include "NodeChunk.h"
#include <assert.h>
#include "ChunkManager.h"
#include "../ocl.h"
#include "../MainClass.h"

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

uint16_t NodeChunk::getOrCreateVertex( uint8_t x, uint8_t y, uint8_t z )
{
   uint16_t index = 0;
   uint32_t mapIndex = ( uint32_t )x << 0 | ( uint32_t )y << 8 | ( uint32_t )z << 16;
   auto it = m_vertexToIndex.find( mapIndex );
   if( it != m_vertexToIndex.end() )
      return it->second;
   else
   {
      index = m_vertices.size();
      m_vertices.push_back( cl_vertex_t( x, y, z ) );
      m_vertexToIndex[mapIndex] = index;
   }

   //index = m_vertices.size();
   //m_vertices.push_back( cl_vertex_t( x, y, z ) );

   return index;
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
            int16_t i1 = 0;
            int16_t i2 = 0;
            int16_t i3 = 0;
            int16_t i4 = 0;
           /* if( ( x == 0 ) && ( mask == 16 ) && ( vertex.block_info & mask ) )
            {
               i1 = getOrCreateVertex( x + faceList[i][0][0], y + faceList[i][0][1], z + faceList[i][0][2] );
               i2 = getOrCreateVertex( x + faceList[i][1][0], y + faceList[i][1][1], z + faceList[i][1][2] );
               i3 = getOrCreateVertex( x + faceList[i][2][0], std::max( ( int ) y - 2, 0 ), z + faceList[i][2][2] );
               i4 = getOrCreateVertex( x + faceList[i][3][0], std::max( ( int ) y - 2, 0 ), z + faceList[i][3][2] );
            }
            else if( ( x == 63 ) && ( mask == 32 ) && ( vertex.block_info & mask ) )
            {
               i1 = getOrCreateVertex( x + faceList[i][0][0], y + faceList[i][0][1], z + faceList[i][0][2] );
               i2 = getOrCreateVertex( x + faceList[i][1][0], y + faceList[i][1][1], z + faceList[i][1][2] );
               i3 = getOrCreateVertex( x + faceList[i][2][0], std::max( ( int ) y - 2, 0 ), z + faceList[i][2][2] );
               i4 = getOrCreateVertex( x + faceList[i][3][0], std::max( ( int ) y - 2, 0 ), z + faceList[i][3][2] );
            }
            else*/ if( ( z == 0 ) && ( mask == 8 ) && ( vertex.block_info & mask ) )
            {
               i1 = getOrCreateVertex( x + faceList[i][0][0], y + faceList[i][0][1], z + faceList[i][0][2] );
               i2 = getOrCreateVertex( x + faceList[i][1][0], y + faceList[i][1][1], z + faceList[i][1][2] );
               i3 = getOrCreateVertex( x + faceList[i][2][0], std::max( ( int ) y - 2, 0 ), z + faceList[i][2][2] );
               i4 = getOrCreateVertex( x + faceList[i][3][0], std::max( ( int ) y - 2, 0 ), z + faceList[i][3][2] );
            }
            else if( ( z == 63 ) && ( mask == 4 ) && ( vertex.block_info & mask ) )
            {
               i1 = getOrCreateVertex( x + faceList[i][0][0], std::max( ( int ) y - 2, 0 ), z + faceList[i][0][2] );
               i2 = getOrCreateVertex( x + faceList[i][1][0], y + faceList[i][1][1], z + faceList[i][1][2] );
               i3 = getOrCreateVertex( x + faceList[i][2][0], y + faceList[i][2][1], z + faceList[i][2][2] );
               i4 = getOrCreateVertex( x + faceList[i][3][0], std::max( ( int ) y - 2, 0 ), z + faceList[i][3][2] );
            }
            else
            {
               i1 = getOrCreateVertex( x + faceList[i][0][0], y + faceList[i][0][1], z + faceList[i][0][2] );
               i2 = getOrCreateVertex( x + faceList[i][1][0], y + faceList[i][1][1], z + faceList[i][1][2] );
               i3 = getOrCreateVertex( x + faceList[i][2][0], y + faceList[i][2][1], z + faceList[i][2][2] );
               i4 = getOrCreateVertex( x + faceList[i][3][0], y + faceList[i][3][1], z + faceList[i][3][2] );
            }


            m_indices.push_back( i1 );
            m_indices.push_back( i2 );
            m_indices.push_back( i3 );

            m_indices.push_back( i3 );
            m_indices.push_back( i4 );
            m_indices.push_back( i1 );
         }
      }
   }
   m_vertexToIndex.clear();

}

void NodeChunk::generateLocalAO()
{
   if( !m_compactedBlocks || m_compactedBlocks->size() < 1 )
      return;

   m_pChunkManager->m_ocl->calulateBlockAO( m_vertices.size(), m_vertices );
}

void NodeChunk::createMesh()
{


   if( m_vertices.size() < 1 )
      return;
   
   GfxApi::VertexDeclaration decl;
   decl.Add( GfxApi::VertexElement( GfxApi::VertexDataSemantic::VCOORD, GfxApi::VertexDataType::UNSIGNED_BYTE, 3, "vertex_position" ) );
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
      vbPtr[offset + 3] = vertexInf.localAO;
   //   *( int* ) ( &vbPtr[offset + 7] ) = 0;
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

   m_compactedBlocks->clear();
   m_compactedBlocks.reset();

}
