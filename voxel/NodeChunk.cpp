#include "NodeChunk.h"
#include <assert.h>
#include "ChunkManager.h"
#include "../ocl.h"
#include "../MainClass.h"
#include <GI/gi.h>
#include "../cubelib/cube.hpp"
#include "octree.h"

#include "AABBTree.h"
#include <GI/gi.h>


const float QEF_ERROR = 1e-1f;
const int QEF_SWEEPS = 4;

int toIntIdx( uint8_t x, uint8_t y, uint8_t z )
{
   int index = 0;
   ( ( char* ) ( &index ) )[0] = x;
   ( ( char* ) ( &index ) )[1] = y;
   ( ( char* ) ( &index ) )[2] = z;
   return index;
}


NodeChunk::NodeChunk( float scale, const AABB& bounds, ChunkManager* pChunkManager, bool hasNodes )
   : m_scale( scale )
   , m_chunk_bounds( bounds )
   , m_pChunkManager( pChunkManager )
   , m_workInProgress( nullptr )
   , m_pIndices( nullptr )
   , m_pVertices( nullptr )
   , m_pOctreeNodes( nullptr )
   , m_bHasNodes( false )
   , m_nodeIdxCurr( 0 )
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

   m_pChunkManager->m_ocl->density3d( ChunkManager::CHUNK_SIZE + 4,
                                      m_chunk_bounds.minPoint.x,
                                      m_chunk_bounds.minPoint.y,
                                      m_chunk_bounds.minPoint.z,
                                      m_scale,
                                      tree_level );

}


void NodeChunk::generateFullEdges()
{

   typedef boost::chrono::high_resolution_clock Clock;

   m_pCellBuffer = boost::make_shared< TVolume3d< cell_t* > >( ChunkManager::CHUNK_SIZE +2, ChunkManager::CHUNK_SIZE +2, ChunkManager::CHUNK_SIZE +2 );
   memset( m_pCellBuffer->getDataPtr(), 0, m_pCellBuffer->m_xyzSize * 4 );

   auto zeroCrossCompact = boost::make_shared<std::vector<cell_t>>();
   zeroCrossCompact->reserve( m_edgesCompact->size() );

   auto compactCorners = boost::make_shared<std::vector<cl_int4_t>>();
   compactCorners->reserve( m_edgesCompact->size() );

   auto& cellBuffer = ( *m_pCellBuffer );

   for( auto &edge : *m_edgesCompact )
   {
      // duplicate each edge to its neighbours
      for( int y = 0; y < 4; y++ )
      {

         int edge_index = edgeToIndex[edge.edge];
         int x1 = edge.grid_pos[0] + edgeDuplicateMask[edge_index][y][0];
         int y1 = edge.grid_pos[1] + edgeDuplicateMask[edge_index][y][1];
         int z1 = edge.grid_pos[2] + edgeDuplicateMask[edge_index][y][2];

         int target_index = edgeDuplicateIndex[edge_index][y];

         if( x1 < 0 || y1 < 0 || z1 < 0 ||
             x1 > ChunkManager::CHUNK_SIZE + 1 ||
             y1 > ChunkManager::CHUNK_SIZE + 1 ||
             z1 > ChunkManager::CHUNK_SIZE + 1 )
         {
            continue;
         }

         if( x1 == 7 && y1 == 0 && z1 == 0 )
         {
            int sdsd = 1;
         }
         cell_t* cell = cellBuffer( x1, y1, z1 );

         if( cell == nullptr )
         {
            zeroCrossCompact->emplace_back();
            cell = &zeroCrossCompact->back();
            //memset( cell, 0, sizeof( cell_t ) );
            //cell->edgeCount = 0;
            cell->corners = 0;
            cellBuffer( x1, y1, z1 ) = cell;

            compactCorners->emplace_back();
            auto& corner = compactCorners->back();
            corner.x = x1;
            corner.y = y1;
            corner.z = z1;
         }

         cell_t* zero1 = cell;

         zero1->positions[target_index][0] = edge.zero_crossing.x;
         zero1->positions[target_index][1] = edge.zero_crossing.y;
         zero1->positions[target_index][2] = edge.zero_crossing.z;

         zero1->normals[target_index][0] = edge.normal.x;
         zero1->normals[target_index][1] = edge.normal.y;
         zero1->normals[target_index][2] = edge.normal.z;

         zero1->materials[target_index] = edge.material;

         zero1->xPos = x1;
         zero1->yPos = y1;
         zero1->zPos = z1;

         if( zero1->xPos > 36 )
         {
            zero1 = zero1;
         }

         //zero1->edgeCount++;
      }

   }



   if( zeroCrossCompact->size() > 0 )
   {
      m_bHasNodes = true;

      m_zeroCrossCompact = zeroCrossCompact;

      m_pChunkManager->m_ocl->getCompactCorners( &( *compactCorners )[0], ( *compactCorners ).size() );

      m_compactCorners = compactCorners;

      auto& corners = ( *m_compactCorners );
      int off = 0;
      for( auto& zero : *m_zeroCrossCompact )
      {
         cl_int4_t& corner = corners[off++];
         zero.corners = corner.w;
      }
   }
   else
   {
      m_bHasNodes = false;
   }

}


