#ifndef		HAS_OCTREE_H_BEEN_INCLUDED1
#define		HAS_OCTREE_H_BEEN_INCLUDED1
//* This is a 3D Manifold Dual Contouring implementation
//* It's a big mess with debug and testing code everywhere, along with comments that don't match.
//* But it's the only published implementation out there at this time, so it's better than nothing.
//* As of now, everything is working. Full vertex clustering with and without manifold criterion!
//*
//* Current issues:
//* - NONE! :D
//*
//* TODO is cleanup and comment
//*/
//
#include <stdint.h>
#include <boost/make_shared.hpp>
#include "mgl/MathGeoLib.h"
#include "qef.h"
#include "utilities.h"

enum NodeType : uint8_t
{
   Internal,
   Leaf
};

struct VertexPositionNormal
{
   float3 position;
   float3 normal;
};

class Vertex
{
public:
   boost::shared_ptr<Vertex> m_pParent;
   int m_index;
   float3 m_normal;
   float3 m_position;
   int m_surface_index;
   //Vector3 Position{ get{ if( qef != null ) return qef.Solve( 1e-6f, 4, 1e-6f ); return Vector3.Zero; } }
   float m_error;
   int m_euler;
   int m_eis[12];
   int m_in_cell;
   QefSolver m_qef;
   bool m_face_prop2;
   bool m_collapsible;
   
   Vertex()
   {
      m_qef.reset();
      memset( m_eis, 0, 4 * 12 );
      m_pParent = nullptr;
      m_index = -1;
      m_euler = 0;
      m_collapsible = true;
      m_position = float3::zero;
      m_normal = float3::zero;
      m_surface_index = -1;
      m_in_cell = 0;
      m_error = 0;
   }
};


class OctreeNodeMdc
{
public:
   int      m_index;
   int32_t  m_childIndex[8];
   uint8_t  m_corners;
   NodeType m_type;
   uint8_t  m_gridX;
   uint8_t  m_gridY;
   uint8_t  m_gridZ;
   uint8_t  m_size;
   int8_t  m_child_index;

   std::vector< boost::shared_ptr< Vertex > > m_vertices;

   OctreeNodeMdc()
      : m_gridX( 0 )
      , m_gridY( 0 )
      , m_gridZ( 0 )
      , m_corners( 0 )
      , m_size( 0 )
      , m_type( NodeType::Leaf )
      , m_index( 0 )
      , m_child_index( 0 )
   {
      for( int i = 0; i < 8; i++ )
      {
         m_childIndex[i] = -1;
      }
   }

   OctreeNodeMdc( float3 inPosition, int inSize, NodeType inType )
      : m_index( 0 )
      , m_size( inSize )
      , m_type( inType )
      , m_corners( 0 )
      , m_gridX( 0 )
      , m_gridY( 0 )
      , m_gridZ( 0 )
      , m_child_index( 0 )
   {
      // init childindices to -1 (empty)
      for( int i = 0; i < 8; i++ )
      {
         m_childIndex[i] = -1;
      }
   }

   void GenerateVertexBuffer( std::vector<VertexPositionNormal>& vertices,
                              std::vector< OctreeNodeMdc >& nodeList )
   {

      if( m_type != NodeType::Leaf )
      {
         for( int i = 0; i < 8; i++ )
         {
            if( m_childIndex[i] != -1 )
            {
               auto node = &nodeList[m_childIndex[i]];
               node->GenerateVertexBuffer( vertices, nodeList );
            }

         }
      }

      if( m_vertices.size() == 0 )
         return;
      for( std::size_t i = 0; i < m_vertices.size(); i++ )
      {
         if( m_vertices[i] == nullptr )
            continue;
         m_vertices[i]->m_index = vertices.size();
         Vec3 outPos;
         m_vertices[i]->m_qef.solve( outPos, 1e-6f, 4, 1e-6f );
         VertexPositionNormal vertex;
         vertex.position.x = outPos.x;
         vertex.position.y = outPos.y;
         vertex.position.z = outPos.z;
         vertex.normal.x = m_vertices[i]->m_normal.x;
         vertex.normal.y = m_vertices[i]->m_normal.y;
         vertex.normal.z = m_vertices[i]->m_normal.z;

         vertices.push_back( vertex );

      }

   }

