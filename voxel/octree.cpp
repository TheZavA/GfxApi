#include "octree.h"
#include <boost/scoped_ptr.hpp>
#include <stdint.h>
#include <unordered_map>
#include <boost/array.hpp>
#include "ChunkManager.h"


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

