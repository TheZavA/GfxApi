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

/* OpenGI error callback */
void GICALLBACK errorCB( unsigned int error, void *data )
{

   /* actually default callback in debug but we want it in release, too */
   fprintf( stderr, "OpenGI error: %s!\n", giErrorString( error ) );

}

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
   , m_bHasNodes( hasNodes )
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



  // memset( m_occupation, 0, 2 * 2 * 2 * 4 );
   int halfSize = ( ChunkManager::CHUNK_SIZE + 1 ) / 2;

   typedef boost::chrono::high_resolution_clock Clock;
   //auto t11 = Clock::now();
   memset( m_pChunkManager->m_pCellBuffer->getDataPtr(), 0, m_pChunkManager->m_pCellBuffer->m_xyzSize * 4 );
   //auto t222 = Clock::now();
   //std::cout << t222 - t11 << " MEMSET \n";

   auto zeroCrossCompact = boost::make_shared<std::vector<cell_t>>();
   zeroCrossCompact->reserve( m_edgesCompact->size() );

   auto compactCorners = boost::make_shared<std::vector<cl_int4_t>>();
   compactCorners->reserve( m_edgesCompact->size() );

   auto& cellBuffer = ( *m_pChunkManager->m_pCellBuffer );

   for( auto &edge : *m_edgesCompact )
   {
      // duplicate each edge to its neighbours
      for( int y = 0; y < 4; y++ )
      {

         int edge_index = edgeToIndex[edge.edge];
         int x1 = edge.grid_pos[0] + edgeDuplicateMask[edge_index][y][0];
         int y1 = edge.grid_pos[1] + edgeDuplicateMask[edge_index][y][1];
         int z1 = edge.grid_pos[2] + edgeDuplicateMask[edge_index][y][2];

         if( x1 < 0 || y1 < 0 || z1 < 0 ||
             x1 > ChunkManager::CHUNK_SIZE + 1 ||
             y1 > ChunkManager::CHUNK_SIZE + 1 ||
             z1 > ChunkManager::CHUNK_SIZE + 1 )
         {
            continue;
         }

         cell_t* cell = cellBuffer( x1, y1, z1 );


         if( cell == nullptr )
         {
            zeroCrossCompact->emplace_back();
            cell = &zeroCrossCompact->back();
            cell->edgeCount = 0;
            cell->corners = 0;
            cellBuffer( x1, y1, z1 ) = cell;

            compactCorners->emplace_back();
            auto& corner = compactCorners->back();
            corner.x = x1;
            corner.y = y1;
            corner.z = z1;
         }

         cell_t* zero1 = cell;

         zero1->positions[zero1->edgeCount][0] = edge.zero_crossing.x;
         zero1->positions[zero1->edgeCount][1] = edge.zero_crossing.y;
         zero1->positions[zero1->edgeCount][2] = edge.zero_crossing.z;

         zero1->normals[zero1->edgeCount][0] = edge.normal.x;
         zero1->normals[zero1->edgeCount][1] = edge.normal.y;
         zero1->normals[zero1->edgeCount][2] = edge.normal.z;

         zero1->materials[zero1->edgeCount] = edge.material;

         zero1->xPos = x1;
         zero1->yPos = y1;
         zero1->zPos = z1;

         zero1->edgeCount++;
      }

   }



   if( zeroCrossCompact->size() > 0 )
   {
      m_bHasNodes = true;

      m_zeroCrossCompact = zeroCrossCompact;

      m_pChunkManager->m_ocl->getCompactCorners( &( *compactCorners )[0], ( *compactCorners ).size() );

      m_compactCorners = compactCorners;

      //for( auto& corner : *compactCorners )
      //{
      //   if( corner.x > 63 || corner.y > 63 || corner.z > 63 )
      //      continue;
      //   m_occupation[corner.x / halfSize][corner.y / halfSize][corner.z / halfSize] = 1;
      //}
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

   if( !m_pVertices || !m_pIndices )
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
      boost::make_shared<GfxApi::VertexBuffer>( m_pVertices->size(), decl, GfxApi::Usage::STATIC_DRAW );

   boost::shared_ptr<GfxApi::IndexBuffer> pIndexBuffer =
      boost::make_shared<GfxApi::IndexBuffer>( mesh, m_pIndices->size(), GfxApi::PrimitiveIndexType::Indices32Bit, GfxApi::Usage::STATIC_DRAW );

   float * vbPtr = reinterpret_cast< float* >( pVertexBuffer->CpuPtr() );
   unsigned int * ibPtr = reinterpret_cast< unsigned int* >( pIndexBuffer->CpuPtr() );

   uint32_t offset = 0;

   for( auto& vertexInf : *m_pVertices )
   {
      vbPtr[offset + 0] = vertexInf.m_xyz.x;
      vbPtr[offset + 1] = vertexInf.m_xyz.y;
      vbPtr[offset + 2] = vertexInf.m_xyz.z;
      vbPtr[offset + 3] = vertexInf.m_normal.x;
      vbPtr[offset + 4] = vertexInf.m_normal.y;
      vbPtr[offset + 5] = vertexInf.m_normal.z;
      *( int* ) ( &vbPtr[offset + 6] ) = vertexInf.m_material;
      *( int* ) ( &vbPtr[offset + 7] ) = 0;
      offset += 8;

      if( vertexInf.m_material == 0 )
      {
         continue;
      }
   }

   pVertexBuffer->Bind();
   pVertexBuffer->UpdateToGpu();
   offset = 0;

   for( auto& idx : *m_pIndices )
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

   ////if (this->m_pTree->getLevel() > 10)
   //{

   //	GIcontext pContext = giCreateContext();
   //	giMakeCurrent(pContext);
   //	giErrorCallback(errorCB, NULL);

   //	/* set attribute arrays */
   //	giBindAttrib(GI_POSITION_ATTRIB, 0);
   //	giBindAttrib(GI_PARAM_ATTRIB, 2);
   //	giAttribPointer(0, 3, GI_FALSE, 3, (float*) m_pVertices->data());
   //	giAttribPointer(1, 3, GI_TRUE, 3, (float*)(m_pVertices->data() + 3*4));
   //	giEnableAttribArray(0);
   //	giEnableAttribArray(1);

   //	/* create mesh */
   //	GIuint uiMesh = giGenMesh();
   //	giBindMesh(uiMesh);
   //	giGetError();
   //	giIndexedMesh(0, m_pVertices->size()-1, mesh->ib->NumIndices(), (unsigned int*) mesh->ib->CpuPtr());
   //}


   m_pVertices.reset();
   m_pIndices.reset();
   //m_pOctreeDrawInfo.reset();
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

   m_bHasNodes = false;

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
      for( int i = 0; i < zeroCl.edgeCount; i++ )
      {
         qef.add( zeroCl.positions[i][0], zeroCl.positions[i][1], zeroCl.positions[i][2],
                  zeroCl.normals[i][0], zeroCl.normals[i][1], zeroCl.normals[i][2] );
      }

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

      for( int i = 0; i < zeroCl.edgeCount; i++ )
      {
         averageNormal += float3( zeroCl.normals[i][0], zeroCl.normals[i][1], zeroCl.normals[i][2] );
      }

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

      int testMat = findPopular( zeroCl.materials, zeroCl.edgeCount );

      leaf->material = testMat;

      if( testMat == 0 )
      {
         testMat = 0;
      }

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

   /*  auto triangleList = boost::make_shared<std::vector<Triangle>>();
    auto vertexList = boost::make_shared<std::vector<float3>>();

     for (int i = 0; i < pIndices->size(); i += 3)
     {
         float3 v1 = (*pVertices)[(*pIndices)[i]].m_xyz ;
         float3 v2 = (*pVertices)[(*pIndices)[i+1]].m_xyz ;
         float3 v3 = (*pVertices)[(*pIndices)[i+2]].m_xyz ;

         triangleList->push_back(Triangle(v1/32, v2/32, v3/32));

       vertexList->push_back(v1);
       vertexList->push_back(v2);
       vertexList->push_back(v3);
     }

     m_pTriangleList = triangleList;

    AABBTreeInfo treeInfo;
    treeInfo.curr_max_depth = 0;
    treeInfo.min_vertices = 3 * 3;
    treeInfo.max_tree_depth = 7;
    treeInfo.left_children = 0;
    treeInfo.right_children = 0;*/

    /*AABBNode* newTree;
    newTree = new AABBNode(*vertexList, treeInfo, 0);
    TextureTreeNode node;
    node.node_bounds = AABB(float3(0.0f), float3(1.0f));
    node.data = float4(0, 0, 0, 0.5f);*/

    // m_pTextureTree.reset(new TOctree<TextureTreeNode>(nullptr, nullptr, node, 1, cube::corner_t::get(0, 0, 0)));

   //  buildTextureOctree(*m_pTextureTree);

     //m_pTriangleList.reset();

   if( pVertices->size() <= 1 || pIndices->size() <= 1 )
   {
      m_bHasNodes = false;
      return;
   }
   m_pVertices = pVertices;
   m_pIndices = pIndices;
}