   void ProcessCell( std::vector<unsigned int>& indexes,
                     std::vector<int>& tri_count,
                     float threshold,
                     std::vector<  OctreeNodeMdc  >& nodeList )
   {
      if( m_type == NodeType::Internal )
      {
         for( int i = 0; i < 8; i++ )
         {
            if( m_childIndex[i] != -1 )
            {
               auto node = &nodeList[m_childIndex[i]];
               node->ProcessCell( indexes, tri_count, threshold, nodeList );
            }

         }

         for( int i = 0; i < 12; i++ )
         {
            int32_t face_nodes[2];

            int c1 = Utilities::TEdgePairs[i][0];
            int c2 = Utilities::TEdgePairs[i][1];

            face_nodes[0] = m_childIndex[c1];
            face_nodes[1] = m_childIndex[c2];

            ProcessFace( face_nodes, Utilities::TEdgePairs[i][2], indexes, tri_count, threshold, nodeList );
         }

         for( int i = 0; i < 6; i++ )
         {
            int32_t edge_nodes[4] =
            {
               m_childIndex[Utilities::TCellProcEdgeMask[i][0]],
               m_childIndex[Utilities::TCellProcEdgeMask[i][1]],
               m_childIndex[Utilities::TCellProcEdgeMask[i][2]],
               m_childIndex[Utilities::TCellProcEdgeMask[i][3]]
            };

            ProcessEdge( edge_nodes, Utilities::TCellProcEdgeMask[i][4], indexes, tri_count, threshold, nodeList );
         }
      }
   }

   void ProcessFace( int32_t nodes[2],
                     int direction,
                     std::vector<unsigned int>& indexes,
                     std::vector<int>& tri_count,
                     float threshold,
                     std::vector<  OctreeNodeMdc >& nodeList )
   {
      if( nodes[0] == -1 || nodes[1] == -1 )
         return;

      auto node0 = &nodeList[nodes[0]];
      auto node1 = &nodeList[nodes[1]];

      if( node0->m_type != NodeType::Leaf ||
          node1->m_type != NodeType::Leaf )
      {
         for( int i = 0; i < 4; i++ )
         {
            int32_t face_nodes[2];

            for( int j = 0; j < 2; j++ )
            {
               auto node = &nodeList[nodes[j]];
               if( node->m_type == NodeType::Leaf )
                  face_nodes[j] = nodes[j];
               else
                  face_nodes[j] = node->m_childIndex[Utilities::TFaceProcFaceMask[direction][i][j]];
            }

            ProcessFace( face_nodes, Utilities::TFaceProcFaceMask[direction][i][2], indexes, tri_count, threshold, nodeList );
         }

         int orders[2][4] =
         {
            { 0, 0, 1, 1 },
            { 0, 1, 0, 1 },
         };

         for( int i = 0; i < 4; i++ )
         {
            int32_t edge_nodes[4];

            for( int j = 0; j < 4; j++ )
            {
               int32_t nodeIndex = nodes[orders[Utilities::TFaceProcEdgeMask[direction][i][0]][j]];
               auto node = &nodeList[nodeIndex];
               if( node->m_type == NodeType::Leaf )
                  edge_nodes[j] = nodeIndex;
               else
                  edge_nodes[j] = node->m_childIndex[Utilities::TFaceProcEdgeMask[direction][i][1 + j]];
            }
            ProcessEdge( edge_nodes, Utilities::TFaceProcEdgeMask[direction][i][5], indexes, tri_count, threshold, nodeList );
         }
      }
   }

   void ProcessEdge( int32_t nodes[4],
                     int direction,
                     std::vector<unsigned int>& indexes,
                     std::vector<int>& tri_count,
                     float threshold,
                     std::vector<  OctreeNodeMdc >& nodeList )
   {
      if( nodes[0] == -1 || nodes[1] == -1 || nodes[2] == -1 || nodes[3] == -1 )
         return;

      auto node0 = &nodeList[nodes[0]];
      auto node1 = &nodeList[nodes[1]];
      auto node2 = &nodeList[nodes[2]];
      auto node3 = &nodeList[nodes[3]];

      if( node0->m_type == NodeType::Leaf &&
          node1->m_type == NodeType::Leaf &&
          node2->m_type == NodeType::Leaf &&
          node3->m_type == NodeType::Leaf )
      {
         ProcessIndexes( nodes, direction, indexes, tri_count, threshold, nodeList );
      }
      else
      {
         for( int i = 0; i < 2; i++ )
         {
            int32_t edge_nodes[4];

            for( int j = 0; j < 4; j++ )
            {
               auto node = &nodeList[nodes[j]];
               if( node->m_type == NodeType::Leaf )
                  edge_nodes[j] = nodes[j];
               else
                  edge_nodes[j] = node->m_childIndex[Utilities::TEdgeProcEdgeMask[direction][i][j]];
            }

            ProcessEdge( edge_nodes, Utilities::TEdgeProcEdgeMask[direction][i][4], indexes, tri_count, threshold, nodeList );
         }
      }
   }

