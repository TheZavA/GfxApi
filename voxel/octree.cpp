#include "octree.h"
#include <boost/scoped_ptr.hpp>
#include <stdint.h>
#include <unordered_map>
#include <boost/array.hpp>
#include "ChunkManager.h"
#include "HashMap.h"
#include "intersection.h"

/*
 * Some of this code is derived from the reference implementation provided by
 * the DC paper's authors. This code is free for any non-commercial use.
 * The CellProc, ContourEdgeProc, ContourFaceProc and ContourProcessEdge
 * funcs and the data they use are derived from the reference implementation
 * which can be found here: http://sourceforge.net/projects/dualcontouring/
 */

 // ----------------------------------------------------------------------------

const float QEF_ERROR = 0.1f;
const int QEF_SWEEPS = 4;

#define EDGE_TEST_NEW 1

int toint2( uint8_t x, uint8_t y, uint8_t z )
{
   int index = 0;
   ( ( char* ) ( &index ) )[0] = x;
   ( ( char* ) ( &index ) )[1] = y;
   ( ( char* ) ( &index ) )[2] = z;
   return index;
}

void GenerateVertexIndices( int32_t node_index, VertexBuffer& vertexBuffer, std::vector<OctreeNode>& node_list )
{
   if( node_index == -1 )
   {
      return;
   }

   OctreeNode* node = &node_list[node_index];

   if( node->type != Node_Leaf )
   {
      for( int i = 0; i < 8; i++ )
      {
         if( node->childrenIdx[i] != -1 )
            GenerateVertexIndices( node->childrenIdx[i], vertexBuffer, node_list );
      }
   }

   if( node->type != Node_Internal )
   {
      node->index = vertexBuffer.size();
      vertexBuffer.push_back( MeshVertex( node->position, node->averageNormal, node->material ) );
   }
}

// ----------------------------------------------------------------------------

void ContourProcessEdge( int32_t node_index[4], int dir, IndexBuffer& indexBuffer, std::vector<OctreeNode>& node_list )
{
   int minSize = 1000000;		// arbitrary big number
   int minIndex = 0;
   int indices[4] = { -1, -1, -1, -1 };
   bool flip = false;
   bool signChange[4] = { false, false, false, false };

   for( int i = 0; i < 4; i++ )
   {
      const int edge = processEdgeMask[dir][i];
      const int c1 = edgevmap[edge][0];
      const int c2 = edgevmap[edge][1];

      const int m1 = ( node_list[node_index[i]].corners >> c1 ) & 1;
      const int m2 = ( node_list[node_index[i]].corners >> c2 ) & 1;


      if( node_list[node_index[i]].size < minSize )
      {
         minSize = node_list[node_index[i]].size;
         minIndex = i;
         flip = m1 != MATERIAL_AIR;
      }

      indices[i] = node_list[node_index[i]].index;

      signChange[i] = ( m1 && !m2 ) || ( !m1 && m2 );
   }

   if( signChange[minIndex] )
   {
      if( !flip )
      {
         indexBuffer.push_back( indices[0] );
         indexBuffer.push_back( indices[1] );
         indexBuffer.push_back( indices[3] );

         indexBuffer.push_back( indices[0] );
         indexBuffer.push_back( indices[3] );
         indexBuffer.push_back( indices[2] );
      }
      else
      {
         indexBuffer.push_back( indices[0] );
         indexBuffer.push_back( indices[3] );
         indexBuffer.push_back( indices[1] );

         indexBuffer.push_back( indices[0] );
         indexBuffer.push_back( indices[2] );
         indexBuffer.push_back( indices[3] );
      }
   }
}

// ----------------------------------------------------------------------------

void EdgeProc( int32_t node_index[4], int dir, IndexBuffer& indexBuffer, std::vector<OctreeNode>& node_list )
{
   if( node_index[0] == -1 || node_index[1] == -1 || node_index[2] == -1 || node_index[3] == -1 )
   {
      return;
   }

   if( node_list[node_index[0]].type != Node_Internal &&
       node_list[node_index[1]].type != Node_Internal &&
       node_list[node_index[2]].type != Node_Internal &&
       node_list[node_index[3]].type != Node_Internal )
   {
      ContourProcessEdge( node_index, dir, indexBuffer, node_list );
   }
   else
   {
      for( int i = 0; i < 2; i++ )
      {
         int32_t edgeNodes[4];
         const int c[4] =
         {
            edgeProcEdgeMask[dir][i][0],
            edgeProcEdgeMask[dir][i][1],
            edgeProcEdgeMask[dir][i][2],
            edgeProcEdgeMask[dir][i][3],
         };

         for( int j = 0; j < 4; j++ )
         {
            if( node_list[node_index[j]].type == Node_Leaf || node_list[node_index[j]].type == Node_Psuedo )
            {
               edgeNodes[j] = node_index[j];
            }
            else
            {
               edgeNodes[j] = node_list[node_index[j]].childrenIdx[c[j]];
            }
         }

         EdgeProc( edgeNodes, edgeProcEdgeMask[dir][i][4], indexBuffer, node_list );
      }
   }
}