//void NodeChunk::buildTextureOctree( TOctree<TextureTreeNode>& tree )
//{
//   AABB& node_bounds = tree.getValue().node_bounds;
//   uint16_t node_level = tree.getLevel();
//
//   bool intersection = false;
//   for( auto& tri : *m_pTriangleList )
//   {
//      if( node_bounds.Intersects( tri ) )
//      {
//         intersection = true;
//         break;
//      }
//   }
//
//   if( intersection && node_level < 6 )
//   {
//      tree.getValue().data.w = 0.5f;// INDEX
//      tree.getValue().data.x = node_bounds.minPoint.x;// INDEX
//      tree.getValue().data.y = node_bounds.minPoint.y;// INDEX
//      tree.getValue().data.z = node_bounds.minPoint.z;// INDEX
//      tree.split();
//      for( auto& corner : cube::corner_t::all() )
//      {
//         auto& child = tree.getChild( corner );
//         uint8_t level = child->getLevel();
//
//
//         float3 center = node_bounds.CenterPoint();
//         float3 c0 = node_bounds.CornerPoint( corner.index() );
//
//         AABB b0( float3( std::min( c0.x, center.x ),
//                          std::min( c0.y, center.y ),
//                          std::min( c0.z, center.z ) ),
//                  float3( std::max( c0.x, center.x ),
//                          std::max( c0.y, center.y ),
//                          std::max( c0.z, center.z ) ) );
//
//
//
//         child->getValue().node_bounds = b0;
//
//
//         buildTextureOctree( *child );
//
//      }
//   }
//   else if( intersection && node_level == 6 )
//   {
//      tree.getValue().data.x = node_bounds.minPoint.x;// dummy color
//      tree.getValue().data.y = node_bounds.minPoint.y;// dummy color
//      tree.getValue().data.z = node_bounds.minPoint.z;// dummy color
//      tree.getValue().data.w = 1.0f;// DATA
//   }
//   else if( !intersection )
//   {
//      tree.getValue().data.w = 0.0f;// EMPTY
//   }
//
//}