   void ProcessIndexes( int32_t nodes[4],
                        int direction,
                        std::vector<unsigned int>& indexes,
                        std::vector<int>& tri_count,
                        float threshold,
                        std::vector<  OctreeNodeMdc  >& nodeList )
   {
      int min_size = 10000000;
      int min_index = 0;
      int indices[4] = { -1, -1, -1, -1 };
      bool flip = false;
      bool sign_changed = false;
      int v_count = 0;

      for( int i = 0; i < 4; i++ )
      {
         int edge = Utilities::TProcessEdgeMask[direction][i];
         int c1 = Utilities::TEdgePairs[edge][0];
         int c2 = Utilities::TEdgePairs[edge][1];

         auto node = nodeList[nodes[i]];

         int m1 = ( node.m_corners >> c1 ) & 1;
         int m2 = ( node.m_corners >> c2 ) & 1;

         if( node.m_size < min_size )
         {
            min_size = node.m_size;
            min_index = i;
            flip = m1 == 1;
            sign_changed = ( ( m1 == 0 && m2 != 0 ) ||
               ( m1 != 0 && m2 == 0 ) );
         }

         //find the vertex index
         int index = 0;
         bool skip = false;

         for( int k = 0; k < 16; k++ )
         {
            int e = Utilities::TransformedEdgesTable[node.m_corners][k];
            if( e == -1 )
            {
               index++;
               continue;
            }
            if( e == -2 )
            {
               skip = true;
               break;
            }
            if( e == edge )
               break;
         }

         if( skip )
            continue;

         v_count++;
         // not sure i ported this correctly
         if( index >= node.m_vertices.size() )
            return;
         boost::shared_ptr<Vertex> v = node.m_vertices[index];
         boost::shared_ptr<Vertex> highest = v;
         while( highest->m_pParent != nullptr )
         {
            //assert( highest->m_pParent->m_error >= 0 );
            if( ( highest->m_pParent->m_error <= threshold
                       && ( ( highest->m_pParent->m_euler == 1 && highest->m_pParent->m_face_prop2 ) ) ) )
            {
               highest = highest->m_pParent;
               v = highest;
            }
            else
               highest = highest->m_pParent;
         }

         indices[i] = v->m_index;

      }

      /*
      * Next generate the triangles.
      * Because we're generating from the finest levels that were collapsed, many triangles will collapse to edges or vertices.
      * That's why we check if the indices are different and discard the triangle, as mentioned in the paper.
      */
      if( sign_changed )
      {
         int count = 0;
         if( !flip )
         {
            if( indices[0] != -1 &&
                indices[1] != -1 &&
                indices[2] != -1 &&
                indices[0] != indices[1] &&
                indices[1] != indices[3] )
            {
               indexes.push_back( indices[0] );
               indexes.push_back( indices[1] );
               indexes.push_back( indices[3] );
               count++;
            }

            if( indices[0] != -1 &&
                indices[2] != -1 &&
                indices[3] != -1 &&
                indices[0] != indices[2] &&
                indices[2] != indices[3] )
            {
               indexes.push_back( indices[0] );
               indexes.push_back( indices[3] );
               indexes.push_back( indices[2] );
               count++;
            }
         }
         else
         {
            if( indices[0] != -1 &&
                indices[3] != -1 &&
                indices[1] != -1 &&
                indices[0] != indices[1] &&
                indices[1] != indices[3] )
            {
               indexes.push_back( indices[0] );
               indexes.push_back( indices[3] );
               indexes.push_back( indices[1] );
               count++;
            }

            if( indices[0] != -1 &&
                indices[2] != -1 &&
                indices[3] != -1 &&
                indices[0] != indices[2] &&
                indices[2] != indices[3] )
            {
               indexes.push_back( indices[0] );
               indexes.push_back( indices[2] );
               indexes.push_back( indices[3] );
               count++;
            }
         }

         if( count > 0 )
            tri_count.push_back( count );
      }
   }