// ----------------------------------------------------------------------------

void FaceProc( int32_t node_index[2], int dir, IndexBuffer& indexBuffer, std::vector<OctreeNode>& node_list )
{
   if( node_index[0] == -1 || node_index[1] == -1 )
   {
      return;
   }

   OctreeNode* node0 = &node_list[node_index[0]];
   OctreeNode* node1 = &node_list[node_index[1]];

   if( node0->type == Node_Internal ||
       node1->type == Node_Internal )
   {
      for( int i = 0; i < 4; i++ )
      {
         int32_t faceNodes[2];
         const int c[2] =
         {
            faceProcFaceMask[dir][i][0],
            faceProcFaceMask[dir][i][1],
         };

         for( int j = 0; j < 2; j++ )
         {
            if( node_list[node_index[j]].type != Node_Internal )
            {
               faceNodes[j] = node_index[j];
            }
            else
            {
               faceNodes[j] = node_list[node_index[j]].childrenIdx[c[j]];
            }
         }

         FaceProc( faceNodes, faceProcFaceMask[dir][i][2], indexBuffer, node_list );
      }

      const int orders[2][4] =
      {
         { 0, 0, 1, 1 },
         { 0, 1, 0, 1 },
      };
      for( int i = 0; i < 4; i++ )
      {
         int32_t edgeNodes[4];
         const int c[4] =
         {
            faceProcEdgeMask[dir][i][1],
            faceProcEdgeMask[dir][i][2],
            faceProcEdgeMask[dir][i][3],
            faceProcEdgeMask[dir][i][4],
         };

         const int* order = orders[faceProcEdgeMask[dir][i][0]];
         for( int j = 0; j < 4; j++ )
         {
            if( node_list[node_index[order[j]]].type == Node_Leaf ||
                node_list[node_index[order[j]]].type == Node_Psuedo )
            {
               edgeNodes[j] = node_index[order[j]];
            }
            else
            {
               edgeNodes[j] = node_list[node_index[order[j]]].childrenIdx[c[j]];
            }
         }

         EdgeProc( edgeNodes, faceProcEdgeMask[dir][i][5], indexBuffer, node_list );
      }
   }
}


void CellProc( int32_t node_index, IndexBuffer& indexBuffer, std::vector<OctreeNode>& node_list )
{
   if( node_index == -1 )
   {
      return;
   }

   OctreeNode* node = &node_list[node_index];

   if( node->type == Node_Internal )
   {
      // for all child cells, call cellproc
      for( int i = 0; i < 8; i++ )
      {
         CellProc( node->childrenIdx[i], indexBuffer, node_list );
      }

      // for all faces inside the cell, call faceproc
      for( int i = 0; i < 12; i++ )
      {
         int32_t faceNodes[2];
         const int c[2] = { cellProcFaceMask[i][0], cellProcFaceMask[i][1] };

         faceNodes[0] = node->childrenIdx[c[0]];
         faceNodes[1] = node->childrenIdx[c[1]];

         FaceProc( faceNodes, cellProcFaceMask[i][2], indexBuffer, node_list );
      }


      for( int i = 0; i < 6; i++ )
      {
         int32_t edgeNodes[4];
         const int c[4] =
         {
            cellProcEdgeMask[i][0],
            cellProcEdgeMask[i][1],
            cellProcEdgeMask[i][2],
            cellProcEdgeMask[i][3],
         };

         for( int j = 0; j < 4; j++ )
         {
            edgeNodes[j] = node->childrenIdx[c[j]];
         }

         EdgeProc( edgeNodes, cellProcEdgeMask[i][4], indexBuffer, node_list );
      }
   }
}