boost::shared_ptr<GfxApi::Mesh> NodeChunk::getMeshPtr()
{
   return m_pMesh;
}


void NodeChunk::createMesh()
{

   if( m_mdcVertices.size() < 1 || m_indices.size() < 1 )
   {
      return;
   }


   GfxApi::VertexDeclaration decl;
   decl.Add( GfxApi::VertexElement( GfxApi::VertexDataSemantic::VCOORD, GfxApi::VertexDataType::FLOAT, 3, "vertex_position" ) );
   decl.Add( GfxApi::VertexElement( GfxApi::VertexDataSemantic::NORMAL, GfxApi::VertexDataType::FLOAT, 3, "vertex_normal" ) );
   decl.Add( GfxApi::VertexElement( GfxApi::VertexDataSemantic::TCOORD, GfxApi::VertexDataType::INT, 2, "vertex_material" ) );

   auto mesh = boost::make_shared<GfxApi::Mesh>( GfxApi::PrimitiveType::TriangleList );
   mesh->Bind();

   boost::shared_ptr<GfxApi::VertexBuffer> pVertexBuffer =
      boost::make_shared<GfxApi::VertexBuffer>( m_mdcVertices.size(), decl, GfxApi::Usage::STATIC_DRAW );

   boost::shared_ptr<GfxApi::IndexBuffer> pIndexBuffer =
      boost::make_shared<GfxApi::IndexBuffer>( mesh, m_indices.size(), GfxApi::PrimitiveIndexType::Indices32Bit, GfxApi::Usage::STATIC_DRAW );

   float * vbPtr = reinterpret_cast< float* >( pVertexBuffer->CpuPtr() );
   unsigned int * ibPtr = reinterpret_cast< unsigned int* >( pIndexBuffer->CpuPtr() );

   uint32_t offset = 0;

   for( auto& vertexInf : m_mdcVertices )
   {
      vbPtr[offset + 0] = vertexInf.position.x;
      vbPtr[offset + 1] = vertexInf.position.y;
      vbPtr[offset + 2] = vertexInf.position.z;
      vbPtr[offset + 3] = vertexInf.normal.x;
      vbPtr[offset + 4] = vertexInf.normal.y;
      vbPtr[offset + 5] = vertexInf.normal.z;
      *( int* ) ( &vbPtr[offset + 6] ) = 1;
      *( int* ) ( &vbPtr[offset + 7] ) = 0;
      offset += 8;

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

   m_mdcVertices.clear();
   m_indices.clear();
   m_pVertices.reset();
   m_pIndices.reset();
   m_zeroCrossCompact.reset();
   m_pOctreeNodes.reset();


}


void NodeChunk::classifyEdges()
{


   //typedef boost::chrono::high_resolution_clock Clock;
  // auto t1 = Clock::now();

   std::vector<cl_edge_info_t> compactedEdges;
   m_pChunkManager->m_ocl->classifyEdges( ChunkManager::CHUNK_SIZE + 4, compactedEdges );

   auto edges_compact = boost::make_shared<std::vector<edge_t>>();

   // reserve some memory so the push back does not have to resize the vector all the time.
   edges_compact->reserve( compactedEdges.size() * 2 );

   auto& edges_compact_ref = *edges_compact;

   for( auto& edge : compactedEdges )
   {
      if( edge.edge_info & 1 )
      {
         edges_compact_ref.emplace_back();
         edge_t& edge_v = edges_compact_ref.back();
         edge_v.grid_pos[0] = edge.grid_pos[0];
         edge_v.grid_pos[1] = edge.grid_pos[1];
         edge_v.grid_pos[2] = edge.grid_pos[2];
         edge_v.edge = 0;
      }
      if( edge.edge_info & 2 )
      {
         edges_compact_ref.emplace_back();
         edge_t& edge_v = edges_compact_ref.back();
         edge_v.grid_pos[0] = edge.grid_pos[0];
         edge_v.grid_pos[1] = edge.grid_pos[1];
         edge_v.grid_pos[2] = edge.grid_pos[2];
         edge_v.edge = 4;
      }
      if( edge.edge_info & 4 )
      {
         edges_compact_ref.emplace_back();
         edge_t& edge_v = edges_compact_ref.back();
         edge_v.grid_pos[0] = edge.grid_pos[0];
         edge_v.grid_pos[1] = edge.grid_pos[1];
         edge_v.grid_pos[2] = edge.grid_pos[2];
         edge_v.edge = 8;
      }
   }

  // m_bHasNodes = false;

   if( edges_compact->size() > 0 )
   {
      m_bHasNodes = true;
      m_edgesCompact = edges_compact;
      //  auto t2 = Clock::now();
      //  std::cout << "..." << (t2 - t1) << "\n";
   }



}


void NodeChunk::generateZeroCross()
{

   int tree_level = m_pTree->getLevel();
   m_pChunkManager->m_ocl->generateZeroCrossing( *m_edgesCompact,
                                                 m_chunk_bounds.minPoint.x,
                                                 m_chunk_bounds.minPoint.y,
                                                 m_chunk_bounds.minPoint.z,
                                                 m_scale,
                                                 tree_level );

}

int findPopular( uint8_t* a, int length )
{

   uint8_t result[10];
   int highestIndex = 0;
   int highestCount = 0;
   memset( result, 0, 10 * sizeof( uint8_t ) );
   for( int i = 0; i < length; i++ )
   {
      result[a[i]]++;
      if( result[a[i]] > highestCount && result[a[i]] != 0 )
      {
         highestCount = result[a[i]];
         highestIndex = a[i];
      }

   }

   return highestIndex;
}

std::vector<uint32_t> NodeChunk::createLeafNodes()
{
   std::vector<uint32_t> leaf_indices;

   m_pOctreeNodes = boost::make_shared<std::vector<OctreeNode>>( m_edgesCompact->size() );

   auto& corners = ( *m_compactCorners );
   auto& nodes = ( *m_pOctreeNodes );
   int off = 0;
   for( auto& zero : *m_zeroCrossCompact )
   {

      cell_t& zeroCl = zero;
      cl_int4_t& corner = corners[off++];

      float3 averageNormal = float3( 0, 0, 0 );
      float3 position;

      QefSolver qef;
   /*   for( int i = 0; i < zeroCl.edgeCount; i++ )
      {
         qef.add( zeroCl.positions[i][0], zeroCl.positions[i][1], zeroCl.positions[i][2],
                  zeroCl.normals[i][0], zeroCl.normals[i][1], zeroCl.normals[i][2] );
      }*/

      Vec3 qefPosition;
      qef.solve( qefPosition, QEF_ERROR, 4, QEF_ERROR );

      position.x = qefPosition.x;
      position.y = qefPosition.y;
      position.z = qefPosition.z;

      if( position.x < zeroCl.xPos || position.x > float( zeroCl.xPos + 1.0f ) ||
          position.y < zeroCl.yPos || position.y > float( zeroCl.yPos + 1.0f ) ||
          position.z < zeroCl.zPos || position.z > float( zeroCl.zPos + 1.0f ) )
      {
         position.x = qef.getMassPoint().x;
         position.y = qef.getMassPoint().y;
         position.z = qef.getMassPoint().z;
      }

    /*  for( int i = 0; i < zeroCl.edgeCount; i++ )
      {
         averageNormal += float3( zeroCl.normals[i][0], zeroCl.normals[i][1], zeroCl.normals[i][2] );
      }*/

      averageNormal = averageNormal.Normalized();

      OctreeNode *leaf;
      uint32_t count = m_nodeIdxCurr;
      if( m_nodeIdxCurr >= nodes.size() )
      {
         nodes.emplace_back();
      }
      leaf = &nodes[m_nodeIdxCurr++];
      leaf->size = 1;
      leaf->minx = zeroCl.xPos;
      leaf->miny = zeroCl.yPos;
      leaf->minz = zeroCl.zPos;
      leaf->type = Node_Leaf;

      //int testMat = findPopular( zeroCl.materials, zeroCl.edgeCount );

      //leaf->material = testMat;

      /*if( testMat == 0 )
      {
         testMat = 0;
      }*/

      leaf->averageNormal = averageNormal;
      leaf->corners = corner.w;
      leaf->position = position;
      leaf->qef = qef.getData();

      if( zero.xPos <= ChunkManager::CHUNK_SIZE + 1
          && zero.yPos <= ChunkManager::CHUNK_SIZE + 1
          && zero.zPos <= ChunkManager::CHUNK_SIZE + 1
          && zero.xPos >= 0
          && zero.yPos >= 0
          && zero.zPos >= 0 )
      {
         leaf_indices.push_back( count );
      }

   }

   return leaf_indices;
}

void NodeChunk::buildTree( const int size, const float threshold )
{

   auto leafs = createLeafNodes();

   m_octree_root_index = createOctree( leafs, *m_pOctreeNodes, threshold, true, m_nodeIdxCurr );

   m_edgesCompact.reset();
}

void NodeChunk::createVertices()
{
   boost::shared_ptr<IndexBuffer> pIndices = boost::make_shared<IndexBuffer>();
   boost::shared_ptr<VertexBuffer> pVertices = boost::make_shared<VertexBuffer>();

   if( m_octree_root_index == -1 )
   {
      m_bHasNodes = false;
      return;
   }

   pVertices->clear();
   pIndices->clear();

   GenerateVertexIndices( m_octree_root_index, *pVertices, *m_pOctreeNodes );
   CellProc( m_octree_root_index, *pIndices, *m_pOctreeNodes );

   if( pVertices->size() <= 1 || pIndices->size() <= 1 )
   {
      m_bHasNodes = false;
      return;
   }
   m_pVertices = pVertices;
   m_pIndices = pIndices;
}

void NodeChunk::generate( float threshold )
{

   std::vector< OctreeNodeMdc > octreeNodes( 220000 );

   ConstructBase( &octreeNodes[0], ChunkManager::CHUNK_SIZE, octreeNodes );


   //if( !m_bHasNodes )
   //{

   //   this->m_samples.reset();
   //   this->m_indices.clear();
   //   this->m_vertices.clear();
   //   this->m_mdcVertices.clear();
   //   this->m_pVertices.reset();
   //   this->m_pIndices.reset();
   //   this->m_tri_count.clear();


   //   return;
   //}

   ( &octreeNodes[0] )->ClusterCellBase( threshold, octreeNodes );

   ( &octreeNodes[0] )->GenerateVertexBuffer( m_mdcVertices, octreeNodes );

   ( &octreeNodes[0] )->ProcessCell( m_indices, m_tri_count, threshold, octreeNodes );

   m_pCellBuffer.reset();

}

void NodeChunk::ConstructBase( OctreeNodeMdc * pNode, int size, std::vector< OctreeNodeMdc >& nodeList )
{
   pNode->m_index = 0;
   pNode->m_size = size;
   pNode->m_type = NodeType::Internal;
   pNode->m_child_index = 0;
   int n_index = 1;
   if( !ConstructNodes( pNode, n_index, nodeList ) )
   {
      //m_bHasNodes = false;
   }
}

bool NodeChunk::ConstructNodes( OctreeNodeMdc * pNode, int& n_index, std::vector< OctreeNodeMdc >& nodeList )
{
   if( pNode->m_size == 1 )
   {
      return ConstructLeaf( pNode, n_index );
   }

   pNode->m_type = NodeType::Internal;
   int child_size = pNode->m_size / 2;

   bool has_children = false;

   float3 gridPos( pNode->m_gridX, pNode->m_gridY, pNode->m_gridZ );

   for( int i = 0; i < 8; i++ )
   {
      pNode->m_index = n_index++;
      float3 child_pos = gridPos +
         float3( Utilities::TCornerDeltas[i][0],
                 Utilities::TCornerDeltas[i][1],
                 Utilities::TCornerDeltas[i][2] ) * ( float ) child_size;

      auto node = &nodeList[m_nodeIdxCurr++];
      node->m_size = child_size;
      node->m_type = NodeType::Internal;
      //auto newNode = OctreeNodeMdc( child_pos, child_size, NodeType::Internal );

      node->m_gridX = static_cast< int32_t >( child_pos.x );
      node->m_gridY = static_cast< int32_t >( child_pos.y );
      node->m_gridZ = static_cast< int32_t >( child_pos.z );

      node->m_child_index = i;
      int childIndex = m_nodeIdxCurr - 1;
      //nodeList.push_back( newNode );
      pNode->m_childIndex[i] = childIndex;

      int index = i;
      if( ConstructNodes( node, n_index, nodeList ) )
         has_children = true;
      else
      {
         pNode->m_childIndex[i] = -1;
      }

   }

   return has_children;
}

bool NodeChunk::ConstructLeaf( OctreeNodeMdc * pNode, int& index )
{
   if( pNode->m_size != 1 )
      return false;

   pNode->m_index = index++;
   pNode->m_type = NodeType::Leaf;
   float samples[8];
   
   auto& cellBuffer = ( *m_pCellBuffer );
   cell_t* cell = cellBuffer( pNode->m_gridX, pNode->m_gridY, pNode->m_gridZ );
   //const auto& samples1 = *m_samples;
   //for( int i = 0; i < 8; i++ )
   //{
   //   if( samples1( pNode->m_gridX + Utilities::TCornerDeltas[i][0],
   //                 pNode->m_gridY + Utilities::TCornerDeltas[i][1],
   //                 pNode->m_gridZ + Utilities::TCornerDeltas[i][2] ) < 0 )
   //      pNode->m_corners |= 1 << i;
   //}

   if( cell == nullptr )
   {
      //m_bHasNodes = false;
      return false;
   }

   pNode->m_corners = cell->corners;

   if( cell->corners == 0 ||
       cell->corners == 255 )
   {
      return false;
   }
   
   

   //m_bHasNodes = true;

   int8_t v_edges[4][12]; //the edges corresponding to each vertex



   int v_index = 0;
   int e_index = 0;

   for( int e = 0; e < 16; e++ )
   {
      int code = Utilities::TransformedEdgesTable[cell->corners][e];
      if( code == -2 )
      {
         v_edges[v_index++][e_index] = -1;
         break;
      }
      if( code == -1 )
      {
         v_edges[v_index++][e_index] = -1;
         e_index = 0;
         continue;
      }

      v_edges[v_index][e_index++] = code;
   }


   pNode->m_vertices.reserve( v_index );

   for( int i = 0; i < v_index; i++ )
   {
      int k = 0;
      auto pVertex = boost::make_shared<Vertex>();
      //pVertex->m_qef.reset();
      float3 normal = float3::zero;
      int ei[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
      while( v_edges[i][k] != -1 )
      {
         ei[v_edges[i][k]] = 1;
         int index = v_edges[i][k];

         if( cell->normals[v_edges[i][k]][0] == 0 || (cell->positions[index][0] == 0 && cell->positions[index][1] == 0 && cell->positions[index][2] == 0 ))
         {
            //std::cout << cell->positions[index][0];
            index = index;
         }
         else
         {
            index = index;
         }

         float3 no = float3( cell->normals[v_edges[i][k]][0],
                             cell->normals[v_edges[i][k]][1],
                             cell->normals[v_edges[i][k]][2] );

         normal += no;
         pVertex->m_qef.add( cell->positions[index][0], 
                             cell->positions[index][1], 
                             cell->positions[index][2],
                             no.x, no.y, no.z );
         k++;
      }

      normal /= ( float ) k;
      normal.Normalize();
      pVertex->m_index = -1;
      pVertex->m_error = 0;
      pVertex->m_pParent = nullptr;
      pVertex->m_collapsible = true;
      pVertex->m_normal = normal;
      pVertex->m_euler = 1;
      memcpy( pVertex->m_eis, ei, 12 * 4 );
      pVertex->m_in_cell = pNode->m_child_index;
      pVertex->m_face_prop2 = true;
      pNode->m_vertices.push_back( pVertex );


   }

   return true;
}