   void ClusterCellBase( float error,
                         std::vector<  OctreeNodeMdc >& nodeList )
   {
      if( m_type != NodeType::Internal )
         return;

      for( int i = 0; i < 8; i++ )
      {
         if( m_childIndex[i] == -1 )
            continue;

         auto node = &nodeList[m_childIndex[i]];
         node->ClusterCell( error, nodeList );
      }
   }

   /*
   * Cell stage
   */
   void ClusterCell( float error,
                     std::vector<  OctreeNodeMdc  >& nodeList )
   {
      if( m_type != NodeType::Internal )
         return;

      // First cluster all the children nodes

      //bool is_collapsible = true;
      for( int i = 0; i < 8; i++ )
      {
         if( m_childIndex[i] == -1 )
            continue;


         auto node = &nodeList[m_childIndex[i]];

         if( node->m_type == NodeType::Leaf )
            continue;

         node->ClusterCell( error, nodeList );
      }

      int surface_index = 0;
      std::vector < boost::shared_ptr< Vertex > > collected_vertices;
      std::vector < boost::shared_ptr< Vertex > > new_vertices;

      /*
      * Find all the surfaces inside the children that cross the 6 Euclidean edges and the vertices that connect to them
      */
      for( int i = 0; i < 12; i++ )
      {
         int32_t face_nodes[2] = { -1, -1 };

         int c1 = Utilities::TEdgePairs[i][0];
         int c2 = Utilities::TEdgePairs[i][1];
         face_nodes[0] = m_childIndex[c1];
         face_nodes[1] = m_childIndex[c2];

         ClusterFace( face_nodes, Utilities::TEdgePairs[i][2], surface_index, collected_vertices, nodeList );
      }

      for( int i = 0; i < 6; i++ )
      {
         int32_t edge_nodes[4] =
         {
            m_childIndex[Utilities::TCellProcEdgeMask[i][0]],
            m_childIndex[Utilities::TCellProcEdgeMask[i][1]],
            m_childIndex[Utilities::TCellProcEdgeMask[i][2]],
            m_childIndex[Utilities::TCellProcEdgeMask[i][3]]
         };

         ClusterEdge( edge_nodes, Utilities::TCellProcEdgeMask[i][4], surface_index, collected_vertices, nodeList );
      }

      int highest_index = surface_index;

      if( highest_index == -1 )
         highest_index = 0;

      for( int i = 0; i < 8; i++ )
      {

         if( m_childIndex[i] == -1 )
            continue;

         auto node = nodeList[m_childIndex[i]];

         for( auto v : m_vertices )
         {
            if( v == nullptr )
               continue;
            if( v->m_surface_index == -1 )
            {
               v->m_surface_index = highest_index++;
               collected_vertices.push_back( v );
            }
         }
      }

      int clustered_count = 0;
      if( collected_vertices.size() > 0 )
      {
         for( int i = 0; i <= highest_index; i++ )
         {
            QefSolver qef;
            float3 normal = float3( 0, 0, 0 );
            int count = 0;
            int edges[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
            int euler = 0;
            int e = 0;
            for( auto v : collected_vertices )
            {
               if( v->m_surface_index == i )
               {
                  /* Calculate ei(Sv) */
                  for( int k = 0; k < 3; k++ )
                  {
                     int edge = Utilities::TExternalEdges[v->m_in_cell][k];
                     edges[edge] += v->m_eis[edge];
                  }
                  /* Calculate e(Svk) */
                  for( int k = 0; k < 9; k++ )
                  {
                     int edge = Utilities::TInternalEdges[v->m_in_cell][k];
                     e += v->m_eis[edge];
                  }
                  euler += v->m_euler;
                  qef.add( v->m_qef.getData() );
                  normal += v->m_normal;
                  count++;
               }
            }

            /*
            * One vertex might have an error greater than the threshold, preventing simplification.
            * When it's just one, we can ignore the error and proceed.
            */

            if( count == 0 )
            {
               continue;
            }

            bool face_prop2 = true;
            for( int f = 0; f < 6 && face_prop2; f++ )
            {
               int intersections = 0;
               for( int ei = 0; ei < 4; ei++ )
               {
                  intersections += edges[Utilities::TFaces[f][ei]];
               }
               if( !( intersections == 0 || intersections == 2 ) )
                  face_prop2 = false;
            }

            boost::shared_ptr< Vertex > new_vertex = boost::make_shared< Vertex >();
            normal /= ( float ) count;
            normal.Normalize();
            new_vertex->m_normal = normal;
            new_vertex->m_qef.add( qef.getData() );
            memcpy( new_vertex->m_eis, edges, 12 * 4 );
            new_vertex->m_euler = euler - e / 4;

            new_vertex->m_in_cell = m_child_index;
            new_vertex->m_face_prop2 = face_prop2;

            Vec3 outVec;

            new_vertex->m_qef.solve( outVec, 1e-6f, 4, 1e-6f );

            new_vertex->m_position.x = outVec.x;
            new_vertex->m_position.y = outVec.y;
            new_vertex->m_position.z = outVec.z;

            float err = new_vertex->m_qef.getError();

            assert( err >= 0 );

            new_vertex->m_collapsible = err <= error;
            new_vertex->m_error = err;

            new_vertices.push_back( new_vertex );


            clustered_count++;

            for( auto v : collected_vertices )
            {
               if( v->m_surface_index == i )
               {

                  v->m_pParent = new_vertex;

               }
            }
         }
      }
      else
      {
         return;
      }

      for( auto v2 : collected_vertices )
      {
         v2->m_surface_index = -1;
      }
      m_vertices = new_vertices;
   }

   void GatherVertices( int32_t n,
                        std::vector<boost::shared_ptr<Vertex>>& dest,
                        int& surface_index,
                        std::vector<  OctreeNodeMdc >& nodeList )
   {
      if( n == -1 )
         return;
      auto node = &nodeList[n];

      if( node->m_size > 1 )
      {
         for( int i = 0; i < 8; i++ )
            GatherVertices( node->m_childIndex[i], dest, surface_index, nodeList );
      }
      else
      {
         for( auto v : node->m_vertices )
         {
            if( v->m_surface_index == -1 )
            {
               v->m_surface_index = surface_index++;
               dest.push_back( v );
            }
         }
      }
   }

   void ClusterFace( int32_t nodes[2],
                     int direction,
                     int& surface_index,
                     std::vector < boost::shared_ptr< Vertex > >& collected_vertices,
                     std::vector< OctreeNodeMdc >& nodeList )
   {
      if( nodes[0] == -1 || nodes[1] == -1 )
         return;

      auto node0 = &nodeList[nodes[0]];
      auto node1 = &nodeList[nodes[1]];

      if( node0->m_type != NodeType::Leaf ||
          node1->m_type != NodeType::Leaf )
      {
         for( int i = 0; i < 4; i++ )
         {
            int32_t face_nodes[2] = { -1, -1 };

            for( int j = 0; j < 2; j++ )
            {
               if( nodes[j] == -1 )
                  continue;
               auto node = &nodeList[nodes[j]];
               if( node->m_type != NodeType::Internal )
                  face_nodes[j] = nodes[j];
               else
                  face_nodes[j] = node->m_childIndex[Utilities::TFaceProcFaceMask[direction][i][j]];
            }
            ClusterFace( face_nodes, Utilities::TFaceProcFaceMask[direction][i][2], surface_index, collected_vertices, nodeList );
         }
      }

      int orders[2][4] =
      {
         { 0, 0, 1, 1 },
         { 0, 1, 0, 1 },
      };

      for( int i = 0; i < 4; i++ )
      {
         int32_t edge_nodes[4] = { -1, -1, -1, -1 };
         for( int j = 0; j < 4; j++ )
         {
            int32_t nodeIndex = nodes[orders[Utilities::TFaceProcEdgeMask[direction][i][0]][j]];
            if( nodeIndex == -1 )
               continue;
            auto node = &nodeList[nodeIndex];

            if( node->m_type != NodeType::Internal )
               edge_nodes[j] = nodeIndex;
            else
               edge_nodes[j] = node->m_childIndex[Utilities::TFaceProcEdgeMask[direction][i][1 + j]];
         }
         ClusterEdge( edge_nodes, Utilities::TFaceProcEdgeMask[direction][i][5], surface_index, collected_vertices, nodeList );
      }
   }

   void ClusterEdge( int32_t nodes[4],
                     int direction,
                     int& surface_index,
                     std::vector < boost::shared_ptr< Vertex > >& collected_vertices,
                     std::vector< OctreeNodeMdc >& nodeList )
   {
      if( ( nodes[0] == -1 ||
            nodeList[nodes[0]].m_type != NodeType::Internal ) &&
          ( nodes[1] == -1 ||
            nodeList[nodes[1]].m_type != NodeType::Internal ) &&
          ( nodes[2] == -1 ||
            nodeList[nodes[2]].m_type != NodeType::Internal ) &&
          ( nodes[3] == -1 ||
            nodeList[nodes[3]].m_type != NodeType::Internal ) )
      {
         ClusterIndexes( nodes, direction, surface_index, collected_vertices, nodeList );
      }
      else
      {
         for( int i = 0; i < 2; i++ )
         {
            int32_t edge_nodes[4] = { -1, -1, -1, -1 };

            for( int j = 0; j < 4; j++ )
            {
               if( nodes[j] == -1 )
                  continue;

               auto node = &nodeList[nodes[j]];
               if( node->m_type == NodeType::Leaf )
                  edge_nodes[j] = nodes[j];
               else
                  edge_nodes[j] = node->m_childIndex[Utilities::TEdgeProcEdgeMask[direction][i][j]];
            }
            ClusterEdge( edge_nodes, Utilities::TEdgeProcEdgeMask[direction][i][4], surface_index, collected_vertices, nodeList );
         }
      }
   }

   void ClusterIndexes( int32_t nodes[4],
                        int direction,
                        int& max_surface_index,
                        std::vector < boost::shared_ptr< Vertex > >& collected_vertices,
                        std::vector<  OctreeNodeMdc >& nodeList )
   {
      if( nodes[0] == -1 &&
          nodes[1] == -1 &&
          nodes[2] == -1 &&
          nodes[3] == -1 )
         return;

      boost::shared_ptr< Vertex > vertices[4];
      int v_count = 0;
      int node_count = 0;

      for( int i = 0; i < 4; i++ )
      {
         if( nodes[i] == -1 )
            continue;
         node_count++;

         int edge = Utilities::TProcessEdgeMask[direction][i];
         int c1 = Utilities::TEdgePairs[edge][0];
         int c2 = Utilities::TEdgePairs[edge][1];

         auto node = &nodeList[nodes[i]];

         int m1 = ( node->m_corners >> c1 ) & 1;
         int m2 = ( node->m_corners >> c2 ) & 1;

         //find the vertex index
         int index = 0;
         bool skip = false;
         for( int k = 0; k < 16; k++ )
         {
            int e = Utilities::TransformedEdgesTable[node->m_corners][k];
            if( e == -1 )
            {
               index++;
               continue;
            }
            if( e == -2 )
            {
               if( !( ( m1 == 0 && m2 != 0 ) || ( m1 != 0 && m2 == 0 ) ) )
                  skip = true;
               break;
            }
            if( e == edge )
               break;
         }

         if( !skip && index < node->m_vertices.size() )
         {
            vertices[i] = node->m_vertices[index];
            while( vertices[i]->m_pParent != nullptr )
               vertices[i] = vertices[i]->m_pParent;
            v_count++;
         }
      }

      if( v_count == 0 )
         return;

      int surface_index = -1;

      for( int i = 0; i < 4; i++ )
      {

         if( vertices[i] == nullptr )
            continue;

         if( vertices[i]->m_surface_index != -1 )
         {
            if( surface_index != -1 && surface_index != vertices[i]->m_surface_index )
            {
               AssignSurface( collected_vertices, vertices[i]->m_surface_index, surface_index );
            }
            else if( surface_index == -1 )
               surface_index = vertices[i]->m_surface_index;
         }
      }

      if( surface_index == -1 )
         surface_index = max_surface_index++;

      for( int i = 0; i < 4; i++ )
      {

         if( vertices[i] == nullptr )
            continue;

         if( vertices[i]->m_surface_index == -1 )
         {
            collected_vertices.push_back( vertices[i] );
         }
         vertices[i]->m_surface_index = surface_index;
      }
   }

   void AssignSurface( std::vector < boost::shared_ptr< Vertex > >& vertices, int from, int to )
   {
      for( auto v : vertices )
      {
         if( v != nullptr && v->m_surface_index == from )
            v->m_surface_index = to;
      }
   }
};
#endif