uint32_t createOctree( std::vector<uint32_t>& leaf_indices,
                       std::vector<OctreeNode>& node_list,
                       const float threshold,
                       bool simplify,
                       int32_t& node_counter )
{

   int index = 0;

   while( leaf_indices.size() > 1 )
   {

      std::vector<uint32_t> tmp_idx_vec;
      std::unordered_map<int, uint32_t> tmp_map;
      for( auto& nodeIdx : leaf_indices )
      {

         auto& node = node_list[nodeIdx];
         OctreeNode* new_node = nullptr;

         boost::array<int, 3> pos;

         int level = node.size * 2;

         uint16_t node_min_x = node.minx;
         uint16_t node_min_y = node.miny;
         uint16_t node_min_z = node.minz;

         pos[0] = ( node_min_x / level ) * level;
         pos[1] = ( node_min_y / level ) * level;
         pos[2] = ( node_min_z / level ) * level;

         auto& it = tmp_map.find( toint2( pos[0], pos[1], pos[2] ) );
         if( it == tmp_map.end() )
         {
            uint32_t index = node_counter;
            if( node_counter >= node_list.size() )
            {
               // carefull, this invalidates the node reference from before
               node_list.push_back( OctreeNode() );
            }
            new_node = &node_list[node_counter++];
            new_node->type = Node_Internal;
            new_node->size = level;
            new_node->minx = pos[0];
            new_node->miny = pos[1];
            new_node->minz = pos[2];
            //new_node->material = node.material;
            new_node->material = 1;
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

         new_node->childrenIdx[idx] = nodeIdx;

      }
      leaf_indices.swap( tmp_idx_vec );
   }


   if( leaf_indices.size() == 0 )
   {
      return -1;
   }

   if( simplify )
   {
      return simplifyOctree( leaf_indices[0], node_list, threshold );
   }

   return leaf_indices[0];

}


uint32_t simplifyOctree( uint32_t node_index, std::vector<OctreeNode>& node_list, float threshold )
{
   if( node_index == -1 )
   {
      return -1;
   }

   auto& node = node_list[node_index];
   if( node.type != Node_Internal )
   {
      // can't simplify!
      return node_index;
   }

   QefSolver qef;
   int signs[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
   int midsign = -1;
   int edgeCount = 0;
   bool isCollapsible = true;

   uint8_t start_mat = node_list[node_index].material;

   for( int i = 0; i < 8; i++ )
   {
      node.childrenIdx[i] = simplifyOctree( node.childrenIdx[i], node_list, threshold );
      if( node.childrenIdx[i] != -1 )
      {
         OctreeNode* child = &node_list[node.childrenIdx[i]];


         if( child->type == Node_Internal )
         {
            isCollapsible = false;
         }
         else
         {
            if( /*child->material == start_mat &&*/ child->position.x > 1 && child->position.x < ChunkManager::CHUNK_SIZE + 1/*- 1*/ &&
                child->position.y > 1 && child->position.y < ChunkManager::CHUNK_SIZE + 1 &&
                child->position.z > 1 && child->position.z < ChunkManager::CHUNK_SIZE + 1/*- 1*/ )
            {

               qef.add( child->qef );

               midsign = ( child->corners >> ( 7 - i ) ) & 1;
               signs[i] = ( child->corners >> i ) & 1;

               edgeCount++;
            }
            else
            {
               isCollapsible = false;
            }
         }


      }
   }

   if( !isCollapsible )
   {
      // at least one child is an internal node, can't collapse
      return node_index;
   }

   // at this point the masspoint will actually be a sum, so divide to make it the average
   Vec3 qefPosition;
   try
   {

      float ret = qef.solve( qefPosition, QEF_ERROR, QEF_SWEEPS, QEF_ERROR );
      if( ret == 0 )
         return node_index;
   }
   catch( int e )
   {
      return node_index;
   }

   float error = qef.getError();

   float3 position( qefPosition.x, qefPosition.y, qefPosition.z );

   if( error > threshold )
   {
      // this collapse breaches the threshold
      return node_index;
   }

   if( position.x < node.minx || position.x > float( node.minx ) + 1.0f ||
       position.y < node.miny || position.y > float( node.miny ) + 1.0f ||
       position.z < node.minz || position.z > float( node.minz ) + 1.0f )
   {
      const auto& mp = qef.getMassPoint();
      position = float3( mp.x, mp.y, mp.z );

   }



   // change the node from an internal node to a 'psuedo leaf' node
   node.corners = 0;
   for( int i = 0; i < 8; i++ )
   {
      if( signs[i] == -1 )
      {
         // Undetermined, use centre sign instead
         node.corners |= ( midsign << i );
      }
      else
      {
         node.corners |= ( signs[i] << i );
      }
   }

   node.averageNormal = float3( 0.f );
   for( int i = 0; i < 8; i++ )
   {
      if( node.childrenIdx[i] != -1 )
      {
         OctreeNode* child = &node_list[node.childrenIdx[i]];
         if( child->type == Node_Psuedo ||
             child->type == Node_Leaf )
         {
            node.averageNormal += child->averageNormal;
         }
      }
   }

   node.averageNormal = node.averageNormal.Normalized();
   node.position = position;
   node.qef = qef.getData();

   for( int i = 0; i < 8; i++ )
   {
      node.childrenIdx[i] = -1;
   }

   node.type = Node_Psuedo;
   return node_index;
}


// -------------------------------------------------------------------------------

void cellProcContourNoInter2( int32_t node_index, int st[3], int len, HashMap* hash, VertexBuffer& vertexBuffer, IndexBuffer& indexBuffer, std::vector<OctreeNode>& node_list )
{
   if( node_index == -1 )
   {
      return;
   }

   OctreeNode* node = &node_list[node_index];

   if( node->type == Node_Internal )
   {
      int nlen = len / 2;
      int nst[3];

      for( int i = 0; i < 8; i++ )
      {

         nst[0] = st[0] + vertMap[i][0] * nlen;
         nst[1] = st[1] + vertMap[i][1] * nlen;
         nst[2] = st[2] + vertMap[i][2] * nlen;

         cellProcContourNoInter2( node->childrenIdx[i], nst, nlen, hash, vertexBuffer, indexBuffer, node_list );

      }

      int32_t fcd[2] = { -1, -1 };
      for( int i = 0; i < 3; i++ )
      {
         for( int j = 0; j < 4; j++ )
         {
            nst[0] = st[0] + nlen + dirCell2[i][j][0] * nlen;
            nst[1] = st[1] + nlen + dirCell2[i][j][1] * nlen;
            nst[2] = st[2] + nlen + dirCell2[i][j][2] * nlen;

            int ed = i * 4 + j;
            int c[2] = { cellProcFaceMask[ed][0], cellProcFaceMask[ed][1] };

            fcd[0] = node->childrenIdx[c[0]];
            fcd[1] = node->childrenIdx[c[1]];

            faceProcContourNoInter2( fcd, nst, nlen, cellProcFaceMask[ed][2], hash, vertexBuffer, indexBuffer, node_list );
         }
      }

      int32_t ecd[4] = { -1, -1, -1, -1 };
      for( int i = 0; i < 6; i++ )
      {
         int c[4] = { cellProcEdgeMask[i][0], cellProcEdgeMask[i][1], cellProcEdgeMask[i][2], cellProcEdgeMask[i][3] };

         for( int j = 0; j < 4; j++ )
         {
            ecd[j] = node->childrenIdx[c[j]];
         }

         int dir = cellProcEdgeMask[i][4];
         nst[0] = st[0] + nlen;
         nst[1] = st[1] + nlen;
         nst[2] = st[2] + nlen;
         if( i % 2 == 0 )
         {
            nst[dir] -= nlen;
         }

         edgeProcContourNoInter2( ecd, nst, nlen, dir, hash, vertexBuffer, indexBuffer, node_list );
      }
   }

}


void faceProcContourNoInter2( int32_t node_index[2], int st[3], int len, int dir, HashMap* hash, VertexBuffer& vertexBuffer, IndexBuffer& indexBuffer, std::vector<OctreeNode>& node_list )
{
   if( node_index[0] == -1 || node_index[1] == -1 )
   {
      return;
   }

   OctreeNode* node0 = &node_list[node_index[0]];
   OctreeNode* node1 = &node_list[node_index[1]];

   OctreeNodeType type[2] = { node0->type, node1->type };

   if( type[0] == Node_Internal || type[1] == Node_Internal )
   {
      int i, j;
      int nlen = len / 2;
      int nst[3];

      // 4 face calls
      int32_t fcd[2] = { -1, -1 };
      int iface = faceProcFaceMask[dir][0][0];
      for( i = 0; i < 4; i++ )
      {
         int c[2] = { faceProcFaceMask[dir][i][0], faceProcFaceMask[dir][i][1] };
         for( int j = 0; j < 2; j++ )
         {
            if( type[j] != Node_Internal )
            {
               fcd[j] = node_index[j];
            }
            else
            {
               fcd[j] = node_list[node_index[j]].childrenIdx[c[j]];
            }
         }

         nst[0] = st[0] + nlen * ( vertMap[c[0]][0] - vertMap[iface][0] );
         nst[1] = st[1] + nlen * ( vertMap[c[0]][1] - vertMap[iface][1] );
         nst[2] = st[2] + nlen * ( vertMap[c[0]][2] - vertMap[iface][2] );

         faceProcContourNoInter2( fcd, nst, nlen, faceProcFaceMask[dir][i][2], hash, vertexBuffer, indexBuffer, node_list );
      }

      // 4 edge calls
      int orders[2][4] = { { 0, 0, 1, 1 },{ 0, 1, 0, 1 } };
      int32_t ecd[4] = { -1, -1, -1, -1 };

      for( i = 0; i < 4; i++ )
      {
         int c[4] = { faceProcEdgeMask[dir][i][1], faceProcEdgeMask[dir][i][2],
                      faceProcEdgeMask[dir][i][3], faceProcEdgeMask[dir][i][4] };

         int* order = orders[faceProcEdgeMask[dir][i][0]];

         for( int j = 0; j < 4; j++ )
         {
            if( type[order[j]] != Node_Internal )
            {
               ecd[j] = node_index[order[j]];
            }
            else
            {
               ecd[j] = node_list[node_index[order[j]]].childrenIdx[c[j]];
            }
         }

         int ndir = faceProcEdgeMask[dir][i][5];
         nst[0] = st[0] + nlen;
         nst[1] = st[1] + nlen;
         nst[2] = st[2] + nlen;
         nst[dir] -= nlen;
         if( i % 2 == 0 )
         {
            nst[ndir] -= nlen;
         }

         edgeProcContourNoInter2( ecd, nst, nlen, ndir, hash, vertexBuffer, indexBuffer, node_list );
      }
   }
}

void edgeProcContourNoInter2( int32_t node_index[4], int st[3], int len, int dir, HashMap* hash, VertexBuffer& vertexBuffer, IndexBuffer& indexBuffer, std::vector<OctreeNode>& node_list )
{
   if( node_index[0] == -1 || node_index[1] == -1 || node_index[2] == -1 || node_index[3] == -1 )
   {
      return;
   }

   OctreeNodeType type[4] = { node_list[node_index[0]].type,
                              node_list[node_index[1]].type,
                              node_list[node_index[2]].type,
                              node_list[node_index[3]].type };

   if( type[0] != Node_Internal &&
       type[1] != Node_Internal &&
       type[2] != Node_Internal &&
       type[3] != Node_Internal )
   {
      processEdgeNoInter2( node_index, st, len, dir, hash, vertexBuffer, indexBuffer, node_list );
   }
   else
   {
      int i, j;
      int nlen = len / 2;
      int nst[3];

      // 2 edge calls
      int32_t ecd[4] = { -1, -1, -1, -1 };
      for( i = 0; i < 2; i++ )
      {
         int c[4] = { edgeProcEdgeMask[dir][i][0],
             edgeProcEdgeMask[dir][i][1],
             edgeProcEdgeMask[dir][i][2],
             edgeProcEdgeMask[dir][i][3] };

         for( int j = 0; j < 4; j++ )
         {
            if( type[j] != Node_Internal )
            {
               ecd[j] = node_index[j];
            }
            else
            {
               ecd[j] = node_list[node_index[j]].childrenIdx[c[j]];
            }
         }

         nst[0] = st[0];
         nst[1] = st[1];
         nst[2] = st[2];
         nst[dir] += nlen * i;

         edgeProcContourNoInter2( ecd, nst, nlen, edgeProcEdgeMask[dir][i][4], hash, vertexBuffer, indexBuffer, node_list );
      }

   }

}

void processEdgeNoInter2( int32_t node_index[4], int st[3], int len, int dir, HashMap* hash, VertexBuffer& vertexBuffer, IndexBuffer& indexBuffer, std::vector<OctreeNode>& node_list )
{
   int max_depth = 6;

   int i, type;
   int ind[4] = { -1, -1, -1, -1 };
   int ht[4] = { 0, 0 , 0 , 0 };
   float mp[4][3];
   memset( mp, 0, sizeof( float ) * 4 * 3 );

   int minSize = 8;		// arbitrary big number
   int minIndex = -1;
   //int indices[4] = { -1, -1, -1, -1 };
   bool flip = false;
   bool sc[4] = { false, false, false, false };

   for( i = 0; i < 4; i++ )
   {
      OctreeNode* node = &node_list[node_index[i]];

      if( node->type != Node_Internal )
      {

         ht[i] = node->size;
         mp[i][0] = node->position.x;
         mp[i][1] = node->position.y;
         mp[i][2] = node->position.z;


         int ed = processEdgeMask[dir][i];
         int c1 = edgevmap[ed][0];
         int c2 = edgevmap[ed][1];

         const int m1 = ( node->corners >> c1 ) & 1;
         const int m2 = ( node->corners >> c2 ) & 1;

         if( node->size < minSize )
         {
            minSize = node->size;
            minIndex = i;
            flip = m1 != MATERIAL_AIR;
         }


         ind[i] = node->index;
         if( ind[i] < 0 )
         {

            ind[i] = vertexBuffer.size();
            node->index = vertexBuffer.size();

            vertexBuffer.push_back( MeshVertex( node->position, node->averageNormal, node->material ) );
         }

         sc[i] =
            ( m1 == MATERIAL_AIR && m2 != MATERIAL_AIR ) ||
            ( m1 != MATERIAL_AIR && m2 == MATERIAL_AIR );

      }

   }

   if( !sc[minIndex] )
   {
      //		printf("I am done!\n" ) ;
      return;
   }

   /************************************************************************/
   /* Performing test                                                      */
   /************************************************************************/

   int fverts[4] /*= { -1, -1, -1, -1 }*/;
   int hasFverts[4] = { 0, 0, 0, 0 };
   int evert = 0;
   int needTess = 0;
   int location[4] /*= { -1, -1, -1, -1 }*/;
   int nvert[4] = { 0,0,0,0 };

   // First, face test
   int nbr[4][2] = { { 0,1 },{ 1,3 },{ 2,3 },{ 0,2 } };
   int fdir[3][4] = {
       { 2,1,2,1 },
       { 0,2,0,2 },
       { 1,0,1,0 } };
   int dir3[3][4][2] = {
       { { 1, -1 },{ 2, 0 },{ 1, 0 },{ 2, -1 } },
       { { 2, -1 },{ 0, 0 },{ 2, 0 },{ 0, -1 } },
       { { 0, -1 },{ 1, 0 },{ 0, 0 },{ 1, -1 } } };

   for( i = 0; i < 4; i++ )
   {
      int a = nbr[i][0];
      int b = nbr[i][1];

      //#ifndef TESS_UNIFORM
              //if (ht[a] != ht[b])
      //#endif
      {
         // Different level, check if the dual edge passes through the face
         if( hash->FindKey( ( int ) ( node_index[a] ), ( int ) ( node_index[b] ), fverts[i], location[i] ) )
         {
            // The vertex was found previously
            hasFverts[i] = 1;
            nvert[i] = 0;
            needTess = 1;
         }
         else
         {
            // Otherwise, we test it here
            int sht = ( ht[a] > ht[b] ? ht[b] : ht[a] );
            int flen = ( 1 << sht );
            int fst[3];

            fst[fdir[dir][i]] = st[fdir[dir][i]];
            fst[dir3[dir][i][0]] = st[dir3[dir][i][0]] + flen * dir3[dir][i][1];
            fst[dir] = st[dir] - ( st[dir] & ( ( 1 << sht ) - 1 ) );

            if( testFace( fst, flen, fdir[dir][i], mp[a], mp[b] ) == 0 )
            {
               // Dual edge does not pass face, let's make a new vertex


               fverts[i] = vertexBuffer.size();
               location[i] = fverts[i];
               nvert[i] = 1;
               OctreeNode* node = &node_list[node_index[i]];
               vertexBuffer.push_back( MeshVertex( float3( 0, 0, 0 ), node->averageNormal, node->material ) );



               hash->InsertKey( ( int ) ( node_index[a] ), ( int ) ( node_index[b] ), fverts[i], location[i] );


               hasFverts[i] = 1;
               needTess = 1;

            }
         }
      }
   }

   // Next, edge test
   int diag = 1;
   if( needTess == 0 )
   {
      // Even if all dual edges pass through faces, the dual complex of an edge may not be convex
      //int st2[3] = { st[0], st[1], st[2] } ;
      //st2[ dir ] += len ;

      diag = testEdge( st, len, dir, node_index, mp, node_list );
      if( diag == 0 )
      {
         // When this happens, we need to create an extra vertex on the primal edge
         needTess = 1;
      }
   }

   float cent[3];
   if( needTess )
   {

      makeEdgeVertex( st, len, dir, mp, cent );

      evert = vertexBuffer.size();
      //OctreeNode* node = &node_list[node_index[0]];

      vertexBuffer.push_back( MeshVertex( float3( cent[0], cent[1], cent[2] ), float3( 0, 0, 0 ), 0 ) );

   }

   int flipped[3];
   if( !flip )
   {
      for( i = 0; i < 3; i++ )
      {
         flipped[i] = i;
      }
   }
   else
   {
      for( i = 0; i < 3; i++ )
      {
         flipped[i] = 2 - i;
      }
   }

   // Finally, let's output triangle
   if( needTess == 0 )
   {
      // Normal splitting of quad
      if( diag == 1 )
      {
         if( node_index[0] != node_index[1] && node_index[1] != node_index[3] )
         {
            int tind1[] = { 0,1,3 };
            if( flip )
            {
               indexBuffer.push_back( ind[3] );
               indexBuffer.push_back( ind[1] );
               indexBuffer.push_back( ind[0] );
            }
            else
            {
               indexBuffer.push_back( ind[0] );
               indexBuffer.push_back( ind[1] );
               indexBuffer.push_back( ind[3] );
            }
         }

         if( node_index[3] != node_index[2] && node_index[2] != node_index[0] )
         {
            int tind2[] = { 3,2,0 };
            if( flip )
            {
               indexBuffer.push_back( ind[3] );
               indexBuffer.push_back( ind[2] );
               indexBuffer.push_back( ind[0] );
            }
            else
            {
               indexBuffer.push_back( ind[0] );
               indexBuffer.push_back( ind[2] );
               indexBuffer.push_back( ind[3] );
            }
         }
      }
      else
      {
         if( node_index[0] != node_index[1] && node_index[1] != node_index[2] )
         {
            int tind1[] = { 0,1,2 };

            for( int j = 0; j < 3; j++ )
            {
               indexBuffer.push_back( ind[tind1[flipped[j]]] );
            }
         }

         if( node_index[1] != node_index[3] && node_index[3] != node_index[2] )
         {
            int tind2[] = { 1,3,2 };


            for( int j = 0; j < 3; j++ )
            {
               indexBuffer.push_back( ind[tind2[flipped[j]]] );
            }
         }
      }
   }
   else
   {
      int nnbr[4][2] = { { 0,1 },{ 1,3 },{ 3,2 },{ 2,0 } };

      // Center-splitting
      for( i = 0; i < 4; i++ )
      {
         int a = nnbr[i][0];
         int b = nnbr[i][1];

         if( hasFverts[i] )
         {
            // Further split each triangle into two

            if( !flip )
            {
               indexBuffer.push_back( ind[a] );
               indexBuffer.push_back( fverts[i] );
               indexBuffer.push_back( evert );

               indexBuffer.push_back( evert );
               indexBuffer.push_back( fverts[i] );
               indexBuffer.push_back( ind[b] );

            }
            else
            {


               indexBuffer.push_back( evert );
               indexBuffer.push_back( fverts[i] );
               indexBuffer.push_back( ind[a] );

               indexBuffer.push_back( ind[b] );
               indexBuffer.push_back( fverts[i] );
               indexBuffer.push_back( evert );
            }



            // Update geometric location of the face vertex
            MeshVertex* vertex = &vertexBuffer[location[i]];

            if( nvert[i] >= 1 )
            {
               vertex->m_xyz.x = cent[0];
               vertex->m_xyz.y = cent[1];
               vertex->m_xyz.z = cent[2];
            }
            else
            {
               vertex->m_xyz.x = ( vertex->m_xyz.x + cent[0] ) / 2;
               vertex->m_xyz.y = ( vertex->m_xyz.y + cent[1] ) / 2;
               vertex->m_xyz.z = ( vertex->m_xyz.z + cent[2] ) / 2;
            }
         }
         else
         {
            // For one triangle with center vertex
            if( node_index[a] != node_index[b] )
            {

               if( !flip )
               {
                  indexBuffer.push_back( ind[a] );
                  indexBuffer.push_back( ind[b] );
                  indexBuffer.push_back( evert );
               }
               else
               {
                  indexBuffer.push_back( evert );
                  indexBuffer.push_back( ind[b] );
                  indexBuffer.push_back( ind[a] );
               }
            }
         }
      }
   }

}

int testFace( int st[3], int len, int dir, float v1[3], float v2[3] )
{
   float vec[3] = { v2[0] - v1[0], v2[1] - v1[1], v2[2] - v1[2] };
   float ax1[3], ax2[3];
   float ed1[3] = { 0,0,0 }, ed2[3] = { 0,0,0 };
   ed1[( dir + 1 ) % 3] = 1;
   ed2[( dir + 2 ) % 3] = 1;

   Intersection::cross( ed1, vec, ax1 );
   Intersection::cross( ed2, vec, ax2 );

   Intersection::Triangle* t1 = new Intersection::Triangle;
   Intersection::Triangle* t2 = new Intersection::Triangle;

   for( int i = 0; i < 3; i++ )
   {

      t1->vt[0][i] = v1[i];
      t1->vt[1][i] = v2[i];
      t1->vt[2][i] = v2[i];

      t2->vt[0][i] = st[i];
      t2->vt[1][i] = st[i];
      t2->vt[2][i] = st[i];
   }
   t2->vt[1][( dir + 1 ) % 3] += len;
   t2->vt[2][( dir + 2 ) % 3] += len;

   if( Intersection::separating( ax1, t1, t2 ) || Intersection::separating( ax2, t1, t2 ) )
   {
      delete t1;
      delete t2;
      //faceVerts++;
      /*
      printf("\n{{{%d, %d, %d},%d,%d},{{%f, %f, %f},{%f, %f, %f}}}\n",
      st[0],st[1], st[2],
      len, dir+1,
      v1[0], v1[1], v1[2],
      v2[0], v2[1], v2[2]) ;
      */
      return 0;
   }
   else
   {
      delete t1;
      delete t2;
      return 1;
   }
};

int testEdge( int st[3], int len, int dir, int32_t node_index[4], float v[4][3], std::vector<OctreeNode>& node_list )
{

   if( node_index[0] == node_index[1] || node_index[1] == node_index[3] || node_index[3] == node_index[2] || node_index[2] == node_index[0] )
   {
      //		return 1 ;
   }

   float p1[3] = { st[0], st[1], st[2] };
   float p2[3] = { st[0], st[1], st[2] };
   p2[dir] += len;

   int nbr[] = { 0,1,3,2,0 };
   int nbr2[] = { 3,2,0,1,3 };
   float nm[3], vec1[4][3], vec2[4][3];
   float d1, d2;

   for( int i = 0; i < 4; i++ )
   {
      for( int j = 0; j < 3; j++ )
      {
         vec1[i][j] = v[i][j] - p1[j];
         vec2[i][j] = v[i][j] - p2[j];
      }
   }

   Intersection::Triangle* t1 = new Intersection::Triangle;
   int tri[2][2][4] = { { { 0,1,3,2 },{ 3,2,0,1 } },{ { 2,0,1,3 },{ 1,3,2,0 } } };
   for( int i = 0; i < 2; i++ )
   {
      // For each triangulation
      for( int j = 0; j < 2; j++ )
      {
         // Starting with each triangle
         int k;

         // Check triangle and dual edge
         float vec1[3], vec2[3], vec3[3], vec[3];
         for( k = 0; k < 3; k++ )
         {
            t1->vt[0][k] = v[tri[i][j][0]][k];
            t1->vt[1][k] = v[tri[i][j][1]][k];
            t1->vt[2][k] = v[tri[i][j][2]][k];

            vec1[k] = t1->vt[1][k] - t1->vt[0][k];
            vec2[k] = t1->vt[2][k] - t1->vt[1][k];
         }

         float axes[3];
         Intersection::cross( vec1, vec2, axes );

         if( Intersection::separating( axes, t1, p1, p2 ) )
         {
            continue;
         }

         // Check diagonal and the other triangle
         for( k = 0; k < 3; k++ )
         {
            t1->vt[0][k] = p1[k];
            t1->vt[1][k] = p2[k];
            t1->vt[2][k] = v[tri[i][j][3]][k];

            vec1[k] = t1->vt[1][k] - t1->vt[0][k];
            vec2[k] = t1->vt[2][k] - t1->vt[1][k];
            vec3[k] = t1->vt[0][k] - t1->vt[2][k];

            vec[k] = v[tri[i][j][2]][k] - v[tri[i][j][0]][k];
         }

         float axes1[3], axes2[3], axes3[3];
         Intersection::cross( vec1, vec, axes1 );
         Intersection::cross( vec2, vec, axes2 );
         Intersection::cross( vec3, vec, axes3 );

         if( Intersection::separating( axes1, t1, v[tri[i][j][0]], v[tri[i][j][2]] ) ||
             Intersection::separating( axes2, t1, v[tri[i][j][0]], v[tri[i][j][2]] ) ||
             Intersection::separating( axes3, t1, v[tri[i][j][0]], v[tri[i][j][2]] ) )
         {
            continue;
         }

         return ( i + 1 );
      }
   }


   return 1;
};


void makeEdgeVertex( int st[3], int len, int dir, float mp[4][3], float v[3] )
{
   int nlen = len / 2;
   v[0] = st[0];
   v[1] = st[1];
   v[2] = st[2];
   v[dir] += nlen;

   /* Barycentric approach */
   float pt[4][2], pv[4], x[2];
   int dir2 = ( dir + 1 ) % 3;
   int dir3 = ( dir2 + 1 ) % 3;
   int seq[] = { 0,1,3,2 };
   float epsilon = 0.000001f;

   for( int i = 0; i < 4; i++ )
   {
      pt[i][0] = mp[seq[i]][dir2];
      pt[i][1] = mp[seq[i]][dir3];
      pv[i] = mp[seq[i]][dir];
   }
   x[0] = st[dir2];
   x[1] = st[dir3];

   // Compute mean-value interpolation

   float vec[4][2], d[4];
   for( int i = 0; i < 4; i++ )
   {
      vec[i][0] = pt[i][0] - x[0];
      vec[i][1] = pt[i][1] - x[1];

      d[i] = sqrt( vec[i][0] * vec[i][0] + vec[i][1] * vec[i][1] );

      if( d[i] < epsilon )
      {

         v[dir] = pv[i];
         if( v[dir] < st[dir] )
         {
            v[dir] = st[dir];
         }
         else if( v[dir] > st[dir] + len )
         {
            v[dir] = st[dir] + len;
         }

         return;
      }
   }

   float w[4] = { 0,0,0,0 }, totw = 0;
   for( int i = 0; i < 4; i++ )
   {
      int i2 = ( i + 1 ) % 4;
      float sine = ( vec[i][0] * vec[i2][1] - vec[i][1] * vec[i2][0] ) / ( d[i] * d[i2] );
      float cosine = ( vec[i][0] * vec[i2][0] + vec[i][1] * vec[i2][1] ) / ( d[i] * d[i2] );

      if( fabs( cosine + 1 ) < epsilon )
      {
         v[dir] = ( pv[i] * d[i2] + pv[i2] * d[i] ) / ( d[i] + d[i2] );

         if( v[dir] < st[dir] )
         {
            v[dir] = st[dir];
         }
         else if( v[dir] > st[dir] + len )
         {
            v[dir] = st[dir] + len;
         }

         return;
      }

      float tan2 = sine / ( 1 + cosine );

      w[i] += ( tan2 / d[i] );
      w[i2] += ( tan2 / d[i2] );

      totw += ( tan2 / d[i] );
      totw += ( tan2 / d[i2] );
   }

   v[dir] = 0;
   for( int i = 0; i < 4; i++ )
   {
      v[dir] += ( w[i] * pv[i] / totw );
   }
   /**/
   if( v[dir] < st[dir] )
   {
      v[dir] = st[dir];
   }
   else if( v[dir] > st[dir] + len )
   {
      v[dir] = st[dir] + len;
   }

};

